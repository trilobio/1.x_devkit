initialize:
    sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak
    sudo cp ./raspi/initialization/config.txt /boot/firmware/config.txt
    sudo cp ./raspi/initialization/can-network.service /etc/systemd/system/can-network.service
    sudo systemctl daemon-reload
    sudo systemctl enable can-network.service
    sudo apt install can-utils python3-can
    @echo "CAN boot config installed. Reboot required before CAN interfaces will appear."
    @echo "Run: sudo reboot"

check-can:
    ip link show can0
    ip link show can1
    systemctl --no-pager status can-network.service
