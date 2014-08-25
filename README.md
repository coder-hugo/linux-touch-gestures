# linux-touch-gestures

## About

linux-touch-gestures is a small utility that convert multi touch gestures to keystrokes. For detecting gestures the 
linux [multi touch protocol](https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt) is used. So every
touch device whose driver implement this protocol should work with linux-touch-gestures. The driver must at least
implement the following events:
* ABS\_MT\_POSITION\_X
* ABS\_MT\_POSITION\_Y
* ABS\_MT\_SLOT
* BTN\_TOOL\_\*

For generating the keystrokes linux-touch-gestures uses the userspace input module. So you need either to load the
module uinput or compile it to the kernel for using linux-touch-gestures.

## Required libraries

The only needed extra library should be [libiniparser](http://ndevilla.free.fr/iniparser/html/index.html).

## Compile and install

After cloning the repostory execute the following commands

```shell
# for executing autogen.sh you need to install autoconf and automake
./autogen.sh
./configure
make
make install
```

## Configuration

The configuration is store in an ini file with the following sections and keys (The default values are marked as bold):

* [Gernaral]
  * TouchDevice (required) -> path to the touch device (/dev/input/...)
* [Scroll]
  * Vertical -> enable vertical scrolling (true, **false**)
  * Horizontal -> enable horizontal scrolling (true, **false**)
  * VerticalDelta -> move distance of a finger for a scroll event (integer, **79**)
  * HorizontalDelta -> move distance of a finger for a scroll event (integer, **30**)
* [Zoom]
  * Enable -> enable the 2 finger zoom (true, **false**)
  * Delta -> move distance of a finger for a zoom event (integer, **200**)
* [Thresholds]
  * Vertical -> threshold for vertical swipe events in percent of the touchpad's height (unsigned integer, **15**)
  * Horizontal -> threshold for horizontal swipe events in percent of the touchpad's width (unsigned integer, **15**)
* [2-Fingers]
  * Up -> combination of keys that should be emulated by swiping with 2 fingers up
  * Down -> combination of keys that should be emulated by swiping with 2 fingers down
  * Left -> combination of keys that should be emulated by swiping with 2 fingers left
  * Right -> combination of keys that should be emulated by swiping with 2 fingers right
* [3-Fingers] ... [5-Fingers] -> same as for [2-Fingers]

The single keys of a combination have to be separate with a plus sign (+). E.g. LEFTCTRL+LEFTALT+UP.
The complete list of available keys can be found [here](src/keys.c).

## Run

Just execute the application like this:
```shell
touch_gestures /path/to/config.file
```
