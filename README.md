# Linux RGB LED effect support for the Roccat Vulcan 100/120 Keyboard

This little binary adds some bling to your Roccat Vulcan 100/120 Keyboard when you use it in Linux. It supports setting up the default "Wave" effect that the keyboard can play on its own, as well as a more advanced homegrown "Impact" effect that responds to your keystrokes, and does some "ghost" typing when you're idle.

## Installation

### Ubuntu via [Launchpad PPA](https://launchpad.net/~duncanthrax/+archive/ubuntu/roccat-vulcan)

```bash
sudo add-apt-repository ppa:duncanthrax/roccat-vulcan
sudo apt-get update
sudo apt install roccat-vulcan
sudo udevadm control --reload-rules && udevadm trigger
# You might have to replug your keyboard or reboot at this
# point, so udev can update the permissions on the device nodes.
roccat-vulcan -f
```

### Manual build (Expert zone)

* You need to install development packages for libevdev and libhidapi.
* Type 'make && make install' in /src. 
* Replug keyboard or reboot unless you want to run as root.
* Run `roccat-vulcan -f`.
