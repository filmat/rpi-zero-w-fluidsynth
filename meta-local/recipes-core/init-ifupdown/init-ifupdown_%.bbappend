do_install:append() {
    # Upstream already defines the wlan0 stanza (including wpa-conf) but does
    # not bring the interface up at boot. Add only the missing "auto" line
    # instead of appending a second, duplicate wlan0 stanza.
    if ! grep -q '^auto wlan0' ${D}${sysconfdir}/network/interfaces; then
        sed -i 's/^iface wlan0 inet dhcp/auto wlan0\n&/' ${D}${sysconfdir}/network/interfaces
    fi
    grep -q '^auto wlan0' ${D}${sysconfdir}/network/interfaces || \
        bbfatal "init-ifupdown: could not add 'auto wlan0' to interfaces"

    # Point wpa_supplicant at the persistent copy on /data (rootfs is read-only)
    if ! grep -q 'wpa-conf /data/wpa_supplicant.conf' ${D}${sysconfdir}/network/interfaces; then
        sed -i 's#wpa-conf /etc/wpa_supplicant.conf#wpa-conf /data/wpa_supplicant.conf#' ${D}${sysconfdir}/network/interfaces
    fi
    grep -q 'wpa-conf /data/wpa_supplicant.conf' ${D}${sysconfdir}/network/interfaces || \
        bbfatal "init-ifupdown: could not point wpa-conf to /data"

    # Seed the persistent copy from the default in the image on first boot
    DATA_CONF=/data/wpa_supplicant.conf
    SEED_CMD="pre-up [ -f $DATA_CONF ] || cp -p /etc/wpa_supplicant.conf $DATA_CONF"
    sed -i "s#^\(\s*\)wpa-conf $DATA_CONF\$#&\n\1$SEED_CMD#" \
        ${D}${sysconfdir}/network/interfaces
    grep -q "pre-up .* $DATA_CONF" ${D}${sysconfdir}/network/interfaces || \
        bbfatal "init-ifupdown: could not add the pre-up seed line"
}
