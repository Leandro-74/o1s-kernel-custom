#!/bin/sh
# Empacota o modulo do KernelSU com o undervolt_o1s.ko compilado.
#
# Uso:
#   ./ksu-modules/undervolt-o1s/package.sh
#
# O .ko precisa existir no build atual:
#   make ARCH=arm64 O=out LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- modules

set -eu

KDIR=$(cd "$(dirname "$0")/../.." && pwd)
SRC="$KDIR/ksu-modules/undervolt-o1s"
KO="$KDIR/out/drivers/soc/samsung/undervolt/undervolt_o1s.ko"
STAGE="$KDIR/out/ksu-modules/undervolt-o1s"
ZIP="$KDIR/out/ksu-modules/undervolt-o1s.zip"

if [ ! -f "$KO" ]; then
	echo "erro: $KO nao encontrado; compile os modulos primeiro" >&2
	exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$SRC/module.prop" "$SRC/service.sh" "$STAGE/"
cp -r "$SRC/webroot" "$STAGE/"
cp "$KO" "$STAGE/undervolt_o1s.ko"
chmod 755 "$STAGE/service.sh"

rm -f "$ZIP"
(cd "$STAGE" && zip -qr "$ZIP" .)

echo "$ZIP"
