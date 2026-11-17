#!/bin/bash
# phccbox v1.1 smoke A2: the digest family against the correct oracle —
# a GCC-built coreutils 9.12 SINGLE-BINARY build (ref2). In multicall
# mode the six digest programs are served by the shared cksum machinery,
# so their --help/--version metadata is the multicall table's, not the
# per-program build's. Criterion stays byte-parity + rc vs ref2.
W=$HOME/phccbox-v11
BOX=$W/phccbox
REF2=$W/ref2/src/src
export LC_ALL=C TZ=UTC
exec > $W/out/smoke-a2.log 2>&1
pass=0; fail=0; failed=""
for n in md5sum sha1sum sha224sum sha256sum sha384sum sha512sum; do
  for flag in --help --version; do
    "$BOX" "$n" $flag > /tmp/a.out 2> /tmp/a.err; brc=$?
    ( exec -a "$n" "$REF2/coreutils" $flag ) > /tmp/b.out 2> /tmp/b.err; rrc=$?
    if [ $brc -eq $rrc ] && cmp -s /tmp/a.out /tmp/b.out && cmp -s /tmp/a.err /tmp/b.err; then
      pass=$((pass+1)); echo "PASS $n $flag (vs gcc single-binary)"
    else
      fail=$((fail+1)); failed="$failed $n$flag"
      echo "FAIL $n $flag: boxrc=$brc ref2rc=$rrc"; diff /tmp/b.out /tmp/a.out | head -4
    fi
  done
done
echo "SMOKE-A2: pass=$pass fail=$fail"
[ -n "$failed" ] && echo "FAILED:$failed"
echo SMOKE-A2-DONE
