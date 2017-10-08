#!/bin/bash

# #################################################################
# This script makes it easy to build MicroPython firmware for ESP32
# #################################################################

# Usage:
# ./BUILD               - run the build, create MicroPython firmware
# ./BUILD -j2           - run the build on multicore system, much faster build
# ./BUILD menuconfig    - run menuconfig to configure ESP32/MicroPython
# ./BUILD clean         - clean the build
# ./BUILD flash         - flash MicroPython firmware to ESP32
# ./BUILD erase         - erase the whole ESP32 Flash
# ./BUILD makefs        - create spiffs image
# ./BUILD flashfs       - create and flash spiffs image to ESP32
# ./BUILD copyfs        - flash prebuilt spiffs image to ESP32


TOOLS_VER=ver20171008.id

#---------------------------------------------------------------------------------------------------------------------
# Check parameters
opt="$1"
if [ "${opt}" == "verbose" ]; then
    opt = "all"
    export MP_SHOW_PROGRESS=yes
fi
opt2="xx"
arg="$1"
jopt="n"

if [ "${opt:0:2}" == "-j" ]; then
    opt2="$2"
    if [ "${opt2}" == "verbose" ]; then
        opt2 = "all"
        export MP_SHOW_PROGRESS=yes
    fi
    if [ "${opt2}" == "" ]; then
        opt2="all"
    fi
    if [ "${opt2}" != "flash" ] && [ "${opt2}" != "all" ]; then
        echo ""
        echo "Wrong parameter, usage: BUILD.sh -j2 [all] | flash"
        echo ""
        exit 1
    else
        arg=${opt2}
    fi
    if [ "$3" == "verbose" ]; then
        export MP_SHOW_PROGRESS=yes
    fi
    jopt="y"

elif [ "${opt}" == "makefs" ] || [ "${opt}" == "flashfs" ] || [ "${opt}" == "copyfs" ]; then
    if [ "$2" == "verbose" ]; then
        export MP_SHOW_PROGRESS=yes
    fi
    arg=${opt}
    opt="none"

elif [ "${opt}" == "makefatfs" ] || [ "${opt}" == "flashfatfs" ] || [ "${opt}" == "copyfatfs" ]; then
    if [ ! -f "build/include/sdkconfig.h" ]; then
        echo ""
        echo "Run './BUILD menuconfig' first."
        echo ""
        exit 1
    fi
    if [ "$2" == "verbose" ]; then
        export MP_SHOW_PROGRESS=yes
    fi
    arg=${opt}
    opt="none"

else
    if [ "${opt}" == "" ]; then
        opt="all"
        arg="all"
    elif [ "${opt}" == "flash" ] || [ "${opt}" == "erase" ] || [ "${opt}" == "monitor" ] || [ "${opt}" == "clean" ] || [ "${opt}" == "all" ] || [ "${opt}" == "menuconfig" ]; then
        opt=""
        if [ "$2" == "verbose" ]; then
            export MP_SHOW_PROGRESS=yes
        fi
    else
        echo ""
        echo "Wrong parameter, usage: BUILD.sh [all] | clean | flash | erase | monitor | menuconfig"
        echo ""
        exit 1
    fi
fi

if [ "${arg}" == "" ]; then
    arg="all"
fi
#---------------------------------------------------------------------------------------------------------------------

# ----------------------
# Check Operating System
# ----------------------
unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=MacOS;;
    CYGWIN*)    machine=Win;;
    MINGW*)     machine=Win;;
    *)          machine=UNKNOWN
esac

if [ "${machine}" == "UNKNOWN" ]; then
    if [ "${machine:0:5}" == "MINGW" ]; then
        machine=Win
    fi
fi

if [ "${machine}" == "UNKNOWN" ]; then
    echo ""
    echo "Unsupported OS detected."
    echo ""
    exit 1
fi

if [ "${machine}" == "Win" ]; then
    if [ ! -f "/usr/bin/tar.exe" ]; then
        echo "Installing tar..."
        pacman -S --noconfirm tar > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo "OK."
        else
            echo "FAILED"
            exit 1
        fi
    fi
    if [ ! -f "/usr/bin/tar.exe" ]; then
        echo ""
        echo "'tar.exe' needed for toolchain extraction not found!"
        echo ""
        exit 1
    fi
	MK_SPIFFS_BIN="mkspiffs.exe"
	MK_FATFS_BIN="mkfatfs.exe"
else
MK_SPIFFS_BIN="mkspiffs"
MK_FATFS_BIN="mkfatfs"
fi

BUILD_BASE_DIR=${PWD}

# #################################################
# Test if toolchains are unpacked and right version
# #################################################
cd ../

# ----------------------------------------
# Remove directories from previous version
# ----------------------------------------
if [ -d "esp-idf" ]; then
    rm -rf esp-idf/ > /dev/null 2>&1
    rmdir esp-idf > /dev/null 2>&1
fi
if [ -d "esp-idf_psram" ]; then
    rm -rf esp-idf_psram/ > /dev/null 2>&1
    rmdir esp-idf_psram > /dev/null 2>&1
fi
if [ -d "xtensa-esp32-elf" ]; then
    rm -rf xtensa-esp32-elf/ > /dev/null 2>&1
    rmdir xtensa-esp32-elf > /dev/null 2>&1
fi
if [ -d "xtensa-esp32-elf_psram" ]; then
    rm -rf xtensa-esp32-elf_psram/ > /dev/null 2>&1
    rmdir xtensa-esp32-elf_psram > /dev/null 2>&1
fi


cd Tools
# -----------------------------------------
# _psram directories are not needed anymore
# -----------------------------------------
if [ -d "esp-idf_psram" ]; then
    rm -rf esp-idf_psram/ > /dev/null 2>&1
    rmdir esp-idf_psram > /dev/null 2>&1
fi
if [ -d "xtensa-esp32-elf_psram" ]; then
    rm -rf xtensa-esp32-elf_psram/ > /dev/null 2>&1
    rmdir xtensa-esp32-elf_psram > /dev/null 2>&1
fi


# ------------------------------------------
# Check esp-idf and xtensa toolchain version
# ------------------------------------------
if [ ! -f "${TOOLS_VER}" ]; then
    echo "Removing old tools versions and cleaning build..."
    # Remove directories from previous version
    if [ -d "esp-idf" ]; then
        rm -rf esp-idf/ > /dev/null 2>&1
        rmdir esp-idf > /dev/null 2>&1
    fi
    if [ -d "xtensa-esp32-elf" ]; then
        rm -rf xtensa-esp32-elf/ > /dev/null 2>&1
        rmdir xtensa-esp32-elf > /dev/null 2>&1
    fi
    rm -f *.id > /dev/null 2>&1
    touch ${TOOLS_VER}
    echo "toolchains & esp-idf version" > ${TOOLS_VER}
    # remove executables
    rm -f ${BUILD_BASE_DIR}/components/mpy_cross_build/mpy-cross/mpy-cross > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/mpy_cross_build/mpy-cross/mpy-cross.exe > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/micropython/mpy-cross/mpy-cross > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/micropython/mpy-cross/mpy-cross.exe > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/mkspiffs/src/mkspiffs > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/mkspiffs/src/mkspiffs.exe > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/mkfatfs/src/mkfatfs > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/components/mkfatfs/src/mkfatfs.exe > /dev/null 2>&1
    # remove build directory and configs
    rm -f ${BUILD_BASE_DIR}/sdkconfig > /dev/null 2>&1
    rm -f ${BUILD_BASE_DIR}/sdkconfig.old > /dev/null 2>&1
    rm -rf ${BUILD_BASE_DIR}/build/ > /dev/null 2>&1
    rmdir ${BUILD_BASE_DIR}/build > /dev/null 2>&1
fi

if [ ! -d "esp-idf" ]; then
    echo "unpacking 'esp-idf'"
    tar -xf esp-idf.tar.xz > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "unpacking 'esp-idf' FAILED"
        exit 1
    fi
fi
if [ ! -d "xtensa-esp32-elf" ]; then
    echo "unpacking ${machine}/xtensa-esp32-elf"
    tar -xf ${machine}/xtensa-esp32-elf.tar.xz > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "unpacking 'xtensa-esp32-elf' FAILED"
        exit 1
    fi
fi

# ##################
cd ${BUILD_BASE_DIR}
# ##################

# ---------------------------------
# Test if mpy-cross has to be build
# -------------------------------------------------------
if [ "${arg}" == "all" ] || [ "${arg}" == "flash" ]; then
    # ###########################################################################
    # Build MicroPython cross compiler which compiles .py scripts into .mpy files
    # ###########################################################################
    if [ ! -f "components/micropython/mpy-cross/mpy-cross" ]; then
        cd components/mpy_cross_build/mpy-cross
        echo "=================="
        echo "Building mpy-cross"
        make > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            cp -f mpy-cross ../../micropython/mpy-cross > /dev/null 2>&1
            make clean > /dev/null 2>&1
            echo "OK."
            echo "=================="
        else
            echo "FAILED"
            echo "=================="
            exit 1
        fi
        cd ${BUILD_BASE_DIR}
        if [ ! -f "components/micropython/mpy-cross/mpy-cross" ]; then
            echo "FAILED"
            echo "=================="
            exit 1
        fi
    fi
    # ###########################################################################
fi

# --------------------------------
# Test if mkspiffs has to be build
# ----------------------------------------------------------------------------------------
if [ "${arg}" == "makefs" ] || [ "${arg}" == "flashfs" ] || [ "${arg}" == "copyfs" ]; then
    # ###########################################################################
    # Build mkspiffs program which creates spiffs image from directory
    # ###########################################################################
    if [ ! -f "components/mkspiffs/src/${MK_SPIFFS_BIN}" ]; then
        echo "=================="
        echo "Building mkspiffs"
        make -C components/mkspiffs/src > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo "OK."
            echo "=================="
        else
            echo "FAILED"
            echo "=================="
            exit 1
        fi
    fi
    # ###########################################################################
fi

# -------------------------------
# Test if mkfatfs has to be build
# --------------------------------------------------------------------------------------------------
if [ "${arg}" == "makefatfs" ] || [ "${arg}" == "flashfatfs" ] || [ "${arg}" == "copyfatfs" ]; then
    # ###########################################################################
    # Build mkfatfs program which creates fatfs image from directory
    # ###########################################################################
    if [ ! -f "components/mkfatfs/src/${MK_FATFS_BIN}" ]; then
        export BUILD_DIR_BASE=${BUILD_BASE_DIR}/build
        echo "================="
        echo "Building mkfatfs [${BUILD_DIR_BASE}]"
        make -C components/mkfatfs/src > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo "OK."
            echo "================="
        else
            echo "FAILED"
            echo "================="
            exit 1
        fi
    fi
    # ###########################################################################
fi

BUILD_TYPE=""
if [ -f "sdkconfig" ]; then
    SDK_PSRAM=$(grep -e CONFIG_SPIRAM_SUPPORT=y sdkconfig)
    if [ "${SDK_PSRAM}" == "CONFIG_SPIRAM_SUPPORT=y" ]; then
        BUILD_TYPE=" with psRAM support"
    fi
fi

# ########################################################
# #              SET XTENSA & ESP-IDF PATHS              #
# ########################################################
cd ../
# Add Xtensa toolchain path to system path, and export path to esp-idf
export PATH=${PWD}/Tools/xtensa-esp32-elf/bin:$PATH
export IDF_PATH=${PWD}/Tools/esp-idf

echo ""
echo "Building MicroPython for ESP32${BUILD_TYPE}"
echo ""
cd ${BUILD_BASE_DIR}

export HOST_PLATFORM=${machine}
export CROSS_COMPILE=xtensa-esp32-elf-

# ########################################################


# Test if valid sdkconfig exists
# #### Test sdkconfig ###############
if [ "${arg}" == "all" ] || [ "${arg}" == "clean" ]; then
    # Test if sdkconfig exists
    # ---------------------------
    if [ ! -f "sdkconfig" ]; then
        echo "sdkconfig NOT FOUND, RUN ./BUILD.sh menuconfig FIRST."
        echo ""
        exit 1
    fi
fi
# ###################################


if [ "${arg}" == "flash" ]; then
    echo "========================================="
    echo "Flashing MicroPython firmware to ESP32..."
    echo "========================================="

    make ${opt} ${arg}
elif [ "${arg}" == "erase" ]; then
    echo "======================"
    echo "Erasing ESP32 Flash..."
    echo "======================"

    make erase_flash
elif [ "${arg}" == "monitor" ]; then
    echo "============================"
    echo "Executing esp-idf monitor..."
    echo "============================"

    make monitor
elif [ "${arg}" == "clean" ]; then
    echo "============================="
    echo "Cleaning MicroPython build..."
    echo "============================="

    #rm -f components/micropython/mpy-cross/mpy-cross > /dev/null 2>&1
    rm -f build/* > /dev/null 2>&1
    rm -f components/mkspiffs/src/*.o > /dev/null 2>&1
    rm -f components/mkspiffs/src/spiffs/*.o > /dev/null 2>&1
    if [ "${MP_SHOW_PROGRESS}" == "yes" ]; then
        make clean
    else
        make clean > /dev/null 2>&1
    fi
elif [ "${arg}" == "menuconfig" ]; then
    make menuconfig
elif [ "${arg}" == "makefs" ]; then
    echo "========================"
    echo "Creating SPIFFS image..."
    echo "========================"
    make makefs
elif [ "${arg}" == "flashfs" ]; then
    echo "================================="
    echo "Flashing SPIFFS image to ESP32..."
    echo "================================="
    make flashfs
elif [ "${arg}" == "copyfs" ]; then
    echo "========================================="
    echo "Flashing default SPIFFS image to ESP32..."
    echo "========================================="
    make copyfs
elif [ "${arg}" == "makefatfs" ]; then
    echo "======================="
    echo "Creating FatFS image..."
    echo "======================="
    make makefatfs
elif [ "${arg}" == "flashfatfs" ]; then
    echo "================================"
    echo "Flashing FatFS image to ESP32..."
    echo "================================"
    make flashfatfs
elif [ "${arg}" == "copyfatfs" ]; then
    echo "========================================"
    echo "Flashing default FatFS image to ESP32..."
    echo "========================================"
    make copyfatfs
else
    echo "================================"
    echo "Building MicroPython firmware..."
    echo "================================"

    if [ "${MP_SHOW_PROGRESS}" == "yes" ]; then
        make  ${opt} ${arg}
    else
        make  ${opt} ${arg} > /dev/null 2>&1
    fi
fi

if [ $? -ne 0 ] && [ "${jopt}" == "y" ]; then
    echo "Build failed, starting again..."
    sleep 2
    if [ "${MP_SHOW_PROGRESS}" == "yes" ]; then
        make  ${opt} ${arg}
    else
        make  ${opt} ${arg} > /dev/null 2>&1
    fi
fi

if [ $? -eq 0 ]; then
    echo "OK."
    if [ "${arg}" == "all" ]; then
        if [ "${BUILD_TYPE}" == "" ]; then
            cp -f build/MicroPython.bin firmware/esp32 > /dev/null 2>&1
            cp -f build/bootloader/bootloader.bin firmware/esp32/bootloader > /dev/null 2>&1
            cp -f build/partitions_singleapp.bin firmware/esp32 > /dev/null 2>&1
            cp -f sdkconfig firmware/esp32 > /dev/null 2>&1
            echo "#!/bin/bash" > firmware/esp32/flash.sh
            make print_flash_cmd >> firmware/esp32/flash.sh 2>/dev/null
            chmod +x firmware/esp32/flash.sh > /dev/null 2>&1
        else
            cp -f build/MicroPython.bin firmware/esp32_psram > /dev/null 2>&1
            cp -f build/bootloader/bootloader.bin firmware/esp32_psram/bootloader > /dev/null 2>&1
            cp -f build/partitions_singleapp.bin firmware/esp32_psram > /dev/null 2>&1
            cp -f sdkconfig firmware/esp32_psram > /dev/null 2>&1
            echo "#!/bin/bash" > firmware/esp32_psram/flash.sh
            make print_flash_cmd >> firmware/esp32_psram/flash.sh 2>/dev/null
            chmod +x firmware/esp32_psram/flash.sh > /dev/null 2>&1
        fi
        echo "Build complete."
        echo "You can now run ./BUILD.sh flash"
        echo "to deploy the firmware to ESP32"
        echo "--------------------------------"
    fi
    echo ""
else
    echo "'make ${arg}' FAILED!"
    echo ""
    exit 1
fi

