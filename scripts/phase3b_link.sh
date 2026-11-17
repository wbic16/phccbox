#!/bin/bash
# Phase 3b: rebuild sed/grep/gzip bundles via their archives (extraction
# semantics), link phccbox dynamic + static, try the +vim variant apart.
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
S2=$W/src
TCC="$GATE/phcc/tcc -B$GATE/phcc"
B=$W/bundles
mkdir -p $B $W/out
exec > $W/out/phase3b.log 2>&1
guard() { local t; t=$(sensors 2>/dev/null | awk '/Tctl/{gsub(/\+|°C/,"",$2); print $2; exit}');
  while [ -n "$t" ] && awk "BEGIN{exit !($t>=75)}"; do echo "[thermal hold $t]"; sleep 30;
    t=$(sensors 2>/dev/null | awk '/Tctl/{gsub(/\+|°C/,"",$2); print $2; exit}'); done; }
set -x
bundle() { local name=$1 main=$2 out=$3; shift 3
  ld -r -o $B/$name-r.o "$@" 2> $B/$name.lderr || { echo "BUNDLE-FAIL ld -r $name"; cat $B/$name.lderr; return 1; }
  nm -g --defined-only $B/$name-r.o | awk 'NF>=3{print $3}' | grep -vx main | sort -u > $B/$name.syms
  objcopy --localize-symbols=$B/$name.syms --redefine-sym main=$main $B/$name-r.o $out || { echo "BUNDLE-FAIL objcopy $name"; return 1; }
  echo "BUNDLE-OK $name ($(wc -l < $B/$name.syms) symbols localized)"
}
guard
bundle sed   sed_main   $B/sed.o   $GATE/src/sed/sed/*.o $GATE/src/sed/lib/libsed.a
bundle grep  grep_main  $B/grep.o  $GATE/src/grep/src/*.o $GATE/src/grep/lib/libgreputils.a
bundle gzip  gzip_main  $B/gzip.o  $S2/gzip-1.14/gzip.o $S2/gzip-1.14/zip.o $S2/gzip-1.14/deflate.o $S2/gzip-1.14/trees.o $S2/gzip-1.14/bits.o $S2/gzip-1.14/unzip.o $S2/gzip-1.14/inflate.o $S2/gzip-1.14/util.o $S2/gzip-1.14/lzw.o $S2/gzip-1.14/lib/libgzip.a
CORE="bundles/phccbox.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o"
LIBS="-lselinux -lpcre2-8 -lm"
( cd $W && $TCC -o phccbox $CORE $LIBS && echo LINK-DYN-OK )
( cd $W && $TCC -o phccbox-vi $CORE bundles/vim.o $LIBS $(readelf -d $GATE/src/vim/src/vim | awk '/NEEDED/{gsub(/[\[\]]/,"",$5); print $5}' | sed -e 's/^lib//' -e 's/\.so\..*//' | sort -u | sed 's/^/-l/' | tr '\n' ' ') && echo LINK-VIM-OK )
( cd $W && $TCC -static -o phccbox-static $CORE $LIBS && echo LINK-STATIC-OK )
ls -la $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
sha256sum $W/phccbox $W/phccbox-vi $W/phccbox-static 2>/dev/null
$W/phccbox --list > $W/out/list.txt 2>&1; cat $W/out/list.txt
# standalone argv[0] self-dispatch evidence (design), compactly:
cd $W/argv0test2
{ echo "gunzip-name:"; ./gunzip -c p.txt.gz; echo "zcat-name:"; ./zcat p.txt.gz;
  echo "bunzip2-name:"; ./bunzip2 -c p.txt.bz2; echo "bzcat-name:"; ./bzcat p.txt.bz2; } > $W/out/argv0-standalone.txt 2>&1
cat $W/out/argv0-standalone.txt
sensors | grep -i tctl
echo PHASE3B-DONE
