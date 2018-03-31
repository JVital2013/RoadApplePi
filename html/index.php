<?php
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
?>
<!DOCTYPE html>
<html manifest="roadapplepi.appcache">
	<head>
		<title>RoadApplePi</title>
		
		<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
		<meta name="mobile-web-app-capable" content="yes">
		<meta name="apple-mobile-web-app-capable" content="yes">
		<link rel="apple-touch-icon" href="/icon.png">
	
		<link rel="stylesheet" href="/styles.css">
		<link rel="stylesheet" href="/opensans.css">
		<link rel="manifest" href="/manifest.json">
		<script src='/script.js'></script>
		<script src='/justgage.js'></script>
		<script src='/raphael-2.1.4.min.js'></script>
		<script src='/localforage.min.js'></script>
		<script src="/fontawesome-all.js"></script>
	</head>
	<body>
		<div class='topBar'>
			<div class='topBarInner'>
				<img class='barLogo' src='/logo.svg' onclick='slideDrawer();' />
				<div class='barTitle' id='barTitle'></div>
				<div class="barRefresh" onclick="menuSelect(selectedMenu)"><i class="fa fa-sync" aria-hidden="true"></i></div>
			</div>
		</div>
		<div class='sideBar' id='sideBar'>
			<div class='mainBodyPadding'></div>
			<div id='menuItem0' onclick='menuSelect(0);' class='menuItem'><i class="fa fa-tachometer-alt" aria-hidden="true" style='font-size: 18pt; vertical-align: middle;'></i><div style='vertical-align: middle; display: inline-block;'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Dashboard</div></div>
			<div id='menuItem1' onclick='menuSelect(1);' class='menuItem'><i class="fa fa-video" aria-hidden="true" style='font-size: 18pt; vertical-align: middle;'></i><div style='vertical-align: middle; display: inline-block;'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Recordings</div></div>
			<div id='menuItem2' onclick='menuSelect(2);' class='menuItem'><i class="fa fa-info-circle" aria-hidden="true" style='font-size: 18pt; vertical-align: middle;'></i><div style='vertical-align: middle; display: inline-block;'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;System Info</div></div>
			<div id='menuItem3' onclick='menuSelect(3);' class='menuItem'><i class="fa fa-cog" aria-hidden="true" style='font-size: 18pt; vertical-align: middle;'></i><div style='vertical-align: middle; display: inline-block;'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Settings</div></div>
		</div>
		<div class='mainBody' onclick='retractDrawer();'>
			<div class='mainBodyPadding'></div>
			<div id='mainContent' class='mainContent'></div>
		</div>
	</body>
</html>
