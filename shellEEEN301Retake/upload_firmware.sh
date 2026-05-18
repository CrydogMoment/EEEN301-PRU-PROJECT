sudo ./stop_pru.sh
sudo ./configure_pins.sh
rm -vf /lib/firmware/am335x-pru0-fw/*.out
sudo cp -v *.out /lib/firmware/am335x-pru0-fw
echo 'am335x-pru0-fw' > /sys/class/remoteproc/remoteproc1/firmware
sudo ./start_pru.sh
