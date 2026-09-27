# Mostek CC3 -> Program Change: pozwala kontrolerom, które nie umieją
# wysyłać Program Change (np. KeyStep 37 mk1, tylko enkodery CC), zmieniać
# brzmienie fluidsynth przez pokrętło ustawione na CC numer 3 (patrz
# CCtoPC.md w korzeniu repo za pełne uzasadnienie decyzji projektowych).
SUMMARY = "ALSA sequencer bridge: MIDI CC 3 -> Program Change for FluidSynth"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://pc-bridge.c \
           file://pc-bridge.init"

S = "${WORKDIR}"

DEPENDS = "alsa-lib"

inherit update-rc.d
INITSCRIPT_NAME = "pc-bridge"
INITSCRIPT_PARAMS = "defaults 95"

# Zmiana CC / kroku bez edycji źródła, np. w local.conf:
# PC_BRIDGE_DEFS:pn-pc-bridge = "-DPC_CC=3 -DSTEP=8"
PC_BRIDGE_DEFS ?= ""

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} ${PC_BRIDGE_DEFS} ${S}/pc-bridge.c -o ${B}/pc-bridge -lasound
}

do_install() {
    install -d ${D}${bindir} ${D}${sysconfdir}/init.d
    install -m 0755 ${B}/pc-bridge ${D}${bindir}/pc-bridge
    install -m 0755 ${S}/pc-bridge.init ${D}${sysconfdir}/init.d/pc-bridge
}
