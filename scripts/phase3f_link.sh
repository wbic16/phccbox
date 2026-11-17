#!/bin/bash
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
TCC="$GATE/phcc/tcc -B$GATE/phcc"
B=$W/bundles
exec > $W/out/phase3f.log 2>&1
set -x
ld -r -o $B/awk-r.o $GATE/src/gawk/*.o $GATE/src/gawk/support/*.o $GATE/src/gawk/support/malloc/*.o 2> $B/awk.lderr || { echo AWK-LDR-FAIL; cat $B/awk.lderr; }
nm -g --defined-only $B/awk-r.o | awk 'NF>=3{print $3}' | grep -vx main | sort -u > $B/awk.syms
objcopy --localize-symbols=$B/awk.syms --redefine-sym main=awk_main $B/awk-r.o $B/awk.o && echo "BUNDLE-OK awk ($(wc -l < $B/awk.syms) symbols localized)"
nm $B/awk.o | grep -c ' U _gl_dynarray' || true
CORE="bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o"
LIBS="-lselinux -lpcre2-8 -lm"
( cd $W && $TCC -o phccbox $CORE $LIBS && echo LINK-DYN-OK )
( cd $W && $TCC -o phccbox-vi $CORE bundles/vim.o $LIBS $(readelf -d $GATE/src/vim/src/vim | awk '/NEEDED/{gsub(/[\[\]]/,"",$5); print $5}' | grep -v 'ld-linux' | sed -e 's/^lib//' -e 's/\.so\..*//' | sort -u | sed 's/^/-l/' | tr '\n' ' ') && echo LINK-VIM-OK )
( cd $W && $TCC -static -o phccbox-static $CORE $LIBS && echo LINK-STATIC-OK )
ls -la $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
sha256sum $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
$W/phccbox --list
echo PHASE3F-DONE
