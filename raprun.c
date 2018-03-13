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
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <regex.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <mysql/mysql.h>
#include <linux/rtc.h>

int main(int argc, char* argv[])
{
	if(argc <= 1) return 1; //Must have a flag passed - no documentation to limit abuse potential

	setuid(0);
	if(getuid() != 0)
	{
		fprintf(stderr, "Could not achieve root\n");
		return 1;
	}

	//***********
	//Shutdown Pi
	//***********
	if(strcmp(argv[1], "-s") == 0) system("shutdown now");

	//**********
	//Restart Pi
	//**********
	if(strcmp(argv[1], "-r") == 0) system("shutdown -r now");

	//**********
	//Sync Clock
	//**********
	if(strcmp(argv[1], "-n") == 0 && argc == 3)
	{
		long long timestampMS = atoll(argv[2]);
		struct timeval setTime;
		struct tm* rtcTime;
		int fd;

		setTime.tv_sec = (time_t)(timestampMS / 1000);
		setTime.tv_usec = (suseconds_t)((timestampMS % 1000) * 1000);
		settimeofday(&setTime, NULL);

		rtcTime = gmtime(&setTime.tv_sec);
		fd = open("/dev/rtc", O_WRONLY);
		if(fd != -1)
		{
			ioctl(fd, RTC_SET_TIME, rtcTime);
			close(fd);
		}
	}

	//************
	//Change to AP
	//************
	if(strcmp(argv[1], "-a") == 0 && argc == 4)
	{
		FILE *fileOld, *fileNew;
		char *fileLine, *sqlQuery, hostname[HOST_NAME_MAX + 1];
		size_t len = 0;
		ssize_t read;
		MYSQL mysql;
		int failCount, strLength;
		regex_t regChk;

		//Configure interfaces file
		fileOld = fopen("/etc/dhcpcd.conf", "r");
		fileNew = fopen("/etc/dhcpcd.new", "w");

		if(fileNew == NULL || fileOld == NULL) fprintf(stderr, "Warning: Could not edit dhcpcd");

		while ((read = getline(&fileLine, &len, fileOld)) != -1) fprintf(fileNew, fileLine);
		fprintf(fileNew, "interface wlan0\nstatic ip_address=192.168.50.1/24\n");
		fclose(fileOld);
		fclose(fileNew);

		system("ifconfig wlan0 down");
		unlink("/etc/dhcpcd.conf");
		unlink("/lib/dhcpcd/dhcpcd-hooks/10-wpa_supplicant");
		rename("/etc/dhcpcd.new", "/etc/dhcpcd.conf");

		//Configure AP info
		fileOld = fopen("/etc/hostapd/hostapd-rap.conf", "r");
		fileNew = fopen("/etc/hostapd/hostapd-rap.new", "w");
		if(fileNew == NULL || fileOld == NULL) fprintf(stderr, "Warning: Could not edit hostapd-rap.conf");

		while ((read = getline(&fileLine, &len, fileOld)) != -1)
		{
			regcomp(&regChk, "^ssid=", REG_EXTENDED);
			if(!regexec(&regChk, fileLine, 0, NULL, 0))
			{
				fprintf(fileNew, "ssid=%s\n", argv[2]);
				continue;
			}
			if(strstr(fileLine, "wpa_passphrase=") != NULL)
			{
				fprintf(fileNew, "wpa_passphrase=%s\n", argv[3]);
				continue;
			}
			fprintf(fileNew, fileLine);
		}
		fclose(fileOld);
		fclose(fileNew);

		unlink("/etc/hostapd/hostapd-rap.conf");
		rename("/etc/hostapd/hostapd-rap.new", "/etc/hostapd/hostapd-rap.conf");

		//Set AP config file (overwriting old)
		fileNew = fopen("/etc/default/hostapd", "w");
		if(fileNew == NULL) fprintf(stderr, "Warning: Could not configure hostapd defaults");
		fprintf(fileNew, "DAEMON_CONF=\"/etc/hostapd/hostapd-rap.conf\"\n");
		fclose(fileNew);

		//Set up hostname resolution
		gethostname(hostname, HOST_NAME_MAX + 1);
		fileNew = fopen("/etc/hosts.dnsmasq", "w");
		if(fileNew == NULL) fprintf(stderr, "Warning: Could not configure hosts.dnsmasq");
		fprintf(fileNew, "192.168.50.1 %s\n", hostname);
		fclose(fileNew);

		//Start AP
		system("systemctl restart dhcpcd");
		system("systemctl enable hostapd");
		system("systemctl restart hostapd");
		system("systemctl enable dnsmasq");
		system("systemctl restart dnsmasq");

		//Connect to MySQL
		mysql_library_init(0, NULL, NULL);
		mysql_init(&mysql);
		failCount = 0;
		while(!mysql_real_connect(&mysql, NULL, "root", "roadapplepi", "roadapplepi", 0, NULL, 0) && failCount < 60)
		{
			sleep(1);
			failCount++;
		}
		if(failCount == 60) fprintf(stderr, "Error: Could not connect to MySQL Server\n");

		//Store current network settings
		mysql_query(&mysql, "DELETE FROM env WHERE name=\"ssid\" OR name=\"psk\" OR name=\"mode\"");
		mysql_query(&mysql, "INSERT INTO env (name, value) VALUES (\"mode\", \"ap\")");
		strLength = snprintf(NULL, 0, "INSERT INTO env (name, value) VALUES (\"ssid\", \"%s\")", argv[2]);
		sqlQuery = malloc(strLength + 1);
		snprintf(sqlQuery, strLength + 1, "INSERT INTO env (name, value) VALUES (\"ssid\", \"%s\")", argv[2]);
		mysql_query(&mysql, sqlQuery);
		free(sqlQuery);
		strLength = snprintf(NULL, 0, "INSERT INTO env (name, value) VALUES (\"psk\", \"%s\")", argv[3]);
		sqlQuery = malloc(strLength + 1);
		snprintf(sqlQuery, strLength + 1, "INSERT INTO env (name, value) VALUES (\"psk\", \"%s\")", argv[3]);
		mysql_query(&mysql, sqlQuery);
		free(sqlQuery);

		//Disconnect from MySQL
		mysql_close(&mysql);
		mysql_library_end();
	}

	//********************
	// Connect to Wireless
	//********************
	if(strcmp(argv[1], "-c") == 0 && argc == 4)
	{
		FILE *fileOld, *fileNew;
		char *fileLine, *sqlQuery;
		size_t len = 0;
		ssize_t read;
		int strLength, failCount, linesSkip = 0;
		MYSQL mysql;

		//Take down services
		system("ifconfig wlan0 down");
		system("systemctl stop hostapd");
		system("systemctl disable hostapd");
		system("systemctl stop dnsmasq");
		system("systemctl disable dnsmasq");

		//Configure interfaces file
		fileOld = fopen("/etc/dhcpcd.conf", "r");
		fileNew = fopen("/etc/dhcpcd.new", "w");

		if(fileNew == NULL || fileOld == NULL) fprintf(stderr, "Warning: Could not edit dhcpcd");

		while ((read = getline(&fileLine, &len, fileOld)) != -1)
		{
			if(linesSkip > 0)
			{
				linesSkip--;
				continue;
			}

			if(strstr(fileLine, "interface wlan0") != NULL && strstr(fileLine, "#") == NULL)
			{
				linesSkip = 1;
				continue;
			}
			fprintf(fileNew, fileLine);
		}
		fclose(fileOld);
		fclose(fileNew);

		unlink("/etc/dhcpcd.conf");
		rename("/etc/dhcpcd.new", "/etc/dhcpcd.conf");

		//Configure WPA-supplicant (overwrite old config)
		fileNew = fopen("/etc/wpa_supplicant/wpa_supplicant.conf", "w");
		if(fileNew == NULL) fprintf(stderr, "Warning: Could not configure wpa_supplicant");
		fprintf(fileNew, "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\nupdate_config=1\n\nnetwork={\n\tssid=\"%s\"\n\tpsk=\"%s\"\n\tkey_mgmt=WPA-PSK\n}\n", argv[2], argv[3]);
		fclose(fileNew);

		//Disable AP Config
		fileNew = fopen("/etc/default/hostapd", "w");
		if(fileNew == NULL) fprintf(stderr, "Warning: Could not configure hostapd defaults");
		fprintf(fileNew, "#hostapd is currently disabled\n");
		fclose(fileNew);

		//Restart dhcpcd
		link("/usr/share/dhcpcd/hooks/10-wpa_supplicant", "/lib/dhcpcd/dhcpcd-hooks/10-wpa_supplicant");
		system("systemctl restart dhcpcd");

		//Connect to MySQL
		mysql_library_init(0, NULL, NULL);
		mysql_init(&mysql);
		failCount = 0;
		while(!mysql_real_connect(&mysql, NULL, "root", "roadapplepi", "roadapplepi", 0, NULL, 0) && failCount < 60)
		{
			sleep(1);
			failCount++;
		}
		if(failCount == 60) fprintf(stderr, "Error: Could not connect to MySQL Server\n");

		//Store current network settings
		mysql_query(&mysql, "DELETE FROM env WHERE name=\"ssid\" OR name=\"psk\" OR name=\"mode\"");
		mysql_query(&mysql, "INSERT INTO env (name, value) VALUES (\"mode\", \"client\")");
		strLength = snprintf(NULL, 0, "INSERT INTO env (name, value) VALUES (\"ssid\", \"%s\")", argv[2]);
		sqlQuery = malloc(strLength + 1);
		snprintf(sqlQuery, strLength + 1, "INSERT INTO env (name, value) VALUES (\"ssid\", \"%s\")", argv[2]);
		mysql_query(&mysql, sqlQuery);
		free(sqlQuery);
		strLength = snprintf(NULL, 0, "INSERT INTO env (name, value) VALUES (\"psk\", \"%s\")", argv[3]);
		sqlQuery = malloc(strLength + 1);
		snprintf(sqlQuery, strLength + 1, "INSERT INTO env (name, value) VALUES (\"psk\", \"%s\")", argv[3]);
		mysql_query(&mysql, sqlQuery);
		free(sqlQuery);

		//Disconnect from MySQL
		mysql_close(&mysql);
		mysql_library_end();
	}

	//*********************************************
	// Discover Buetooth devices and return as JSON
	//*********************************************
	if(strcmp(argv[1], "-d") == 0)
	{
		inquiry_info *inquiryInfo = NULL;
		int max_rsp, num_rsp, dev_id, sock, len, flags, i;
		char addr[19] = { 0 };
		char name[248] = { 0 };

		dev_id = hci_get_route(NULL);
		sock = hci_open_dev( dev_id );
		if (dev_id < 0 || sock < 0)
		{
			fprintf(stderr, "Error accessing bluetooth radio\n");
			return 1;
		}

		len = 8;
		max_rsp = 255;
		flags = IREQ_CACHE_FLUSH;
		inquiryInfo = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

		num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &inquiryInfo, flags);
		if( num_rsp < 0 )
		{
			fprintf(stderr, "Error Discovering bluetooth devices");
			return 1;
		}

		printf("[");
		for (i = 0; i < num_rsp; i++)
		{
			ba2str(&(inquiryInfo+i)->bdaddr, addr);
			memset(name, 0, sizeof(name));
			if (hci_read_remote_name(sock, &(inquiryInfo+i)->bdaddr, sizeof(name),name, 0) < 0) strcpy(name, "Unnamed");
			printf("[\"%s\", \"%s\"]", addr, name);

			if(i < num_rsp - 1) printf(", ");
		}

		printf("]\n");
		free(inquiryInfo);
		close(sock);
	}

	//*****************************
	// Pair with a bluetooth device
	//*****************************
	if(strcmp(argv[1], "-p") == 0 && (argc == 3 || argc == 4))
	{
		regex_t macReg;
		int readPipe[2], writePipe[2]; //Named from parent's prespective
		pid_t btctlPid;
		char readBuffer[128], *writeBuffer;
		int failCount, strLength;
		bool enterPin = false;

		//Validate bluetooth address
		regcomp(&macReg, "[A-Fa-f0-9]{2}:[A-Fa-f0-9]{2}:[A-Fa-f0-9]{2}:[A-Fa-f0-9]{2}:[A-Fa-f0-9]{2}:[A-Fa-f0-9]{2}", REG_EXTENDED);
		if(regexec(&macReg, argv[2], 0, NULL, 0))
		{
			printf("Not a valid MAC address: %s\n", argv[2]);
			return 1;
		}

		if(access("/usr/bin/bluetoothctl", X_OK) == -1)
		{
			printf("Cannot find bluetoothctl\n");
			exit(1);
		}

		pipe(readPipe);
		pipe(writePipe);
		btctlPid = fork();

		//Fork off bluetoothctl
		if(btctlPid == 0)
		{
			close(writePipe[1]);
			close(readPipe[0]);

			dup2(writePipe[0], 0);
			dup2(readPipe[1], 1);
			dup2(readPipe[1], 2);
			execl("/usr/bin/bluetoothctl", "bluetoothctl", (char *)NULL);
		}


		close(writePipe[0]);
		close(readPipe[1]);
		fcntl(readPipe[0], F_SETFL, O_NONBLOCK);

		//Wait for bluetoothctl to be ready
		while(read(readPipe[0], readBuffer, 128) == -1 || strstr(readBuffer, "[bluetooth]") == NULL) usleep(1000);

		//Send "power on"
		write(writePipe[1], "power on\n", strlen("power on\n") + 1);
		failCount = 0;
		while(failCount < 10000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, "succeeded") != NULL) break;
				else failCount++;
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 10000)
		{
			printf("Could not power on bluetooth adapter\n");
			return 1;
		}

		//Register agent
		write(writePipe[1], "agent on\n", strlen("agent on\n") + 1);
		failCount = 0;
		while(failCount < 10000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, "registered") != NULL) break;
				else failCount++;
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 10000)
		{
			printf("Could not register agent\n");
			return 1;
		}

		//Use default agent
		write(writePipe[1], "default-agent\n", strlen("default-agent\n") + 1);
		failCount = 0;
		while(failCount < 10000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, "successful") != NULL) break;
				else failCount++;
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 10000)
		{
			printf("Could not switch to default agent\n");
			return 1;
		}

		//Remove the device we want to pair (no check for success)
		strLength = snprintf(NULL, 0, "remove %s\n", argv[2]);
		writeBuffer = malloc(strLength + 1);
		snprintf(writeBuffer, strLength + 1, "remove %s\n", argv[2]);
		write(writePipe[1], writeBuffer, strLength + 1);
		free(writeBuffer);

		while(true)
		{
			if(read(readPipe[0], readBuffer, 128) != -1 && strstr(readBuffer, argv[2]) != NULL) break;
			else usleep(1000);
		}

		//Turn scanning on
		write(writePipe[1], "scan on\n", strlen("scan on\n") + 1);
		failCount = 0;
		while(failCount < 10000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, "started") != NULL) break;
				else failCount++;
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 10000)
		{
			printf("Could not initialize device scanning\n");
			return 1;
		}

		//Wait for requested device
		failCount = 0;
		while(failCount < 30000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, argv[2]) != NULL) break;
				else failCount++;
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 30000)
		{
			printf("Could not find %s after scanning for 30 seconds\n", argv[2]);
			return 1;
		}

		//Turn scanning back off
		write(writePipe[1], "scan off\n", strlen("scan off\n") + 1);
		failCount = 0;
		while(failCount < 10000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, "stopped") != NULL) break;
				else failCount++;
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 10000)
		{
			printf("Could not stop device scanning\n");
			return 1;
		}

		//Pair with the device
		strLength = snprintf(NULL, 0, "pair %s\n", argv[2]);
		writeBuffer = malloc(strLength + 1);
		snprintf(writeBuffer, strLength + 1, "pair %s\n", argv[2]);
		write(writePipe[1], writeBuffer, strLength + 1);
		free(writeBuffer);

		failCount = 0;
		while(failCount < 30000)
		{
			if(read(readPipe[0], readBuffer, 128) != -1)
			{
				if(strstr(readBuffer, "Enter PIN code") != NULL)
				{
					if(argc == 3)
					{
						printf("%s requested a pin code, but none was given\n", argv[2]);
						return 1;
					}
					else
					{
						enterPin = true;
						break;
					}
				}
				else if(strstr(readBuffer, "successful") != NULL) break; //We don't care if pin was given, but not requested
			}
			else failCount++;
			usleep(1000);
		}
		if(failCount == 30000)
		{
			printf("Could not pair with %s after 30 seconds\n", argv[2]);
			return 1;
		}

		//Enter PIN (if necessary)
		if(enterPin)
		{
			strLength = snprintf(NULL, 0, "%s\n", argv[3]);
			writeBuffer = malloc(strLength + 1);
			snprintf(writeBuffer, strLength + 1, "%s\n", argv[3]);
			write(writePipe[1], writeBuffer, strLength + 1);
			free(writeBuffer);

			failCount = 0;
			while(failCount < 30000)
			{
				if(read(readPipe[0], readBuffer, 128) != -1)
				{
					if(strstr(readBuffer, "successful") != NULL) break;
					else failCount++;
				}
				else failCount++;
				usleep(1000);
			}
			if(failCount == 30000)
			{
				printf("%s did not accept %s as its pin code\n", argv[2], argv[3]);
				return 1;
			}
		}

		//Exit bluetoothctl
		write(writePipe[1], "exit\n", strlen("exit\n") + 1);
		wait(NULL);
	}

	return 0;
}
