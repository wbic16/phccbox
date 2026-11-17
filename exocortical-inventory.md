# Exocortical → phccbox — feature inventory & port record

Task 35.6.8/8.7.12/6.5.9 · Parent task: phccbox (41.4.6/6.5.9/5.4.10)

**Source of truth:** fresh clone of `wbic16/exocortical` on aletheia-core
(`git@github.com:wbic16/exocortical`), commit
**`0872ca2a1b4c882ccff61a71ddcf8af281590f92`** (2026-11-07 17:35 -0600,
"Add enable-browser-sandbox.sh…"). 52 files. The older partial look in
`~/workspace/exocortical-check/` was used as a hint only; it is *stale* —
it lacks `enable-browser-sandbox.sh`, the newest file in the repo.

**What the site map said vs what is there:** "Ubuntu-based reconfiguration
scripts for running an Exocortex node" — accurate for the bulk (37 shell
scripts), but the repo also carries a Python AgentMail client, a
llama-index RAG engine, a complete browser-side nightly-ops console
(exo.js, ~19.7 KB of behaviour), and a chat-portal page whose inline
script builds SQ `/api/v2/select` URLs. All of those are inventoried as
features below, by function, not by extension.

## Safety rule (binding on all porting and verification)

The original scripts reconfigure nodes. **No original and no port was, or
will be, executed against a real node's system** — no zram/swap changes,
no package installs, no service changes, no proxy/firewall/NFS changes on
aletheia-core or anywhere else. Verification used only:

- stub-`PATH` harnesses (every external command replaced by a logging
  stub with canned output) for command-stream comparison;
- fake `HOME` / scratch working directories;
- the ports' `EXO_SYSROOT` prefix (every absolute system path the applet
  touches is redirected under a scratch root) and `EXO_DRYRUN`
  (commands and writes are transcribed, not performed);
- byte comparison of generated file bodies against the heredocs in the
  original sources.

Where a feature could not be exercised even that far, its receipt says
"static" and verification is a behaviour table derived from reading the
original. `unshare -rm` is not available on aletheia-core (uid_map write
denied), so no mount-namespace sandbox was used.

## Port conventions (all applets)

- **Names** are the original command names (`rpush.sh` → applet `rpush`;
  `exo-agentmail.py` → `exo-agentmail`; exo.js's console → `exo-console`;
  ports.html's submitter → `ports`).
- **Native C, not transliteration:** pipelines over files (e.g.
  `lsmod | grep zram`) are implemented by reading the underlying data
  (`/proc/modules`, `/proc/meminfo`) in C; external *tools* the original
  drives (apt, systemctl, rsync, ollama, …) are still driven, with the
  same argv, because driving them is the feature.
- **`EXO_SYSROOT=<dir>`** prefixes every absolute path under `/etc`,
  `/var`, `/usr`, `/opt`, `/srv`, `/mnt`, `/sys`, `/source` (not `/proc`,
  `/dev`, or `$HOME`-relative paths — live kernel data and the caller's
  home are the point of those). **`EXO_DRYRUN=1`** performs no command
  and no write; everything is transcribed. **`EXO_TRANSCRIPT=<file>`**
  appends one line per external command (`RUN …`) and per file effect
  (`WRITE/APPEND/MKDIR/COPY …`) in both modes, so a port's transcript
  can be diffed against a stub-harness log of the original.
- `sudo` is invoked by the ports exactly where the original invokes it
  (inside a stub harness, the `sudo` stub logs and does not execute).

## Classification key

- **(A) utility/computation** — ported directly; behaviour is producing
  output, querying, or delegating to one named tool.
- **(B) system-configuration action** — the port performs the same
  operations through the same interfaces (same files written with the
  same bytes, same commands with the same argv).
- **(C) asset / vendored / config** — not an applet; evidence stated.
- **(D) service / long-running / delegated-computation** — the port
  implements the command surface and states the boundary.

## Feature inventory (40 features → 40 applets)

### (A) Utility / computation — 8

| # | Source | Applet | Purpose (from the source) | Inputs | Outputs / effects | Dependencies |
|---|---|---|---|---|---|---|
| 1 | benchmark.sh | `benchmark` | Run beebjit's build + benchmark if installed | — | If `/opt/beebjit` exists: cd there, run `./build.sh`, `./benchmark.sh` | /opt/beebjit scripts |
| 2 | browse.sh | `browse` | Discover Exocortex nodes on the LAN | — | Runs `avahi-browse _exocortex_exocortical._tcp`, passes through output | avahi-browse |
| 3 | cluster-status.sh | `cluster-status` | One-line reachability for 5 named nodes | — | For logos-prime, halcyon-vector, aletheia-core, chrysalis-hub, aurora-continuum: `ping -c 1`, keeps lines matching `ms` + `bytes from`, drops `packet loss` lines | ping |
| 4 | rag.sh + rag.py | `rag` | Retrieval-augmented query over a source tree with a local LLM | query [dir] [model] | Runs `/opt/exopy/bin/python rag.py <dir> <model> <query>` (stderr discarded); defaults dir `/usr/local/exollama`, model `llama3.2`; usage + exit 1 without a query. **rag.py itself is (D):** embeddings (HF bge-small via llama-index) + Ollama query — delegated ML computation; the applet preserves the exact command surface and invokes the same engine, see boundary note in the port section | python3 + llama-index + ollama |
| 5 | rpush.sh | `rpush` | rsync a directory to another node | dir server [account] | `rsync -varzP <dir> <acct>@<server>:<dir>/`; account defaults `wbic16`; usage + exit 1 if dir/server missing | rsync, ssh |
| 6 | squid-cache-monitor.sh | `squid-cache-monitor` | Squid cache health dashboard + live colourised access-log tail | — | Dashboard from `squidclient mgr:5min/active_requests/mem`, `du` of `/var/spool/squid`, awk over `/var/log/squid/store.log` (>100 MB objects); then follows `/var/log/squid/access.log`, colourising TCP_HIT green, TCP_MEM_HIT bright green, TCP_MISS red, TCP_TUNNEL yellow. The follow phase is long-running (D-like); the port implements it as the same unbounded follow | squidclient, du, tail semantics |
| 7 | exo.js | `exo-console` | "Exocortex 2130 Nightly Ops Console": 15-minute-slot log journal for the evening (18:00–24:00), categories dev/ops/sales, persona tags, per-category distribution, Mon–Sun weekly overview (15 min per log), Tessera day counter (from 2023-03-26), motivational quotes, markdown session export | Browser UI + localStorage `exocortex_data` | The port keeps the identical data model (logs + weeklyLogs), slot math (local time, floor to 15 min), validation messages, Tessera-day formula (ceil of abs diff in days), and the **byte-exact markdown export format** of `generateMarkdownExport`; storage becomes a JSON file (default `~/.exocortex-2130.json`, override `EXO_CONSOLE_STORE`) in the same shape as the localStorage payload. DOM rendering and the success beep are presentation, not behaviour, and are not ported | (browser) |
| 8 | ports.html (inline script) | `ports` | Chat portal: pick a host (6) and a persona (101, numbered 1–101), submit phext/coordinate/message to that persona's SQ endpoint | host, persona #, p (default `index`), c (default `1.1.1/1.1.1/1.1.1`), s (message) | Builds `http://<host>:<11000+n>/api/v2/select?p=…&c=…&s=…` — exactly the form GET the page's `submitter()` issues — and with `--fetch` performs it. The page's static pod lists are the same directory data; the applet's `list` mode prints it | SQ HTTP API (only under `--fetch`) |

### (B) System-configuration actions — 29

| # | Source | Applet | Purpose | Inputs | Outputs / effects (files written, commands run) | Dependencies |
|---|---|---|---|---|---|---|
| 9 | enable-browser-sandbox.sh | `enable-browser-sandbox` | AppArmor profile so Playwright Chrome can use user namespaces on Ubuntu 24.04 | [--undo] | Root-only. Writes `/etc/apparmor.d/playwright-chromium` (generated profile, abi line only if `/etc/apparmor.d/abi/4.0` exists), loads with `apparmor_parser -r`, verifies by launching the newest Playwright Chrome headless **with sandbox on** as `$SUDO_USER`, stamps `/etc/exocortical-browser-sandbox.done`. `--undo`: `apparmor_parser -R`, removes both files. Refuses (exit 1) when the userns restriction is not `1` | apparmor_parser, Chrome |
| 10 | enable-npm-proxy.sh | `enable-npm-proxy` | Point npm at the ranch Squid cache | squid-ip | Requires CA at `/usr/local/share/ca-certificates/ranch-choir-squid-ca.crt` (else error exit 1); `npm config set` proxy (3128) + https-proxy (3129), global and local; cafile; prints both gets; `sudo update-ca-certificates`; `npm view openclaw version` | npm |
| 11 | enable-virtual-memory.sh | `enable-virtual-memory` | Create and enable an 80 GB swapfile, persist in fstab | — | Guard `/etc/exocortical-virtual-memory.done` (if present: message + `top` swap line, exit 0). Else `sudo mkdir -v /var/cache/swap`; in that dir: `dd` 20M×4K zero swapfile (if absent), chmod 600, `mkswap`, `swapon`, `swaplabel`; appends `/var/cache/swap/swapfile none swap sw 0 0` to `/etc/fstab` if absent; stamps done-file | sudo, dd, mkswap, swapon, top |
| 12 | enable-zram.sh | `enable-zram` | (Re)create zram swap sized 1.5× RAM | — | If zram module loaded: `swapoff /dev/zram0`, `rmmod zram`. `modprobe zram`; MemTotal from `/proc/meminfo`; writes bytes (RAM×1.5) to `/sys/block/zram0/disksize`; prints `Allocated <GB> GB`; `mkswap -fq`, `swapon -p 5`, `zramctl` | modprobe, mkswap, swapon, zramctl |
| 13 | fast-go-setup.sh | `fast-go-setup` | Install Go 1.25.6 tarball to /usr/local, advise on PATH | — | `wget` the release if not in cwd; `tar zxvf` in `/usr/local` if no `go` dir; greps `~/.bashrc` for the two PATH lines and prints "Go ready for use." / the PATH warning / the exact export line to add | wget, tar, go |
| 14 | hwe-enable.sh | `hwe-enable` | Install the HWE kernel | — | `sudo apt install linux-generic-hwe-24.04` | apt |
| 15 | install.sh | `install` | Install the rag pair to /usr/local | — | If absent: `sudo mkdir /usr/local/exocortical`, `sudo chown $USER:$USER`; `cp rag.* /usr/local/exocortical` (cwd glob) | sudo, cp |
| 16 | install-keys.sh | `install-keys` | Install all Mirrorborn public keys into `~/.ssh/authorized_keys`, idempotently | — | Creates `~/.ssh` (700) + `authorized_keys` (600); appends each of the 12 embedded keys whose blob (field 2) is absent; prints `install-keys: <a> added, <s> already present (12 total)` | (file I/O only) |
| 17 | lemonade.sh | `lemonade` | Install lemonade-server from its PPA | — | `sudo add-apt-repository ppa:lemonade-team/stable`; `sudo apt install lemonade-server` | apt |
| 18 | maintenance.sh | `maintenance` | Node maintenance pass | — | `openclaw doctor`; `sudo apt update && sudo apt upgrade`; `git status`; `openclaw update` | openclaw, apt, git |
| 19 | nfs-provision-client.sh | `nfs-provision-client` | Mount orchid-flow's NFS shares on a node | — | `apt-get` nfs-common; mkdir `/mnt/mirrorborn/{models,shared,exocortex,mirrorborn,bliss,backups}`; backs up `/etc/fstab` (timestamped), appends 6 aligned entries if absent; `mount -a`; per-share `mountpoint -q` ✓/✗ report; optional interactive Ollama models symlink (prompt, rsync move, `ln -sf`); `sudo systemctl daemon-reload` | apt-get, mount, rsync |
| 20 | nfs-provision-server.sh | `nfs-provision-server` | Export the 4 TB shares from orchid-flow | — | `apt-get` nfs-kernel-server; creates `/srv/mirrorborn/<6 shares>`, chown wbic16, chmod 755; backs up + appends 6 export lines to `/etc/exports` (subnet 192.168.86.0/24, rw,sync,no_subtree_check,no_root_squash); `exportfs -ra`; enable+restart nfs-kernel-server; ufw allows if active; prints `exportfs -v` indented | apt-get, exportfs, systemctl, ufw |
| 21 | nvidia-setup.sh | `nvidia-setup` | CUDA toolkit + NVIDIA driver via vendor repo | — | `wget` cuda-keyring deb; `sudo dpkg -i`; `sudo apt-get update`; install cuda-toolkit-12-6; install nvidia-open, nvidia-driver-565-open, nvidia-kernel-common-565 | wget, dpkg, apt-get |
| 22 | nvme-expand-disk.sh | `nvme-expand-disk` | Grow the root LV to fill the NVMe | — | Once-guard `/etc/exocortical-nvme.done`: `growpart /dev/nvme0n1 3`, `lsblk`, `lvextend -l+100%FREE`, `resize2fs`, stamp. Always: `sudo pvs`, `sudo vgs`, `df -h` | growpart, lvm2, e2fsprogs |
| 23 | optimize-llm-inference.sh | `optimize-llm-inference` | Tune a node for MoE LLM inference (AMD 780M) | env: EXOCORTEX_SHELL (0/1/2), FORCE, PIN_CPU_OVER_GB | RAM tier from MemTotal (≥80000 MB→"96", ≥24000→"32", else "16"). Phases: (1) write `/etc/sysctl.d/90-exocortical-llm.conf` (swappiness 60 etc.) + `sysctl --system`; (2) udev rule pinning NVMe scheduler mq-deadline + immediate apply; (3) GRUB `amdgpu.gttsize` by tier (32768/16384/8192) + `iommu=pt`, `update-grub`; (4) Ollama systemd override (HOST/VULKAN/IGPU/MAX_LOADED_MODELS by tier, NUM_CTX from tier×shell table, FLASH_ATTENTION, KEEP_ALIVE), restore missing Vulkan/ggml libs from the matching Ollama release when an AMD GPU is present, daemon-reload + restart, then verify a non-CPU `inference compute` line in the journal (loud stderr warning otherwise); (5) model pulls by shell (1→nemotron-cascade-2:30b; 2→qwen3.5:122b/27b/9b by tier); (5b) pin models larger than ¾ of GTT to CPU via `ollama create` with `num_gpu 0`; (6) ufw allows 11434 + 1337 from 192.168.0.0/16. Done-guard `/etc/exocortical-llm-optimized.done` with FORCE=1 override | ollama, systemctl, ufw, lspci, journalctl |
| 24 | patch-amd-gpu.sh | `patch-amd-gpu` | ROCm/AMDGPU apt repos + install | — | ROCm GPG key via `apt-key` and dearmored to `/etc/apt/keyrings/rocm.gpg`; writes amdgpu.list + rocm.list (6.3.1 noble) and the pin file — **quirk:** the pin file is written with `echo '…\n…'` (no `-e`), so it literally contains backslash-n sequences; the port reproduces those bytes. Installs rocm-dkms, amdgpu-dkms, rocm | wget, gpg, apt |
| 25 | patch-libuv.sh | `patch-libuv` | Build & install libuv 1.51.0 from source | — | In `/source`: clone libuv if absent, `git pull --depth 100`, checkout v1.51.0, `apt install automake libtool`, `./autogen.sh`, `./configure`, `make && sudo make install` | git, autotools |
| 26 | provision-squid-client.sh | `provision-squid-client` | Route a node through the ranch Squid cache | squid-ip | Downloads CA from `http://<ip>/squid-ca-cert.crt` to `/usr/local/share/ca-certificates/…` (fatal if unreachable), `update-ca-certificates`; writes `/etc/apt/apt.conf.d/02ranch-proxy`; Ollama systemd override proxy.conf + restart if active; `/etc/profile.d/ranch-proxy.sh` (644) + `/etc/environment` rewrite (stale *_proxy lines deleted, 6 appended); `/etc/pip/pip.conf`; npm globals if npm exists; verifies HTTP (apt repo), HTTPS (Ollama registry — fatal flag), HTTPS (PyPI — "SKIPPED" wording) through the proxy | curl, systemctl, npm |
| 27 | provision-squid-server.sh | `provision-squid-server` | Provision the Squid cache server (2 TB cache, SSL-bump) | [listen-ip, default 0.0.0.0] | Installs squid-openssl/openssl/nginx-light/apparmor-utils, `aa-complain` squid profile; disables apache2 if present; generates a 4096-bit CA (subj `/C=US/ST=Nebraska/L=Ranch/O=RanchChoir/OU=ExocortexCache/CN=Ranch Choir Squid CA`, 3650 days) if absent, DER copy, exports PEM to `/var/www/html/squid-ca-cert.crt`; `security_file_certgen` DB; writes the full `/etc/squid/squid.conf` (ports 3128/3129 ssl-bump, 90-day refresh patterns for ollama/apt/pip/npm/crates, workers 4…); `squid -z`, restart+enable squid and nginx; ufw allows. **Quirk:** the apache2 check compares *stdout* of `systemctl status apache2` with the not-found text, which systemctl prints to *stderr* — the "already disabled" branch is effectively unreachable; the port replicates the comparison as written | squid, openssl, nginx, systemctl |
| 28 | repair-openclaw.sh | `repair-openclaw` | Reinstall OpenClaw from the pinned release | — | Moves `~/.openclaw` aside to `~/.openclaw-historic-snapshot`; `/opt/openclaw` (sudo mkdir/chown); downloads `OpenClaw-2026.10.7-amd64.deb` if absent; `sudo apt install ./<deb>`; `curl -fsSL https://openclaw.ai/install.sh \| bash` | wget, apt, curl |
| 29 | setup.sh | `setup` | The full node setup, 11 phases | [exec] | Re-exec protocol: does git config + `git pull`, then re-runs itself with `exec` unless already in exec mode. Phases: (1) one batched apt install (34 packages), docker snap, libuv patch gate `/etc/exo-uv-ready`; (2) Avahi: install `exocortex.service` to `/etc/avahi/services/` + restart; (3) Ollama via vendor installer pipe; (4) rustup stable + 5 cargo crates in parallel (phext-shell, hello-phext, quickfork, sq, phext-lattice), PATH line appended to `~/.bashrc` if absent; (5) `/opt/exopy` venv + batched pip installs + agentmail/dotenv; (6) ed25519 key if absent, then **gates on `/etc/exo-ready`** — prints the pubkey and exits 1 until the operator touches it; (7) phoronix/ROCm/beebjit into /opt (parallel); (8) 44 wbic16 repos cloned into `/source`, ≤8 concurrent, plus one https clone; (9) micro-agent (dead branch: LLM_AGENT=ollama); (10) systemd-resolved fallback DNS file + restart, `apt upgrade`; (11) libphext-rs proof-tools installer if present. Ends `Setup Complete.` | apt, snap, cargo, pip, git |
| 30 | squid-uninstall-client.sh | `squid-uninstall-client` | Undo provision-squid-client | — | Removes CA + `update-ca-certificates`; removes apt proxy conf, Ollama override (+rmdir), profile.d file; deletes `*_proxy=` lines (any case) from `/etc/environment` with sed semantics; removes pip.conf; daemon-reload + conditional ollama restart; npm config delete (global) if npm exists | systemctl, npm |
| 31 | ssh-setup.sh | `ssh-setup` | First-boot SSH + source bootstrap | — | `sudo visudo` (interactive, driven as-is); `sudo mkdir /source` + chown; `ssh-keygen` if no `~/.ssh/id_ed25519` (bare, interactive); clone exocortical into /source; run `./enable-virtual-memory.sh` then `./setup.sh` from the clone | git, ssh-keygen, visudo |
| 32 | sync-ollama-models.sh | `sync-ollama-models` | Bidirectional Ollama model sync between two nodes | remote-host (required) | `killall ollama` locally and via ssh; chown `/usr/share/ollama` to $USER both sides; rsync models remote→local then local→remote; chown back to ollama:ollama both sides | ssh, rsync, sudo |
| 33 | tailscale.sh | `tailscale` | Refresh + gate the Tailscale installer | — (interactive) | `sudo chown $USER:$USER tailscale*.sh` (cwd glob); downloads `https://tailscale.com/install.sh` over `tailscale_install.sh`; `git diff`, `git status`; prompts `Review the upstream changes, press Y to proceed`; only on exact `Y`: chmod +x and `sudo ./tailscale_install.sh`; `git add tailscale_install.sh`, `git status` | curl, git |
| 34 | tb-auth.sh | `tb-auth` | Auto-authorize Thunderbolt devices | — | Writes the udev rule `ACTION=="add", SUBSYSTEM=="thunderbolt", ATTR{authorized}=="0", ATTR{authorized}="1"` to `/etc/udev/rules.d/99-thunderbolt-auto.rules` via `sudo tee` (so the rule is also echoed to stdout); `udevadm control --reload-rules`; `udevadm trigger --subsystem-match=thunderbolt` | udevadm |
| 35 | tb-setup.sh | `tb-setup` | Thunderbolt/USB4 mesh fabric (tb0 → tbfab bridge) | — | Hostname→10.53.x.y map (26 nodes, embedded); unknown hostname → error exit 1. modprobes; waits ≤5×2 s for thunderbolt0; builds bridge `tbfab` (mtu 65520), enslaves every `thunderbolt*` interface, assigns `<addr>/16`; installs `/etc/systemd/system/tb-fabric.service` (ExecStart=/source/exocortical/tb-setup.sh) + enable if absent | ip, modprobe, systemctl |
| 36 | usb-ssd-fixup.sh | `usb-ssd-fixup` | I/O + journald tuning for USB-SSD roots | — | Writes `/etc/sysctl.d/99-minimize-disk-io.conf` (swappiness 1, dirty 80/50, cache pressure 10…) + `sysctl --system` if absent; disables unattended-upgrades + apt-daily timers; writes `none` to `/sys/block/sda/queue/scheduler` (via tee — echoed); sed-edits `/etc/fstab` adding `noatime` to `/` and `/boot` ext4 lines; volatile journald conf + restart if absent | systemctl, sysctl |
| 37 | wifi-optimizations.sh | `wifi-optimizations` | fq + BBR congestion control | — | `sysctl -w net.core.default_qdisc=fq`, `sysctl -w net.ipv4.tcp_congestion_control=bbr`; appends both lines to `/etc/sysctl.conf` via `sudo tee -a` (echoed to stdout) | sysctl |

### (D) Services / long-running / delegated — 3

| # | Source | Applet | Purpose | Boundary |
|---|---|---|---|---|
| 38 | exo-agentmail.py | `exo-agentmail` | AgentMail client: read this node's identity (`email`, `name`) from `/etc/mirrorborn.phext`, load `AGENTMAIL_API_KEY` (+ `.env` in cwd, python-dotenv semantics: no override of existing env), GET the inbox and print it, then send when recipient+subject+message are all non-empty | The port speaks the same HTTPS API (`https://api.agentmail.to/v0`, Bearer key; `AGENTMAIL_BASE_URL` override exists **only** as a test hook) with libcurl: GET `/inboxes/<email>`, POST `/inboxes/<email>/messages/send` with JSON `{to, subject, text}`. Defaults mirror argparse exactly (`--recipient will@phext.io`, `--subject "Message from <name>"`, `--message ""` → usage line). **Presentation divergence, stated:** Python prints the SDK object's repr; the port prints the API's JSON body — same data, different rendering. Defaults are evaluated *after* the inbox GET, as in the original (the GET and its print precede argparse) |
| 39 | hello-phext.sh | `hello-phext` | Launch the hello-phext Rocket server | Sets `ROCKET_ADDRESS=0.0.0.0` and execs `~/.cargo/bin/hello-phext`. The port does exactly that (a real `exec`, so the applet *becomes* the server, like the shell script). The server itself is a separate Rust binary — out of scope, as it is for the shell script |
| 40 | setup-phext-mcp.sh | `setup-phext-mcp` | Install + launch mcphost with the phext MCP server | `go install github.com/mark3labs/mcphost@latest`; clone wbic16/phext-mcp into /source if absent; `cargo build --release`; copy `mcphost-config.yml` to `~/.mcphost.yml` if absent (**quirk:** the cp is relative to the phext-mcp cwd, where that file does not live — the copy fails and, there being no `set -e`, the "Installed default mcphost config" line prints anyway; the port reproduces the attempt and the message flow, and notes the quirk); finally execs `$HOME/go/bin/mcphost -m ollama:llama4:scout` — the interactive session is the boundary: the port hands over the terminal exactly as the script does |

## (C) Assets, vendored code, config — 12 files, no applet

| File | What it is | Evidence |
|---|---|---|
| readme.md | Repo documentation (package list, repo list, tooling notes) | Markdown prose; no executable content |
| agents.md | Coordination rules for agents working in the repo | Markdown prose |
| .gitignore | Git ignore rules (`*.deb`, `.tb-switch`, `.env`, …) | Config for git, not a program |
| version.sh | One-line sourced fragment: `export REV=1` | No shebang; executing it has no observable effect (sets a variable in its own subshell); it is a version stamp meant for sourcing, not a command |
| exo.css | Stylesheet for the ops console (16.6 KB) | Pure CSS — selectors/custom properties only, zero behaviour; the behaviour it styles is exo.js → `exo-console` |
| index.html | Page shell for the ops console | Static HTML whose only script reference is `exo.js`; the DOM ids are the console's rendering contract — presentation, ported behaviour lives in `exo-console` |
| ports.html | The portal *page* itself (17.4 KB of link pods + the form) | Static HTML/CSS data; its one behaviour (the `submitter()` URL builder) is ported as `ports` above — the page as a document is an asset |
| world.phext | A phext scroll containing an HTML link list of node hello-phext endpoints | Data content in phext form; no code |
| exocortex.service | Avahi service-group XML advertising `_exocortex_exocortical._tcp` port 22 | Declarative config consumed by avahi-daemon; `setup` installs it (the install step is part of the `setup` applet) |
| mcphost-config.yml | mcphost configuration data (phext MCP server command) | YAML data consumed by mcphost; referenced by `setup-phext-mcp` |
| install_ollama.sh | **Vendored upstream** Ollama installer (15.9 KB) | It is Ollama's official install script committed to the repo; nothing in the repo invokes it (`setup.sh` pipes `https://ollama.com/install.sh` fresh). The repo's own behaviour — fetching and running the vendor installer — is inside `setup` phase 3, which is ported |
| tailscale_install.sh | **Vendored upstream** Tailscale installer (731 lines) | Header: "Copyright (c) Tailscale Inc & contributors"; `tailscale.sh` re-downloads it fresh from tailscale.com on every run. The repo's feature is the gate around it (`tailscale`), which is ported |

## Coverage accounting

- Files in repo at 0872ca2a: **52**
- Features ported to native C applets: **40 of 40** identified features
  (37 shell scripts, exo-agentmail.py, exo.js's console behaviour,
  ports.html's submitter behaviour). rag.py is a component of feature
  `rag` (counted once, with its (D) boundary stated).
- Asset / vendored / config files, no applet, each evidenced above: **12**
- Unported features: **none**. Presentation-only aspects (DOM rendering,
  the console's beep, Python's object-repr printing) are called out
  per-feature rather than silently dropped.

## Port section (landed 2026-11-07)

All 40 features are ported as native C applets in the extended binary
`phccbox-exo` (2,654,444 bytes, SHA-256
`a9413cd05e6c3905e6dbddc66874be51f8ffdb682380bf3e9ed4dc36e3a4c15f`,
56 applets: 16 v1 core + 40 Exocortical), built entirely by phcc via the
v1 bundle mechanism. Sources: `~/workspace/phccbox/exo-src/`
(`exo_util.c` shared layer + `applets_a.c`–`applets_g.c`); heredoc and
table literals are generated mechanically from this repo at 0872ca2a by
`exo-src/gen_literals.py` into `exo-src/gen/*.h`. Dispatcher:
`~/workspace/phccbox/src/phccbox2.c` (v1 source kept pristine).

Port conventions beyond §"Port conventions": system-changing applets
honour `EXO_SYSROOT` (root-prefix override), `EXO_DRYRUN` and
`EXO_TRANSCRIPT`; user-level originals that self-elevate via `sudo tee`
are delegated through `sudo tee` in production mode with the original's
own flag spelling (`-a` vs `--append`).

Receipts: `~/workspace/phccbox/RECEIPTS.md` (§ Exocortical family) —
per-feature method and result for all 40, 15/15 heredoc byte-proofs,
the classified-asymmetry list, the reproduced-quirk list, and the
port-bug fix list. Harness: `~/workspace/phccbox/exo-test/`.

**Final coverage: features 40, ported 40, asset-class files 12,
unported 0.**

## Surprises vs the brief's hint list

1. `enable-browser-sandbox.sh` exists in the repo (newest commit) but
   not in the `exocortical-check` hint directory.
2. The site map's "~135 KB Shell" undersells it: the two vendored
   installers alone are ~36 KB, and the repo carries ~35 KB of JS/HTML
   behaviour and data that the description doesn't mention.
3. `install_ollama.sh` is dead code in-repo (vendored, never invoked);
   `setup.sh` fetches the same installer fresh from ollama.com.
