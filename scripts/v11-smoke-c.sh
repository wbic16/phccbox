#!/bin/bash
# phccbox v1.1 smoke C: v1 applet regression (vs system tools on stable
# cases + self roundtrips) and the --install symlink farm.
W=$HOME/phccbox-v11
BOX=$W/phccbox
export LC_ALL=C TZ=UTC
exec > $W/out/smoke-c.log 2>&1
pass=0; fail=0; failed=""
ck() { # name condition-result(0/1)
  if [ $1 -eq 0 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); failed="$failed $2"; echo "FAIL $2"; fi
}
cd $W/smoke && rm -rf reg && mkdir reg && cd reg
printf 'one two\nthree two\n' > t.txt
# sed vs system sed
diff <($BOX sed 's/two/2/g' t.txt) <(sed 's/two/2/g' t.txt) >/dev/null; ck $? sed-subst
diff <($BOX sed -n '2p' t.txt) <(sed -n '2p' t.txt) >/dev/null; ck $? sed-np
# grep family vs system grep (flag forms)
diff <($BOX grep -c two t.txt) <(grep -c two t.txt) >/dev/null; ck $? grep-count
diff <($BOX egrep 'one|three' t.txt) <(grep -E 'one|three' t.txt) >/dev/null; ck $? egrep
diff <($BOX fgrep 't.o' t.txt) <(grep -F 't.o' t.txt) >/dev/null; ck $? fgrep
# awk vs system awk
diff <($BOX awk '{s+=$2} END{print s}' <(printf 'a 1\nb 2\n')) <(awk '{s+=$2} END{print s}' <(printf 'a 1\nb 2\n')) >/dev/null; ck $? awk-sum
diff <($BOX awk -F: '{print $1}' /etc/passwd | head -3) <(awk -F: '{print $1}' /etc/passwd | head -3) >/dev/null; ck $? awk-field
# sh (dash) behaviour
[ "$($BOX sh -c 'echo $((6*7))')" = "42" ]; ck $? sh-arith
diff <($BOX sh -c 'for i in 1 2 3; do echo n$i; done') <(sh -c 'for i in 1 2 3; do echo n$i; done') >/dev/null; ck $? sh-loop
[ "$($BOX ash -c 'echo via-ash')" = "via-ash" ]; ck $? ash-name
# gzip roundtrip: box compress -> system decompress, hash compare
$BOX gzip -n -c t.txt > t.txt.gz && gzip -dc t.txt.gz | cmp - t.txt; ck $? gzip-roundtrip
$BOX zcat t.txt.gz | cmp - t.txt; ck $? zcat
cp t.txt.gz g2.gz && $BOX gunzip -f g2.gz && cmp g2 t.txt; ck $? gunzip
# bzip2 roundtrip
$BOX bzip2 -c t.txt > t.txt.bz2 && bzip2 -dc t.txt.bz2 | cmp - t.txt; ck $? bzip2-roundtrip
$BOX bzcat t.txt.bz2 | cmp - t.txt; ck $? bzcat
cp t.txt.bz2 b2.bz2 && $BOX bunzip2 -f b2.bz2 && cmp b2 t.txt; ck $? bunzip2
# v1 customs replaced by GNU: --help must now succeed with GNU text
$BOX true --help >/dev/null 2>&1; [ $? -eq 0 ]; ck $? true-gnu-help
$BOX false; [ $? -eq 1 ]; ck $? false-rc
$BOX true; [ $? -eq 0 ]; ck $? true-rc
$BOX yes | head -2 | grep -qx y; ck $? yes-gnu
# dispatch errors
$BOX nosuchapplet 2>/dev/null; [ $? -eq 127 ]; ck $? unknown-127
# ---- symlink farm ----
rm -rf farm && $BOX --install farm > install.out 2>&1; ck $? install-rc
n=$(find farm -type l | wc -l); [ "$n" -eq 122 ]; ck $? farm-count-122
export PATH=$PWD/farm:/usr/bin:/bin
diff <(ls --version | head -1) <(echo 'ls (GNU coreutils) 9.12') >/dev/null; ck $? farm-ls
[ "$(wc -l < t.txt)" = "2" ]; ck $? farm-wc
farm/\[ 3 -gt 2 ]; ck $? farm-lbracket
diff <(sha256sum t.txt) <(/usr/bin/sha256sum t.txt) >/dev/null; ck $? farm-sha
echo "SMOKE-C: pass=$pass fail=$fail"
[ -n "$failed" ] && echo "FAILED:$failed"
echo SMOKE-C-DONE
