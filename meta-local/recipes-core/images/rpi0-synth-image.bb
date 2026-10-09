require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL:append = " kernel-modules wpa-supplicant wpa-supplicant-passphrase linux-firmware-rpidistro-bcm43430 usbutils alsa-utils fluidsynth-bin timgm6mb-soundfont synth-autostart pc-bridge"
IMAGE_FEATURES += "ssh-server-dropbear"

# Read-only rootfs: survives power loss; persistent state lives on /data
IMAGE_FEATURES += "read-only-rootfs"

# Partition layout with a persistent /data partition
WKS_FILE = "sdimage-synth.wks"

# Mount point for the persistent /data partition (the rootfs is read-only,
# so it cannot be created at runtime)
create_data_mountpoint () {
    install -d ${IMAGE_ROOTFS}/data
}
ROOTFS_POSTPROCESS_COMMAND += "create_data_mountpoint "
