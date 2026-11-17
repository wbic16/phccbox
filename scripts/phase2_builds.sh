#!/bin/bash
# Phase 2: build dash / gzip / bzip2 with phcc (tip 9d070f26); probe coreutils single-binary.
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
TCC="$GATE/phcc/tcc -B$GATE/phcc"
mkdir -p $W/out $W/src
exec > $W/out/phase2.log 2>&1
guard() { local t; t=$(sensors 2>/dev/null | awk '/Tctl/{gsub(/\+|°C/,"",$2); print $2; exit}');
  while [ -n "$t" ] && awk "BEGIN{exit !($t>=75)}"; do echo "[thermal hold $t]"; sleep 30;
    t=$(sensors 2>/dev/null | awk '/Tctl/{gsub(/\+|°C/,"",$2); print $2; exit}'); done; }
set -x
cd $W/src
tar xf $W/dl/dash-0.5.12.tar.gz
tar xf $W/dl/gzip-1.14.tar.xz
tar xf $W/dl/bzip2-1.0.8.tar.gz
guard
( cd dash-0.5.12 && ./configure CC="$TCC" > conf.log 2>&1 && make -j8 > make.log 2>&1; echo dash_rc=$?; find . -name dash -type f -exec ls -la {} \; )
guard
( cd gzip-1.14 && ./configure CC="$TCC" > conf.log 2>&1 && make -j8 > make.log 2>&1; echo gzip_rc=$?; ls -la gzip 2>/dev/null; tail -5 make.log )
guard
( cd bzip2-1.0.8 && make -j8 CC="$TCC" > make.log 2>&1; echo bzip2_rc=$?; ls -la bzip2 2>/dev/null; tail -5 make.log )
# coreutils single-binary probe (fresh tree; expect the F18/nanosleep defect)
guard
tar xf $HOME/phcc-bench-run/tarballs/coreutils-9.12.tar.xz
( cd coreutils-9.12 && ./configure CC="$TCC" --enable-single-binary=symlinks > conf.log 2>&1; echo cu_conf_rc=$?; grep -m2 -i 'single' conf.log; make -j8 > make.log 2>&1; echo cu_make_rc=$?; grep -m4 -n 'error' make.log )
# standalone sizes (gate + phase2 products)
for b in $GATE/src/sed/sed/sed $GATE/src/grep/src/grep $GATE/src/gawk/gawk $GATE/src/vim/src/vim dash-0.5.12/dash gzip-1.14/gzip bzip2-1.0.8/bzip2; do ls -la $b 2>/dev/null; done
sensors | grep -i tctl
echo PHASE2-DONE
