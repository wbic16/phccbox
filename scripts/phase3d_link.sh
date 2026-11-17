#!/bin/bash
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
S2=$W/src
TCC="$GATE/phcc/tcc -B$GATE/phcc"
B=$W/bundles
exec > $W/out/phase3d.log 2>&1
set -x
bundle() { local name=$1 main=$2 out=$3; shift 3
  ld -r -o $B/$name-r.o "$@" 2> $B/$name.lderr || { echo "BUNDLE-FAIL ld -r $name"; cat $B/$name.lderr; return 1; }
  nm -g --defined-only $B/$name-r.o | awk 'NF>=3{print $3}' | grep -vx main | sort -u > $B/$name.syms
  objcopy --localize-symbols=$B/$name.syms --redefine-sym main=$main $B/$name-r.o $out || { echo "BUNDLE-FAIL objcopy $name"; return 1; }
  echo "BUNDLE-OK $name ($(wc -l < $B/$name.syms) symbols localized)"
}
bundle sed  sed_main  $B/sed.o  $GATE/src/sed/sed/*.o $GATE/src/sed/lib/libsed.a $GATE/src/sed/lib/malloc/*.o
bundle grep grep_main $B/grep.o $GATE/src/grep/src/*.o $GATE/src/grep/lib/libgreputils.a $GATE/src/grep/lib/malloc/*.o
bundle sh   sh_main   $B/sh.o   $S2/dash-0.5.12/src/*.o $S2/dash-0.5.12/src/bltin/*.o
CORE="bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o"
LIBS="-lselinux -lpcre2-8 -lm"
( cd $W && $TCC -o phccbox $CORE $LIBS && echo LINK-DYN-OK )
( cd $W && $TCC -o phccbox-vi $CORE bundles/vim.o $LIBS $(readelf -d $GATE/src/vim/src/vim | awk '/NEEDED/{gsub(/[\[\]]/,"",$5); print $5}' | grep -v 'ld-linux' | sed -e 's/^lib//' -e 's/\.so\..*//' | sort -u | sed 's/^/-l/' | tr '\n' ' ') && echo LINK-VIM-OK )
( cd $W && $TCC -static -o phccbox-static $CORE $LIBS && echo LINK-STATIC-OK )
ls -la $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
sha256sum $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
$W/phccbox --list
echo PHASE3D-DONE
