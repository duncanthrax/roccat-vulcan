# Linux RGB LED effect support for the Roccat Vulcan 100/120 Keyboard

This little binary adds some bling to your Roccat Vulcan 100/120 Keyboard when you use it in Linux. It supports setting up the default "Wave" effect that the keyboard can play on its own, as well as a more advanced homegrown "Impact" effect that responds to your keystrokes, and does some "ghost" typing when you're idle.

![Demo GIF Image](demo.gif)

## Installation

### Ubuntu via [Launchpad PPA](https://launchpad.net/~duncanthrax/+archive/ubuntu/roccat-vulcan)

```bash
sudo add-apt-repository ppa:duncanthrax/roccat-vulcan
sudo apt-get update
sudo apt install roccat-vulcan
sudo udevadm control --reload-rules && udevadm trigger
# You might have to replug your keyboard or reboot at this
# point, so udev can update the permissions on the device nodes.
roccat-vulcan
```

### Manual build (Expert zone)

* You need to install development packages for libevdev and libhidapi.
* Type 'make && make install' in /src. 
* Replug keyboard or reboot unless you want to run as root.
* Run `roccat-vulcan`.


## If it does not work ...
* When not running as root, you need to be a member of
  the `plugdev` group (check with `id`), and udev must have
  set up the device nodes accordingly.
* Check the vendor/device ID of your keyboard with `lsusb`.
  The entry should look like this:
  ```
  Bus 001 Device 035: ID 1e7d:307a ROCCAT
  ```
  If your IDs aren't `1e7d:307a` or `1e7d:3098`, add an issue
  about it.
  
## Changing effect colors
Effects use up to 10 colors which can be changed by specifying
the `-c` command line option. Colors are specified as RGB values
(decimal integer numbers), with an effective range of `0` (off)
to `255` (full brightness for this color).
However, since effects use color transformations on a time axis,
it can make sense to specify values larger than 255 or smaller
than zero. For example, if you want an effect color to be
"redder" for longer, set the R value to 2500 instead of 255.
Likewise, if you want to remove "greenishness" for longer,
set the G value to a negative number.

These are the default colors of the "impact" effect:

```
colorIdx    R      G      B  Desc
------------------------------------------------
0           0      0    119  Base keyboard color (dark blue)
1        2303      0   -255  Typing color, initial key (over-red, under-blue)
2        2303      0   -143  Typing color, first neighbor key
3        2303      0      0  Typing color, second neighbor key
4         187      0    204  Ghost typing color, initial key
5         153      0    187  Ghost typing color, first neighbor key
6          85      0    170  Ghost typing color, second neighbor key
```

For example: To change the base keyboard color to green, specify
`-c 0:0,120,0`.
