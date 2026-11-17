#!/bin/bash
# Exocortical port A/B receipt driver. Runs originals (unprivileged, stub PATH,
# fake HOME, scratch cwd) and ports (EXO_SYSROOT scratch or stub PATH) and
# records stdout/rc/op-stream/file-tree evidence per case under cases/<name>/.
# NOTHING here touches real system state: every effect command is a stub.
T=$HOME/phccbox-work/exo-test
STUBBIN=$T/stubbin
ORIG=$HOME/exocortical-fresh
BIN=$HOME/phccbox-work/exo-stage/phccbox
BASEENV="USER=wbic16"
mkdir -p $T/cases

say() { echo "$@" >> $T/RESULTS.txt; }
fresh() { C=$T/cases/$1; rm -rf $C; mkdir -p $C/orig/work $C/orig/home $C/port/work $C/port/home; : > $C/orig.log; : > $C/port-stub.log; }
ops_port() { grep -E '^(RUN|CAP|RUN-SH|RUN-BG|EXEC|GET|POST|REEXEC) ' $1 2>/dev/null | sed -E 's/^(RUN|CAP|RUN-SH|RUN-BG|EXEC|GET|POST|REEXEC) //'; }
norm() { sed -E -e 's/[0-9]{14}/TS14/g' -e 's/ +$//' -e "s|$C|CASE|g" "$1"; }
verdict() { # name
  local n=$1
  echo "=== $n ===" >> $T/RESULTS.txt
  echo "orig rc=$(cat $C/orig.rc 2>/dev/null) port rc=$(cat $C/port.rc 2>/dev/null)" >> $T/RESULTS.txt
  if diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff; then echo "stdout: IDENTICAL" >> $T/RESULTS.txt; else echo "stdout: DIFF (see $n/out.diff)" >> $T/RESULTS.txt; fi
  if diff <(norm $C/orig.log) <(ops_port $C/port.log | norm /dev/stdin) > $C/ops.diff; then echo "opstream: IDENTICAL" >> $T/RESULTS.txt; else echo "opstream: DIFF (see $n/ops.diff)" >> $T/RESULTS.txt; fi
}
run_orig() { # script args... (stdin from $C/stdin if present)
  ( cd $C/orig/work && env -i $BASEENV HOME=$C/orig/home PATH=$STUBBIN:/usr/bin:/bin \
      STUBLOG=$C/orig.log STUBROOT=$C/orig/root "${CASEENV[@]}" \
      bash $ORIG/$1 "${@:2}" < $C/stdin > $C/orig.out 2> $C/orig.err; echo $? > $C/orig.rc )
}
run_orig_py() { # file args...
  ( cd $C/orig/work && env -i $BASEENV HOME=$C/orig/home PATH=$STUBBIN:/usr/bin:/bin \
      STUBLOG=$C/orig.log STUBROOT=$C/orig/root "${CASEENV[@]}" \
      python3 $ORIG/$1 "${@:2}" < $C/stdin > $C/orig.out 2> $C/orig.err; echo $? > $C/orig.rc )
}
run_port() { # applet args...  (EXO_SYSROOT=$C/port/root unless PORT_NOROOT=1)
  local rootenv=()
  [ "${PORT_NOROOT:-0}" = "1" ] || rootenv=(EXO_SYSROOT=$C/port/root)
  ( cd $C/port/work && env -i $BASEENV HOME=$C/port/home PATH=$STUBBIN:/usr/bin:/bin \
      STUBLOG=$C/port-stub.log "${rootenv[@]}" EXO_TRANSCRIPT=$C/port.log "${CASEENV[@]}" "${PORTENV[@]}" \
      $BIN $1 "${@:2}" < $C/stdin > $C/port.out 2> $C/port.err; echo $? > $C/port.rc )
}
tree() { ( cd $1 && find . -type f | sort | xargs -r sha256sum ); }
: > $T/RESULTS.txt
CASEENV=(); PORTENV=()
