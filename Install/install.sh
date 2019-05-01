# updates and stuff
sudo apt-get update
sudo apt-get -y upgrade

#keep screen on and remove cursor if no movment
sudo apt-get install -y unclutter

#enable ssh
sudo touch /boot/ssh

#move our stuff to a better spot
sudo mkdir -p /usr/local/bin/FSWaiver
sudo cp ./run_waiver.sh /usr/local/bin/FSWaiver/run_waiver.sh
sudo cp ../WaiverServ/bin/ARM/Release/WaiverServ.out /usr/local/bin/FSWaiver/WaiverServ
sudo cp ../WaiverServ/FS_Waiver_Apr_2019.png /usr/local/bin/FSWaiver/FS_Waiver_Apr_2019.png

#Install udev rules for tablet
sudo cp ./50-topaz.rules /etc/udev/rules.d/  50-topaz.rules

#Execute/Read Permissions
sudo chmod a+r  /usr/local/bin/FSWaiver/
sudo chmod a+rx /usr/local/bin/FSWaiver/WaiverServ
sudo chmod a+rx /usr/local/bin/FSWaiver/run_waiver.sh
sudo chmod a+r /usr/local/bin/FSWaiver/FS_Waiver_Apr_2019.png


#Make Folder To store Waivers
sudo mkdir /FSWaiver
sudo chmod a+rw /FSWaiver

#remove annoying thing asking us to change password
sudo rm /etc/xdg/lxsession/LXDE-pi/sshpwd.sh &
sudo rm /etc/xdg/lxsession/LXDE/sshpwd.sh &

#attempt to change the keyboard layout
sudo cp tools/keyboard /etc/default/keyboard
invoke-rc.d keyboard-setup start

#Do a number of things
#including trying again to keep the screen from blanking
sudo cp tools/.bashrc /home/pi/.bashrc

#Rotate the screen
sudo echo "display_rotate=1" > /boot/config.txt

#Install autostart
sudo sh tools/install_autostart.sh autostart_waiver