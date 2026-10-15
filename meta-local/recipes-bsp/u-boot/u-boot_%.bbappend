FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# bootdelay.cfg: boot without the 2 s countdown; 0 still lets a key pressed
#   before U-Boot starts interrupt the boot, -2 would disable that entirely
# no-usb-preboot.cfg: no USB keyboard and no "usb start" in preboot
# 0001-...patch: drop usbkbd from the default stdin (rpi.env); note that a
#   saved uboot.env on the card overrides the built-in default environment
SRC_URI += "file://bootdelay.cfg \
            file://no-usb-preboot.cfg \
            file://0001-rpi-drop-usbkbd-from-default-stdin.patch \
            "
