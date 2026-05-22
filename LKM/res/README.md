- Using ISRs in kernel modules: page 729 of The Book
- Mutex version of ebbchar at https://github.com/derekmolloy/exploringBB/blob/master/extras/kernel/ebbcharmutex/ebbcharmutex.c

# Building

- Make sure the beaglebone has the linux headers installed.
  From this directory, `sudo apt install ./res/linux-headers-5.10.168-ti-r68_1bullseye_armhf.deb`.
- Build with `sudo make`.

# Once you've built a kernel module
- Insert with `sudo insmod rootkit.ko`
- To see if it is loaded use `lsmod`
- Get some info with `modinfo rootkit.ko`
- Remove with `sudo rmmod rootkit.ko`
- Look at kernel log file to see the messages from a kernel module:
  - `sudo su -`
    - Must be a superuser to see these super cool secrets. Logout with `exit`.
  - `tail -f /var/log/kern.log`
