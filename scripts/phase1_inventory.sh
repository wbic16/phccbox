#!/bin/bash
# Phase 1: BusyBox inventory (from source, defconfig) + reference checks.
W=$HOME/phccbox-work
mkdir -p $W/out
exec > $W/out/phase1.log 2>&1
set -x
cd $W
tar xf dl/busybox-1.37.0.tar.bz2
cd busybox-1.37.0
make defconfig >/dev/null 2>&1
grep -c '^CONFIG_' .config
make -j8 >/dev/null 2>&1
ls -la busybox
./busybox --list > $W/out/busybox-defconfig-applets.txt
wc -l $W/out/busybox-defconfig-applets.txt
# system busybox reference
/usr/bin/busybox --list > $W/out/busybox-system-applets.txt
wc -l $W/out/busybox-system-applets.txt
ls -la /usr/bin/busybox
# does standalone GNU grep dispatch on argv[0] (egrep/fgrep)?
G=$HOME/phcc-gate-run/src/grep/src/grep
mkdir -p $W/argv0test && cd $W/argv0test
cp $G ./egrep; cp $G ./fgrep
printf 'a+b\naab\n' > in.txt
echo "egrep-as-name:"; ./egrep 'a+b' in.txt
echo "fgrep-as-name:"; ./fgrep 'a+b' in.txt
echo "grep-plain:";    $G 'a+b' in.txt
# library deps of gate-built standalone tools
for b in $HOME/phcc-gate-run/src/sed/sed/sed $HOME/phcc-gate-run/src/grep/src/grep $HOME/phcc-gate-run/src/gawk/gawk $HOME/phcc-gate-run/src/vim/src/vim; do echo "== $b"; ldd $b 2>/dev/null | head -8; done
# coreutils version + program inventory (for the coverage map)
CU=$HOME/phcc-gate-run/src/coreutils
grep -m1 'AC_INIT' $CU/configure.ac 2>/dev/null || grep -m1 'PACKAGE_VERSION' $CU/config.h 2>/dev/null
ls $CU/src/*.c | xargs -n1 basename | sed 's/\.c$//' > $W/out/coreutils-programs.txt
wc -l $W/out/coreutils-programs.txt
sensors | grep -i tctl
echo PHASE1-DONE
