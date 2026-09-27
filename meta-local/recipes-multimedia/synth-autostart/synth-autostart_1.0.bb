# Autostart syntezatora i kontrolerów

SUMMARY = "Autostart syntezatora i binding klawiatur"
DESCRIPTION = "Odpala fluidsynth i podpina Keystep 37 oraz MPK Mini przy starcie na jego wejscie"
SECTION = "multimedia"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://synth file://synth-connect file://respawn"

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "synth"
INITSCRIPT_PARAMS = "defaults 90"

do_install() {
    install -d ${D}${sysconfdir}/init.d/
    install -m 0755 ${WORKDIR}/synth ${D}${sysconfdir}/init.d/
    install -d ${D}${bindir}/
    install -m 0755 ${WORKDIR}/synth-connect ${D}${bindir}/
    install -m 0755 ${WORKDIR}/respawn ${D}${bindir}/
}

RDEPENDS:${PN} += "alsa-utils-aconnect alsa-utils-amixer fluidsynth-bin timgm6mb-soundfont"
