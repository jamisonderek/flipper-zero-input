# flipper-zero-input
Alternative input for the Flipper Zero

![Flipper Zero](flipper_zero.png)


## Introduction

This project aims to provide an alternative input method for the Flipper Zero. The Flipper Zero is a multi-tool device that can be used for a variety of tasks, such as hacking, pentesting, and hardware hacking. However, the default input method for the Flipper Zero is limited to using the D-Pad for input. This often results in people typing short names for their files, like `liv.sub` instead of a descriptive name like `living room light.sub`.  This project aims to expand the input capabilities of the Flipper Zero by adding support for external input devices, such as the Chatpad and Mobile phone input devices.

- Installing firmware with Chatpad and Mobile input support
  - [Firmware Installation](#firmware-installation)

- Bluetooth Phone
  - [Bluetooth Setup](#bluetooth-setup)

- Chatpad
  - [Chatpad Hardware](#chatpad-hardware)
  - [Chatpad Setup](#chatpad-setup)


## Firmware Installation

The following steps will guide you through the process of installing the latest version of the firmware on your Flipper Zero.

Install prerequisites before proceeding:

   - Install [git](https://git-scm.com/downloads)
   - NOTE: I have a YouTube video on setting up a [Windows development environment](https://youtu.be/gqovwRkn2xw) or [Ubuntu environment](https://youtu.be/PaqK1H9brZQ).

The following directions are for **Windows users using a command prompt**. If you are using a different operating system or using PowerShell, you will need to adjust the commands accordingly. If you don't want to use the latest `dev` branch, you can replace `"dev"` with the branch or tag you would like to use. Please see [using non-dev branches or tags](#using-non-dev-branches-or-tags) below for more information.

Before proceeding, be sure to connect your Flipper Zero to your computer. Close qFlipper, lab.flipper.net, or any other application that may be using your Flipper Zero USB port!

### Official firmware

```bash
mkdir \repos
cd \repos
rd /s /q flipperzero-firmware 2>nul
git clone --recursive -j 8 https://github.com/flipperdevices/flipperzero-firmware.git flipperzero-firmware
rd /s /q flipper-zero-input 2>nul
git clone https://github.com/jamisonderek/flipper-zero-input.git
cd flipperzero-firmware
git pull
git checkout "1.1.2"
cd applications
xcopy ..\..\flipper-zero-input\firmware-overlay\ofw-1.1.2\applications\*.* . /e /y
cd ..
git stash push -u
git checkout "dev"
git stash pop
fbt vscode_dist
fbt COMPACT=1 DEBUG=0 FORCE=1 flash_usb_full

```

### Momentum firmware

```bash
mkdir \repos
cd \repos
rd /s /q flipperzero-firmware 2>nul
git clone --recursive -j 8 https://github.com/Next-Flip/Momentum-Firmware.git flipperzero-firmware
rd /s /q flipper-zero-input 2>nul
git clone https://github.com/jamisonderek/flipper-zero-input.git
cd flipperzero-firmware
git pull
git checkout "mntm-008"
cd applications
xcopy ..\..\flipper-zero-input\firmware-overlay\mntm-008\applications\*.* . /e /y
cd ..
git stash push -u
git checkout "dev"
git stash pop
fbt vscode_dist
fbt COMPACT=1 DEBUG=0 FORCE=1 flash_usb_full

```

### Unleashed firmware

```bash
mkdir \repos
cd \repos
rd /s /q flipperzero-firmware 2>nul
git clone --recursive -j 8 https://github.com/DarkFlippers/unleashed-firmware.git flipperzero-firmware
rd /s /q flipper-zero-input 2>nul
git clone https://github.com/jamisonderek/flipper-zero-input.git
cd flipperzero-firmware
git pull
git checkout "unlshd-079"
cd applications
xcopy ..\..\flipper-zero-input\firmware-overlay\unl-079\applications\*.* . /e /y
cd ..
git stash push -u
git checkout "dev"
git stash pop
fbt vscode_dist
fbt COMPACT=1 DEBUG=0 FORCE=1 flash_usb_full

```

### RogueMaster firmware

```bash
mkdir \repos
cd \repos
rd /s /q flipperzero-firmware 2>nul
git clone --recursive -j 8 https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git flipperzero-firmware
rd /s /q flipper-zero-input 2>nul
git clone https://github.com/jamisonderek/flipper-zero-input.git
cd flipperzero-firmware
git pull
git checkout "RM1202-0837-0.420.0-6d10bad"
cd applications
xcopy ..\..\flipper-zero-input\firmware-overlay\rm-1202-0837-0.420.0-6d10bad\applications\*.* . /e /y
cd ..
git stash push -u
git checkout "dev"
git stash pop
fbt vscode_dist
fbt COMPACT=1 DEBUG=0 FORCE=1 flash_usb_full                

```

### Using non-dev branches or tags
NOTE: instead of `git checkout "dev"` you can replace `"dev"` with the branch or tag of the firmware you would like to use. The following branches and tags are available:

Official firmware:
- `dev`
- `release`
- [tags](https://github.com/flipperdevices/flipperzero-firmware/tags), like `1.1.2`
    
Momentum firmware:

- `dev`
- `release`
- [tags](https://github.com/Next-Flip/Momentum-Firmware/tags), like `mntm-008`

Unleashed firmware:
- `dev`
- `release`
- [tags](https://github.com/DarkFlippers/unleashed-firmware/tags), like `unlshd-079`

RogueMaster firmware:
- `420`
- [tags](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/tags), like `RM1202-0837-0.420.0-6d10bad`


### Notes about the above scripts

- You can replace `\repros` with any directory you would like to use.

- We recursively clone the firmware repo into a folder named `flipperzero-firmware`. You can choose a different name if you would like, but be sure to also update the other commands to use the new name.

- We clone the `flipper-zero-input` repo into a folder named `flipper-zero-input`. You can choose a different name if you would like, but be sure to also update the other commands to use the new name.

- The firmware-overlays contains updated `text_input.c`, `rpc.c`, `rpc_storage.c` and `settings/applications.fam` files which were part of the original firmware. We sync our firmware repo to the version I was using when I made the change, that way we know those files ONLY contain the necessary modifications.

- The overlays are slightly different between firmware, since they have different text_input.c implementations (everyone has a slightly different keyboard implementation).

- Once we apply the overlay, we use `git stash push -u` to stash away our edits. We then checkout the new version of firmware and use `git stash pop` to apply our edits in the new code. This assumes that the two versions of the firmware are compatible with the changes we made. If they are not, you will need to manually apply the changes to the new firmware.

## Bluetooth Setup

The easiest way to input text on the Flipper Zero is to use a Bluetooth on your phone. This requires that you are running the [Flipper Zero mobile app](https://docs.flipper.net/mobile-app) and have paired it with your Flipper Zero. 

NOTE: if you switched firmware the pairing may be lost, so you may need to forget the device and add it again.

Once you have the mobile app working, the following steps will send text to the text input to the Flipper Zero.
1. On your Flipper, go to `Settings`/`Chatpad`. This will create a "input-line.txt" file on the SD card. You can exit the settings, if you don't actually have a chatpad.
2. Open the [mobile app](https://docs.flipper.net/mobile-app) and connect to your Flipper Zero.
3. Click on the "Options" (it's on the first tab).
4. Click on the "File Manager".
5. You should be on the SD card folder (if not, click on the `/ext` folder).
6. Open the "input-line.txt" file (this file was created from step 1 above.)
7. Type a line of text you would like to send to the Flipper Zero.
   - NOTE: If you press enter on the phone, only the first line will used (and then a "save" will be submitted)
8. On your Flipper make sure you are in the text input screen (the one that shows a keyboard).
9. On your phone, click on the "Save" (or checkmark) button at the top right.
10. The text will be sent to the Flipper Zero and you should see it in the text input!

In the future, it would be great to have a more seamless integration with the Flipper Zero mobile app. You can find the existing apps here:
   - [https://github.com/flipperdevices/Flipper-Android-App](https://github.com/flipperdevices/Flipper-Android-App)
   - [https://github.com/flipperdevices/Flipper-iOS-App](https://github.com/flipperdevices/Flipper-Android-App)

Let me know if you want to collaborate on a mobile app improvement!  (@CodeAllNight on Discord.)

## Chatpad Hardware

![Chatpad wiring](chatpad_wiring.png)

The Xbox 360 Chatpad is a small keyboard that was originally designed for the Xbox 360. You can still find them on eBay for around $15USD. The model I used was **X814365-001**, which is a wired keypad.

To take it apart you will need:
   - T6 screwdriver
   - A small phillips screwdriver
   - Something to pry the case apart (I used a small fork)

I ordered a separate 7-pin, 1.25mm to Dupont 2.54mm adapter from Amazon. This allows me to connect the Chatpad to the Flipper Zero. You could also cut the existing cable and solder the wires to the Flipper Zero, but I wanted to keep the Chatpad intact. The adapter I used was the following:
   - [7-pin, 1.25mm to Dupont 2.54mm adapter](https://www.amazon.com/gp/product/B07PWZTC88)

To take the Chatpad apart, follow these steps:
   - Remove the four T6 screws on the back of the Chatpad.
   - Carefully pry the case apart.
   - Remove the ribbon cable from the PCB.
   - Remove the 5 tiny phillips screws holding the PCB in place.
   - Carefully remove the PCB from the case.

Connect the adapter to the Chatpad PCB:
   - Connect the 7-pin, 1.25mm connector to the Chatpad PCB.
   - Connect the Dupont 2.54mm connector to the Flipper Zero.
   - Pin 1 on the Chatpad has a little triangle on the PCB.
   - Connect Pin 1 on the Chatpad to Pin 9 `3V3` on the Flipper Zero.
   - Connect Pin 2 on the Chatpad to Pin 15 `C1` on the Flipper Zero.
   - Connect Pin 3 on the Chatpad to Pin 16 `C2` on the Flipper Zero.
   - Connect Pin 4 on the Chatpad to Pin 18 `GND` on the Flipper Zero.
   - Pins 5, 6, and 7 on the Chatpad are not used (they are for audio).

## Chatpad Setup

Once you have installed the firmware using the [Quick Installation](#quick-installation) or [Firmware Overlay Installation](#firmware-overlay-installation) steps, you can connect the Chatpad to the Flipper Zero. **Every time you restart the Flipper Zero you will need to reconnect the Chatpad.**

1. Connect the Chatpad to the Flipper Zero (Flipper GPIO pins 9, 15, 16, 18 -- see [Chatpad Hardware](#chatpad-hardware) for more details).
2. Turn on the Flipper Zero.
3. Click "OK" and then select the "Settings" option.
4. Select the "Chatpad" option.
5. Click on the "Chatpad" menu item. Click "OK" to turn on the chatpad.
6. You should see "Chatpad is ON" and then "Chatpad is READY".
7. Press a key on the Chatpad and you should see the key pressed on the Flipper Zero screen.

You can set Macros in the Chatpad Config.

1. Choose "Config" from the Chatpad menu.
2. For the "Macro" option, choose the letter you would like to assign a macro to.
3. Click "OK" and then type the text you would like to assign to the macro. (You can use the Chatpad to type the text.)
4. Cick "Save"
5. To use the macro, hold the "People" key (next to the green button on the chatpad) and then press the letter you assigned the macro to.

## Support

If you have need help, I am here for you. Also, I would love your feedback on Flipper Zero input devices!  The best way to get support is tag me **(@CodeAllNight)** on Discord in any of the Flipper Zero firmware servers.

If you want to support my work, you can donate via [https://ko-fi.com/codeallnight](https://ko-fi.com/codeallnight) or you can [buy a FlipBoard](https://www.tindie.com/products/makeithackin/flipboard-macropad-keyboard-for-flipper-zero/) from MakeItHackin with software & tutorials from me (@CodeAllNight).