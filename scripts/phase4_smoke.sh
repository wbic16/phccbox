#!/bin/bash
# Phase 4: smoke corpus — phccbox applets (via argv[0] links) vs standalone
# reference tools; stdout bytes + exit code compared. Plus farm demo + sizes.
W=$HOME/phccbox-work
GATE=$HOME/phcc-gate-run
S2=$W/src
T=$W/smoke; mkdir -p $T $W/out
exec > $W/out/phase4.log 2>&1
PB=$W/phccbox
PBVI=$W/phccbox-vi
RSED=$GATE/src/sed/sed/sed
RGREP=$GATE/src/grep/src/grep
RAWK=$GATE/src/gawk/gawk
RSH=$S2/dash-0.5.12/src/dash
RBZ=$S2/bzip2-1.0.8/bzip2
RGZ=$S2/gzip-1.14/gzip
RVIM=$GATE/src/vim/src/vim
# argv[0] link dirs
L=$W/boxlinks; mkdir -p $L
for a in $($PB --list); do ln -sf $PB $L/$a; done
VL=$W/vilinks; mkdir -p $VL; [ -x $PBVI ] && ln -sf $PBVI $VL/vi
# reference argv[0] copies
RL=$W/reflinks; mkdir -p $RL
ln -sf $RSH $RL/ash
# inputs
printf 'hello world\nfoo bar foo\nlast line\n' > $T/words.txt
printf 'a:1\nb:2\nc:3\n' > $T/nums.txt
printf 'ok 200\nerr 503\nok 200\n' > $T/log.txt
$RBZ -c $T/words.txt > $T/words.txt.bz2
$RGZ -n -c $T/words.txt > $T/words.txt.gz
printf 'echo from-stdin\nexit 3\n' > $T/script.sh
PASS=0; FAIL=0; declare -A AP APF
cc() { # name boxcmd refcmd stdin
  local name=$1 bc=$2 rcx=$3 inf=$4 ap=${1%%/*}
  eval "$bc"  < "$inf" > $T/box.out 2>$T/box.err; local brc=$?
  eval "$rcx" < "$inf" > $T/ref.out 2>$T/ref.err; local rrc=$?
  AP[$ap]=$(( ${AP[$ap]:-0} + 1 ))
  if [ $brc -eq $rrc ] && cmp -s $T/box.out $T/ref.out; then
    echo "PASS $name"; PASS=$((PASS+1))
  else
    echo "FAIL $name rc(box=$brc ref=$rrc)"; FAIL=$((FAIL+1)); APF[$ap]=$(( ${APF[$ap]:-0} + 1 ))
    diff <(head -6 $T/box.out) <(head -6 $T/ref.out) | head -10
  fi
}
DN=/dev/null
cc "sed/subst"     "$L/sed 's/foo/QUUX/g'"        "$RSED 's/foo/QUUX/g'"        $T/words.txt
cc "sed/n2p"       "$L/sed -n '2p'"               "$RSED -n '2p'"               $T/words.txt
cc "sed/delete"    "$L/sed '/err/d'"              "$RSED '/err/d'"              $T/log.txt
cc "sed/multi-e"   "$L/sed -e 's/o/0/g' -e 's/l/L/g'" "$RSED -e 's/o/0/g' -e 's/l/L/g'" $T/words.txt
cc "grep/count"    "$L/grep -c ok"                "$RGREP -c ok"                $T/log.txt
cc "grep/basic"    "$L/grep 503"                  "$RGREP 503"                  $T/log.txt
cc "grep/invert"   "$L/grep -v ok"                "$RGREP -v ok"                $T/log.txt
cc "grep/ere-flag" "$L/grep -E 'o(k|r)'"          "$RGREP -E 'o(k|r)'"          $T/log.txt
cc "egrep/plus"    "$L/egrep 'o+'"                "$RGREP -E 'o+'"              $T/words.txt
cc "egrep/alt"     "$L/egrep 'err|last'"          "$RGREP -E 'err|last'"        $T/log.txt
cc "fgrep/litdot"  "$L/fgrep 'o.'"                "$RGREP -F 'o.'"              $T/words.txt
cc "fgrep/plain"   "$L/fgrep foo"                 "$RGREP -F foo"               $T/words.txt
cc "awk/field"     "$L/awk '{print \$2}'"         "$RAWK '{print \$2}'"         $T/words.txt
cc "awk/sum"       "$L/awk -F: '{s+=\$2} END{print s}'" "$RAWK -F: '{s+=\$2} END{print s}'" $T/nums.txt
cc "awk/nf"        "$L/awk 'BEGIN{print \"start\"} {print NF}'" "$RAWK 'BEGIN{print \"start\"} {print NF}'" $T/words.txt
cc "awk/count"     "$L/awk '\$1==\"err\"{c++} END{print c+0}'" "$RAWK '\$1==\"err\"{c++} END{print c+0}'" $T/log.txt
cc "sh/arith"      "$L/sh -c 'echo hello; x=6; echo \$((x*7))'" "$RSH -c 'echo hello; x=6; echo \$((x*7))'" $DN
cc "sh/loop"       "$L/sh -c 'for i in 1 2 3; do printf \"%s,\" \$i; done; echo'" "$RSH -c 'for i in 1 2 3; do printf \"%s,\" \$i; done; echo'" $DN
cc "sh/stdin"      "$L/sh"                        "$RSH"                        $T/script.sh
cc "sh/name0"      "$L/sh -c 'echo name=\$0'"     "$RSH -c 'echo name=\$0'"     $DN
cc "ash/basic"     "$L/ash -c 'echo ash-ok; test 1 -eq 1 && echo yes'" "$RL/ash -c 'echo ash-ok; test 1 -eq 1 && echo yes'" $DN
cc "bzip2/c"       "$L/bzip2 -c"                  "$RBZ -c"                     $T/words.txt
cc "bzip2/c9"      "$L/bzip2 -9 -c"               "$RBZ -9 -c"                  $T/log.txt
cc "bunzip2/c"     "$L/bunzip2 -c $T/words.txt.bz2" "$RBZ -d -c $T/words.txt.bz2" $DN
cc "bzcat/plain"   "$L/bzcat $T/words.txt.bz2"    "$RBZ -d -c $T/words.txt.bz2"  $DN
cc "gzip/nc"       "$L/gzip -n -c"                "$RGZ -n -c"                  $T/words.txt
cc "gzip/n1c"      "$L/gzip -n -1 -c"             "$RGZ -n -1 -c"               $T/log.txt
cc "gunzip/c"      "$L/gunzip -c $T/words.txt.gz" "$RGZ -d -c $T/words.txt.gz"  $DN
cc "zcat/plain"    "$L/zcat $T/words.txt.gz"      "$RGZ -d -c $T/words.txt.gz"  $DN
cc "true/rc"       "$L/true"                      "/usr/bin/true"               $DN
cc "false/rc"      "$L/false"                     "/usr/bin/false"              $DN
cc "yes/head"      "$L/yes | head -3"             "/usr/bin/yes | head -3"      $DN
cc "yes/args"      "$L/yes abc def | head -2"     "/usr/bin/yes abc def | head -2" $DN
cc "dispatch/form" "$PB sed 's/foo/ZZ/'"          "$RSED 's/foo/ZZ/'"           $T/words.txt
cc "dispatch/unknown" "$PB nosuchapplet"          "( exit 127 )"                $DN
# vi cases against the +vim variant (file-editing, compare result files)
if [ -x $PBVI ]; then
  printf 'alpha\nbeta foo\ngamma\n' > $T/e1.txt; cp $T/e1.txt $T/e2.txt
  $VL/vi -es -u NONE -c '%s/foo/BAR/' -c 'wq' $T/e1.txt </dev/null >/dev/null 2>&1; vrc1=$?
  $RVIM  -es -u NONE -c '%s/foo/BAR/' -c 'wq' $T/e2.txt </dev/null >/dev/null 2>&1; vrc2=$?
  if [ $vrc1 -eq $vrc2 ] && cmp -s $T/e1.txt $T/e2.txt; then echo "PASS vi/ex-subst"; PASS=$((PASS+1)); else echo "FAIL vi/ex-subst rc($vrc1/$vrc2)"; FAIL=$((FAIL+1)); fi
  AP[vi]=1; [ $vrc1 -ne $vrc2 ] && APF[vi]=1
else
  echo "SKIP vi (no phccbox-vi)"
fi
echo "=== PER-APPLET ==="
for a in "${!AP[@]}"; do echo "$a: $(( ${AP[$a]} - ${APF[$a]:-0} ))/${AP[$a]}"; done | sort
echo "=== TOTAL pass=$PASS fail=$FAIL ==="
# symlink farm demo
$PB --install $W/farm
bash -c 'export PATH='"$W"'/farm:/usr/bin:/bin; hash -r; command -v sed awk; sed "s/foo/FARM/" '"$T"'/words.txt; awk "{print NR\": \"\$0}" '"$T"'/nums.txt | head -2; true && echo farm-true-ok'
# sizes
echo "=== SIZES ==="
stat -c '%n %s' $W/phccbox $W/phccbox-vi $W/phccbox-static $W/busybox-1.37.0/busybox /usr/bin/busybox 2>/dev/null
echo "standalone sum (sed grep gawk dash gzip bzip2):"
stat -c '%s' $RSED $RGREP $RAWK $RSH $RGZ $RBZ | awk '{s+=$1} END{print s}'
echo "standalone sum + vim:"
stat -c '%s' $RSED $RGREP $RAWK $RSH $RGZ $RBZ $RVIM | awk '{s+=$1} END{print s}'
sensors | grep -i tctl
echo PHASE4-DONE
