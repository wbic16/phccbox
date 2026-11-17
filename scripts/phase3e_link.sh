#!/bin/bash
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
TCC="$GATE/phcc/tcc -B$GATE/phcc"
B=$W/bundles
exec > $W/out/phase3e.log 2>&1
set -x
find $GATE/src/sed/lib -name '*.o' | sort > $B/sed-libobjs.txt; wc -l $B/sed-libobjs.txt
ld -r -o $B/sed-r.o $GATE/src/sed/sed/*.o $(cat $B/sed-libobjs.txt) 2> $B/sed.lderr || { echo SED-LDR-FAIL; cat $B/sed.lderr; }
nm -g --defined-only $B/sed-r.o | awk 'NF>=3{print $3}' | grep -vx main | sort -u > $B/sed.syms
objcopy --localize-symbols=$B/sed.syms --redefine-sym main=sed_main $B/sed-r.o $B/sed.o && echo "BUNDLE-OK sed ($(wc -l < $B/sed.syms) symbols localized)"
nm $B/sed.o | grep -c ' U ' ; nm $B/sed.o | grep ' U ' | grep -vE 'GLIBC|@' | head -20
CORE="bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o"
LIBS="-lselinux -lpcre2-8 -lm"
( cd $W && $TCC -o phccbox $CORE $LIBS && echo LINK-DYN-OK )
( cd $W && $TCC -o phccbox-vi $CORE bundles/vim.o $LIBS $(readelf -d $GATE/src/vim/src/vim | awk '/NEEDED/{gsub(/[\[\]]/,"",$5); print $5}' | grep -v 'ld-linux' | sed -e 's/^lib//' -e 's/\.so\..*//' | sort -u | sed 's/^/-l/' | tr '\n' ' ') && echo LINK-VIM-OK )
( cd $W && $TCC -static -o phccbox-static $CORE $LIBS && echo LINK-STATIC-OK )
ls -la $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
sha256sum $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
$W/phccbox --list
echo PHASE3E-DONE
