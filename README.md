# RoadApplePi 
![RoadApplePi Logo](/html/icon.png?raw=true)
RoadApplePi is a simple to install system for your Raspberry Pi designed to record dashcam videos and engine information from your car in real-time, and then make it all accessable from an easy-to-use Web App that can be viewed on your smartphone or computer.
## Features
 - **Dashcam Recordings with car stats:** Whenever your RoadApplePi is powered on and connected to a webcam and Bluetooth OBD2 reader, it's recording every piece of information it can scrape up. Using hardware-accelerated video encoding, this results in smooth dashcam footage and an instrument cluster that shows exactly what you (and others around you) were doing at any given time.
 - **Real-time OBD(2) Information:** See what's going on under-the hood in real time with our "Dashboard" view
 - **Web App Access:** Many "carputers" based around the Raspberry Pi require the installation of a screen in your car, making them difficult to install. RoadApplePi works around this issue by giving you a modern web app that can be accessed from any internet-enabled device.
 - **Offline Access:** Need to view dashcam recordings while away from your RoadApplePi? Not a problem! The Web App works without an active connection to your Pi, and you can cache videos and instrument clusters for offline viewing.
 - **Wireless connectivity:** Most of the time, your car will be out of range of any WiFi network. RoadApplePi solves this issue by having the option to create a wireless network, specifically for ease-of-access to the web app. You also have the option to connect to an existing wireless network if you're in range.
 - **Set it up and forget about it:** If you're going all the way with an automated car power system, you won't have to touch your RoadApplePi again after the initial setup (see "Power Options") for more details
 - **..And more to come!** RoadApplePi is free software, developed in my free time. Depending on your help and support I hope to implement other features, such as:
	- More OBD pids (car stats)
	- The ability to read and log engine trouble codes
	- Tune your ECM
	- Export dashcam recordings with superimposed obd information as a video for complete compatibility
	- Become fully self-aware (just joking ... I hope)
## What you need
RoadApplePi is designed to work with as little or much as you have. Bare minimum, you'll need
 - **A Raspberry Pi.** Any model will work, but the RPi Zero W  or RPi 3 is recommended since they have Bluetooth and WiFi built in. Otherwise, a USB Bluetooth/WiFi dongle is required
	- Side note: This software was developed almost entirely on a RPi Zero W with few issues. However, due to the slow processor on this model, some things run slow. The Real-Time dashboard runs around 15 seconds behind, and loading the instrument cluster on recorded videos may take 20-30 seconds (after which, they play fine). If you're opting to use a slower model, I recommend doing the initial setup of a spare Raspberry Pi 2/3 to speed up the software compilation process. After that, the SD Card can be moved to your pi of choice
 - **Class 10 SD Card with Raspbian Stretch (optionally, the lite flavor):** This software was written to work on Raspbian Stretch, but it may work on other OSs. YMMV. Also, I recommend using at least a 16GB SD Card to give you enough space for your dashcam recordings. I've been using the same 32GB SD Card for the past 6 months with no issues. This gives me enough space to retain, on average, 3 weeks of recordings. If you need help setting up an SD Card with Raspbian, see [here](https://www.raspberrypi.org/documentation/installation/installing-images/).
 - **A Webcam:** All development was done with an old USB webcam, but any webcam should work including Raspberry Pi-specific ones
 - **A power source:** Powering the beast is the trickiest part of the whole build. If you're just getting started, I'd recommend a 2A car phone charger with a micro USB cable to power the Pi. This won't automatically/cleanly power off your Pi, but it'll get it powered until you have a better solution in place. See "Power Options" for more details.
 - **Bluetooth ELM327 OBDII adapter (optional)** While technically optional, a Bluetooth ELM327 adapter is required to record engine information. They're around $10 on Amazon and completely worth the price.
## Installation
 1. Flash Raspbian (lite) onto an SD Card and boot your Raspberry Pi, performing any initial setup needed to get it connected to the internet
 2. From the command line, run:
	```
	sudo apt update && sudo apt install git
	git clone https://github.com/JVital2013/RoadApplePi
	cd RoadApplePi
	./setup.sh
	sudo reboot
	```
	The setup script may take several hours to run, depending on your Raspberry Pi model
 3. On another device, go to http://raspberrypi/.
	--Note: Your device hostname can be changed by running `sudo raspi-config` on your pi. I've changed mine to "roadapplepi"
 4. Put it into Access Point mode, specifying a network SSID and Password
 5. After verifying that you can connect to the newly created WiFi network, shut down your Pi and move it out to your car. Connect your webcam to the Pi and the OBDII reader to your car and power the Pi back on
 6. With your ignition in the "Run" position, go to http://raspberrypi/ (or other hostname if you changed it). From the settings menu, pair your pi with the OBDII reader. Most of them use "1234" for the PIN.
 7. Go for a drive and see how it works!
## Power Options
Designed to power with a SleepyPi 2. Sketch available at RoadApplePi.ino. Instructions are a WIP
## Licensing
RoadApplePi is released under the GPLv3. For more information, see COPYING
## Credits
The following software has made RoadApplePi possible
 - FFMpeg ([https://www.ffmpeg.org/](https://www.ffmpeg.org/)). Licensed under LGPL
 - JustGage ([http://justgage.com/](http://justgage.com/)). Licensed under the MIT License
 - localForage ([https://localforage.github.io/localForage/](https://localforage.github.io/localForage/)). Licensed under the Apache License
 - Open Sans ([https://fonts.google.com/specimen/Open+Sans](https://fonts.google.com/specimen/Open+Sans)). Licensed under the Apache License

Special thanks to Zipcar for making the dream possible! I'd been playing with the idea for this project for quite some time, when on March 14, 2017, [Zipcar ran a competition to give away 314 Raspberry Pi Zero Ws](http://www.zipcar.com/piday). [I was a lucky winner with this idea](https://twitter.com/JVital2013/status/841522799109890052). Thanks to this stroke of luck, I've been able to share this fun project with you all.

Also, extra special thanks to my wife, who put up with me shoving this project under her nose every time I made a tiny bit of progress.
## Donate
If you really like this project and want to give back, donations are accepted! Instructions are a WIP
