#!/bin/bash
# phccbox v1.1 smoke B: functional byte-compare, box vs gcc reference 9.12.
# Each case runs the same script with $P = program prefix in twin scratch
# dirs; stdout/stderr/rc and a file manifest (paths + modes + sha256) are
# compared after normalizing the scratch path and the program prefix.
W=$HOME/phccbox-v11
BOX=$W/phccbox
REF=$W/ref/src/src
export LC_ALL=C TZ=UTC
exec > $W/out/smoke-b.log 2>&1
pass=0; fail=0; failed=""
manifest() { ( cd "$1" && find . -printf '%y %m %p -> %l\n' | sort && find . -type f -exec sha256sum {} + | sed 's| \./|  |' | sort -k2 ); }
runcase() { # name script
  local name=$1 script=$2
  local da=$W/smoke/dirA db=$W/smoke/dirB
  rm -rf "$da" "$db"; mkdir -p "$da" "$db"
  touch -d '2026-01-01 00:00:00 UTC' "$da" "$db"
  script=${script/$'\n';/$'\n'}
  ( cd "$da" && P="$REF" D="$da" eval "$script" ) > /tmp/ra.out 2> /tmp/ra.err; local rrc=$?
  ( cd "$db" && P="$BOX" D="$db" eval "$script" ) > /tmp/rb.out 2> /tmp/rb.err; local brc=$?
  sed -i "s|$da|DIR|g; s|$REF/||g" /tmp/ra.out /tmp/ra.err
  sed -i "s|$db|DIR|g; s|$BOX|PROG|g" /tmp/rb.out /tmp/rb.err
  sed -i -E '/: [0-9]+ Killed/d' /tmp/ra.err /tmp/rb.err
  manifest "$da" > /tmp/ma.txt; manifest "$db" > /tmp/mb.txt
  # ref invokes per-program binaries; box invokes "phccbox <name>".
  # Program-name prefixes in stderr (e.g. "ls: ...") match because the ref
  # binaries are invoked by bare name via $P/<name> below; $P itself is
  # normalized above.
  if [ $brc -eq $rrc ] && cmp -s /tmp/ra.out /tmp/rb.out && cmp -s /tmp/ra.err /tmp/rb.err && cmp -s /tmp/ma.txt /tmp/mb.txt; then
    pass=$((pass+1)); echo "PASS $name"
  else
    fail=$((fail+1)); failed="$failed $name"
    echo "FAIL $name: boxrc=$brc refrc=$rrc"
    diff /tmp/ra.out /tmp/rb.out | head -4 | sed 's/^/  out: /'
    diff /tmp/ra.err /tmp/rb.err | head -4 | sed 's/^/  err: /'
    diff /tmp/ma.txt /tmp/mb.txt | head -4 | sed 's/^/  man: /'
  fi
}
# $P is a directory for ref (binaries $P/<name>) but the box binary itself.
# Unify: define p() in each eval context.
PRE='
p() { local n=$1; shift; if [ -d "$P" ]; then local bf=$n; [ "$n" = install ] && bf=ginstall; ( exec -a "$n" "$P/$bf" "$@" ); else "$P" "$n" "$@"; fi; }
printf "alpha\nbeta\ngamma\ndelta\n" > words.txt
printf "x:10\ny:20\nz:30\n" > pairs.txt
seq 1 20 > nums.txt
cp words.txt fixed.txt
'

runcase cat        "$PRE; p cat words.txt; p cat < nums.txt | tail -2"
runcase head-tail  "$PRE; p head -3 nums.txt; p tail -4 nums.txt; p tail -n +18 nums.txt"
runcase wc         "$PRE; p wc words.txt nums.txt; p wc -l -w -c words.txt"
runcase sort       "$PRE; p sort -r words.txt; p sort -t: -k2 -n pairs.txt"
runcase uniq       "$PRE; printf 'a\na\nb\nb\nb\nc\n' | p sort | p uniq -c; printf 'a\na\nb\n' | p uniq -d"
runcase cut        "$PRE; p cut -d: -f2 pairs.txt; p cut -c1-2 words.txt"
runcase tr         "$PRE; p tr a-z A-Z < words.txt; p tr -d a < words.txt"
runcase comm-join  "$PRE; printf 'a\nc\n' > s1; printf 'a\nb\n' > s2; p comm s1 s2; p join pairs.txt pairs.txt"
runcase tsort      "$PRE; printf 'a b\nb c\na c\n' | p tsort"
runcase paste      "$PRE; p paste words.txt pairs.txt; p paste -d: words.txt words.txt"
runcase split      "$PRE; p split -l 6 nums.txt part-; ls part-*; cat part-ab"
runcase csplit     "$PRE; p csplit -f chunk- nums.txt 11 2>/dev/null; ls chunk-*"
runcase fmt-fold   "$PRE; p fmt -w 16 words.txt; p fold -w 3 words.txt | head -4"
runcase expand     "$PRE; printf 'a\tb\n' | p expand; printf 'a  b\n' | p unexpand -a"
runcase nl         "$PRE; p nl words.txt"
runcase od         "$PRE; p od -c words.txt | head -3; p od -An -tx1 pairs.txt"
runcase cksum      "$PRE; p cksum words.txt nums.txt; p cksum -a sha256 words.txt; p sum words.txt"
runcase hashes     "$PRE; for h in md5sum sha1sum sha224sum sha256sum sha384sum sha512sum b2sum; do p \$h words.txt; done; p md5sum words.txt > h.txt; p md5sum -c h.txt"
runcase base-code  "$PRE; p base64 words.txt; p base64 words.txt | p base64 -d; p base32 nums.txt | head -2; p basenc --base16 words.txt"
runcase seq        "$PRE; p seq 5; p seq -w 3 9; p seq 1 0.5 2; p seq -s, 4"
runcase expr       "$PRE; p expr 2 + 3; p expr length alphabet; p expr match abcdef abc; p expr 10 % 3"
runcase factor     "$PRE; p factor 1234567891011 97 360; p factor 18446744073709551616 2>/dev/null || true"
runcase numfmt     "$PRE; p numfmt --to=si 1234567; p numfmt --to=iec 1048576; printf '1K\n2M\n' | p numfmt --from=si"
runcase date       "$PRE; p date -d @1234567890 +%Y-%m-%dT%H:%M:%S; p date -d '2026-11-07 12:00:00 UTC' +%s; p date -u -d @0 +%a"
runcase printf     "$PRE; p printf '%05.2f|%x|%s|%c\n' 3.14159 255 hi Z; p printf '%e %g\n' 12345.678 0.00001234"
runcase echo-yes   "$PRE; p echo -e 'a\tb'; p echo -n x; echo; p yes custom | head -3"
runcase test       "$PRE; p test -f words.txt; echo rc1=\$?; p test 5 -gt 3; echo rc2=\$?; p [ a = a ]; echo rc3=\$?; p [ -z '' ]; echo rc4=\$?; p test abc != abd; echo rc5=\$?"
runcase pathnames  "$PRE; p basename /a/b/c.txt .txt; p dirname /a/b/c.txt; ln -s words.txt wlink; p readlink wlink; p realpath -m /a/../b; p pathchk /ok/path; p pathchk -p '' ; echo rc=\$?"
runcase ls         "$PRE; mkdir -p tree/sub; touch -d '2026-01-02 03:04:05 UTC' tree/a.txt tree/sub/b.txt tree tree/sub; p ls -la --time-style=+%s tree; p dir tree; p vdir tree; p ls -R tree"
runcase cp-mv-ln   "$PRE; p cp words.txt copy.txt; p mv copy.txt moved.txt; p ln moved.txt hard.txt; p ln -s moved.txt soft.txt; p link moved.txt link2.txt; p unlink link2.txt; p ls -1; p stat -c '%h %n' hard.txt"
runcase mkdir-rm   "$PRE; p mkdir -p d1/d2; p touch d1/d2/f; p rm -r d1; p mkdir gone; p rmdir gone; p ls -1a"
runcase touch-stat "$PRE; p touch -d '2026-03-04 05:06:07 UTC' st.txt; p stat --format='%n:%s:%a:%F' st.txt words.txt; p truncate -s 10 st.txt; p stat -c %s st.txt"
runcase install    "$PRE; p install -m 750 words.txt inst.bin; p install -d newdir/sub; p ls -1; p stat -c '%a %n' inst.bin newdir"
runcase mktemp     "$PRE; f=\$(p mktemp tmp.XXXXXX); case \$f in tmp.??????) echo pattern-ok;; *) echo pattern-BAD;; esac; test -f \$f && echo exists; mv \$f tmp.fixed; d=\$(p mktemp -d td.XXXX); test -d \$d && echo direxists; mv \$d td.fixed"
runcase mkfifo-nod "$PRE; p mkfifo pipe1; p stat -c '%n %F' pipe1; p mknod nod1 c 1 3 2>&1; echo nodrc=\$?"
runcase chmod-grp  "$PRE; p chmod 640 words.txt; p chgrp \$(id -gn) words.txt 2>&1; echo grc=\$?; p chown \$(id -un) words.txt 2>&1; echo orc=\$?; p stat -c '%a %U %G' words.txt"
runcase df-du      "$PRE; mkdir -p dud; printf 'abc' > dud/f1; head -c 5000 /dev/zero > dud/f2; p du -sb dud; p du --apparent-size -sb dud; p df --output=target,fstype ."
runcase dd         "$PRE; seq 1 100 > dd-in; p dd if=dd-in of=dd-out bs=10 count=3 2>&1 | grep -v 'bytes copied'; cat dd-out"
runcase tee-env    "$PRE; echo hello | p tee t1.txt t2.txt; FOO=bar p printenv FOO; p env -i A=1 B=2 /usr/bin/printenv A B"
runcase identity   "$PRE; p id -u; p id -un; p whoami; p groups; p logname 2>&1; echo lrc=\$?; p hostname; p arch; p uname -s -m; p hostid; p tty < /dev/null; echo trc=\$?"
runcase nproc      "$PRE; p nproc; p nproc --all"
runcase who-uptime "$PRE; p who 2>&1; echo wrc=\$?; p users 2>&1; echo urc=\$?; p uptime -p; p uptime -s"
runcase timeout    "$PRE; p timeout 5 sleep 0.1; echo rc1=\$?; p timeout 0.2 sleep 5; echo rc2=\$?; p nice -n 3 true; echo rc3=\$?; p timeout --signal=KILL 0.2 sleep 5; echo rc4=\$?"
runcase stty       "$PRE; p stty size < /dev/null 2>&1; echo src=\$?; p stty -a < /dev/null 2>&1 | head -1; echo src2=\$?"
runcase dircolors  "$PRE; TERM=xterm p dircolors -b; p dircolors --print-database | head -5; p dircolors --print-database | p md5sum"
runcase stdbuf     "$PRE; p stdbuf -o0 /usr/bin/true; echo rc1=\$?; p stdbuf -oL /usr/bin/echo hi 2>&1; echo rc2=\$?"
runcase chroot     "$PRE; p chroot /nonexistent-dir true 2>&1; echo crc=\$?"
runcase selinux    "$PRE; p runcon echo hi 2>&1; echo rrc=\$?; p chcon -t tmp_t words.txt 2>&1; echo crc=\$?"
runcase shred      "$PRE; cp words.txt sh.txt; p shred -n 0 -z sh.txt; p od -An -tx1 sh.txt | head -2"
runcase pr-ptx     "$PRE; p pr -t -l 10 words.txt; p ptx words.txt | head -4"
runcase kill       "$PRE; p kill -l | tr '\n' ' '; echo; sleep 30 & SLP=\$!; p kill -TERM \$SLP; sleep 0.3; kill -0 \$SLP 2>/dev/null; echo alive_rc=\$?"
runcase pwd        "$PRE; mkdir -p deep/deeper; cd deep/deeper; p pwd; p pwd -L"
runcase sleep      "$PRE; p sleep 0.05; echo rc=\$?; p sleep 0; echo rc0=\$?"
runcase sync       "$PRE; p sync; echo rc=\$?"
echo "SMOKE-B: pass=$pass fail=$fail"
[ -n "$failed" ] && echo "FAILED:$failed"
echo SMOKE-B-DONE
