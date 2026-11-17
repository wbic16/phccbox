# Part 3 cases: NFS pair, tb-setup, setup family, tailscale, optimize,
# exo-agentmail / exo-console / ports. Sourced after driver.sh.
# NOTE: driver.sh resets RESULTS.txt; parts are run in separate invocations
# and this file appends its own results to RESULTS3.txt via R3.
R3=$T/RESULTS.txt

# --- nfs-provision-client: orig prefix (dies at fstab cp) + port full + quirk run ---
fresh nfs-provision-client; : > $C/stdin
mkdir -p $C/port/root/etc $C/port/root/mnt/mirrorborn
printf '# fstab\nUUID=x / ext4 defaults 0 1\n' > $C/port/root/etc/fstab
run_orig nfs-provision-client.sh; run_port nfs-provision-client
echo "=== nfs-provision-client === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $R3
echo "--- orig stdout:" >> $R3; cat $C/orig.out >> $R3
echo "--- orig ops:" >> $R3; cat $C/orig.log >> $R3
echo "--- port stdout:" >> $R3; cat $C/port.out >> $R3
echo "--- port ops:" >> $R3; ops_port $C/port.log >> $R3
echo "--- port fstab:" >> $R3; cat $C/port/root/etc/fstab >> $R3
ls $C/port/root/etc/fstab.bak.* >> $R3 2>/dev/null
# quirk run: every mountpoint reports mounted -> ((MOUNTED++)) set -e death in the original
fresh nfs-provision-client-quirk; : > $C/stdin
mkdir -p $C/port/root/etc; printf '# fstab\norchid-flow:/srv/mirrorborn marker\n' > $C/port/root/etc/fstab
CASEENV=(MOUNTPOINT_RC=0)
run_port nfs-provision-client
echo "=== nfs-provision-client-quirk (port) === rc=$(cat $C/port.rc) (expect 1: set -e death after first check)" >> $R3
cat $C/port.out >> $R3
CASEENV=()

# --- nfs-provision-server: orig prefix + port full ---
fresh nfs-provision-server; : > $C/stdin
mkdir -p $C/port/root/etc; printf '# exports seed\n' > $C/port/root/etc/exports
run_orig nfs-provision-server.sh; run_port nfs-provision-server
echo "=== nfs-provision-server === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $R3
echo "--- orig stdout:" >> $R3; cat $C/orig.out >> $R3
echo "--- orig ops:" >> $R3; cat $C/orig.log >> $R3
echo "--- port stdout:" >> $R3; cat $C/port.out >> $R3
echo "--- port ops:" >> $R3; ops_port $C/port.log >> $R3
echo "--- port exports:" >> $R3; cat $C/port/root/etc/exports >> $R3

# --- tb-setup: full A/B (hostname stub is in the map) ---
fresh tb-setup; : > $C/stdin
mkdir -p $C/port/root/etc/systemd/system
run_orig tb-setup.sh; run_port tb-setup
echo "=== tb-setup === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $R3
diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff && echo "stdout: IDENTICAL" >> $R3 || { echo "stdout: DIFF" >> $R3; cat $C/out.diff >> $R3; }
diff <(norm $C/orig.log) <(ops_port $C/port.log | norm /dev/stdin) > $C/ops.diff && echo "opstream: IDENTICAL" >> $R3 || { echo "opstream: DIFF" >> $R3; cat $C/ops.diff >> $R3; }
echo "--- port unit file:" >> $R3; cat $C/port/root/etc/systemd/system/tb-fabric.service 2>/dev/null >> $R3

# --- setup-phext-mcp: full A/B (all effects stubbed or failing harmlessly) ---
fresh setup-phext-mcp; : > $C/stdin
run_orig setup-phext-mcp.sh; run_port setup-phext-mcp
echo "=== setup-phext-mcp === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $R3
echo "--- orig stdout:" >> $R3; cat $C/orig.out >> $R3
echo "--- port stdout:" >> $R3; cat $C/port.out >> $R3
echo "--- orig ops:" >> $R3; cat $C/orig.log >> $R3
echo "--- port ops:" >> $R3; ops_port $C/port.log >> $R3
echo "--- orig err:" >> $R3; cat $C/orig.err >> $R3
echo "--- port err:" >> $R3; cat $C/port.err >> $R3

# --- repair-openclaw: full A/B (deb wget stubbed; curl|bash gets stub text) ---
fresh repair-openclaw; : > $C/stdin
run_orig repair-openclaw.sh; run_port repair-openclaw
echo "=== repair-openclaw === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $R3
diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff && echo "stdout: IDENTICAL" >> $R3 || { echo "stdout: DIFF" >> $R3; cat $C/out.diff >> $R3; }
diff <(norm $C/orig.log) <(ops_port $C/port.log | norm /dev/stdin) > $C/ops.diff && echo "opstream: IDENTICAL" >> $R3 || { echo "opstream: DIFF" >> $R3; cat $C/ops.diff >> $R3; }
echo "orig err: $(cat $C/orig.err)" >> $R3; echo "port err: $(cat $C/port.err)" >> $R3

# --- nvidia-setup ---
fresh nvidia-setup; : > $C/stdin
run_orig nvidia-setup.sh; PORT_NOROOT=1 run_port nvidia-setup
verdict nvidia-setup

# --- patch-amd-gpu: production-mode A/B + rooted pin-file bytes ---
fresh patch-amd-gpu; : > $C/stdin; PORT_NOROOT=1
run_orig patch-amd-gpu.sh; run_port patch-amd-gpu
verdict patch-amd-gpu
PORT_NOROOT=0
fresh patch-amd-gpu-rooted; : > $C/stdin
mkdir -p $C/port/root/etc/apt/preferences.d $C/port/root/etc/apt/sources.list.d $C/port/root/etc/apt/keyrings
run_port patch-amd-gpu
echo "--- pin file bytes (od tail):" >> $R3
od -c $C/port/root/etc/apt/preferences.d/rocm-pin-600 2>/dev/null | head -5 >> $R3
echo "--- amdgpu.list: $(cat $C/port/root/etc/apt/sources.list.d/amdgpu.list 2>/dev/null)" >> $R3
echo "--- rocm.list: $(cat $C/port/root/etc/apt/sources.list.d/rocm.list 2>/dev/null)" >> $R3

# --- tailscale: Y and n paths ---
fresh tailscale-y; printf 'Y\n' > $C/stdin
for h in orig port; do printf 'OLD-INSTALLER\n' > $C/$h/work/tailscale_install.sh; printf '# ts\n' > $C/$h/work/tailscale.sh; done
run_orig tailscale.sh; PORT_NOROOT=1 run_port tailscale
verdict tailscale-y
diff $C/orig/work/tailscale_install.sh $C/port/work/tailscale_install.sh > /dev/null && echo "tailscale-y installer file: IDENTICAL" >> $R3 || echo "tailscale-y installer file: DIFF" >> $R3
fresh tailscale-n; printf 'n\n' > $C/stdin
for h in orig port; do printf 'OLD-INSTALLER\n' > $C/$h/work/tailscale_install.sh; printf '# ts\n' > $C/$h/work/tailscale.sh; done
run_orig tailscale.sh; PORT_NOROOT=1 run_port tailscale
verdict tailscale-n

# --- optimize-llm-inference: guard A/B ---
fresh optimize-guard; : > $C/stdin
mkdir -p $C/port/root/etc; touch $C/port/root/etc/exocortical-llm-optimized.done
run_orig optimize-llm-inference.sh; run_port optimize-llm-inference
echo "=== optimize-guard === orig rc=$(cat $C/orig.rc) port rc=$(cat $C/port.rc)" >> $R3
diff <(norm $C/orig.out) <(norm $C/port.out) > $C/out.diff && echo "stdout: IDENTICAL" >> $R3 || { echo "stdout: DIFF" >> $R3; cat $C/out.diff >> $R3; }

# --- optimize-llm-inference: port full flow (rooted) ---
fresh optimize-full; : > $C/stdin
mkdir -p $C/port/root/etc/default $C/port/root/etc/sysctl.d $C/port/root/etc/udev/rules.d $C/port/root/sys/class/drm
printf 'GRUB_DEFAULT=0\nGRUB_CMDLINE_LINUX_DEFAULT="quiet splash"\nGRUB_CMDLINE_LINUX=""\n' > $C/port/root/etc/default/grub
printf '[gpu] placeholder\njournal line: inference compute id=0 name=AMD description="Radeon 890M" total="16.0 GiB"\n' > $C/journal.txt
CASEENV=(JOURNAL_FILE=$C/journal.txt)
PORTENV=()
run_port optimize-llm-inference
echo "=== optimize-full (port) === rc=$(cat $C/port.rc)" >> $R3
cat $C/port.out >> $R3
echo "--- ops:" >> $R3; cat $C/port.log >> $R3
echo "--- grub after:" >> $R3; cat $C/port/root/etc/default/grub >> $R3
echo "--- override conf:" >> $R3; cat $C/port/root/etc/systemd/system/ollama.service.d/exocortical.conf 2>/dev/null >> $R3
echo "--- sysctl conf:" >> $R3; cat $C/port/root/etc/sysctl.d/90-exocortical-llm.conf 2>/dev/null >> $R3
echo "--- udev rule:" >> $R3; cat $C/port/root/etc/udev/rules.d/60-exocortical-iosched.rules 2>/dev/null >> $R3
CASEENV=()

# --- setup: port-only, seeded exopy stubs + gate exit, then full pass ---
fresh setup-gate; : > $C/stdin
mkdir -p $C/port/root/opt/exopy/bin $C/port/home/.ssh $C/port/root/etc/avahi/services
printf '#!/bin/sh\necho "pip3 $*" >> %s\n' "$C/port.log" > $C/port/root/opt/exopy/bin/pip3
printf '#!/bin/sh\necho "pip $*" >> %s\n' "$C/port.log" > $C/port/root/opt/exopy/bin/pip
chmod +x $C/port/root/opt/exopy/bin/pip3 $C/port/root/opt/exopy/bin/pip
printf 'ssh-ed25519 AAAATESTKEY test@host\n' > $C/port/home/.ssh/id_ed25519.pub
printf '# service file\n' > $C/port/work/exocortex.service
printf '#!/bin/sh\necho PATCHLIBUV-RAN\n' > $C/port/work/patch-libuv.sh
chmod +x $C/port/work/patch-libuv.sh
run_port setup
echo "=== setup-gate (port) === rc=$(cat $C/port.rc) (expect 1 at the exo-ready gate)" >> $R3
cat $C/port.out >> $R3
echo "--- transcript:" >> $R3; cat $C/port.log >> $R3

fresh setup-full; : > $C/stdin
mkdir -p $C/port/root/opt/exopy/bin $C/port/root/etc/systemd/resolved.conf.d $C/port/home/.ssh $C/port/root/source $C/port/root/etc/avahi/services
printf '#!/bin/sh\necho "pip3 $*" >> %s\n' "$C/port.log" > $C/port/root/opt/exopy/bin/pip3
printf '#!/bin/sh\necho "pip $*" >> %s\n' "$C/port.log" > $C/port/root/opt/exopy/bin/pip
chmod +x $C/port/root/opt/exopy/bin/pip3 $C/port/root/opt/exopy/bin/pip
touch $C/port/root/etc/exo-ready $C/port/root/etc/exo-uv-ready
printf 'ssh-ed25519 AAAATESTKEY test@host\n' > $C/port/home/.ssh/id_ed25519.pub
printf '# service file\n' > $C/port/work/exocortex.service
run_port setup
echo "=== setup-full (port) === rc=$(cat $C/port.rc)" >> $R3
tail -5 $C/port.out >> $R3
echo "clone count: $(grep -c 'git clone git@github.com:wbic16' $C/port.log)" >> $R3
echo "--- bashrc:" >> $R3; cat $C/port/home/.bashrc 2>/dev/null >> $R3
echo "--- DNS conf:" >> $R3; cat $C/port/root/etc/systemd/resolved.conf.d/fallback.conf 2>/dev/null >> $R3
echo "--- avahi service copied:" >> $R3; ls $C/port/root/etc/avahi/services/ 2>/dev/null >> $R3

# --- ssh-setup / patch-libuv: port-only transcripts ---
fresh ssh-setup-port; : > $C/stdin
run_port ssh-setup
echo "=== ssh-setup (port) === rc=$(cat $C/port.rc)" >> $R3
cat $C/port.log >> $R3
fresh patch-libuv-port; : > $C/stdin
mkdir -p $C/port/root/source
run_port patch-libuv
echo "=== patch-libuv (port) === rc=$(cat $C/port.rc)" >> $R3
cat $C/port.log >> $R3

# --- install: port-only (original would write the node's real /usr/local/exocortical) ---
fresh install-port; : > $C/stdin
cp $ORIG/rag.sh $ORIG/rag.py $C/port/work/
run_port install
echo "=== install (port) === rc=$(cat $C/port.rc)" >> $R3
cat $C/port.log >> $R3
ls $C/port/root/usr/local/exocortical/ 2>/dev/null >> $R3

# --- benchmark: port-only with seeded beebjit markers ---
fresh benchmark-port; : > $C/stdin
mkdir -p $C/port/root/opt/beebjit
printf '#!/bin/sh\necho BUILD-MARKER\n' > $C/port/root/opt/beebjit/build.sh
printf '#!/bin/sh\necho BENCH-MARKER\n' > $C/port/root/opt/beebjit/benchmark.sh
chmod +x $C/port/root/opt/beebjit/build.sh $C/port/root/opt/beebjit/benchmark.sh
run_port benchmark
echo "=== benchmark (port, seeded) === rc=$(cat $C/port.rc) out=$(cat $C/port.out)" >> $R3

echo "PART3 DONE" >> $R3
