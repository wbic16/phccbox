#!/bin/bash
# Phase 3: bundle each program's objects (ld -r + objcopy localize/rename
# main), compile the dispatcher with phcc, link phccbox (dynamic + static).
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
S2=$W/src
TCC="$GATE/phcc/tcc -B$GATE/phcc"
B=$W/bundles
mkdir -p $B $W/out
exec > $W/out/phase3.log 2>&1
guard() { local t; t=$(sensors 2>/dev/null | awk '/Tctl/{gsub(/\+|°C/,"",$2); print $2; exit}');
  while [ -n "$t" ] && awk "BEGIN{exit !($t>=75)}"; do echo "[thermal hold $t]"; sleep 30;
    t=$(sensors 2>/dev/null | awk '/Tctl/{gsub(/\+|°C/,"",$2); print $2; exit}'); done; }
set -x
bundle() { # name mainname out objs...
  local name=$1 main=$2 out=$3; shift 3
  ld -r -o $B/$name-r.o "$@" 2> $B/$name.lderr || { echo "BUNDLE-FAIL ld -r $name"; cat $B/$name.lderr; return 1; }
  nm -g --defined-only $B/$name-r.o | awk 'NF>=3{print $3}' | grep -vx main | sort -u > $B/$name.syms
  objcopy --localize-symbols=$B/$name.syms --redefine-sym main=$main $B/$name-r.o $out || { echo "BUNDLE-FAIL objcopy $name"; return 1; }
  echo "BUNDLE-OK $name ($(wc -l < $B/$name.syms) symbols localized)"
}
guard
# object inventories
ls $GATE/src/sed/lib/*.o >/dev/null 2>&1 && SEDLIB=$GATE/src/sed/lib/*.o || SEDLIB=$GATE/src/sed/lib/libsed.a
bundle sed    sed_main    $B/sed.o    $GATE/src/sed/sed/*.o $SEDLIB
bundle grep   grep_main   $B/grep.o   $GATE/src/grep/src/*.o $GATE/src/grep/lib/*.o
bundle awk    awk_main    $B/awk.o    $GATE/src/gawk/*.o $GATE/src/gawk/support/*.o
bundle sh     sh_main     $B/sh.o     $S2/dash-0.5.12/src/*.o
bundle bzip2  bzip2_main  $B/bzip2.o  $S2/bzip2-1.0.8/bzip2.o $S2/bzip2-1.0.8/blocksort.o $S2/bzip2-1.0.8/huffman.o $S2/bzip2-1.0.8/crctable.o $S2/bzip2-1.0.8/randtable.o $S2/bzip2-1.0.8/compress.o $S2/bzip2-1.0.8/decompress.o $S2/bzip2-1.0.8/bzlib.o
bundle gzip   gzip_main   $B/gzip.o   $S2/gzip-1.14/*.o $S2/gzip-1.14/lib/*.o
# vim attempt (tolerant)
VIMOBJS=$(ls $GATE/src/vim/src/objects/*.o 2>/dev/null)
if [ -n "$VIMOBJS" ]; then bundle vim vim_main $B/vim.o $VIMOBJS && echo VIM-BUNDLE-OK || echo VIM-BUNDLE-FAILED; fi
readelf -d $GATE/src/vim/src/vim | grep NEEDED
# dispatcher + link
cp $W/phccbox.c $W/phccbox-build.c
( cd $W && $TCC -c phccbox-build.c -o bundles/phccbox.o && echo DISP-OK )
guard
( cd $W && $TCC -o phccbox bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o -lselinux -lpcre2-8 -lm && echo LINK-DYN-OK )
ls -la $W/phccbox 2>/dev/null
( cd $W && $TCC -static -o phccbox-static bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o -lselinux -lpcre2-8 -lm && echo LINK-STATIC-OK )
ls -la $W/phccbox-static 2>/dev/null
$W/phccbox --list
# do the standalone gzip/bzip2 self-dispatch on argv[0]? (design evidence)
mkdir -p $W/argv0test2 && cd $W/argv0test2
printf 'payload payload payload\n' > p.txt
$S2/gzip-1.14/gzip -n -c p.txt > p.txt.gz
cp $S2/gzip-1.14/gzip ./gunzip; cp $S2/gzip-1.14/gzip ./zcat
echo "standalone-as-gunzip:"; ./gunzip -c p.txt.gz
echo "standalone-as-zcat:";   ./zcat p.txt.gz
$S2/bzip2-1.0.8/bzip2 -c p.txt > p.txt.bz2
cp $S2/bzip2-1.0.8/bzip2 ./bunzip2; cp $S2/bzip2-1.0.8/bzip2 ./bzcat
echo "standalone-as-bunzip2:"; ./bunzip2 -c p.txt.bz2
echo "standalone-as-bzcat:";   ./bzcat p.txt.bz2
sensors | grep -i tctl
echo PHASE3-DONE
