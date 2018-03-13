#!/bin/bash
echo -e "\e[1;4;246mRoadApplePi Setup v1.0\e[0m
Welcome to RoadApplePi setup. RoadApplePi is \"Black Box\" software that
can be retrofitted into any car with an OBD port. This software is meant
to be installed on a Raspberry Pi running unmodified Raspbian Stretch,
but it may work on other OSs or along side other programs and modifications.
Use with anything other then out-of-the-boxVanilla Raspbain Stretch is not
supported.

This script will download, compile, and install the necessary dependencies
before finishing installing RoadApplePi itself. Depending on your model of
Raspberry Pi, this may take several hours.
"

#Prompt user if they want to continue
read -p "Would you like to continue? (y/N)" answer
if [ "$answer" == "n" ] || [ "$answer" == "N" ] || [ "$answer" == "" ]
then
	echo "Setup aborted"
	exit
fi

#################
# Update System #
#################
echo -e "\e[1;4;93mStep 1. Updating system\e[0m"
sudo apt update
sudo apt upgrade

###########################################
# Install pre-built dependencies from Apt #
###########################################
echo -e "\e[1;4;93mStep 2. Install pre-built dependencies from Apt\e[0m"
sudo apt install -y dnsmasq hostapd libbluetooth-dev apache2 php7.0 php7.0-mysql php7.0-bcmath mariadb-server libmariadbclient-dev libmariadbclient-dev-compat uvcdynctrl
sudo systemctl disable hostapd dnsmasq

################
# Build FFMpeg #
################
echo -e "\e[1;4;93mStep 3. Build ffmpeg (this may take a while)\e[0m"
wget https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.gz
tar -xvf ffmpeg-3.4.2.tar.gz
cd ffmpeg-3.4.2
./configure --enable-gpl --enable-nonfree --enable-mmal --enable-omx --enable-omx-rpi
make
sudo make install

#######################
# Install RoadApplePi #
#######################
echo -e "\e[1;4;93mStep 99. Building and installing RoadApplePi\e[0m"
cd ..
make clean
make
sudo make install

sudo cp -r html /var/www/
sudo chown -R www-data:www-data /var/www/html
sudo chmod -R 0755 /var/www/html
sudo cp raprec.service /lib/systemd/system
sudo chown root:root /lib/systemd/system/raprec.service
sudo chmod 0755 /lib/systemd/system/raprec.service
sudo systemctl daemon-reload
sudo systemctl enable raprec
sudo cp hostapd-rap.conf /etc/hostapd
sudo cp dnsmasq.conf /etc
sudo mysql < roadapplepi.sql

echo "\nDone! Please reboot your Raspberry Pi now"
