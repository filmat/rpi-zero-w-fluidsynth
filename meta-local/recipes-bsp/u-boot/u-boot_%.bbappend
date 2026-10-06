FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Start bez odliczania (2 s domyślnie); 0 nadal pozwala przerwać start
# klawiszem wciśniętym przed startem U-Boota, -2 wyłączyłoby to całkiem
SRC_URI += "file://bootdelay.cfg"
