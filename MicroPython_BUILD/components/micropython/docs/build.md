---

### Building **MicroPython** for ESP32

---

Clone the MicroPython repository

```
git clone https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo.git
```

*Xtensa toolchains and esp-idf are provided as tar archives. They will be automatically unpacked on* **first run** *of* **BUILD.sh** *script*

Note: If running on Windows, you may get false error **unpacking 'esp-idf' FAILED**. Run ./BUILD.sh again, the toolchain will be unpacked successfully.


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
You can use -jn option to make the build process faster. If using too high **n** the build may fail because of **race condition**, if that happens, run build again or run without the -j option.

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

| Syntax  | Notes |
| - | - |
| **./BUILD.sh**            | run the build, create MicroPython firmware |
| **./BUILD.sh -jn**        | run the build on multicore system, much faster build. Replace **n** with the number of cores on your system |
| **./BUILD.sh menuconfig** | run menuconfig to configure ESP32/MicroPython |
| **./BUILD.sh clean**      | clean the build |
| **./BUILD.sh flash**      | flash MicroPython firmware to ESP32 |
| **./BUILD.sh erase**      | erase the whole ESP32 Flash |
| **./BUILD.sh monitor**    | run esp-idf terminal program |
| **./BUILD.sh makefs**     | create SPIFFS file system image which can be flashed to ESP32 |
| **./BUILD.sh flashfs**    | flash SPIFFS file system image to ESP32, if not created, create it first |
| **./BUILD.sh copyfs**     | flash the default SPIFFS file system image to ESP32 |
| **./BUILD.sh makefatfs**  | create FatFS file system image which can be flashed to ESP32 |
| **./BUILD.sh flashfatfs** | flash FatFS file system image to ESP32, if not created, create it first |
| **./BUILD.sh copyfatfs**  | flash the default FatFS file system image to ESP32 |

As default the build process runs **silently**, without showing compiler output. You can change that by adding **verbose** as the last parameter to *BUILD.sh*.

**To build with psRAM support:**

In menuconfig select **→ Component config → ESP32-specific → Support for external, SPI-connected RAM**

In menuconfig select **→ Component config → ESP32-specific → SPI RAM config → Make RAM allocatable using heap_caps_malloc**

After the successful build the firmware files will be placed into **firmware** directory. **flash.sh** script will also be created.

---

### Updating to the latest version

To update to the latest commits, go to **MicroPython_ESP32_psRAM_LoBo** directory and execute:
```
git pull
git submodule update --init --recursive
```

If you get the warning:
```
error: Your local changes to the following files would be overwritten by merge:
	....
	....
Please commit your changes or stash them before you merge.
```
you may execute:
```
git stash
```
and execute the **git pull** again.



