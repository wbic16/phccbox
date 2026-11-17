#!/bin/bash
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
S2=$W/src
TCC="$GATE/phcc/tcc -B$GATE/phcc"
B=$W/bundles
exec > $W/out/phase3c.log 2>&1
set -x
ls $S2/gzip-1.14/*.o
ld -r -o $B/gzip-r.o $S2/gzip-1.14/*.o $S2/gzip-1.14/lib/libgzip.a 2> $B/gzip.lderr || { echo GZIP-LDR-FAIL; cat $B/gzip.lderr; }
nm -g --defined-only $B/gzip-r.o | awk 'NF>=3{print $3}' | grep -vx main | sort -u > $B/gzip.syms
objcopy --localize-symbols=$B/gzip.syms --redefine-sym main=gzip_main $B/gzip-r.o $B/gzip.o && echo "BUNDLE-OK gzip ($(wc -l < $B/gzip.syms) symbols localized)"
CORE="bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o"
LIBS="-lselinux -lpcre2-8 -lm"
( cd $W && $TCC -o phccbox $CORE $LIBS && echo LINK-DYN-OK )
( cd $W && $TCC -o phccbox-vi $CORE bundles/vim.o $LIBS $(readelf -d $GATE/src/vim/src/vim | awk '/NEEDED/{gsub(/[\[\]]/,"",$5); print $5}' | sed -e 's/^lib//' -e 's/\.so\..*//' | sort -u | sed 's/^/-l/' | tr '\n' ' ') && echo LINK-VIM-OK )
( cd $W && $TCC -static -o phccbox-static $CORE $LIBS && echo LINK-STATIC-OK )
ls -la $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
sha256sum $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
$W/phccbox --list
cat $W/out/argv0-standalone.txt
echo PHASE3C-DONE
