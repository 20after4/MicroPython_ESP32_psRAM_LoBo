# MicroPython for ESP32

# with support for 4MB of psRAM

---

**This repository can be used to build MicroPython for modules with psRAM as well as for regular ESP32 modules without psRAM.**

For building with **psRAM** support, special versions of *esp-idf* and *Xtensa toolchain* are needed (included). Otherwise, standard (master or release) *esp-idf* and toolchain can be used (included).

---

MicroPython works great on ESP32, but the most serious issue is still (as on most other MicroPython boards) limited amount of free memory.

ESP32 can use external **SPI RAM (psRAM)** to expand available RAM up to 16MB. Currently, there is only one module which incorporates **4MB** of psRAM, the **ESP-WROVER module**

It is hard to get, but it is available on some **ESP-WROVER-KIT boards** (the one on which this build was tested on).

[Pycom](https://www.pycom.io/webshop) is also offering the boards and OEM modules with 4MB of psRAM, to be available in August/September (some OEM modules already available).

AnalogLamb is also offering [ALB32-WROVER](https://www.analoglamb.com/product/alb32-wrover-esp32-module-with-64mb-flash-and-32mb-psram/) module with **8MB Flash** and **4MB psRAM** in ESP-WROOM-32 footprint package. Available for pre-order, will be released August 8, 2017.

---

Some basic [documentation](https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo/blob/master/MicroPython_BUILD/components/micropython/docs/README.md) specific to this **MicroPython** port is available. It will soon be updated to include the documentation for all added/changed modules.

Some examples can be found in [modules_examples](https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo/tree/master/MicroPython_BUILD/components/micropython/esp32/modules_examples) directory.

---

This repository contains all the tools and sources necessary to **build working MicroPython firmware** which can fully use the advantages of **4MB** (or more) of **psRAM**

It is **huge difference** between MicroPython running with **less than 100KB** of free memory and running with **4MB** of free memory.

---

## **The MicroPython firmware is built as esp-idf component**

This means the regular esp-idf **menuconfig** system can be used for configuration. Besides the ESP32 configuration itself, some MicroPython options can also be configured via **menuconfig**.

This way many features not available in standard ESP32 MicroPython are enabled, like unicore/dualcore, all Flash speed/mode options etc. No manual *sdkconfig.h* editing and tweaking is necessary.

---

### Features

* MicroPython build is based on latest build (1.9.1) from [main Micropython repository](https://github.com/micropython/micropython)
* ESP32 build is based on [MicroPython's ESP32 build](https://github.com/micropython/micropython-esp32/tree/esp32/esp32) with added changes needed to build on ESP32 with psRAM
* Special [esp-idf branch](https://github.com/espressif/esp-idf/tree/feature/psram_malloc) is used, with some modifications needed to build MicroPython
* Special build of **Xtensa ESP32 toolchain** is needed for building psRAM enabled application. It is included in this repository.
* Default configuration has **4MB** of MicroPython heap, **64KB** of MicroPython stack, **~200KB** of free DRAM heap for C modules and functions
* MicroPython can be built in **unicore** (FreeRTOS & MicroPython task running only on the first ESP32 core, or **dualcore** configuration (MicroPython task running on ESP32 **App** core)
* ESP32 Flash can be configured in any mode, **QIO**, **QOUT**, **DIO**, **DOUT**
* **BUILD.sh** script is provided to make **building** MicroPython firmware as **easy** as possible
* Internal filesystem is built with esp-idf **wear leveling** driver, so there is less danger of damaging the flash with frequent writes. File system parameters (start address, size, ...) can be set via **menuconfig**.
* **sdcard** module is included which uses esp-idf **sdmmc** driver and can work in **SD mode** (*1-bit* and *4-bit*) or in **SPI mode** (sd card can be connected to any pins). SPI mode cannot be selected if building with psRAM. On ESP-WROVER-KIT it works without changes, for imformation on how to connect sdcard on other boards look at *components/micropython/esp32/modesp.c*
* **Native ESP32 VFS** support for spi Flash & sdcard filesystems.
* **SPIFFS** filesystem support, can be used instead of FatFS in SPI Flash. Configurable via **menuconfig**
* **RTC Class** is added to machine module, including methods for synchronization of system time to **ntp** server, **deepsleep**, **wakeup** from deepsleep **on external pin** level, ...
* **Time zone** can be configured via **menuconfig** and is used when syncronizing time from NTP server
* Files **timestamp** is correctly set to system time both on internal fat filesysten and on sdcard
* Built-in **ymodem module** for fast transfer of text/binary files to/from host
* Some additional frozen modules are added, like **pye** editor, **urequests**, **functools**, **logging**, ...
* **Btree** module included, can be Enabled/Disabled via **menuconfig**
* **Eclipse** project files included. To include it into Eclipse goto File->Import->Existing Projects into Workspace->Select root directory->[select *MicroPython_BUILD* directory]->Finish. **Rebuild index**.
* **_threads** module greatly improved, inter-thread notifications and messaging included
* **Neopixel** module using ESP32 **RMT** peripheral with many new features
* **i2c** module uses ESP32 hardware i2c driver
* **curl** module added
* **ssh** module added
* **display** module added with full support for spi TFT displays
* **DHT** module implemented using ESP32 RMT peripheral
* **mqtt** module added, implemented in C, runs in separate task
* **telnet** module added, connect to REPL via WiFi using telnet protocol
* **ftp** module added, runs as separate ESP32 task
* **spi** module uses ESP32 hardware spi driver

---


### How to Build

---

Clone this repository, as it uses some submodules, use --recursive option

```
git clone --recursive https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo.git
```

*Toolchains and esp-idf are provided as tar archives. They will be automatically unpacked on* **first run** *of* **BUILD.sh** *script*

**Goto MicroPython_BUILD directory**

---

To change some ESP32 & Micropython options or to create initial **sdkconfig** run:
```
./BUILD.sh menuconfig
```

To build the MicroPython firmware, run:
```
./BUILD.sh
```
You can use -jn option (n=number of cores to use) to make the build process faster (it only takes les than 15 seconds with -j4). If using too high **n** the build may fail, if that happens, run build again or run without the -j option.

If no errors are detected, you can now flash the MicroPython firmware to your board. Run:
```
./BUILD.sh flash
```
The board stays in bootloader mode. Run your terminal emulator and reset the board.

You can also run *./BUILD.sh monitor* to use esp-idf's terminal program, it will reset the board automatically.

*After changing* **sdkconfig.h** (via menuconfig) *always run* **./BUILD.sh clean** *before new build*

---


### BUILD.sh

Included *BUILD.sh* script makes **building** MicroPython firmware **easy**.

Usage:

* **./BUILD.sh**               - run the build, create MicroPython firmware
* **./BUILD.sh -jn**           - run the build on multicore system, much faster build. Replace **n** with the number of cores on your system
* **./BUILD.sh menuconfig**    - run menuconfig to configure ESP32/MicroPython
* **./BUILD.sh clean**         - clean the build
* **./BUILD.sh flash**         - flash MicroPython firmware to ESP32
* **./BUILD.sh erase**         - erase the whole ESP32 Flash
* **./BUILD.sh monitor**       - run esp-idf terminal program
* **./BUILD.sh makefs**        - create SPIFFS file system image which can be flashed to ESP32
* **./BUILD.sh flashfs**       - flash SPIFFS file system image to ESP32, if not created, create it first
* **./BUILD.sh copyfs**        - flash the default SPIFFS file system image to ESP32

As default the build process runs silently, without showing compiler output. You can change that by exporting variable **MP_SHOW_PROGRESS=yes** before executing *BUILD.sh*.

**To build with psRAM support add** *psram* **as the last parameter.**

After the successful build the firmware files will be placed into **firmware** directory. **flash.sh** script will also be created.

---


#### Using **SPIFFS** filesystem

**SPIFFS** filesystem can be used on internal spi Flash instead of **FatFS**.

If you want to use it configure it via **menuconfig**  *→ MicroPython → File systems → Use SPIFFS*

**Prepared** image file can be flashed to ESP32, if not flashed, filesystem will be formated after first boot.


SFPIFFS **image** can be **prepared** on host and flashed to ESP32:

Copy the files to be included on spiffs into **components/spiffs_image/image/** directory. Subdirectories can also be added.

Execute:

`./BUILD.sh makefs`

to create **spiffs image** in **build** directory **without flashing** to ESP32

Execute:

`./BUILD.sh flashfs`

to create **spiffs image** in **build** directory and **flash** it to ESP32

Execute:

`./BUILD.sh copyfs`

to **flash default spiffs image** *components/spiffs_image/spiffs_image.img* to ESP32

---


### Known issues

* In **dual core** mode, the reset reason after deepsleep may be incorrectly detected. In **unicore** mode reset reason is detected correctly.
* On **psRAM** build **socket** module and all modules which uses it can be loaded only if the firmware is built in **unicore** mode

---


### Some examples

Using new machine methods and RTC:
```
import machine

rtc = machine.RTC()

rtc.init((2017, 6, 12, 14, 35, 20))

rtc.now()

rtc.ntp_sync(server="<ntp_server>" [,update_period=])
  <ntp_server> can be empty string, then the default server is used ("pool.ntp.org")

rtc.synced()
  returns True if time synchronized to NTP server

rtc.wake_on_ext0(Pin, level)
rtc.wake_on_ext1(Pin, level)
  wake up from deepsleep on pin level

machine.deepsleep(time_ms)
machine.wake_reason()
  returns tuple with reset & wakeup reasons
machine.wake_description()
  returns tuple with strings describing reset & wakeup reasons


```

Using sdcard module:
```
import sdcard, uos

sd = sdcard.SDCard()
uos.listdir(/sd)
```

Working directory can be changed to root of the sd card automatically:
```
>>> import sdcard,uos
>>> sd = sdcard.SDCard(True)
---------------------
Initializing SD Card: OK.
---------------------
 Mode:  SD (4bit)
 Name: SL08G
 Type: SDHC/SDXC
Speed: default speed (25 MHz)
 Size: 7580 MB
  CSD: ver=1, sector_size=512, capacity=15523840 read_bl_len=9
  SCR: sd_spec=2, bus_width=5

>>> uos.listdir()
['overlays', 'bcm2708-rpi-0-w.dtb', ......
>>>

```

---

Tested on **ESP-WROVER-KIT v3**
![Tested on](https://raw.githubusercontent.com/loboris/MicroPython_ESP32_psRAM_LoBo/master/Documents/ESP-WROVER-KIT_v3_small.jpg)

---

### Example terminal session


```

I (47) boot: ESP-IDF  2nd stage bootloader
I (47) boot: compile time 21:04:46
I (69) boot: Enabling RNG early entropy source...
I (69) qio_mode: Enabling QIO for flash chip GD
I (75) boot: SPI Speed      : 40MHz
I (88) boot: SPI Mode       : QIO
I (100) boot: SPI Flash Size : 4MB
I (113) boot: Partition Table:
I (124) boot: ## Label            Usage          Type ST Offset   Length
I (147) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (170) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (193) boot:  2 factory          factory app      00 00 00010000 00100000
I (217) boot: End of partition table
I (230) boot: Disabling RNG early entropy source...
I (247) boot: Loading app partition at offset 00010000
I (1440) boot: segment 0: paddr=0x00010018 vaddr=0x00000000 size=0x0ffe8 ( 65512) 
I (1440) boot: segment 1: paddr=0x00020008 vaddr=0x3f400010 size=0x30c50 (199760) map
I (1457) boot: segment 2: paddr=0x00050c60 vaddr=0x3ffb0000 size=0x04204 ( 16900) load
I (1487) boot: segment 3: paddr=0x00054e6c vaddr=0x40080000 size=0x00400 (  1024) load
I (1510) boot: segment 4: paddr=0x00055274 vaddr=0x40080400 size=0x15050 ( 86096) load
I (1563) boot: segment 5: paddr=0x0006a2cc vaddr=0x400c0000 size=0x00074 (   116) load
I (1564) boot: segment 6: paddr=0x0006a348 vaddr=0x00000000 size=0x05cc0 ( 23744) 
I (1588) boot: segment 7: paddr=0x00070010 vaddr=0x400d0018 size=0xa25c0 (665024) map
I (1614) boot: segment 8: paddr=0x001125d8 vaddr=0x50000000 size=0x00008 (     8) load
I (1642) cpu_start: PSRAM mode: flash 40m sram 40m
I (1657) cpu_start: PSRAM initialized, cache is in even/odd (2-core) mode.
I (1680) cpu_start: Pro cpu up.
I (1692) cpu_start: Starting app cpu, entry point is 0x4008237c
I (0) cpu_start: App cpu up.
I (4341) heap_alloc_caps: SPI SRAM memory test OK
I (4341) heap_alloc_caps: Initializing. RAM available for dynamic allocation:
I (4347) heap_alloc_caps: At 3F800000 len 00400000 (4096 KiB): SPIRAM
I (4369) heap_alloc_caps: At 3FFAE2A0 len 00001D60 (7 KiB): DRAM
I (4389) heap_alloc_caps: At 3FFBA310 len 00025CF0 (151 KiB): DRAM
I (4410) heap_alloc_caps: At 3FFE0440 len 00003BC0 (14 KiB): D/IRAM
I (4432) heap_alloc_caps: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (4453) heap_alloc_caps: At 40095450 len 0000ABB0 (42 KiB): IRAM
I (4474) cpu_start: Pro cpu start user code
I (4533) cpu_start: Starting scheduler on PRO CPU.
I (2828) cpu_start: Starting scheduler on APP CPU.

FreeRTOS running on BOTH CORES, MicroPython task started on App Core.

Allocating uPY stack: size=65536 bytes
Allocating uPY heap:  size=4194048 bytes (in SPIRAM using pvPortMallocCaps)

Reset reason: Power on reset Wakeup: Power on wake
I (3138) phy: phy_version: 350, Mar 22 2017, 15:02:06, 0, 2

Starting WiFi ...
WiFi started
Synchronize time from NTP server ...
Time set
19:5:35 14/7/2017

MicroPython c3fd0cf-dirty on 2017-07-14; ESP-WROVER module with ESP32+psRAM
Type "help()" for more information.
>>> 
>>> import micropython
>>> micropython.mem_info()
stack: 736 out of 64512
GC: total: 4097984, used: 11200, free: 4086784
 No. of 1-blocks: 117, 2-blocks: 14, max blk sz: 264, max free sz: 255416
>>> 
>>> a = ['esp32'] * 200000
>>> 
>>> a[123456]
'esp32'
>>> 
>>> micropython.mem_info()
stack: 736 out of 64512
GC: total: 4097984, used: 811664, free: 3286320
 No. of 1-blocks: 133, 2-blocks: 19, max blk sz: 50000, max free sz: 205385
>>> 

```
