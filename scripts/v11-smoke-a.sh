#!/bin/bash
# phccbox v1.1 smoke A: --help / --version parity for every coreutils name.
# Box (subcommand form) vs gcc reference build of the same coreutils 9.12.
# Criterion: stdout bytes, stderr bytes, exit code all identical.
W=$HOME/phccbox-v11
BOX=$W/phccbox
REF=$W/ref/src/src
export LC_ALL=C TZ=UTC
exec > $W/out/smoke-a.log 2>&1
NAMES=$(cat $W/out/cu-symlinks.txt)
pass=0; fail=0; failed=""
chk() { # name refbin flag
  local n=$1 rb=$2 flag=$3
  "$BOX" "$n" $flag > /tmp/a.out 2> /tmp/a.err; local brc=$?
  ( exec -a "$n" "$rb" $flag ) > /tmp/b.out 2> /tmp/b.err; local rrc=$?
  if [ $brc -eq $rrc ] && cmp -s /tmp/a.out /tmp/b.out && cmp -s /tmp/a.err /tmp/b.err; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); failed="$failed $n$flag"
    echo "FAIL $n $flag: boxrc=$brc refrc=$rrc outdiff=$(cmp -s /tmp/a.out /tmp/b.out || echo Y) errdiff=$(cmp -s /tmp/a.err /tmp/b.err || echo Y)"
  fi
}
for n in $NAMES; do
  rb=$REF/$n
  [ "$n" = ginstall ] && { rb=$REF/ginstall; n=install; }
  chk "$n" "$rb" --help
  chk "$n" "$rb" --version
done
# the multicall name itself: box "coreutils" vs the phcc standalone single binary
"$BOX" coreutils --help > /tmp/a.out 2>/tmp/a.err; brc=$?
( exec -a coreutils $W/cu/src/src/coreutils --help ) > /tmp/b.out 2>/tmp/b.err; rrc=$?
if [ $brc -eq $rrc ] && cmp -s /tmp/a.out /tmp/b.out && cmp -s /tmp/a.err /tmp/b.err; then pass=$((pass+1)); else fail=$((fail+1)); failed="$failed coreutils--help"; echo "FAIL coreutils --help"; fi
echo "SMOKE-A: pass=$pass fail=$fail"
[ -n "$failed" ] && echo "FAILED:$failed"
echo SMOKE-A-DONE
