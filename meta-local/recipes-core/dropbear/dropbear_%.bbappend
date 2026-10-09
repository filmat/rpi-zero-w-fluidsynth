do_install:append() {
      # Keep SSH host keys on the persistent /data partition (rootfs is
      # read-only); read_only_rootfs_hook respects an existing setting.
      echo 'DROPBEAR_RSAKEY_DIR="/data/dropbear"' >> ${D}${sysconfdir}/default/dropbear
}
