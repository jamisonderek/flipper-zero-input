# flipper-zero-input
Alternative input for the Flipper Zero

![Flipper Zero](flipper_zero.png)


## Introduction

This project aims to provide an alternative input method for the Flipper Zero. The Flipper Zero is a multi-tool device that can be used for a variety of tasks, such as hacking, pentesting, and hardware hacking. However, the default input method for the Flipper Zero is limited to using the D-Pad for input. This often results in people typing short names for their files, like `liv.sub` instead of a descriptive name like `living room light.sub`.  This project aims to expand the input capabilities of the Flipper Zero by adding support for external input devices, such as the Chatpad and Mobile phone input devices.

## Quick Installation

The following steps will allow you to quickly test the Chatpad and Mobile input features on **your** Flipper Zero. However, these prebuilt versions of the firmware may not be the latest version available. If you would like to install the latest version of the firmware, please follow the instructions in the [Patch Installation](#patch-installation) section below.

1. Download the prebuilt package for your Flipper Zero (NOTE: these firmware packages are modified versions of the official that include Chatpad and Mobile input support)

    - [Official firmware v1.1.2 - modified with Chatpad and Mobile input support](https://github.com/jamisonderek/flipper-zero-input/blob/main/prebuilt/flipper-official-1.1.2.tgz)
    - [Momentum firmware v008 - modified with Chatpad and Mobile input support](https://github.com/jamisonderek/flipper-zero-input/blob/main/prebuilt/flipper-mntm-008.tgz)
    - [Unleashed firmware v079 - modified with Chatpad and Mobile input support](https://github.com/jamisonderek/flipper-zero-input/blob/main/prebuilt/flipper-unlshd-079.tgz)
    - [RogueMaster firmware - modified with Chatpad and Mobile input support](#)

2. Install the firmware package on your Flipper Zero

    - Connect your Flipper Zero to your computer via USB
    - Open qFlipper
    - Click on the "Install from File" link (bottom right corner)
    - Select the downloaded firmware package from step 1
    - Click "Open" and then "Install"

3. Your Flipper Zero will be updated with the new firmware.


## Patch Installation