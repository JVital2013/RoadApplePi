# RoadApplePi 
RoadApplePi is a simple to install system for your Raspberry Pi designed to record dashcam videos and engine information from your car in real-time, and then make it all accessable from an easy-to-use Web App that can be viewed on your smartphone or computer.

Demonstration video: [https://www.youtube.com/watch?v=-G1HIgr2cvU](https://www.youtube.com/watch?v=-G1HIgr2cvU)

## Features
 - **Dashcam Recordings with car stats:** Whenever your RoadApplePi is powered on and connected to a webcam and Bluetooth OBD2 reader, it's recording every piece of information it can scrape up. Using hardware-accelerated video encoding, this results in smooth dashcam footage and an instrument cluster that shows exactly what you (and others around you) were doing at any given time.
 - **Real-time OBD (II) Information:** See what's going on under-the hood in real time with our "Dashboard" view
 - **Web App Access:** Many "carputers" based around the Raspberry Pi require the installation of a screen in your car, making them difficult to install. RoadApplePi works around this issue by giving you a modern web app that can be accessed from any internet-enabled device.
 - **Offline Access:** Need to view dashcam recordings while away from your RoadApplePi? Not a problem! The Web App works without an active connection to your Pi, and you can cache videos and instrument clusters for offline viewing.
 - **Wireless connectivity:** Most of the time, your car will be out of range of any WiFi network. RoadApplePi solves this issue by having the option to create a wireless network, specifically for ease-of-access to the web app. You also have the option to connect to an existing wireless network if you're in range.
 - **Set it up and forget about it:** If you're going all the way with an automated car power system, you won't have to touch your RoadApplePi again after the initial setup (see "Power and Time Options" for more details)
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
 - **A power source:** Powering the beast is the trickiest part of the whole build. If you're just getting started, I'd recommend a 2A car phone charger with a micro USB cable to power the Pi. This won't automatically/cleanly power off your Pi, but it'll get it powered until you have a better solution in place. See "Power and Time Options" for more details.
 - **Bluetooth ELM327 OBDII adapter (optional)** While technically optional, a Bluetooth ELM327 adapter is required to record engine information. They're around $10 on Amazon and completely worth the price.

## Installation
Updated for Buster
 1. Flash Raspbian (lite) onto an SD Card and boot your Raspberry Pi, performing any initial setup needed to get it connected to the internet
 2. From the command line, run:
	```
	sudo apt update && sudo apt install -y git
	git clone https://github.com/matt2005/RoadApplePi
	cd RoadApplePi
	./setup.sh
	sudo reboot
	```
	The setup script may take several hours to run, depending on your Raspberry Pi model
 3. On another device, go to `http://raspberrypi/`.
	--Note: Your device hostname can be changed by running `sudo raspi-config` on your pi. I've changed mine to "roadapplepi". A feature to change this from the web app will be added in the future
 4. Put it into Access Point mode, specifying a network SSID and Password
 5. After verifying that you can connect to the newly created WiFi network, shut down your Pi and move it out to your car. Connect your webcam to the Pi and the OBDII reader to your car and power the Pi back on
 6. With your ignition in the "Run" position, go to `http://raspberrypi/` (or other hostname if you changed it). From the settings menu, pair your pi with the OBDII reader. Most of them use "1234" for the PIN.
 7. Go for a drive and see how it works!

## Power and Time Options
If you've made it to this point, great! You should have a fully functional black box in your car. However, you have most likely run into two issues

 1. Raspberry Pis don't come with any sort of Real-Time Clock (RTC). In an internet-less location such as your car, your pi won't keep the time when turned off. You can partially solve this issue by synchronizing the time on the Pi with the time on your connected device (e.g. phone) via the web app, but this isn't optimal.
 2. Powering your Pi with your car's cigarette lighter if fraught with peril. In many cars, these ports are not "always on" and turn off with the car. This won't gracefully shut down your Pi, corrupting all your data. Even if the port *is* always on, you don't want the Pi recording 24/7. There are power management options in the web app, but they again aren't optimal

To solve both of these issues, there are two solutions that I recommend

### Mausberry Circuits 3A Car Supply / Switch and an RTC
Out of the two, this is the simplest option, although it requires two components. The Mausberry Circuits 3A Car Supply (https://mausberry-circuits.myshopify.com/collections/frontpage/products/3a-car-supply-switch-1) performs two functions: it steps the car voltage down to a Pi-Friendly level, and gracefully shuts the Pi down automatically when the car powers off. On their web site, they mention wiring the device into your battery and ignition. Don't fret, you don't need to go to this extreme: you can use fuse taps to tap an always-on line, and an ignition-powered line in your fusebox. For more details on this, take a look at the SleepyPi 2 option.

Unfortunately, the Mausberry power supply does not contain an RTC, so you'll need to purchase one separately (unless you don't care about incorrect times). For more information, see https://cdn-learn.adafruit.com/downloads/pdf/adding-a-real-time-clock-to-raspberry-pi.pdf.

Fair warning, I haven't actually tried this, but it should work fine.

### SleepyPi 2
Although this option is more complicated in initial setup compared to the Mausberry, it's the path I opted for and I've had great success. The SleepyPi 2 (https://spellfoundry.com/product/sleepy-pi-2/) is inherently an Arduino-based "Hat" for the Raspberry Pi that can be programmed as needed. Out-of-the-box, the SleepyPi 2 performs 2 functions that we care about: voltage transformation to Raspberry Pi friendly voltages, and an easy-to-configure RTC. However, it does not automatically power off the Pi when the car is switched off.
 - It must be programmed to do this. I've already modified one of Spell Foundry's example programs to do just this, and is the `RoadApplePi.ino` file in this repository. To flash this sketch onto your Sleepy Pi, see https://spellfoundry.com/sleepy-pi/programming-arduino-ide/.
 - You need to build a voltage divider for the program to be able to properly read the current power status of the car.  Thanks to Spell Foundry, here's the voltage divider you need to build:  
![Voltage Divider](https://spellfoundry.com/wp/wp-content/uploads/2016/11/Car-GPIO-Level-Signaller.png)  
You can build this circuit on whatever you would like, but I used a solderable breadboard. For those less well-versed in reading schematics, here's a quick explanation of what you're doing
	 - *(Not shown in this schematic)* Wire an always-hot line from your fuse box to the VIN of the SleepyPi
	 - Wire an ignition-dependent line into the voltage divider (signified by the 14.4V in the diagram)
	 - Wire a ground wire from your car into both the voltage divider (very bottom of the diagram) and the 0V part of the power input on the SleepyPi.
	 - *The outputs of the voltage divider are on the right of the diagram.* If you're using my SleepyPi sketch, wire the 3.09V output into Pin 14 of the Sleepy Pi, and the 0V output to the SleepyPi's GND
 - Once assembled, you're left with a 3-layer sandwich: your raspberry pi, the sleepy pi, and your voltage divider. Here's what mine looks like installed in my car:
 ![Isn't it purdy?](/../screenshots/mypi.jpg?raw=true)

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
If you really like this project and want to give back, donations are accepted!

 - **Bitcoin:** 177S69eip9mqpygozXKq9tzggK5aMpzKxQ
 - **Ethereum:** 0x40E182eF00F6834936Db68A1868699B0b23D8094  
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=BJUZ3R3JVAFK6)

