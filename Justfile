initialize:
    sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak
    sudo cp ./raspi/initialization/config.txt /boot/firmware/config.txt
    sudo cp ./raspi/initialization/can-network.service /etc/systemd/system/can-network.service
    sudo systemctl daemon-reload
    sudo systemctl enable can-network.service
    sudo apt install can-utils python3-can cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
    @echo "CAN boot config installed. Reboot required before CAN interfaces will appear."
    @echo "Run: sudo reboot"

check-can:
    ip link show can0
    ip link show can1
    systemctl --no-pager status can-network.service

init-cmake:
    cd firmware/tool_devkit && cmake --preset Debug
    cd firmware/powered_deck_module_devkit && cmake --preset Debug
    cd firmware/powered_deck_slot_devkit && cmake --preset Debug

build-firmware-binaries:
    mkdir -p ../bootloader-2/raspi/binaries
    cd firmware/tool_devkit && cmake --build --preset Debug
    cd firmware/powered_deck_module_devkit && cmake --build --preset Debug
    cd firmware/powered_deck_slot_devkit && cmake --build --preset Debug
    cp firmware/tool_devkit/build/Debug/tool_devkit.bin bootloader-2/raspi/binaries/
    cp firmware/powered_deck_module_devkit/build/Debug/pdm_devkit.bin bootloader-2/raspi/binaries/
    cp firmware/powered_deck_slot_devkit/build/Debug/pds_devkit.bin bootloader-2/raspi/binaries/

clean:
    rm -rf firmware/tool_devkit/build
    rm -rf firmware/powered_deck_module_devkit/build
    rm -rf firmware/powered_deck_slot_devkit/build
    rm -rf bootloader-2/raspi/binaries

clean-build: clean init-cmake build-firmware-binaries
