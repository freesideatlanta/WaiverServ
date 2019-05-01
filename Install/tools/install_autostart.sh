mkdir -p /etc/xdg/lxsession/LXDE/
mkdir -p /etc/xdg/lxsession/LXDE-pi/
mkdir -p /home/pi/.config/lxsession/LXDE/
mkdir -p /home/pi/.config/lxsession/LXDE-pi/
mkdir -p /root/.config/lxsession/LXDE/
mkdir -p /root/.config/lxsession/LXDE-pi/

sudo cp $1 /etc/xdg/lxsession/LXDE/autostart &
sudo cp $1 /etc/xdg/lxsession/LXDE-pi/autostart &
sudo cp $1 /home/pi/.config/lxsession/LXDE-pi/autostart &
sudo cp $1 /home/pi/.config/lxsession/LXDE/autostart &
sudo cp $1 /root/.config/lxsession/LXDE-pi/autostart &
sudo cp $1 /root/.config/lxsession/LXDE/autostart &
sudo apt-get install -y python-qt4