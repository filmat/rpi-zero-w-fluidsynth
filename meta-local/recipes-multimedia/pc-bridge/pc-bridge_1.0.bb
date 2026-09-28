# Mostek CC3 -> Program Change: pozwala kontrolerom, które nie umieją
# wysyłać Program Change (np. KeyStep 37 mk1, tylko enkodery CC), zmieniać
# brzmienie fluidsynth przez pokrętło ustawione na CC numer 3 (patrz
# CCtoPC.md w korzeniu repo za pełne uzasadnienie decyzji projektowych).
# Do tego gałki ADSR, filtra, vibrato i efektów (CC 73, 75, 79, 72, 74, 71,
# 76, 91, 93) zamienia na NRPN SoundFont 2.01, na które fluidsynth nie
# reaguje sam z siebie (tabela nrpn_map w pc-bridge.c).
SUMMARY = "ALSA sequencer bridge for FluidSynth: CC 3 -> Program Change, synth knobs -> SoundFont NRPN"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://pc-bridge.c \
           file://pc-bridge.init \
           file://respawn"

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
    # nazwa inna niz w synth-autostart (tez instaluje "respawn"), zeby
    # oba pakiety w tym samym obrazie nie kolidowaly o ta sama sciezke
    install -m 0755 ${S}/respawn ${D}${bindir}/pcbridge-respawn
}
