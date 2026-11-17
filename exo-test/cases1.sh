# Part 1 cases: A-class + simple B-class. Sourced after driver.sh.

# --- browse (rc 0, then stub rc 5 propagation) ---
fresh browse; : > $C/stdin
run_orig browse.sh; run_port browse
verdict browse
fresh browse-rc5; : > $C/stdin; CASEENV=(STUBRC_avahi_browse=5)
run_orig browse.sh; run_port browse
verdict browse-rc5
CASEENV=()

# --- cluster-status ---
fresh cluster-status; : > $C/stdin
run_orig cluster-status.sh; run_port cluster-status
verdict cluster-status

# --- rpush: usage, 2-arg, 3-arg ---
fresh rpush-usage; : > $C/stdin
run_orig rpush.sh; run_port rpush
verdict rpush-usage
fresh rpush; : > $C/stdin
run_orig rpush.sh /data/dir node-x; run_port rpush /data/dir node-x
verdict rpush
fresh rpush-acct; : > $C/stdin
run_orig rpush.sh /data/dir node-x otheracct; run_port rpush /data/dir node-x otheracct
verdict rpush-acct

# --- rag: no-arg usage; with args (real /opt/exopy/bin/python, missing rag.py) ---
fresh rag-usage; : > $C/stdin
run_orig rag.sh; run_port rag
verdict rag-usage
fresh rag; : > $C/stdin
run_orig rag.sh "what is phext"; run_port rag "what is phext"
verdict rag

# --- hwe-enable, lemonade ---
fresh hwe-enable; : > $C/stdin
run_orig hwe-enable.sh; run_port hwe-enable
verdict hwe-enable
fresh lemonade; : > $C/stdin
run_orig lemonade.sh; run_port lemonade
verdict lemonade

# --- maintenance ---
fresh maintenance; : > $C/stdin
run_orig maintenance.sh; run_port maintenance
verdict maintenance

# --- tb-auth (port in production-transcript mode: ops go through sudo stubs) ---
fresh tb-auth; : > $C/stdin; PORTENV=(); PORT_NOROOT=1
run_orig tb-auth.sh; run_port tb-auth
verdict tb-auth
PORT_NOROOT=0

# --- wifi-optimizations (port rooted: file effect + tee echo) ---
fresh wifi-optimizations; : > $C/stdin
mkdir -p $C/port/root/etc; : > $C/port/root/etc/sysctl.conf
run_orig wifi-optimizations.sh; run_port wifi-optimizations
verdict wifi-optimizations
echo "--- wifi port sysctl.conf:" >> $T/RESULTS.txt; cat $C/port/root/etc/sysctl.conf >> $T/RESULTS.txt
# wifi in production mode (no sysroot): port must issue the same sudo tee ops
fresh wifi-optimizations-prod; : > $C/stdin; PORT_NOROOT=1
run_orig wifi-optimizations.sh; run_port wifi-optimizations
verdict wifi-optimizations-prod
PORT_NOROOT=0

# --- enable-npm-proxy: usage; missing CA (real path absent on node; port root lacks it) ---
fresh npm-proxy-usage; : > $C/stdin
run_orig enable-npm-proxy.sh; run_port enable-npm-proxy
verdict npm-proxy-usage
fresh npm-proxy-noca; : > $C/stdin
run_orig enable-npm-proxy.sh 192.168.86.50; run_port enable-npm-proxy 192.168.86.50
verdict npm-proxy-noca
fresh npm-proxy; : > $C/stdin
mkdir -p $C/port/root/usr/local/share/ca-certificates
touch $C/port/root/usr/local/share/ca-certificates/ranch-choir-squid-ca.crt
run_port enable-npm-proxy 192.168.86.50
echo "=== npm-proxy (port-only full flow, CA seeded in root) ===" >> $T/RESULTS.txt
cat $C/port.out >> $T/RESULTS.txt; echo "rc=$(cat $C/port.rc)" >> $T/RESULTS.txt
ops_port $C/port.log >> $T/RESULTS.txt

# --- install-keys: fresh HOME, then idempotent second pass on the port side state ---
fresh install-keys; : > $C/stdin
run_orig install-keys.sh
cp -r $C/orig/home $C/orig/home1
run_port install-keys
verdict install-keys
if diff -r $C/orig/home $C/port/home > $C/home.diff; then echo "install-keys home trees: IDENTICAL" >> $T/RESULTS.txt; else echo "install-keys home trees: DIFF" >> $T/RESULTS.txt; cat $C/home.diff >> $T/RESULTS.txt; fi
stat -c '%a %n' $C/port/home/.ssh $C/port/home/.ssh/authorized_keys >> $T/RESULTS.txt 2>/dev/null
# second run over the applet-produced state (counts must be 0 added, 12 skipped)
( cd $C/port/work && env -i $BASEENV HOME=$C/port/home PATH=/usr/bin:/bin $BIN install-keys > $C/port2.out 2>&1; echo $? > $C/port2.rc )
echo "install-keys port run2: rc=$(cat $C/port2.rc) out=$(cat $C/port2.out)" >> $T/RESULTS.txt
( cd $C/orig/work && env -i $BASEENV HOME=$C/orig/home PATH=/usr/bin:/bin bash $ORIG/install-keys.sh > $C/orig2.out 2>&1; echo $? > $C/orig2.rc )
echo "install-keys orig run2: rc=$(cat $C/orig2.rc) out=$(cat $C/orig2.out)" >> $T/RESULTS.txt

# --- hello-phext: seeded fake server binary in both HOMEs ---
fresh hello-phext; : > $C/stdin
for h in orig port; do
  mkdir -p $C/$h/home/.cargo/bin
  printf '#!/bin/sh\necho "FAKE-SERVER addr=$ROCKET_ADDRESS args=$*"\n' > $C/$h/home/.cargo/bin/hello-phext
  chmod +x $C/$h/home/.cargo/bin/hello-phext
done
run_orig hello-phext.sh extra args; run_port hello-phext extra args
verdict hello-phext

# --- enable-virtual-memory: guard branch (port root seeded with done file) ---
fresh enable-virtual-memory-guard; : > $C/stdin
mkdir -p $C/port/root/etc; touch $C/port/root/etc/exocortical-virtual-memory.done
run_orig enable-virtual-memory.sh; run_port enable-virtual-memory
verdict enable-virtual-memory-guard

# --- nvme-expand-disk: guard branch (done file exists on the node; port root seeded) ---
fresh nvme-guard; : > $C/stdin
mkdir -p $C/port/root/etc; touch $C/port/root/etc/exocortical-nvme.done
run_orig nvme-expand-disk.sh; run_port nvme-expand-disk
verdict nvme-guard

# --- fast-go-setup: seeded bashrc variants + absent /usr/local/go on node ---
fresh fast-go; : > $C/stdin
for h in orig port; do printf '# bashrc\nexport PATH="$PATH:/usr/local/go/bin"\n' > $C/$h/home/.bashrc; done
run_orig fast-go-setup.sh; run_port fast-go-setup
verdict fast-go
fresh fast-go-warn; : > $C/stdin
for h in orig port; do printf '# empty bashrc\n' > $C/$h/home/.bashrc; mkdir -p $C/$h/home/go/bin; done
run_orig fast-go-setup.sh; run_port fast-go-setup
verdict fast-go-warn

echo "PART1 DONE" >> $T/RESULTS.txt
