#!/bin/sh
# ATMOS: the buttons and layer / favourites logic on the real UI code (host build, no hardware).
# Run from the repository root after the generators have filled build/gen (tools/gen_*.py, as build.sh does).
set -e
mkdir -p build/host
sed 's/^int main(void)/int ui_test_main(void)/' tests/ui_test.c > tests/ui_test_lib.c
cc -O1 -ffp-contract=off -w -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/atmos_test tests/atmos_test.c -lm
build/host/atmos_test | grep -E "FAIL|atmos_test:"
rm -f tests/ui_test_lib.c
