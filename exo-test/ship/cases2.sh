# Part 2 cases: complex B-class, D-class, JS/Python ports. Sourced after driver.sh.
say2() { echo "$@" >> $T/RESULTS.txt; }

# --- enable-zram ---
fresh enable-zram; : > $C/stdin
mkdir -p $C/port/root/sys/block/zram0
run_orig enable-zram.sh; run_port enable-zram
verdict enable-zram
echo "port disksize content: $(cat $C/port/root/sys/block/zram0/disksize 2>/dev/null)" >> $T/RESULTS.txt

# --- enable-virtual-memory: port full flow (rooted) ---
fresh enable-virtual-memory-full; : > $C/stdin
mkdir -p $C/port/root/etc; : > $C/port/root/etc/fstab
run_port enable-virtual-memory
echo "=== enable-virtual-memory-full (port only; node has the real done file) ===" >> $T/RESULTS.txt
cat $C/port.out >> $T/RESULTS.txt; echo "rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
ops_port $C/port.log >> $T/RESULTS.txt
echo "--- fstab after:" >> $T/RESULTS.txt; cat $C/port/root/etc/fstab >> $T/RESULTS.txt
echo "--- done file: $(cat $C/port/root/etc/exocortical-virtual-memory.done 2>/dev/null)" >> $T/RESULTS.txt

# --- sync-ollama-models ---
fresh sync-ollama-models; : > $C/stdin
run_orig sync-ollama-models.sh node-b; run_port sync-ollama-models node-b
verdict sync-ollama-models
fresh sync-ollama-models-noarg; : > $C/stdin
run_orig sync-ollama-models.sh; run_port sync-ollama-models
echo "=== sync-ollama-models-noarg === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
echo "orig err: $(cat $C/orig.err)" >> $T/RESULTS.txt
echo "port err: $(cat $C/port.err)" >> $T/RESULTS.txt

# --- usb-ssd-fixup: port rooted full flow + file bodies; orig stub-safe run ---
fresh usb-ssd-fixup; : > $C/stdin
printf '/dev/sda1 / ext4 defaults 0 1\n/dev/sda2 /boot ext4 defaults 0 2\nother line\n' > $C/port_fstab_seed
mkdir -p $C/port/root/etc/sysctl.d; cp $C/port_fstab_seed $C/port/root/etc/fstab
run_orig usb-ssd-fixup.sh; run_port usb-ssd-fixup
echo "=== usb-ssd-fixup === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff && echo "stdout: IDENTICAL" >> $T/RESULTS.txt || { echo "stdout: DIFF" >> $T/RESULTS.txt; cat $C/out.diff >> $T/RESULTS.txt; }
echo "--- port sysctl conf:" >> $T/RESULTS.txt; cat $C/port/root/etc/sysctl.d/99-minimize-disk-io.conf 2>/dev/null >> $T/RESULTS.txt
echo "--- port journald conf:" >> $T/RESULTS.txt; cat $C/port/root/etc/systemd/journald.conf.d/volatile.conf 2>/dev/null >> $T/RESULTS.txt
cp $C/port_fstab_seed $C/fstab_sed_check
sed -i 's|/ ext4 defaults 0|/ ext4 defaults,noatime 0|' $C/fstab_sed_check
sed -i 's|/boot ext4 defaults 0|/boot ext4 defaults,noatime 0|' $C/fstab_sed_check
diff $C/fstab_sed_check $C/port/root/etc/fstab > $C/fstab.diff && echo "fstab edit: MATCHES real sed" >> $T/RESULTS.txt || { echo "fstab edit: DIFF" >> $T/RESULTS.txt; cat $C/fstab.diff >> $T/RESULTS.txt; }

# --- provision-squid-client: orig prefix + port full ---
fresh provision-squid-client; : > $C/stdin
mkdir -p $C/port/root/etc/systemd/system $C/port/root/etc/profile.d $C/port/root/etc/pip $C/port/root/usr/local/share/ca-certificates $C/port/root/etc/apt/apt.conf.d
printf 'PATH=/usr/bin\nhttp_proxy=http://old:1\nKEEP=1\nHTTP_PROXY=http://old:2\n' > $C/port/root/etc/environment
run_orig provision-squid-client.sh 192.168.86.50; run_port provision-squid-client 192.168.86.50
echo "=== provision-squid-client === orig rc=$(cat $C/orig.rc) (expected: dies at step-2 heredoc EACCES) port rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
echo "--- orig stdout:" >> $T/RESULTS.txt; cat $C/orig.out >> $T/RESULTS.txt
echo "--- orig ops:" >> $T/RESULTS.txt; cat $C/orig.log >> $T/RESULTS.txt
echo "--- port stdout:" >> $T/RESULTS.txt; cat $C/port.out >> $T/RESULTS.txt
echo "--- port ops:" >> $T/RESULTS.txt; ops_port $C/port.log >> $T/RESULTS.txt
echo "--- port files:" >> $T/RESULTS.txt
for f in etc/apt/apt.conf.d/02ranch-proxy etc/systemd/system/ollama.service.d/proxy.conf etc/profile.d/ranch-proxy.sh etc/pip/pip.conf etc/environment; do echo "### $f"; cat $C/port/root/$f 2>/dev/null; done >> $T/RESULTS.txt
# client verify-fail variant: ollama registry curl fails
fresh provision-squid-client-fail; : > $C/stdin
CASEENV=(CURL_FAIL_PATTERN=registry.ollama.ai CURL_FAIL_RC=22)
run_port provision-squid-client 192.168.86.50
echo "=== provision-squid-client-fail (port) === rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
tail -8 $C/port.out >> $T/RESULTS.txt
CASEENV=()

# --- provision-squid-server: orig prefix (apache2 quirk) + port to certgen wall ---
fresh provision-squid-server; : > $C/stdin
run_orig provision-squid-server.sh; run_port provision-squid-server
echo "=== provision-squid-server === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
echo "--- orig stdout:" >> $T/RESULTS.txt; cat $C/orig.out >> $T/RESULTS.txt
echo "--- orig ops:" >> $T/RESULTS.txt; cat $C/orig.log >> $T/RESULTS.txt
echo "--- port stdout:" >> $T/RESULTS.txt; cat $C/port.out >> $T/RESULTS.txt
echo "--- port ops:" >> $T/RESULTS.txt; ops_port $C/port.log >> $T/RESULTS.txt
ls $C/port/root/etc/squid/ssl/ $C/port/root/var/www/html/ 2>/dev/null >> $T/RESULTS.txt

# --- squid-uninstall-client: full A/B both ollama states ---
fresh squid-uninstall; : > $C/stdin; PORT_NOROOT=1; CASEENV=(OLLAMA_ACTIVE_RC=0)
run_orig squid-uninstall-client.sh; run_port squid-uninstall-client
verdict squid-uninstall
fresh squid-uninstall-idle; : > $C/stdin; PORT_NOROOT=1; CASEENV=(OLLAMA_ACTIVE_RC=1)
run_orig squid-uninstall-client.sh; run_port squid-uninstall-client
verdict squid-uninstall-idle
PORT_NOROOT=0; CASEENV=()

# --- squid-cache-monitor: unseeded A/B shape + seeded port ---
fresh squid-cache-monitor; : > $C/stdin
# original under a timeout guard: its final tail -f would follow a real
# access.log forever if one existed (none does on this node; belt and braces)
( cd $C/orig/work && env -i $BASEENV HOME=$C/orig/home PATH=$STUBBIN:/usr/bin:/bin \
    STUBLOG=$C/orig.log STUBROOT=$C/orig/root timeout 5 bash $ORIG/squid-cache-monitor.sh < $C/stdin > $C/orig.out 2> $C/orig.err; echo $? > $C/orig.rc )
( cd $C/port/work && env -i $BASEENV HOME=$C/port/home PATH=$STUBBIN:/usr/bin:/bin \
    EXO_SYSROOT=$C/port/root EXO_TRANSCRIPT=$C/port.log timeout 3 $BIN squid-cache-monitor < $C/stdin > $C/port.out 2>$C/port.err; echo $? > $C/port.rc )
echo "=== squid-cache-monitor (unseeded) === orig rc recorded in file, port under timeout" >> $T/RESULTS.txt
diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff && echo "stdout: IDENTICAL" >> $T/RESULTS.txt || { echo "stdout: DIFF" >> $T/RESULTS.txt; cat $C/out.diff >> $T/RESULTS.txt; }
fresh squid-cache-monitor-seeded; : > $C/stdin
mkdir -p $C/port/root/var/log/squid $C/port/root/var/spool/squid
printf '1733500000.000 RELEASE 0001 200 1733400000 1733600000 1733700000 text/html 209715200 209715200 - - GET http://x/y\n1733500001.000 RELEASE 0002 200 1733400000 1733600000 1733700000 text/html 5242880 5242880 - - GET http://x/z\n' > $C/port/root/var/log/squid/store.log
printf '1733500002.000 12 192.168.86.5 TCP_HIT/200 500 GET http://a - HIER_NONE/- text/html\n1733500003.000 30 192.168.86.6 TCP_MISS/200 900 GET http://b - HIER_DIRECT/1.2.3.4 text/html\n' > $C/port/root/var/log/squid/access.log
( cd $C/port/work && env -i $BASEENV HOME=$C/port/home PATH=$STUBBIN:/usr/bin:/bin \
    EXO_SYSROOT=$C/port/root timeout 2 $BIN squid-cache-monitor < $C/stdin > $C/port.out 2>$C/port.err; echo $? > $C/port.rc )
echo "=== squid-cache-monitor-seeded (port) === rc=$(cat $C/port.rc) (124=timeout in follow, expected)" >> $T/RESULTS.txt
cat $C/port.out >> $T/RESULTS.txt
echo "--- ANSI check:" >> $T/RESULTS.txt; grep -c $'\033' $C/port.out >> $T/RESULTS.txt

# --- enable-browser-sandbox: refusal A/B ---
fresh enable-browser-sandbox; : > $C/stdin; PORT_NOROOT=1
run_orig enable-browser-sandbox.sh; run_port enable-browser-sandbox
echo "=== enable-browser-sandbox (non-root refusal) === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
echo "orig out: $(cat $C/orig.out)" >> $T/RESULTS.txt
echo "port out: $(cat $C/port.out)" >> $T/RESULTS.txt
PORT_NOROOT=0
# port full flow in DRYRUN (transcript only)
fresh enable-browser-sandbox-dry; : > $C/stdin
( cd $C/port/work && env -i $BASEENV HOME=$C/port/home PATH=$STUBBIN:/usr/bin:/bin EXO_DRYRUN=1 EXO_TRANSCRIPT=$C/port.log $BIN enable-browser-sandbox < $C/stdin > $C/port.out 2>$C/port.err; echo $? > $C/port.rc )
echo "=== enable-browser-sandbox-dry (port, non-root: still refuses, getuid gate is mode-independent) === rc=$(cat $C/port.rc) out=$(cat $C/port.out)" >> $T/RESULTS.txt

# --- squid-cache-monitor: production-mode A/B (read-only applet; both read
# the node's real /var state, timeout-guarded against the follow) ---
fresh squid-cache-monitor-prod; : > $C/stdin
( cd $C/orig/work && env -i $BASEENV HOME=$C/orig/home PATH=$STUBBIN:/usr/bin:/bin \
    STUBLOG=$C/orig.log timeout 5 bash $ORIG/squid-cache-monitor.sh < $C/stdin > $C/orig.out 2> $C/orig.err; echo $? > $C/orig.rc )
( cd $C/port/work && env -i $BASEENV HOME=$C/port/home PATH=$STUBBIN:/usr/bin:/bin \
    EXO_TRANSCRIPT=$C/port.log timeout 5 $BIN squid-cache-monitor < $C/stdin > $C/port.out 2>$C/port.err; echo $? > $C/port.rc )
echo "=== squid-cache-monitor-prod === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff && echo "stdout: IDENTICAL" >> $T/RESULTS.txt || { echo "stdout: DIFF" >> $T/RESULTS.txt; cat $C/out.diff >> $T/RESULTS.txt; }

echo "PART2A DONE" >> $T/RESULTS.txt
