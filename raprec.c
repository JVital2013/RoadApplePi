/*
 * Copyright 2018 James Vital
 * This software is licensed under the GNU General Public License
 *
 * This file is part of RoadApplePi.
 * RoadApplePi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RoadApplePi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with RoadApplePi.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <syslog.h>
#include <errno.h>
#include <linux/rtc.h>

#define RECORDING_DURATION 300
#define MAX_DISK_USAGE 80
#define MAX_OBD_SIZE 100

//Global variables
static volatile sig_atomic_t ffmpegPid = 0, recMgr = 0, obdLogger = 0, recRstrtFlag = 1, loggingOBD = 1, obdReset = 0;

//Function definitions
int compareLLong(const void* a, const void* b);
float calcDiskUsage();
void rapCleanup();
int set_interface_attribs (int fd, int speed);
void removechars(char* str, char c);
void obdCmd(int fd, char *cmd, char *resp);
int *getCodes(char *obdCode);

//Event handlers
static void recIntHandler(int signum __attribute__((unused)));
static void mainSigHandler(int signum __attribute__((unused)));
static void obdIntHandler(int signum __attribute__((unused)));
static void forwardToOBDLogger(int signum __attribute__((unused)));
static void fullObdDump(int signum __attribute__((unused)));
static void recChldHandler(int, siginfo_t *, void *);

//Main Function
int main()
{
	typedef struct sigaction SigAction;
	SigAction action, resetOBD;
	struct tm rtcTime;
	struct timeval setTime;
	pid_t waitRslt;
	int failCount, rtc;

	//Set up syslog
	openlog("RoadApplePi", 0, LOG_DAEMON);

	//Synchronize time with RTC
	failCount = 0;
	while(failCount < 10)
	{
		rtc = open("/dev/rtc", O_RDONLY);
		if(rtc == -1)
		{
			failCount++;
			continue;
		}
		if(ioctl(rtc, RTC_RD_TIME, &rtcTime) == -1)
		{
			close(rtc);
			failCount++;
			sleep(1);
		}
		else
		{
			close(rtc);
			setTime.tv_sec = timegm(&rtcTime);
			setTime.tv_usec = 0;
			settimeofday(&setTime, NULL);
			break;
		}
	}
	if(failCount == 10) syslog(LOG_WARNING, "Error: Could not synchronize with RTC. Continuing anyway\n");

	//**********************************************
	//Recording manager and associated child threads
	//**********************************************
	recMgr = (sig_atomic_t)fork();
	if(recMgr == 0)
	{
		int i, strLength, firstRun = 1;
		unsigned int startTime;
		char *fileStr;
		SigAction recIntAction, chldHandler;

		//Check if webcam exists
		failCount = 0;
		while(access("/dev/video0", F_OK) == -1 && failCount < 60)
		{
			sleep(1);
			failCount++;
		}
		if(failCount == 60)
		{
			syslog(LOG_ERR, "Error: No webcam found!\n");
			exit(1);
		}

		//Set UVC options for my webcam (MOST LIKELY WEBCAM SPECIFIC)
		system("uvcdynctrl -s 'Exposure, Auto' -- 3");
		system("uvcdynctrl -s 'Exposure, Auto Priority' -- 1");

		//When killed, kill ffmpeg
		sigfillset(&recIntAction.sa_mask);
		recIntAction.sa_handler = recIntHandler;
		sigaction(SIGINT, &recIntAction, 0);

		//Handle ffmpeg dying
		sigfillset(&chldHandler.sa_mask);
		chldHandler.sa_flags = SA_SIGINFO;
		chldHandler.sa_sigaction = recChldHandler;
		sigaction(SIGCHLD, &chldHandler, 0);

		//The eternal (until interrupted) recording loop
		while(1)
		{
			recRstrtFlag = 1;
			ffmpegPid = (sig_atomic_t)fork();

			if(ffmpegPid == 0)
			{
				//Redirect output of ffmpeg to the bit bucket
				int fd = open("/dev/null", O_WRONLY);
				dup2(fd, 1);
				dup2(fd, 2);
				close(fd);

				//Build filename as date in milliseconds
				struct timeval tv;
				gettimeofday(&tv, NULL);
				strLength = snprintf(NULL, 0, "/var/www/html/vids/%ld%03ld.mp4", tv.tv_sec, tv.tv_usec / 1000) + 1;
				fileStr = malloc(strLength);
				snprintf(fileStr, strLength, "/var/www/html/vids/%ld%03ld.mp4", tv.tv_sec, tv.tv_usec / 1000);

				execl("/usr/local/bin/ffmpeg", "ffmpeg", "-y", "-i", "/dev/video0", "-c:v", "h264_omx", "-b:v", "5M", fileStr, (char *)NULL);
			}

			//Let ffmpeg run for RECORDING_DURATION. Also do cache maintainence
			for(i = 0; i <= RECORDING_DURATION && recRstrtFlag; i++)
			{
				if(firstRun) firstRun = 0;
				else if(i == 0)
				{
					startTime = time(NULL);
					rapCleanup(); //This will kill obdLogger if it takes longer than 5 minutes to start. Probably a good thing
					i += time(NULL) - startTime;
				}
				sleep(1);
			}

			//Done recording, kill (and/or) wait for ffmpeg to close
			waitRslt = waitpid((pid_t)ffmpegPid, NULL, WNOHANG);
			if(waitRslt == 0)
			{
				kill((pid_t)ffmpegPid, SIGINT);
				waitpid((pid_t)ffmpegPid, NULL, 0);
			}
		}

		//Clean up Recording Manager thread
		exit(0);
	}

	//**********
	//OBD Logger
	//**********
	obdLogger = (sig_atomic_t)fork();
	if(obdLogger == 0)
	{
		unsigned long *resultLen;
		char *query, *obdAddr, obdResp[MAX_OBD_SIZE], lastResp[0xE1][MAX_OBD_SIZE], *obdCmdBuf, jsonCodes[452]; //225 Codes * 2 characters each + 1 (start bracket) + 1 (null terminator)
		const char *portname = "/dev/rfcomm0";
		int i, fd, cmdLength, offset, newCode = 0, *codeBatch, availableCodes[0xE1], supportedCodes[0xE1], checkCodes[0xE1];
		MYSQL mysql;
		MYSQL_RES *queryResult;
		MYSQL_ROW resultRow;
		struct timeval tv;

		mysql_library_init(0, NULL, NULL);
		mysql_init(&mysql);
		failCount = 0;
		while(!mysql_real_connect(&mysql, NULL, "roadapplepi", "roadapplepi", "roadapplepi", 0, NULL, 0) && failCount < 60)
		{
			sleep(1);
			failCount++;
		}
		if(failCount == 60)
		{
			syslog(LOG_ERR, "Error: Could not connect to MySQL Server\n");
			exit(1);
		}

		//Get OBD Bluetooth MAC Address
		mysql_query(&mysql, "SELECT value FROM env WHERE name='currentOBD'");
		queryResult = mysql_store_result(&mysql);
		if((resultRow = mysql_fetch_row(queryResult)))
		{
			resultLen = mysql_fetch_lengths(queryResult);
			obdAddr = malloc(resultLen[0]);
			strcpy(obdAddr, resultRow[0]);
		}
		else
		{
			syslog(LOG_ERR, "Error: No Bluetooth MAC address saved!\n");
			mysql_close(&mysql);
			mysql_library_end();
			exit(1);
		}
		mysql_free_result(queryResult);

		//Bind Bluetooth serial port
		cmdLength = snprintf(NULL, 0, "rfcomm bind 0 %s", obdAddr) + 1;
		query = malloc(cmdLength);
		snprintf(query, cmdLength, "rfcomm bind 0 %s", obdAddr);
		system(query);
		free(query);
		if(access(portname, F_OK) == -1)
		{
			syslog(LOG_ERR, "Error: Could not open COM port for OBD!\n");
			mysql_close(&mysql);
			mysql_library_end();
			exit(1);
		}

		//Initialize serial connection
		fd = open (portname, O_RDWR | O_NOCTTY | O_NDELAY);
		if (fd < 0)
		{
			syslog(LOG_ERR, "error %d opening %s: %s\n", errno, portname, strerror (errno));
			system("rfcomm release 0");
			mysql_close(&mysql);
			mysql_library_end();
			return -1;
		}
		if(set_interface_attribs (fd, B38400) != 0)
		{
			syslog(LOG_ERR, "There was an error setting up %s\n", portname);
			system("rfcomm release 0");
			mysql_close(&mysql);
			mysql_library_end();
			return -1;
		}

		//Initialize ELM327
		obdCmd(fd, "ATZ", NULL);

		//Get Available codes
		memset(availableCodes, 0, sizeof(availableCodes));
		availableCodes[0x0] = 1;
		for(offset = 0x0; offset < 0xE0; offset += 0x20) if(availableCodes[offset])
		{
			cmdLength = snprintf(NULL, 0, "01%02X1", offset) + 1;
			obdCmdBuf = malloc(cmdLength);
			snprintf(obdCmdBuf, cmdLength, "01%02X1", offset);
			obdCmd(fd, obdCmdBuf, obdResp);
			free(obdCmdBuf);

			codeBatch = getCodes(obdResp);
			memset(obdResp, 0, sizeof(obdResp));

			for(i = 1; i < 0x21; i++) availableCodes[i + offset] = codeBatch[i];
		}

		//Set OBD codes that this program currently supports	
		memset(supportedCodes, 0, sizeof(supportedCodes));
		supportedCodes[0x04] = 1; //Engine Load
		supportedCodes[0x05] = 1; //Coolant Temperature
		supportedCodes[0x06] = 1; //Fuel Trims
		supportedCodes[0x07] = 1; // |
		supportedCodes[0x08] = 1; // |
		supportedCodes[0x09] = 1; // v
		supportedCodes[0x0B] = 1; //Intake manifold absolute pressure
		supportedCodes[0x0C] = 1; //RPM
		supportedCodes[0x0D] = 1; //Vehicle Speed
		supportedCodes[0x0E] = 1; //Timing advance
		supportedCodes[0x0F] = 1; //Air Intake Temperature
		supportedCodes[0x11] = 1; //Throttle Position

		//AND the program-supported and car-supported codes together and disable every 20th code
		//Build JSON
		//Initialize lastResp
		strcpy (jsonCodes, "[");
		for(i = 0; i < 0xE1; i++)
		{
			checkCodes[i] = availableCodes[i] && supportedCodes[i];
			if(i % 20 == 0) checkCodes[i] = 0;
			sprintf(jsonCodes + strlen(jsonCodes), "%d,", checkCodes[i]);
			
			strcpy(lastResp[i], "\0");
		}

		//Save JSON to SQL
		jsonCodes[strlen(jsonCodes) - 1] = ']';
		gettimeofday(&tv, NULL);
		cmdLength = snprintf(NULL, 0, "INSERT INTO obdCodes (timestamp, codes) VALUES (%ld%03ld, \"%s\");", tv.tv_sec, tv.tv_usec / 1000, jsonCodes) + 1;
		query = malloc(cmdLength);
		snprintf(query, cmdLength, "INSERT INTO obdCodes (timestamp, codes) VALUES (%ld%03ld, \"%s\");", tv.tv_sec, tv.tv_usec / 1000, jsonCodes);
		mysql_query(&mysql, query);
		free(query);

		//When killed, kill OBD logging
		sigfillset(&action.sa_mask);
		action.sa_handler = obdIntHandler;
		sigaction(SIGINT, &action, 0);
		
		//When I receive SIGUSR1, save all values again
		sigfillset(&resetOBD.sa_mask);
		resetOBD.sa_handler = fullObdDump;
		resetOBD.sa_flags = SA_NODEFER;
		sigaction(SIGUSR1, &resetOBD, 0);

		//Infinite (Until Interrupted) loop
		while(loggingOBD)
		{
			//Initialize MySQL query	
			query = malloc(56);
			strcpy(query, "INSERT INTO obdLog (timestamp, mode, pid, value) VALUES");
		
			//Interpret Codes
			for(i = 0; i < 0xE1 && loggingOBD && !obdReset; i++) if(checkCodes[i])
			{
				//Run available and implemented OBD codes
				cmdLength = snprintf(NULL, 0, "01%02X1", i) + 1;
				obdCmdBuf = malloc(cmdLength);
				snprintf(obdCmdBuf, cmdLength, "01%02X1", i);
				obdCmd(fd, obdCmdBuf, obdResp);
				free(obdCmdBuf);

				//Parse codes if new
				if(strcmp(lastResp[i], obdResp) != 0)
				{
					strcpy(lastResp[i], obdResp);
					gettimeofday(&tv, NULL);
					offset = strlen(query);
					newCode = 1;
					
					switch(i)
					{
						//Calculated Engine Load and throttle position
						case 0x04: case 0x11:
						cmdLength = snprintf(NULL, 0, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 2.55)) + 1;
						query = realloc(query, offset + cmdLength);
						snprintf(query + offset, cmdLength, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 2.55));
						break;

						//Coolant Temperature and Air Intake Temperature
						case 0x05: case 0x0F:
						cmdLength = snprintf(NULL, 0, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)strtol(obdResp + 4, NULL, 16) - 40) + 1;
						query = realloc(query, offset + cmdLength);
						snprintf(query + offset, cmdLength, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)strtol(obdResp + 4, NULL, 16) - 40);
						break;

						//Fuel Trims
						case 0x06 ... 0x09:
						cmdLength = snprintf(NULL, 0, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 1.28) - 100) + 1;
						query = realloc(query, offset + cmdLength);
						snprintf(query + offset, cmdLength, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 1.28) - 100);
						break;

						//Intake manifold absolute pressure and speed
						case 0x0B: case 0x0D:
						cmdLength = snprintf(NULL, 0, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)strtol(obdResp + 4, NULL, 16)) + 1;
						query = realloc(query, offset + cmdLength);
						snprintf(query + offset, cmdLength, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)strtol(obdResp + 4, NULL, 16));
						break;

						//Engine RPM
						case 0x0C:
						cmdLength = snprintf(NULL, 0, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 4)) + 1;
						query = realloc(query, offset + cmdLength);
						snprintf(query + offset, cmdLength, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 4));
						break;

						//Timing advance
						case 0x0E:
						cmdLength = snprintf(NULL, 0, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 2) - 64) + 1;
						query = realloc(query, offset + cmdLength);
						snprintf(query + offset, cmdLength, " (%ld%03ld, 1, %d, \"%d\"),", tv.tv_sec, tv.tv_usec / 1000, i, (int)(strtol(obdResp + 4, NULL, 16) / 2) - 64);
						break;
					}
				}
				memset(obdResp, 0, sizeof(obdResp));
			}
			
			//Store in MySQL and free memory
			if(newCode)
			{
				query[strlen(query) - 1] = ';';
				mysql_query(&mysql, query);
				newCode = 0;
			}
			free(query);
			
			//We've been told to do a fresh dump of OBD information, so clear lastResp array
			if(obdReset)
			{
				memset(lastResp, 0, MAX_OBD_SIZE * 0xE1);
				obdReset = 0;
			}
		}

		//Clean up thread
		system("rfcomm release 0");
		mysql_close(&mysql);
		mysql_library_end();
		exit(0);
	}

	//Handle recieving interrupt
	sigfillset(&action.sa_mask);
	action.sa_handler = mainSigHandler;
	sigaction(SIGINT, &action, 0);

	//Forward SIGUSR1 to obdLogger
	sigfillset(&resetOBD.sa_mask);
	resetOBD.sa_flags = SA_NODEFER;
	resetOBD.sa_handler = forwardToOBDLogger;
	sigaction(SIGUSR1, &resetOBD, 0);

	//Run until all children terminate, ignoring unhandled signals
	do {waitRslt = waitpid((pid_t)recMgr, NULL, 0);} while(waitRslt == -1 && errno == EINTR);
	do {waitRslt = waitpid((pid_t)obdLogger, NULL, 0);} while(waitRslt == -1 && errno == EINTR);
	return 0;
}

//Sorting logic
int compareLLong(const void* a, const void* b)
{
	if(*(long long*)a == *(long long*)b) return 0;
	return *(long long*)a < *(long long*)b ? -1 : 1;
}

//Calculate disk usage
float calcDiskUsage()
{
	struct statvfs stats;
	if(statvfs("/", &stats) != 0) return 0; //If I can't get the cache size, say that 0% of the disk is used
	return 100 - (((float)stats.f_bfree / (float)stats.f_blocks) * 100);
}

//RoadApplePi Cache Cleanup
void rapCleanup()
{
	int strLength, i = 0, vidCount = 0;
	float diskUsage;
	long long *vidArray = malloc(sizeof(long long));
	DIR *vidDir;
	struct dirent *entry;
	char *dateStr, *builtStr;
	MYSQL mysql;

	vidDir = opendir("/var/www/html/vids");
	if(vidDir == NULL)
	{
		syslog(LOG_WARNING, "Error: Could not manage cache size. Continuing...\n");
		return;
	}

	//Count Videos in cache and store them in array
	while((entry = readdir(vidDir)) != NULL) if(entry->d_type == DT_REG)
	{
		if(vidCount != 0) vidArray = realloc(vidArray, (vidCount + 1) * sizeof(long long));

		dateStr = strdup(entry->d_name);
		vidArray[vidCount] = atoll(strtok(dateStr, "."));
		free(dateStr);
		vidCount++;
	}
	closedir(vidDir);
	qsort(vidArray, vidCount, sizeof(long long), compareLLong);

	diskUsage = calcDiskUsage();

	//Delete excess videos
	for(i = 0; i < vidCount && diskUsage > MAX_DISK_USAGE; i++)
	{
		strLength = snprintf(NULL, 0, "/var/www/html/vids/%lld.mp4", vidArray[i]);
		builtStr = malloc(strLength + 1);
		snprintf(builtStr, strLength + 1, "/var/www/html/vids/%lld.mp4", vidArray[i]);
		unlink(builtStr);
		free(builtStr);

		diskUsage = calcDiskUsage();
	}

	//Manage MySQL database if we deleted videos
	if(i != 0)
	{
		mysql_library_init(0, NULL, NULL);
		mysql_init(&mysql);
		if(mysql_real_connect(&mysql, NULL, "roadapplepi", "roadapplepi", "roadapplepi", 0, NULL, 0))
		{
			strLength = snprintf(NULL, 0, "DELETE FROM obdLog WHERE timestamp<%lld", vidArray[i - 1]);
			builtStr = malloc(strLength + 1);
			snprintf(builtStr, strLength + 1, "DELETE FROM obdLog WHERE timestamp<%lld", vidArray[i - 1]);
			mysql_query(&mysql, builtStr);
			free(builtStr);

			strLength = snprintf(NULL, 0, "DELETE FROM obdCodes WHERE timestamp<%lld", vidArray[i - 1]);
			builtStr = malloc(strLength + 1);
			snprintf(builtStr, strLength + 1, "DELETE FROM obdCodes WHERE timestamp<%lld", vidArray[i - 1]);
			mysql_query(&mysql, builtStr);
			free(builtStr);

			mysql_close(&mysql);
		}
		else syslog(LOG_WARNING, "Could not connect to MySQL database for cache size management. Skipping...\n");
	}

	//Send signal to parent to forward to obdLogger to do a fresh dump of obd information
	kill(getppid(), SIGUSR1);

	//Clean up
	mysql_library_end();
	free(vidArray);
}

//When the main thread recieves a sigint, kill the logger and recorder
static void mainSigHandler(int signum __attribute__((unused)))
{
	if(recMgr != 0)
	{
		kill((pid_t)recMgr, SIGINT);
		waitpid((pid_t)recMgr, NULL, 0);
	}
	if(obdLogger != 0)
	{
		kill((pid_t)obdLogger, SIGINT);
		waitpid((pid_t)obdLogger, NULL, 0);
	}
	exit(0);
}

//When recorder recieves a sigint, stop looping through OBD codes
static void obdIntHandler(int signum __attribute__((unused)))
{
	loggingOBD = 0;
}

//WHen recorder recieves sigint, kill ffmpeg
static void recIntHandler(int signum __attribute__((unused)))
{
	if(ffmpegPid != 0)
	{
		sleep(1); //In case ffmpeg already recieved a kill
		kill((pid_t)ffmpegPid, SIGINT);
		waitpid((pid_t)ffmpegPid, NULL, 0);
	}
	rapCleanup();
	exit(0);
}

//Forward SIGUSR1 to obdLogger
static void forwardToOBDLogger(int signum __attribute__((unused)))
{
	if(obdLogger != 0) kill((pid_t)obdLogger, SIGUSR1);
}

//Do a full dump of OBD information
static void fullObdDump(int signum __attribute__((unused)))
{
	obdReset = 1;
}

//When ffmpeg dies, restart it
static void recChldHandler(int sig, siginfo_t *info, void *ucontext)
{
	if(info->si_pid == (pid_t)ffmpegPid) recRstrtFlag = 0;
}

//Sets up serial connection
int set_interface_attribs (int fd, int speed)
{
	int n = 0;
	struct termios term;

	fcntl(fd, F_SETFL, 0);
	if ((n = tcgetattr(fd, &term)) == -1)
	{
		syslog(LOG_ERR, "Error getting default serial settings\n");
		return -1;
	}

	if ((n = cfsetispeed(&term, speed)) == -1)
	{
		syslog(LOG_ERR, "Error configuring serial baud rate\n");
		return -1;
	}

	if ((n = cfsetospeed(&term, speed)) == -1)
	{
		syslog(LOG_ERR, "Error configuring serial baud rate\n");
		return -1;
	}

	term.c_cflag |= (CLOCAL | CREAD);
	term.c_cflag &= ~PARENB;
	term.c_cflag &= ~CSTOPB;
	term.c_cflag &= ~CSIZE;
	term.c_cflag |= CS8;
	term.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	term.c_iflag &= ~(IXON | IXOFF | IXANY);
	term.c_cflag &= ~CRTSCTS;
	term.c_oflag &= ~OPOST;

	if ((n = tcsetattr(fd, TCSANOW, &term)) == -1)
	{
		syslog(LOG_ERR, "Error setting serial configuration\n");
		return -1 ;
	}
	return 0;
}

//Strips all instances of c from str
void removechars(char* str, char c)
{
	char *pr = str, *pw = str;
	while(*pr)
	{
		*pw = *pr++;
		pw += (*pw != c);
	}
	*pw = '\0';
}

//Runs an OBD command
void obdCmd(int fd, char *cmd, char *resp)
{
	char *cmdBuffer, buf[MAX_OBD_SIZE];
	int n, cmdLength, receivedBytes;

	cmdLength = snprintf(NULL, 0, "%s\r", cmd) + 1;
	cmdBuffer = malloc(cmdLength);
	snprintf(cmdBuffer, cmdLength, "%s\r", cmd);
	write (fd, cmdBuffer, cmdLength);

	n = receivedBytes = 0;
	memset(buf, 0, sizeof(buf));
	do {
		n = read(fd, buf + receivedBytes, MAX_OBD_SIZE - 1);
		if(n > 0) receivedBytes += n;
	} while(strstr(buf, ">") == NULL);

	removechars(buf, '\r');
	removechars(buf, '\n');
	removechars(buf, ' ');
	if(resp != NULL) strncpy(resp, buf + cmdLength - 2, strlen(buf) - cmdLength + 1);

	free(cmdBuffer);
}

//Decodes available codes. Should feed it the response of "01 00", "01 20", etc
int *getCodes(char *obdCode)
{
	static int codeArray[0x21];
	int i, j, k, holdHex;
	char hexBuffer[2];

	j = 4;
	memset(codeArray, 0, sizeof(codeArray));
	
	for(i = 4; i < strlen(obdCode); i++)
	{
		snprintf(hexBuffer, 2, "%c", obdCode[i]);
		holdHex = (int)strtol(hexBuffer, NULL, 16);

		k = 0;
		while(holdHex)
		{
			if (holdHex & 1) codeArray[j] = 1;
			j--;
			k++;
			holdHex >>= 1;
		}
		j += k + 4;
	}

	return codeArray;
}
