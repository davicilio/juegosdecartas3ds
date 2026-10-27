#!/bin/sh
# Empaqueta JuegosDeCartas.cia para instalarlo con FBI.
#
# Necesita haber compilado antes (make) y tener makerom y bannertool:
#   MAKEROM=/ruta/makerom BANNERTOOL=/ruta/bannertool tools/make_cia.sh
set -eu

cd "$(dirname "$0")/.."
MAKEROM=${MAKEROM:-makerom}
BANNERTOOL=${BANNERTOOL:-bannertool}
# Nombre interno (8 letras como mucho); el que se ve en el menú HOME sale del .smdh
TITLE=JDCartas
VERSION_MAJOR=1
VERSION_MINOR=0
VERSION_MICRO=0

for f in JuegosDeCartas.elf JuegosDeCartas.smdh romfs/gfx/sprites.t3x; do
	[ -f "$f" ] || { echo "Falta $f: compila primero con make" >&2; exit 1; }
done

mkdir -p build
"$BANNERTOOL" makebanner -i cia/banner.png -a cia/banner.wav -o build/banner.bnr

"$MAKEROM" -f cia -o JuegosDeCartas.cia -target t -exefslogo \
	-elf JuegosDeCartas.elf -icon JuegosDeCartas.smdh -banner build/banner.bnr \
	-rsf cia/app.rsf \
	-major "$VERSION_MAJOR" -minor "$VERSION_MINOR" -micro "$VERSION_MICRO" \
	-DAPP_TITLE="$TITLE" \
	-DAPP_PRODUCT_CODE=CTR-P-JDCE \
	-DAPP_UNIQUE_ID=0xF4C70 \
	-DAPP_ROMFS=romfs

echo "Creado JuegosDeCartas.cia"
