#!/bin/bash
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
TCC="$GATE/phcc/tcc -B$GATE/phcc"
exec > $W/out/phase4pre.log 2>&1
set -x
( cd $W && $TCC -DWITH_VIM -c phccbox-build.c -o bundles/phccbox-vim.o && echo DISP-VIM-OK )
COREV="bundles/phccbox-vim.o bundles/sed.o bundles/grep.o bundles/awk.o bundles/sh.o bundles/bzip2.o bundles/gzip.o bundles/vim.o"
LIBS="-lselinux -lpcre2-8 -lm"
( cd $W && $TCC -o phccbox-vi $COREV $LIBS $(readelf -d $GATE/src/vim/src/vim | awk '/NEEDED/{gsub(/[\[\]]/,"",$5); print $5}' | grep -v 'ld-linux' | sed -e 's/^lib//' -e 's/\.so\..*//' | sort -u | sed 's/^/-l/' | tr '\n' ' ') && echo LINK-VIM2-OK )
ls -la $W/phccbox $W/phccbox-vi 2>/dev/null
sha256sum $W/phccbox $W/phccbox-vi 2>/dev/null
$W/phccbox --list | tr '\n' ' '; echo
$W/phccbox-vi --list | tr '\n' ' '; echo
echo PHASE4PRE-DONE
