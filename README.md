# PicoPQP

This project is a flight proven (TRL9) networking stack, bootloader, application (firmware) and ground seqment software tool framework for RP2040 (found on Rasperry Pi Pico) based pico-satellite subsystems.

This project uses the LibPQP networking stack library.

## Licensing

This project is dual-licensed: you may use it under the terms of the GNU Affero General Public License version 3 (AGPLv3), or the GNU Affero General Public License v3.0 (AGPLv3).

This project depends on the LibPQP library, which is licensed under AGPLv3 or a commercial license.

When this project is used together with the AGPLv3‑licensed version of the library, the resulting combined work is available only under the AGPLv3.

Users who obtain a commercial license for the library may instead use this project under the GPLv3 terms.

## Components

### initialize.sh

This script shall be run as the first step. It initializes the libpqp subrepository, and fetches the selected version of the [pico-sdk](https://github.com/raspberrypi/pico-sdk).

### can2040

This folder contains a selected version of the [can2040](https://github.com/KevinOConnor/can2040) project, that is a software CAN bus implementation for Raspberry Pi RP2040 and RP2350 micro-controllers.

### gnd_apps

This folder contains the ground segment support framework and remote management applications. Each application is built around gnd_app_base.

### libpqp

The LibPQP library is included as a subrepositry.

### pico_app

This folder contains the subsystem application base, that can be used to develop custom firmware for a satellite subsystem.

### pico_bootloader

This folder contains the subsystem bootloader, that is related to the pico_app firmware.

### pico_bridge

This is a specialized bootloader with the USB interface enabled. When loaded on a Raspberry Pi Pico, it acts as a bridge between your computer and your subsystem through the available communication interfaces (eg. HDUART or CAN).

### pqp_gnd_hub

This is a software component to be run on your computer, that interfaces between the bridge (USB) and the gnd apps.

## GND Apps

The build.sh script can be used to build all the gnd apps.

- bootloader_latch: reboots the subsystem, and prevent automatic firmware start

- erase_fw: when the device is in bootlader mode, the firmware or all (the bootloader, and a small config area is exempted) of the flash can be erased

- ping: request small status report (uptime, and indication that the subsystem is running the firmware or is in bootloader mode)

- reboot: reboots the device

- run_fw: when the device is in bootlader mode, the firmware can be started

- set_routing_table: set (from a .csv file) or clear the subsystem's routing table. in case the target is the bridge, a simplified config menu provided

- transfer_test: data echo test (classical ping functionality)

- tty: interactive teletype terminal

- upload_fw: upload firmware binary to the device

## Pico Bootloader

The bootloader runs on the Core 0 of the RP2040.

Files:

 - bootloader.c: configures the hardware and invokes the LibPQP to fullfill bootloader functionality on the subsystem.

 - bootloader_memmap.ld and memmap.h: RAM and flash address range allocations

 - build.sh: a script that builds the bootloader

 - libpqp_foreign.c: implements hardware dependent functions required by LibPQP

 - libpqp_hws.h: header file of some hardware dependent functions, that - altough not part of LibPQP - is implemented in libpqp_foreign.c, due to the required interaction between those functions.

 - pqp_addresses.h: contains system-wide subsystem addresses

 - pqp_defines.h: contains the configuration of the subsystem

## Pico App

The Core 0 of the RP2040 runs the core functionality of the LibPQP based framework.

The Core 1 runs the user's application.

Files:

 - app.c: configures the hardware and invokes the LibPQP, then start the firmware on Core 1

 - app_memmap.ld: RAM and flash address range allocations

 - build.sh: a script that builds the application
 
 - libpqp_foreign.c and libpqp_hws.h: these are just symlinks to the same files in the pico_bootloader folder

 - main.c: this contains the user's source code that implements the subsystem uniqe functionality (further source file can be added, by editing CMakeLists.txt)

 - memmap.h, pqp_addresses.h, and pqp_defines.h: due to forbidden discrepancy, these are just symlinks to the same files in the pico_bootloader folder





