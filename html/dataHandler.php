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

if($_GET['action'] == "") die();
set_time_limit(0);

//Gets OBD log in date range
if($_GET['action'] == "obdLog")
{
	if($_GET['min'] == "" || $_GET['max'] == "") die();

	$mysqli = new mysqli('127.0.0.1', 'roadapplepi', 'roadapplepi', 'roadapplepi');
	if ($mysqli->connect_errno) die("[]");

	$limitString = "timestamp >= ".$_GET['min'].($_GET['max'] == 0 ? "" : " and timestamp < ".$_GET['max']);

	$sql = "SELECT timestamp, mode, pid, value from obdLog where $limitString order by timestamp asc";
	if(!$result = $mysqli->query($sql)) die("[]");

	while($obdEntry = $result->fetch_assoc()) $obdLog[] = $obdEntry;
	
	$result->free();
	$mysqli->close();

	echo json_encode($obdLog);
	exit();
}

//Gets latest OBD Information
if($_GET['action'] == "obdDump" && is_numeric($_GET['timestamp']))
{
	//Only called during gauge sync, which requires an accurate clock. Sync time now
	system("raprun -n " . $_GET['timestamp']);
	
	$mysqli = new mysqli('127.0.0.1', 'roadapplepi', 'roadapplepi', 'roadapplepi');
	if ($mysqli->connect_errno) die("[]");
	
	$sql = "SELECT codes FROM obdCodes ORDER BY timestamp DESC LIMIT 1";
	if(!$result = $mysqli->query($sql)) die("[]");
	$obdCodes = $result->fetch_assoc();
	if($obdCodes['codes'] == "") die("[]");
	
	$recordedCodes = json_decode($obdCodes["codes"]);
	$result->free();
	
	$sql = "SELECT timestamp, mode, pid, value from obdLog order by timestamp desc LIMIT 1000";
	if(!$result = $mysqli->query($sql)) die("[]");
	$obdLog = array();
	while(($obdEntry = $result->fetch_assoc()) && count($obdLog) < array_count_values($recordedCodes)[1])
		if(!array_key_exists($obdEntry["pid"], $obdLog)) $obdLog[$obdEntry["pid"]] = $obdEntry["value"];
	
	$result->free();
	$mysqli->close();
	
	echo json_encode($obdLog);
	exit();
}

//Stream current obd information
if($_GET['action'] == "obdStream")
{
	header('Content-Type: text/event-stream');
	header('Cache-Control: no-cache');
	
	$mysqli = new mysqli('127.0.0.1', 'roadapplepi', 'roadapplepi', 'roadapplepi');
	if ($mysqli->connect_errno) die("data: []");
	
	$result = $mysqli->query("SELECT timestamp from obdLog order by timestamp desc limit 1");
	if($obdEntry = $result->fetch_assoc()) $lastTime = bcsub($obdEntry["timestamp"], "1000");
	$result->free();
	
	while(true)
	{
		$obdLog = array();
		$query = "SELECT timestamp, mode, pid, value from obdLog where timestamp > $lastTime order by timestamp asc";
		$lastTime = time() . "000";
		$result = $mysqli->query($query);
		
		while($obdEntry = $result->fetch_assoc()) $obdLog[] = $obdEntry;
		$result->free();
		
		if(count($obdLog) != 0)
		{
			echo "data: " . json_encode($obdLog) . "\n\n";
			ob_end_flush();
			flush();
		}
		
		sleep(1);
	}
	$mysqli->close();
	exit();
}

//Guesses at supported codes based off of given time
if($_GET['action'] == "obdCodes")
{
	if($_GET['timestamp'] == "") die();

	$mysqli = new mysqli('127.0.0.1', 'roadapplepi', 'roadapplepi', 'roadapplepi');
	if ($mysqli->connect_errno) die("[]");

	$timestamp = $_GET['timestamp'];
	$sql = "SELECT codes FROM obdCodes WHERE timestamp <= $timestamp ORDER BY timestamp DESC LIMIT 1";
	if(!$result = $mysqli->query($sql)) die("[]");
	$obdCodes = $result->fetch_assoc();

	$result->free();
	$mysqli->close();

	if($obdCodes['codes'] == "") $obdCodes['codes'] = "[]";
	echo $obdCodes['codes'];
	exit();
}

//Load current settings
if($_GET['action'] == "currentSettings")
{
	$mysqli = new mysqli('127.0.0.1', 'roadapplepi', 'roadapplepi', 'roadapplepi');
	if ($mysqli->connect_errno) die();

	$sql = "SELECT value FROM env WHERE name='currentOBD'";
	$result = $mysqli->query($sql);
	$dataToReturn["OBD"] = "";
	$getValue = $result->fetch_assoc();
	$dataToReturn["OBD"] = $getValue['value'];
	if($dataToReturn["OBD"] == "") $dataToReturn["OBD"] = "Not paired!";

	$sql = "SELECT value FROM env WHERE name='mode'";
	$result = $mysqli->query($sql);
	$getValue = $result->fetch_assoc();
	$dataToReturn["Mode"] = ($getValue['value'] == "client" ? 1 : 0);

	$sql = "SELECT value FROM env WHERE name='ssid'";
	$result = $mysqli->query($sql);
	$getValue = $result->fetch_assoc();
	$dataToReturn["SSID"] = $getValue['value'];

	$sql = "SELECT value FROM env WHERE name='psk'";
	$result = $mysqli->query($sql);
	$getValue = $result->fetch_assoc();
	$dataToReturn["PSK"] = $getValue['value'];

	$result->free();
	$mysqli->close();

	echo json_encode($dataToReturn);
	exit();
}

//Fetch all available videos
if($_GET['action'] == "vidList")
{
	$timestamps = scandir("/var/www/html/vids");
	$totalFiles = count($timestamps);
	for($i = 0; $i < $totalFiles; $i++)
	{
		$splitArr = explode(".", $timestamps[$i]);
		if(count($splitArr) == 2 && $splitArr[1] == "mp4") $timestamps[$i] = $splitArr[0];
		else unset($timestamps[$i]);
	}

	$timestamps = array_values($timestamps);
	sort($timestamps);
	echo (count($timestamps) == 0 ? "[]" : json_encode($timestamps));
	exit();
}

if($_GET['action'] == "vidDownload" && $_GET['timestamp'] != "")
{
	header('Content-Description: File Transfer');
    header('Content-Type: application/octet-stream');
    header('Content-Disposition: attachment; filename="'.$_GET['timestamp'].'.mp4"');
    header('Expires: 0');
    header('Cache-Control: must-revalidate');
    header('Pragma: public');
    header('Content-Length: ' . filesize("/var/www/html/vids/".$_GET['timestamp'].".mp4"));
    readfile("/var/www/html/vids/".$_GET['timestamp'].".mp4");
    exit;
}

//Configure wireless
if(in_array($_GET['action'], array('ap', 'ct')))
{
	$ssid = addslashes($_POST['ssid']);
	$password = addslashes($_POST['password']);
	system("raprun ".($_GET['action'] == "ap" ? "-a" : "-c")." ".$ssid." ".$password);
	exit();
}

//Discover bluetooth devices
if($_GET['action'] == 'discover')
{
	ob_start();
	system("raprun -d", $retStatus);
	$returnedJson = ob_get_clean();
	if($retStatus == 0) echo $returnedJson;
	exit();
}

//Pair with a bluetooth device
if($_GET['action'] == 'pair')
{
	ob_start();
	system("raprun -p ".$_POST['mac']." ".$_POST['pin'], $retStatus);
	$exitText = ob_get_clean();
	
	if($retStatus == 0)
	{
		$toJS = array(true, $_POST['mac']);
		
		$mysqli = new mysqli('127.0.0.1', 'roadapplepi', 'roadapplepi', 'roadapplepi');
		$mysqli->query("delete from env where name='currentOBD'");
		$mysqli->query("insert into env (name, value) values ('currentOBD', '".$mysqli->real_escape_string($_POST['mac'])."')");
		$mysqli->close();
	}
	else $toJS = array(false, $exitText);
	
	echo json_encode($toJS);
	exit();
}

//Power management
if($_GET['action'] == "shutdown") system("raprun -s");
if($_GET['action'] == "restart") system("raprun -r");

//Sync time with connected device
if($_GET['action'] == "timeSync" && is_numeric($_GET['timestamp']))
{
	$returnCode = 0;
	system("raprun -n " . $_GET['timestamp'], $returnCode);
	echo ($returnCode == 0 ? "success" : "fail");
	exit();
}
?>
