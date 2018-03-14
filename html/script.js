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

var sideBar = false;
var offline = false;
var selectedMenu = 0;
var showGauges = 1;
var imperial = 0;
var forageLoaded = 0;
var logReturned = false;
var gaugesBuilt = false;
var refreshGauges = false;
var liveGaugesRunning = false;
var savedVideos = [];
var savedOBDLogs = [];
var updateLastTimeInstance, lastSeconds, lastHours, lastMinutes, obdStream, getSupportedGauges, getInitialStatus, refreshValue, obdLog, obdCodes, fetchCodes, fetchGauges, fetchVideo, obdPlayerInstance, currentVideo, videoArray, dateArray, videoSelect, calcEngineLoad, coolantTemp, b1stft, b1ltft, b2stft, b2ltft, intakePressure, rpm, speed, timingAdvance, airTemp, throttlePos;

var LOADING_TEXT = "<div style='height: 10px;'></div><div class='loadWait'>Loading, please wait...</div>";

function slideDrawer()
{
	document.getElementById('sideBar').style.transform = "translateX(" + (sideBar ? '-305px' : '0px') + ")";
	sideBar = (sideBar ? false : true);
}
function retractDrawer()
{
	if(sideBar)
	{
		document.getElementById('sideBar').style.transform = 'translateX(-305px)';
		sideBar = false;
	}
}
function menuSelect(menuNumber)
{
	//Make sure localForage has loaded its information before continuing
	if(forageLoaded != 2)
	{
		setTimeout(function(){menuSelect(menuNumber)}, 100);
		return;
	}

	//Change selection and retract drawer
	document.getElementById('menuItem' + selectedMenu).className = 'menuItem';
	selectedMenu = menuNumber;
	sessionStorage.setItem('selectedMenu', selectedMenu);
	document.getElementById('menuItem' + selectedMenu).className = 'menuItem selected';
	retractDrawer();
	
	//Cancel any pending async tasks
	liveGaugesRunning = false;
	clearTimeout(obdPlayerInstance);
	clearTimeout(updateLastTimeInstance);
	if(typeof obdStream != "undefined") obdStream.close();
	if(typeof fetchGauges != "undefined") fetchGauges.abort();
	if(typeof fetchCodes != "undefined") fetchCodes.abort();
	if(typeof fetchVideo != "undefined") fetchVideo.abort();
	if(typeof getSupportedGauges != "undefined") getSupportedGauges.abort();
	if(typeof getInitialStatus != "undefined") getInitialStatus.abort();
	
	//Load the new view
	mainContent = document.getElementById('mainContent');
	barTitle = document.getElementById('barTitle');
	
	switch(menuNumber)
	{
		case 0:
		barTitle.innerHTML = "Dashboard";
		if(typeof(EventSource) !== "undefined")
		{
			mainContent.innerHTML = LOADING_TEXT;
			livePlayer();
		}
		else mainContent.innerHTML = "<div style='height: 10px;'></div><div class='loadWait'>Sorry! Internet Explorer does not support the Dashboard. Please use a real browser.</div>";
		break;

		case 1:
		barTitle.innerHTML = "Recordings";
		mainContent.innerHTML = LOADING_TEXT;
		xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function()
		{
			//Online mode, fetch from server
			if(this.readyState == 4 && this.status == 200)
			{
				try { videoArray = JSON.parse(this.responseText); }
				catch(err)
				{
					mainContent.innerHTML = "<div style='height: 10px;'></div><div class='loadWait'>There was an error loading this page</div>";
					return;
				}

				//Clean up old cached OBD information on videos we don't have saved
				if(videoArray.length != 0) for(i = 0; i < savedOBDLogs.length; i++) if(savedOBDLogs[i] < videoArray[0] && savedVideos.indexOf(savedOBDLogs[i]) == -1)
				{
					localforage.removeItem("obdLog" + savedOBDLogs[i]);
					localforage.removeItem("supportedCode" + savedOBDLogs[i]);
					savedOBDLogs.splice(i, 1);
					localforage.setItem('savedOBDLogs', savedOBDLogs);
					i--;
				}

				//Merge cached videos
				videoArray = videoArray.concat(savedVideos);
				videoArray.sort();
				videoArray = Array.from(new Set(videoArray));
				
				mainContent.innerHTML = '';
				offline = false;
				parseVideos();
			}
			
			//Offline mode, view cached videos only
			else if (xhttp.readyState == 4 && xhttp.status == 0)
			{
				mainContent.innerHTML = "<div style='height: 10px;'></div><div class='offlineMessage'><i class='fa fa-times-circle' aria-hidden='true'></i> Only cached videos are available in offline mode.</div>";
				videoArray = savedVideos;
				offline = true;
				parseVideos();
			}
		};
		xhttp.open("GET", "dataHandler.php?action=vidList", true);
		xhttp.send();
		break;
		
		case 2:
		barTitle.innerHTML = "Settings";
		mainContent.innerHTML = "";

		//Padding
		paddingBox = document.createElement('div');
		paddingBox.style.height = '10px';
		mainContent.appendChild(paddingBox);

		//Measurement Units
		measureLabel = document.createElement('div');
		measureLabel.className = 'settingsLabel';
		measureLabel.innerHTML = "Measurement Units";
		mainContent.appendChild(measureLabel);

		measureList = document.createElement('div');
		measureList.className = 'settingsList';
		mainContent.appendChild(measureList);

		metricButton = document.createElement('div');
		metricButton.className = "settingsItem toggle" + (imperial ? "" : " toggleSelected");
		metricButton.innerHTML = "Metric";
		metricButton.id = "metricButton";
		metricButton.onclick = function()
		{
			document.getElementById('metricButton').className = "settingsItem toggle toggleSelected";
			document.getElementById('imperialButton').className = "settingsItem toggle";
			imperial = 0;
			localStorage.setItem("imperial", 0);
		}
		measureList.appendChild(metricButton);

		imperialButton = document.createElement('div');
		imperialButton.className = "settingsItem toggle" + (imperial ? " toggleSelected" : "");
		imperialButton.style.textAlign = "right";
		imperialButton.innerHTML = "Imperial";
		imperialButton.id = "imperialButton";
		imperialButton.onclick = function()
		{
			document.getElementById('imperialButton').className = "settingsItem toggle toggleSelected";
			document.getElementById('metricButton').className = "settingsItem toggle";
			imperial = 1;
			localStorage.setItem("imperial", 1);
		}
		measureList.appendChild(imperialButton);
		
		//Load online-only results
		xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function()
		{
			if (this.readyState == 4 && this.status == 200)
			{
				try { var currentSettings = JSON.parse(this.responseText); }
				catch(err)
				{
					mainContent.innerHTML += "<div style='height: 10px;'></div><div class='offlineMessage'><i class='fa fa-times-circle' aria-hidden='true'></i> There was an error loading the rest of your settings</div>";
					return;
				}
	
				//Bluetooth
				btLabel = document.createElement('div');
				btLabel.className = 'settingsLabel';
				btLabel.innerHTML = "OBD II Bluetooth Device";
				mainContent.appendChild(btLabel);
		
				btList = document.createElement('div');
				btList.className = 'settingsList';
				mainContent.appendChild(btList);
		
				currentBt = document.createElement('div');
				currentBt.className = "settingsItem settingsItemNoClick";
				currentBt.style.textAlign = 'left';
				currentBt.id = 'currentBt';
				currentBt.innerHTML = "<span style='font-weight: bold;'>Currently paired device: </span>" + currentSettings["OBD"];
				btList.appendChild(currentBt);
		
				pairBt = document.createElement('div');
				pairBt.className = "settingsItem settingsButton";
				pairBt.id = "pairBt";
				pairBt.innerHTML = "Scan for nearby devices";
				pairBt.onclick = function() { btScan(); };
				btList.appendChild(pairBt);
		
				//Network
				networkLabel = document.createElement('div');
				networkLabel.className = 'settingsLabel';
				networkLabel.innerHTML = "Networking";
				mainContent.appendChild(networkLabel);
		
				networkList = document.createElement('div');
				networkList.className = 'settingsList';
				mainContent.appendChild(networkList);
		
				networkPanel = document.createElement('div');
				networkPanel.className = "settingsItem settingsItemNoClick";
				networkPanel.style.textAlign = 'left';
				networkPanel.style.lineHeight = 2;
				networkList.appendChild(networkPanel);
		
				apSet = document.createElement('label');
				accessPoint = document.createElement('input');
				accessPoint.type = 'radio';
				accessPoint.name = 'networkSwitch';
				accessPoint.value = 'ap';
				accessPoint.id='apButton'
				accessPoint.onclick = function(){document.getElementById('ssid').value = ""; document.getElementById('password').value = ""}
				apLabel = document.createElement('span');
				apLabel.innerHTML = "Create Access Point";
				apSet.appendChild(accessPoint);
				apSet.appendChild(apLabel);
				networkPanel.appendChild(apSet);
				networkPanel.appendChild(document.createElement('br'));
		
				ctSet = document.createElement('label');
				client = document.createElement('input');
				client.type = 'radio';
				client.name = 'networkSwitch';
				client.value = 'ct';
				client.id='clientButton';
				client.onclick = function(){document.getElementById('ssid').value = ""; document.getElementById('password').value = ""}
				ctLabel = document.createElement('span');
				ctLabel.innerHTML = "Connect to existing network";
				ctSet.appendChild(client);
				ctSet.appendChild(ctLabel);
				networkPanel.appendChild(ctSet);
				networkPanel.appendChild(document.createElement('br'));
		
				padding = document.createElement('div');
				padding.style.height = '10px';
				networkPanel.appendChild(padding);
		
				ssidSet = document.createElement('label');
				ssidLabel = document.createElement('span');
				ssidLabel.innerHTML = "SSID: ";
				ssid = document.createElement('input');
				ssid.type = 'text';
				ssid.name = 'ssid';
				ssid.id = 'ssid';
				ssid.value = currentSettings["SSID"];
				ssidSet.appendChild(ssidLabel);
				ssidSet.appendChild(ssid);
				networkPanel.appendChild(ssidSet);
		
				pwSet = document.createElement('label');
				pwLabel = document.createElement('span');
				pwLabel.innerHTML = "Password: ";
				password = document.createElement('input');
				password.type = 'text';
				password.name = 'password';
				password.id = 'password';
				password.value = currentSettings["PSK"];
				pwSet.appendChild(pwLabel);
				pwSet.appendChild(password);
				networkPanel.appendChild(pwSet);
				networkPanel.appendChild(document.createElement('br'));
		
				networkSave = document.createElement('div');
				networkSave.className = 'settingsItem settingsButton';
				networkSave.innerHTML = "Save changes";
				networkSave.id = 'networkSave';
				networkSave.onclick = function()
				{
					getNetType = document.getElementsByName('networkSwitch');
					for(i = 0; i < getNetType.length; i++) if(getNetType[i].checked) break;
					if(i == getNetType.length || getNetType[i].value == 'dn') return;
			
					if(document.getElementById('password').value.length < 8)
					{
						alert("Your password must be at least 8 characters");
						return;
					}
			
					networkInfo = new FormData();
					networkInfo.append("ssid", document.getElementById('ssid').value);
					networkInfo.append("password", document.getElementById('password').value);

					currentSettings["Mode"] = (getNetType[i].value == 'ap' ? 0 : 1);
					currentSettings["SSID"] = document.getElementById('ssid').value;
					currentSettings["PSK"] = document.getElementById('password').value;
			
					url = "dataHandler.php?action=" + getNetType[i].value;
					xhttp = new XMLHttpRequest();
					xhttp.open("POST", url, true);
					xhttp.send(networkInfo);
					document.getElementById("mainContent").innerHTML = "<div style='height: 10px;'></div><div class='settingsLabel'>" +  (getNetType[i].value == 'ap' ? "Access Point Creation" : "Client Connection") + " Successful</div><div class='settingsList'><div class='settingsItem settingsItemNoClick' style='text-align: center;'>You have switched your RoadApplePi into " + (getNetType[i].value == 'ap' ? "access point" : "client") + " mode. Please give it a minute to reconfigure itself, then connect to it accordingly</div></div>";
				};
				networkList.appendChild(networkSave);

				if(currentSettings["Mode"] == 0) document.getElementById('apButton').checked = true;
				else document.getElementById('clientButton').checked = true;
				
				//Time sync
				timeLabel = document.createElement('div');
				timeLabel.className = 'settingsLabel';
				timeLabel.innerHTML = "Time sync";
				mainContent.appendChild(timeLabel);
		
				timeList = document.createElement('div');
				timeList.className = 'settingsList';
				mainContent.appendChild(timeList);
		
				syncTime = document.createElement('div');
				syncTime.className = "settingsItem";
				syncTime.id = "syncTime";
				syncTime.innerHTML = "<i class='far fa-clock'></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Sync RoadApplePi time with this device";
				syncTime.onclick = function()
				{
					this.innerHTML = "<i class='far fa-clock'></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Syncing time...";
					timeSyncer = new XMLHttpRequest();
					timeSyncer.onreadystatechange = function()
					{
						if(this.readyState == 4 && this.status == 200)
						{
							if(this.responseText == "success") document.getElementById('syncTime').innerHTML = "<i class='far fa-clock'></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Time successfully synced!";
							else document.getElementById('syncTime').innerHTML = "<i class='far fa-clock'></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Time sync failed! Tap to try again";
							setTimeout(function(){document.getElementById('syncTime').innerHTML = "<i class='far fa-clock'></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Sync RoadApplePi time with this device";}, 5000);
						}
					};
					timeSyncer.open("GET", "dataHandler.php?action=timeSync&timestamp=" + Date.now(), true);
					timeSyncer.send();
					
				};
				timeList.appendChild(syncTime);
				
				//Power Management
				powerLabel = document.createElement('div');
				powerLabel.className = 'settingsLabel';
				powerLabel.innerHTML = "Power Management";
				mainContent.appendChild(powerLabel);
		
				powerList = document.createElement('div');
				powerList.className = 'settingsList';
				mainContent.appendChild(powerList);
		
				shutdownButton = document.createElement('div');
				shutdownButton.className = 'settingsItem';
				shutdownButton.innerHTML = "<i class='fa fa-power-off' ></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Shutdown";
				shutdownButton.onclick = function() { powerManagement(0); };
				powerList.appendChild(shutdownButton);
		
				restartButton = document.createElement('div');
				restartButton.className = 'settingsItem';
				restartButton.innerHTML = "<i class='fa fa-sync' ></i>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Restart";
				restartButton.onclick = function() { powerManagement(1); };
				powerList.appendChild(restartButton);
		
				//Copyright
				copy = document.createElement('div');
				copy.className = "copy";
				copy.innerHTML = "&copy 2018 James Vital<br />This software is licensed under the GNU General Public License";
				mainContent.appendChild(copy);
			}
			else if (xhttp.readyState == 4 && xhttp.status == 0)
			{
				paddingDiv = document.createElement('div');
				paddingDiv.style.height = '10px';
				mainContent.appendChild(paddingDiv);
				
				offMessage = document.createElement('div');
				offMessage.innerHTML = "<i class='fa fa-times-circle' aria-hidden='true'></i> Only some settings are available while offline.";
				offMessage.className = "offlineMessage";
				mainContent.appendChild(offMessage);
			}
		};
		xhttp.open("GET", "dataHandler.php?action=currentSettings", true);
		xhttp.send();

		break;
	}
}

function loadVideo(newVideo)
{
	if(typeof fetchGauges != "undefined") fetchGauges.abort();
	if(typeof fetchCodes != "undefined") fetchCodes.abort();
	
	obdLog = [];
	gaugesBuilt = logReturned = false;

	currentVideo = newVideo = parseInt(newVideo);
	vidPlayer = document.getElementById('dashPlayer');
	
	//Opportunistically load from cache
	if(savedVideos.indexOf(videoArray[newVideo]) != -1) localforage.getItem('video' + videoArray[newVideo], function (err, value){if(value != null) vidPlayer.src = URL.createObjectURL(value);});
	else vidPlayer.src = "/vids/" + videoArray[newVideo] + ".mp4";
	
	navBar = document.getElementById('navbar');
	navBar.innerHTML = "";
	
	if(newVideo != 0)
	{
		lastIcon = document.createElement('i');
		lastIcon.className = 'fa fa-step-backward navbarArrows';
		lastSpan = document.createElement('span');
		lastSpan.onclick = function(){loadVideo(newVideo - 1)};
		lastSpan.appendChild(lastIcon);
		navBar.appendChild(lastSpan);
	}
	
	navBar.appendChild(videoSelect);
	videoSelect.selectedIndex = newVideo;
	
	if(newVideo != videoArray.length - 1)
	{
		nextIcon = document.createElement('i');
		nextIcon.className = 'fa fa-step-forward navbarArrows';
		nextSpan = document.createElement('span');
		nextSpan.onclick = function(){loadVideo(newVideo + 1)};
		nextSpan.appendChild(nextIcon);
		navBar.appendChild(nextSpan);
		vidPlayer.onended = function(){loadVideo(newVideo + 1)};
	}
	else vidPlayer.onended = null;
	
	//Set up function for caching video
	downloadBox = document.getElementById('downloadBox');
	downloadBox.className = "downloadBox";
	if(savedVideos.indexOf(videoArray[newVideo]) != -1)
	{
		downloadBox.style.color = "green";
		downloadBox.innerHTML = "<i class='fa fa-check-circle' aria-hidden='true'></i> This video is cached";
		downloadBox.onclick = removeFromCache;
		downloadBox.onmouseout = function()
		{
			this.style.color = "green";
			this.innerHTML = "<i class='fa fa-check-circle' aria-hidden='true'></i> This video is cached";
		};
		downloadBox.onmouseover = function()
		{
			this.style.color = "red";
			this.innerHTML = "<i class='fas fa-times-circle'></i> Delete from cache";
		};
	}
	else
	{
		downloadBox.style.color = "";
		downloadBox.innerHTML = "Cache video for offline viewing";
		downloadBox.onclick = addToCache;
		downloadBox.onmouseover = null;
		downloadBox.onmouseout = null;
	}


	//Get supported OBD codes
	document.getElementById("gaugeBox").innerHTML = "<div style='padding: 15px;'>Loading gauges...</div>";
	localforage.getItem('supportedCode' + videoArray[newVideo], function(err, value)
	{
		if(value == null)
		{
			fetchCodes = new XMLHttpRequest();
			fetchCodes.onreadystatechange = function()
			{
				if(this.readyState == 4 && this.status == 200)
				{
					obdCodes = JSON.parse(this.responseText);
					localforage.setItem("supportedCode" + videoArray[newVideo], obdCodes);
				}
			};
			fetchCodes.open("GET", "dataHandler.php?action=obdCodes&timestamp=" + videoArray[newVideo], true);
			fetchCodes.send();
		}
		else obdCodes = value;

		//Get OBD log for this timeframe and play video + gauges
		localforage.getItem('obdLog' + videoArray[newVideo], function(err2, value2)
		{
			if(value2 == null)
			{
				fetchGauges = new XMLHttpRequest();
				fetchGauges.onreadystatechange = function()
				{
					if(this.readyState == 4 && this.status == 200)
					{
						obdLog = JSON.parse(this.responseText);
						localforage.setItem("obdLog" + videoArray[newVideo], obdLog);
						savedOBDLogs.push(videoArray[newVideo]);
						localforage.setItem('savedOBDLogs', savedOBDLogs);

						logReturned = true;
						document.getElementById('dashPlayer').play();
					}
				};
				fetchGauges.open("GET", "dataHandler.php?action=obdLog&min=" + videoArray[newVideo] + "&max=" + (videoArray.length == newVideo ? 0 : videoArray[newVideo + 1]), true);
				fetchGauges.send();
			}
			else
			{
				obdLog = value2;
				logReturned = true;
				document.getElementById('dashPlayer').play();
			}
		});
	});
}

function powerManagement(mode)
{
	url = "dataHandler.php?action=" + (mode == 0 ? "shutdown" : "restart");
	xhttp = new XMLHttpRequest();
	xhttp.open("GET", url, true);
	xhttp.send();
	document.getElementById("mainContent").innerHTML = "<div style='height: 10px;'></div><div class='settingsLabel'>" + (mode == 0 ? "Shutdown" : "Restart") + " Successful</div><div class='settingsList'><div class='settingsItem settingsItemNoClick' style='text-align: center;'>Your RoadApplePi is currently " + (mode == 0 ? "shutting down. You may now close this page" : "restarting. Please refresh this page in a few minutes") + "</div></div>";
	
}

function btScan()
{
	pairBt = document.getElementById('pairBt');
	pairBt.onclick = null;
	pairBt.className = "settingsItem settingsButton disabled";
	pairBt.innerHTML = "Scanning..."
	
	xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function()
	{
		if (this.readyState == 4 && this.status == 200)
		{
			try { deviceArray = JSON.parse(this.responseText); }
			catch(err)
			{
				pairBt.className = "settingsItem settingsButton";
				pairBt.innerHTML = "Error scanning; tap to try again";
				pairBt.onclick = function() { btScan(); };
				return;
			}
			if(deviceArray.length == 0)
			{
				pairBt.className = "settingsItem settingsButton";
				pairBt.innerHTML = "No devices found; tap to try again";
				pairBt.onclick = function() { btScan(); };
				return;
			}
			
			pairBt.className = "settingsItem settingsItemNoClick";
			pairBt.innerHTML = "";
			
			discoverLabel = document.createElement('div');
			discoverLabel.style.fontWeight = "bold";
			discoverLabel.innerHTML = "Discovered Devices";
			pairBt.appendChild(discoverLabel);
			
			deviceName = document.createElement('select');
			deviceName.style.width = "100%";
			deviceName.style.background = "none";
			deviceName.style.height = "30px";
			deviceName.style.marginBottom = "10px";
			deviceName.id = "deviceName";
			
			newOpt = [];
			for(i = 0; i < deviceArray.length; i++)
			{
				newOpt[i] = document.createElement('option');
				newOpt[i].value = deviceArray[i][0];
				newOpt[i].innerHTML = deviceArray[i][1] + " [" + deviceArray[i][0] + "]";
				deviceName.appendChild(newOpt[i]);
			}
			
			pairBt.appendChild(deviceName);
			
			pinLabel = document.createElement('div');
			pinLabel.style.fontWeight = "bold";
			pinLabel.innerHTML = "PIN Code";
			pairBt.appendChild(pinLabel);
			
			
			pinInput = document.createElement('input');
			pinInput.type='text';
			pinInput.id='pinInput';
			pinInput.style.width = "100%";
			pinInput.style.float = "none";
			pinInput.placeholder = "1234";
			pinInput.style.marginBottom = '10px';
			pairBt.appendChild(pinInput);
			
			buttonBox = document.createElement('div');
			buttonBox.style.textAlign = 'center';
			pairBt.appendChild(buttonBox);
			
			rescan = document.createElement('div');
			rescan.className = "insetButton";
			rescan.id = "rescan";
			rescan.innerHTML = "Rescan";
			rescan.onclick = function() { btScan(); };
			buttonBox.appendChild(rescan);
			
			pair = document.createElement('div');
			pair.className = "insetButton";
			pair.id = "pair";
			pair.innerHTML = "Pair";
			pair.onclick = function() { btPair(); };
			buttonBox.appendChild(pair);
			
		}
	};
	xhttp.open("GET", "dataHandler.php?action=discover", true);
	xhttp.send();
}

function btPair()
{
	xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function()
	{
		if (this.readyState == 4 && this.status == 200)
		{
			try { pairResponse = JSON.parse(this.responseText); }
			catch(err)
			{
				pairBt.className = "settingsItem settingsButton";
				pairBt.innerHTML = "Error pairing; tap to try again";
				pairBt.onclick = function() { btScan(); };
				return;
			}
			
			//Success
			if(pairResponse[0])
			{
				currentSettings["OBD"] = pairResponse[1];
				document.getElementById('currentBt').innerHTML = "<span style='font-weight: bold;'>Currently paired device: </span>" + currentSettings["OBD"];
				
				pairBt.className = "settingsItem settingsButton";
				pairBt.innerHTML = "Pair success! Tap to scan again";
				pairBt.onclick = function() { btScan(); };
				return;
			}
			
			//Fail
			else
			{
				pairBt.className = "settingsItem settingsButton";
				pairBt.innerHTML = pairResponse[1] + "; tap to try again";
				pairBt.onclick = function() { btScan(); };
				return;
			}
			
			pairBt.className = "settingsItem settingsButton";
			pairBt.onclick = function() { btScan(); };
			return;
		}
	};
	
	btInfo = new FormData();
	btInfo.append("mac", document.getElementById('deviceName').value);
	btInfo.append("pin", document.getElementById('pinInput').value);
	
	xhttp.open("POST", "dataHandler.php?action=pair", true);
	xhttp.send(btInfo);
	
	pairBt = document.getElementById('pairBt');
	pairBt.className = "settingsItem settingsButton disabled";
	pairBt.innerHTML = "Pairing...";
}
function buildGauges()
{
	gaugeBox = document.getElementById('gaugeBox');
	for(i = 0; i < 0xE1; i++) if(obdCodes[i]) switch(i)
	{
		case 0x04:
		newGauge = document.createElement('div');
		newGauge.id = "calcEngineLoad";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		calcEngineLoad = new JustGage({
			id: "calcEngineLoad",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: 0,
			max: 100,
			title: "Calculated Engine Load",
			label: "Percent"
		});
		break;
		case 0x05:
		newGauge = document.createElement('div');
		newGauge.id = "coolantTemp";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		coolantTemp = new JustGage({
			id: "coolantTemp",
			value: (imperial ? Math.round(refreshValue[i] * 1.8) + 32 : refreshValue[i]),
			relativeGaugeSize: true,
			min: -40,
			max: (imperial ? 419 : 215),
			title: "Coolant Tempreature",
			label: (imperial ? "°F" : "°C")
		});
		break;
		case 0x06:
		newGauge = document.createElement('div');
		newGauge.id = "b1stft";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		b1stft = new JustGage({
			id: "b1stft",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: -100,
			max: 100,
			title: "Short-Term Fuel Trim",
			label: "Bank 1"
		});
		break;
		case 0x07:
		newGauge = document.createElement('div');
		newGauge.id = "b1ltft";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		b1ltft = new JustGage({
			id: "b1ltft",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: -100,
			max: 100,
			title: "Long-Term Fuel Trim",
			label: "Bank 1"
		});
		break;
		case 0x08:
		newGauge = document.createElement('div');
		newGauge.id = "b2stft";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		b2stft = new JustGage({
			id: "b2stft",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: -100,
			max: 100,
			title: "Short-Term Fuel Trim",
			label: "Bank 2"
		});
		break;
		case 0x09:
		newGauge = document.createElement('div');
		newGauge.id = "b2ltft";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		b2ltft = new JustGage({
			id: "b2ltft",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: -100,
			max: 100,
			title: "Long-Term Fuel Trim",
			label: "Bank 2"
		});
		break;
		case 0x0B:
		newGauge = document.createElement('div');
		newGauge.id = "intakePressure";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		intakePressure = new JustGage({
			id: "intakePressure",
			value: (imperial ? Math.round(refreshValue[i] * 0.14503773773020923) : refreshValue[i]),
			relativeGaugeSize: true,
			min: 0,
			max: (imperial ? 37 : 255),
			title: "Intake Manifold Pressure",
			label: (imperial ? "PSI" : "kPa")
		});
		break;
		case 0x0C:
		newGauge = document.createElement('div');
		newGauge.id = "rpm";
		newGauge.className = "obdGauge";

		gaugeBox.appendChild(newGauge);
		rpm = new JustGage({
			id: "rpm",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: 0,
			max: 16383,
			title: "RPM",
		});
		break;
		case 0x0D:
		newGauge = document.createElement('div');
		newGauge.id = "speed";
		newGauge.className = "obdGague";

		gaugeBox.appendChild(newGauge);
		speed = new JustGage({
			id: "speed",
			value: (imperial ? Math.round(refreshValue[i] * 0.62137119223733) : refreshValue[i]),
			relativeGaugeSize: true,
			min: 0,
			max: (imperial ? 160 : 255),
			title: "Speed",
			label: (imperial ? "MPH" : "KPH")
		});
		break;
		case 0x0E:
		newGauge = document.createElement('div');
		newGauge.id = "timingAdvance";
		newGauge.className = "obdGague";

		gaugeBox.appendChild(newGauge);
		timingAdvance = new JustGage({
			id: "timingAdvance",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: -64,
			max: 64,
			title: "Timing Advance",
			label: "°before TDC"
		});
		break;
		case 0x0F:
		newGauge = document.createElement('div');
		newGauge.id = "airTemp";
		newGauge.className = "obdGague";

		gaugeBox.appendChild(newGauge);
		airTemp = new JustGage({
			id: "airTemp",
			value: (imperial ? Math.round(refreshValue[i] * 1.8) + 32 : refreshValue[i]),
			relativeGaugeSize: true,
			min: -40,
			max: (imperial ? 419 : 215),
			title: "Air Intake Temp",
			label: (imperial ? "°F" : "°C")
		});
		break;
		case 0x11:
		newGauge = document.createElement('div');
		newGauge.id = "throttlePos";
		newGauge.className = "obdGague";

		gaugeBox.appendChild(newGauge);
		throttlePos = new JustGage({
			id: "throttlePos",
			value: refreshValue[i],
			relativeGaugeSize: true,
			min: 0,
			max: 100,
			title: "Throttle Position",
			label: "Percent"
		});
		break;
	}
}
function setAllGauges()
{
	for(i = 0; i < 0xE1; i++) if(obdCodes[i]) switch(i)
	{
		case 0x04: calcEngineLoad.refresh(refreshValue[i]); break;
		case 0x05: coolantTemp.refresh(imperial ? Math.round(refreshValue[i] * 1.8) + 32 : refreshValue[i]); break;
		case 0x06: b1stft.refresh(refreshValue[i]); break;
		case 0x07: b1ltft.refresh(refreshValue[i]); break;
		case 0x08: b2stft.refresh(refreshValue[i]); break;
		case 0x09: b2ltft.refresh(refreshValue[i]); break;
		case 0x0B: intakePressure.refresh(imperial ? Math.round(refreshValue[i] * 0.14503773773020923) : refreshValue[i]); break;
		case 0x0C: rpm.refresh(refreshValue[i]); break;
		case 0x0D: speed.refresh(imperial ? Math.round(refreshValue[i] * 0.62137119223733) : refreshValue[i]); break;
		case 0x0E: timingAdvance.refresh(refreshValue[i]); break;
		case 0x0F: airTemp.refresh(imperial ? Math.round(refreshValue[i] * 1.8) + 32 : refreshValue[i]); break;
		case 0x11: throttlePos.refresh(refreshValue[i]); break;
	}
}
function updateGauge(gaugeObject)
{
	if(gaugeObject.mode == 1 && obdCodes[parseInt(gaugeObject.pid)]) switch(parseInt(gaugeObject.pid))
	{
		case 0x04: calcEngineLoad.refresh(gaugeObject.value); break;
		case 0x05: coolantTemp.refresh(imperial ? Math.round(gaugeObject.value * 1.8) + 32 : gaugeObject.value); break;
		case 0x06: b1stft.refresh(gaugeObject.value); break;
		case 0x07: b1ltft.refresh(gaugeObject.value); break;
		case 0x08: b2stft.refresh(gaugeObject.value); break;
		case 0x09: b2ltft.refresh(gaugeObject.value); break;
		case 0x0B: intakePressure.refresh(imperial ? Math.round(gaugeObject.value * 0.14503773773020923) : gaugeObject.value); break;
		case 0x0C: rpm.refresh(gaugeObject.value); break;
		case 0x0D: speed.refresh(imperial ? Math.round(gaugeObject.value * 0.62137119223733) : gaugeObject.value); break;
		case 0x0E: timingAdvance.refresh(gaugeObject.value); break;
		case 0x0F: airTemp.refresh(imperial ? Math.round(gaugeObject.value * 1.8) + 32 : gaugeObject.value); break;
		case 0x11: throttlePos.refresh(gaugeObject.value); break;
	}
}
function livePlayer()
{
	//Get currently support gauges and build them
	currentTime = new Date();
	getSupportedGauges = new XMLHttpRequest();
	getSupportedGauges.onreadystatechange = function()
	{
		if(this.readyState == 4 && this.status == 200)
		{
			obdCodes = JSON.parse(this.responseText);
			if(obdCodes.length == 0)
			{
				mainContent.innerHTML = "<div style='height: 10px;'></div><div class='loadWait'>No gauge information has been recorded</div>";
				return;
			}
			getInitialStatus = new XMLHttpRequest();
			getInitialStatus.onreadystatechange = function()
			{
				if(this.readyState == 4 && this.status == 200)
				{
					refreshValue = JSON.parse(this.responseText);
					gaugeHolder = document.createElement('div');
					gaugeHolder.className = "prettyBox";
					
					lastUpdateHolder = document.createElement('div');
					lastUpdateHolder.className = "lastUpdateHolder";
					
					lastUpdateText = document.createElement('div');
					lastUpdateText.className = "lastUpdateText";
					lastUpdateText.id = "lastUpdateText";
					lastUpdateText.innerHTML = "Finishing gauge sync..."
					lastUpdateHolder.appendChild(lastUpdateText);
					gaugeHolder.appendChild(lastUpdateHolder);
					
					gaugeBox = document.createElement('div');
					gaugeBox.className = 'gaugeBox';
					gaugeBox.id = 'gaugeBox';
					gaugeHolder.appendChild(gaugeBox);
					
					mainContent.innerHTML = "";
					mainContent.appendChild(gaugeHolder);
					
					buildGauges();
					setAllGauges();
				
					obdStream = new EventSource("dataHandler.php?action=obdStream");
					obdStream.onmessage = function(event)
					{
						currentTime = new Date();
						latestUpdate = JSON.parse(event.data);
						latestUpdate.forEach(function(element)
						{
							liveGaugesRunning = true;
							lastSeconds = Math.round((currentTime.getTime() - parseInt(element.timestamp)) / 1000);
							
							lastHours = Math.floor(lastSeconds / 3600);
							lastSeconds %= 3600;
							lastMinutes = Math.floor(lastSeconds / 60);
							lastSeconds %= 60;
							
							updateGauge(element);
						});
					};
					
					updateLastTimeInstance = setInterval(updateLastTime, 1000);
				}
			}
			getInitialStatus.open("GET", "dataHandler.php?action=obdDump&timestamp=" + currentTime.getTime(), true);
			getInitialStatus.send();
		}
		else if (this.readyState == 4 && this.status == 0) mainContent.innerHTML = "<div style='height: 10px;'></div><div class='offlineMessage'><i class='fa fa-times-circle' aria-hidden='true'></i> This feature is not available in offline mode</div>";
	}
	getSupportedGauges.open("GET", "dataHandler.php?action=obdCodes&timestamp=" + currentTime.getTime(), true);
	getSupportedGauges.send();
}
function updateLastTime()
{
	if(liveGaugesRunning)
	{
		lastSeconds++;
		if(lastSeconds == 60)
		{
			lastSeconds = 0;
			lastMinutes++;
		}
		if(lastMinutes == 60)
		{
			lastMinutes = 0;
			lastHours++;
		}
		lastUpdateText.innerHTML = "Last Update: " + (lastHours == 0 ? "" : lastHours + " hours, ") + (lastMinutes == 0 ? "" : lastMinutes + " minutes, ") + lastSeconds + " seconds ago";
	}
}
function obdPlayer()
{
	if(!showGauges) return;
	if(!logReturned)
	{
		obdPlayerInstance = setTimeout(obdPlayer, 100);
		return;
	}

	videoTime = (document.getElementById('dashPlayer').currentTime * 1000) + parseInt(videoArray[currentVideo]);
	gaugeBox = document.getElementById('gaugeBox');

	if(parseInt(obdLog[0].timestamp) > videoTime)
	{
		gaugesBuilt = false;
		if(gaugeBox.innerHTML != ("<div style='padding: 15px;'>OBD Information starts " + Math.round((obdLog[0].timestamp - parseInt(videoArray[currentVideo])) / 1000) + " seconds into this clip</div>"))
			gaugeBox.innerHTML = "<div style='padding: 15px;'>OBD Information starts " + Math.round((obdLog[0].timestamp - parseInt(videoArray[currentVideo])) / 1000) + " seconds into this clip</div>";
		obdPlayerInstance = setTimeout(obdPlayer, obdLog[0].timestamp - videoTime);
		return;
	}
	
	//Find where we're at in the array. If necessary, determine last value when gauges need refreshed (ex. skipping around)
	timeIndex = 0;
	refreshValue = [];
	while(parseInt(obdLog[timeIndex].timestamp) < videoTime)
	{
		if(refreshGauges || !gaugesBuilt) refreshValue[obdLog[timeIndex].pid] = obdLog[timeIndex].value;
		timeIndex++;
	}
	
	//If we're before a keyframe, we may not have a starting value. Read it from the keyframe
	if(refreshGauges || !gaugesBuilt)
		for(i = 0; i < 0xE1; i++) if(obdCodes[i])
			for(j = i; typeof refreshValue[i] == "undefined" && j + timeIndex < obdLog.length; j++)
				if(obdLog[timeIndex + j].pid == i)
					refreshValue[i] = obdLog[timeIndex + j].value;

	//Build gauges
	if(!gaugesBuilt)
	{
		gaugeBox.innerHTML = "";
		buildGauges();
		gaugesBuilt = true;
	}
	
	//Gauges are to be refreshed
	if(refreshGauges)
	{
		setAllGauges();
		refreshGauges = false;
	}

	//Most of the time, this function "falls through" until here. Show the new value
	updateGauge(obdLog[timeIndex]);
	
	//Run again at next gauge update
	if(obdLog.length != timeIndex + 1) obdPlayerInstance = setTimeout(obdPlayer, obdLog[timeIndex + 1].timestamp - videoTime - 100);
}
function addToCache()
{
	document.getElementById('downloadBox').innerHTML = "Downloading... 0% Complete";
	document.getElementById('downloadBox').onclick = null;
	
	fetchVideo = new XMLHttpRequest();
	fetchVideo.responseType = "blob";
	fetchVideo.onreadystatechange = function()
	{
		if(this.readyState == 4 && this.status == 200)
		{
			document.getElementById('downloadBox').color = "#ffd800";
			document.getElementById('downloadBox').innerHTML = "Saving to local database...";
			
			localforage.setItem("video" + videoArray[currentVideo], this.response).then(function()
			{
				savedVideos.push(videoArray[currentVideo]);
				localforage.setItem('savedVideos', savedVideos);

				downloadBox = document.getElementById('downloadBox');
				downloadBox.style.color = "green";
				downloadBox.innerHTML = "<i class='fa fa-check-circle' aria-hidden='true'></i> This video is cached";
				downloadBox.onclick = removeFromCache;
				downloadBox.onmouseout = function()
				{
					this.style.color = "green";
					this.innerHTML = "<i class='fa fa-check-circle' aria-hidden='true'></i> This video is cached";
				};
				downloadBox.onmouseover = function()
				{
					this.style.color = "red";
					this.innerHTML = "<i class='fas fa-times-circle'></i> Delete from cache";
				};
			});
		}
	};
	fetchVideo.open("GET", "/dataHandler.php?action=vidDownload&timestamp=" + videoArray[currentVideo], true);
	fetchVideo.onprogress = function (evt){if(evt.lengthComputable) document.getElementById('downloadBox').innerHTML = "Downloading: " + Math.round((evt.loaded / evt.total) * 100) + "% Complete";};
	fetchVideo.send();

}
function removeFromCache()
{
	if(!confirm("Are you sure you want to remove this video from your cache?")) return;
	
	localforage.removeItem("video" + videoArray[currentVideo]);
	savedVideos.splice(savedVideos.indexOf(videoArray[currentVideo]), 1);
	localforage.setItem('savedVideos', savedVideos);
	
	if(offline) menuSelect(1);
	else
	{
		downloadBox = document.getElementById('downloadBox');
		downloadBox.onmouseover = null;
		downloadBox.onmouseout = null;
		downloadBox.style.color = "";
		downloadBox.innerHTML = "Cache video for offline viewing";
		downloadBox.onclick = addToCache;
	}
}

function parseVideos()
{
	//Create an array of human-readable strings
	dateArray = [];
	videoSelect = document.createElement('select');
	videoSelect.id = 'videoSelect';
	videoSelect.onchange = function(){loadVideo(document.getElementById('videoSelect').value);};
	videoSelect.className = 'videoSelect';
	
	if(videoArray.length == 0)
	{
		mainContent.innerHTML += "<div style='height: 10px;'></div><div class='loadWait'>You have no recorded videos</div>";
		return;
	}
	
	for(i = 0; i < videoArray.length; i++)
	{
		newOption = document.createElement('option');
		
		getDate = new Date(parseInt(videoArray[i])); //Time is already in milliseconds
		switch(getDate.getMonth())
		{
			case 0: dateArray[i] = "January"; break;
			case 1: dateArray[i] = "February"; break;
			case 2: dateArray[i] = "March"; break;
			case 3: dateArray[i] = "April"; break;
			case 4: dateArray[i] = "May"; break;
			case 5: dateArray[i] = "June"; break;
			case 6: dateArray[i] = "July"; break;
			case 7: dateArray[i] = "August"; break;
			case 8: dateArray[i] = "September"; break;
			case 9: dateArray[i] = "October"; break;
			case 10: dateArray[i] = "November"; break;
			case 11: dateArray[i] = "December"; break;
		}
		
		minute = getDate.getMinutes();
		if(minute < 10) minute = "0" + minute;
		second = getDate.getSeconds();
		if(second < 10) second = "0" + second;
		
		dateArray[i] += " " + getDate.getDate() + ", " + getDate.getFullYear() + " " + getDate.getHours() + ":" + minute + ":" + second;
		newOption.value = i;
		newOption.innerHTML = dateArray[i];
		videoSelect.appendChild(newOption);
	}
	
	//Create the interface
	video = document.createElement('video');
	video.className = "dashPlayer";
	video.controls = true;
	video.id='dashPlayer';
	video.onplay = obdPlayer;
	video.onpause = function()
	{
		refreshGauges = true;
		clearTimeout(obdPlayerInstance);
	};
	video.ontimeupdate = function()
	{
		getDate = new Date(parseInt(videoArray[currentVideo]) + this.currentTime * 1000); //Time is already in milliseconds
		switch(getDate.getMonth())
		{
			case 0: shownTime = "January"; break;
			case 1: shownTime = "February"; break;
			case 2: shownTime = "March"; break;
			case 3: shownTime = "April"; break;
			case 4: shownTime = "May"; break;
			case 5: shownTime = "June"; break;
			case 6: shownTime = "July"; break;
			case 7: shownTime = "August"; break;
			case 8: shownTime = "September"; break;
			case 9: shownTime = "October"; break;
			case 10: shownTime = "November"; break;
			case 11: shownTime = "December"; break;
		}
		
		minute = getDate.getMinutes();
		if(minute < 10) minute = "0" + minute;
		second = getDate.getSeconds();
		if(second < 10) second = "0" + second;
		
		shownTime += " " + getDate.getDate() + ", " + getDate.getFullYear() + " " + getDate.getHours() + ":" + minute + ":" + second;
		
		document.getElementById('videoSelect').childNodes[currentVideo].innerHTML = shownTime;
	};
	
	paddingBox = document.createElement('div');
	paddingBox.style.height = '10px';
	mainContent.appendChild(paddingBox);
	
	navBar = document.createElement('div');
	navBar.className = "navbar";
	navBar.id = "navbar";

	downloadBox = document.createElement('div');
	downloadBox.className = 'downloadBox';
	downloadBox.id = 'downloadBox';
	
	prettyBox = document.createElement('div');
	prettyBox.className = 'prettyBox';
	prettyBox.appendChild(video);
	prettyBox.appendChild(navBar);
	prettyBox.appendChild(downloadBox);
	mainContent.appendChild(prettyBox);

	gaugeHolder = document.createElement('div');
	gaugeHolder.className = "prettyBox";
	
	gsHolder = document.createElement('div');
	gsHolder.className = "gsHolder";
	
	gsLabel = document.createElement('div');
	gsLabel.className = "gsLabel";
	gsLabel.innerHTML = "Show gauges";
	gsHolder.appendChild(gsLabel);
	
	gsWrapper = document.createElement('div');
	gsWrapper.className = "gsWrapper";
	
	gsLabel = document.createElement('label');
	gsLabel.className = "switch";
	
	gsInput = document.createElement('input')
	gsInput.type = "checkbox";
	gsInput.checked = showGauges;
	gsInput.addEventListener('click', function()
	{
		showGauges = !showGauges;
		localStorage.setItem("showGauges", (showGauges ? 1 : 0));
		
		if(showGauges)
		{
			document.getElementById('gaugeBox').innerHTML = "<div style='padding: 15px;'>Loading gauges...</div>";
			document.getElementById('gaugeBox').style.display = "";
			obdPlayerInstance = setTimeout(obdPlayer, 0);
		}
		else
		{
			clearTimeout(obdPlayerInstance);
			gaugesBuilt = false;
			document.getElementById('gaugeBox').style.display = "none";
			document.getElementById('gaugeBox').innerHTML = "";
		}
	});
	gsLabel.appendChild(gsInput);
	
	gsSlider = document.createElement('span');
	gsSlider.className = "slider";
	gsLabel.appendChild(gsSlider);
	gsWrapper.appendChild(gsLabel);
	gsHolder.appendChild(gsWrapper);
	gaugeHolder.appendChild(gsHolder);
	
	gaugeBox = document.createElement('div');
	gaugeBox.className = 'gaugeBox';
	gaugeBox.id = 'gaugeBox';
	if(!showGauges) gaugeBox.style.display = "none";
	gaugeHolder.appendChild(gaugeBox);
	
	mainContent.appendChild(gaugeHolder);

	paddingBox = document.createElement('div');
	paddingBox.style.height = '10px';
	mainContent.appendChild(paddingBox);
	
	loadVideo(videoArray.length - 1);
}

window.addEventListener("load", function()
{
	if(localStorage.getItem('imperial') != null) imperial = parseInt(localStorage.getItem('imperial'));
	if(localStorage.getItem('showGauges') != null) showGauges = parseInt(localStorage.getItem('showGauges'));
	if(sessionStorage.getItem('selectedMenu') != null) selectedMenu = parseInt(sessionStorage.getItem('selectedMenu'));
	localforage.getItem('savedOBDLogs', function (err, value)
	{
		if(value != null) savedOBDLogs = value;
		forageLoaded++;
	});
	localforage.getItem('savedVideos', function (err, value)
	{
		if(value != null) savedVideos = value;
		forageLoaded++;
	});

	menuSelect(selectedMenu);
});
