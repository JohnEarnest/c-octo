C-Octo Build Guide
==================
This document collects information about building and using C-Octo on various host operating systems and devices.
Start by obtaining a copy of the c-octo repository and navigating to its root via your favorite terminal application.

MacOS X
-------
First, install SDL2. The easiest way is probably to use [MacPorts](https://www.macports.org) or [Homebrew](https://brew.sh):
```
brew install sdl2
```

You can then use the Makefile to build and install:
```
make && sudo make install
```

The same procedure, modulo the choice of package manager, will work on most Linux distros.

Windows (MinGW)
---------------
First, download the MinGW development release of SDL2. Extract the contents of the `i686-w64-mingw32` directory in the SDL tarball somewhere convenient. The C-Octo Makefile assumes `C:/mingw_dev_lib/`; edit the `WINDOWS_SDL_PATH` definition as needed.

If it isn't already, add the `bin` subdirectory of the directory where you saved the SDL2 headers and libs to your `PATH`. (see Control Panel -> System Properties -> Advanced tab -> Environment Variables...)

Then you can use the Makefile to build:
```
make
```
You'll probably want to create a `.octo.rc` in your MinGW home directory and tweak the settings:
```
cp octo.rc ~/.octo.rc
```

Windows (MSVC)
--------------
Install SDL2 somewhere convenient, and create a project which links against and includes it appropriately:

- Project -> Properties -> Configuration Properties -> VC++ Directories -> Include Directories -> Edit -> add the path to the `include`s.
- Project -> Properties -> Configuration Properties -> VC++ Directories -> Library Directories -> Edit -> add the path to the `libs`.
- Project -> Properties -> Configuration Properties -> C/C++ -> Preprocessor -> add `_CRT_SECURE_NO_WARNINGS;` to suppress complaints about using `fopen()`.
- Project -> Properties -> Linker -> Input -> Additional Dependencies -> Edit -> add `SDL2.lib; SDL2main.lib;`.
- Project -> Properties -> Linker -> System -> SubSystem -> make sure it is `Windows (/SUBSYSTEM:WINDOWS)`.
- Add files from `c-octo/src/`:
	- `octo_compiler.h`
	- `octo_emulator.h`
	- `octo_cartridge.h`
	- `octo_util.h`
	- `octo_de.c`

The source file `octo_util.h` contains a rudimentary shim for using Win32 filesystem APIs instead of POSIX `<dirent.h>` where necessary for Octode. Feel free to gaze in horror at it for a time. Or don't.

When built with MSVC, `octo-de` or `octo-run` will treat `%USERPROFILE%` as a substitute for the POSIX-style `$HOME` environment variable. As on other platforms, create a `.octo.rc` file in this directory to configure preferences as desired.

PocketCHIP
----------
The [PocketCHIP](https://en.wikipedia.org/wiki/CHIP_(computer)#Pocket_CHIP_and_Pockulus) was an inexpensive handheld ARM Linux-based computer that can still be found easily on eBay. Given the stock "out of the box" configuration, to install new software you will need to connect it to local WiFi and then update its package manager configuration to install SDL and development tools. [This Page](http://chip.jfpossibilities.com/chip/debian/) explains fixing `apt` configuration in detail. Summarizing, you will make the following changes:

- Update `/etc/apt/sources.list` and `/etc/apt/preferences`, changing all instances of `opensource.nextthing.co` (which is dead) to `chip.jfpossibilities.com`.
- Update `update /etc/apt/apt.conf`, adding the line `Acquire::Check-Valid-Until "0";`.

Then run the following commands to install SDL2. (Note: by default, the root password is `chip`.)
```
sudo apt update
sudo apt-get install libsdl2-dev
```
You should then be able to use the c-octo makefile:
```
make && sudo make install
```
Before running `octo-de` or `octo-run` on the PocketCHIP, make the following changes to your newly-minted `~/.octo.rc`:
```
ui.windowed=0
ui.software_render=1
ui.win_scale=1
```
Running in windowed mode seems to degrade performance slightly, and running in "hardware accelerated" mode will make Octode _unusably slow_ on this device!

Once you've verified that `octo-de` is installed in your path and has an appropriate config file, you can install it on the homescreen by editing `/usr/share/pocket-home/config.json`. Copy `octoicon.gif` to `/usr/share/pocket-home/appIcons/` and modify one of the entries to look like this:
```
{
	"name": "Octo IDE",
	"icon": "appIcons/octoicon.gif",
	"shell": "octo-de",
},
```
Restart, and you'll have immediate access to Octode from the home screen!

OLPC
----
[OLPC](https://en.wikipedia.org/wiki/OLPC_XO)s are rugged, low-power Linux-based laptops developed for education. All of these devices are based on a distribution of Fedora 18, and while the first model, the XO-1, was based on an i586 "Geode" processor, later models use armv7l processors. I've developed a [setup script](https://gist.github.com/JohnEarnest/24cfaf815e01c7c8295dd9e7856c7d02) which will bring an OLPC from its default "out of the box" configuration to a workable development environment. The minimum you can probably get away with is installing a functioning package repository and a c compiler:
```
sudo yum install -y --nogpgcheck http://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
sudo yum install -y gcc-c++ make
```

On the OLPCs, SDL2 must be built [from source](https://www.libsdl.org/download-2.0.php). Experimentally, I found that v2.0.2 builds on these machines, and is sufficiently new for `octode`. SDL2 needs some [X11](https://en.wikipedia.org/wiki/X_Window_System) development libraries present to be able to actually initialize a video driver. For audio support, we also need [ALSA](https://en.wikipedia.org/wiki/Advanced_Linux_Sound_Architecture) development libraries:
```
sudo yum -y install libX11-devel libXext-devel alsa-lib-devel
cd ~
mkdir sdl2
cd sdl2
wget --no-check-certificate https://www.libsdl.org/release/SDL2-2.0.2.tar.gz
tar xvzf SDL2-2.0.2.tar.gz
cd SDL2-2.0.2
```

On the XO-1 and XO-1.5 (i586), we must explicitly disable the generation of SSE instructions; these don't exist on Geodes, despite what libtool thinks:
```
./configure --disable-sse; make; sudo make install
```
On the XO-1.75 and XO-4 (ARM devices), `./configure` needs no special flags:
```
./configure; make; sudo make install
```

Make yourself a sandwich; expect this to take about 15 minutes to build on an ARM device or closer to 35 on an i586. After installation you may need to add `/usr/local/bin` to your path so that you can find `sdl2-config`:
```
export PATH=/usr/local/bin/:$PATH
which sdl2-config
```

You should then be able to use the c-octo makefile:
```
make && sudo make install
```
Before using `octo-de` or `octo-run`, make the following changes to your newly-minted `~/.octo.rc`:
```
ui.windowed=1
ui.software_render=1
ui.win_scale=1
```
At time of writing, running in fullscreen mode or scaling the UI on the OLPC degrades performance noticeably, especially with the monitor panel open. If Octo runs slowly you may get `(snd_pcm_recover) underrun occurred` warnings at the console- these are harmless, but can be annoying. Setting `ui.volume=0` will disable audio entirely.

If you have an OLPC and would like to do some experiments to improve this, suggestions are welcome!
