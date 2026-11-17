# phccbox v1 — receipts

Built 2026-11-07 (evening) on aletheia-core. Compiler: phcc @ published tip
`9d070f26` (`tcc version 1.0.0 2026-11-07 HEAD@9d070f26`), the same build the
publication-gate run used.

---

# phccbox v1.1 — receipts (coreutils single-binary step)

Built 2026-11-07 (late evening) on aletheia-core. Compiler: phcc @ published
tip `32674fc3` (`tcc version 1.0.0 2026-11-07 HEAD@32674fc3`), fresh clone,
gcc-built on the node. Work area: ranch `~/phccbox-v11/` (scripts, logs,
smoke output in `out/`; local copies of the smoke scripts in
`~/workspace/phccbox/scripts/v11-smoke-{a,a2,b,c}.sh`).

## The step

GNU coreutils **9.12** (tarball SHA-256 `a4801985…`, the version the gate
and benchmark runs used) configured
`CC='tcc -B<tree>' ./configure --enable-single-binary=symlinks
--enable-install-program=arch,hostname,kill,uptime` on a clean tree:
**configure rc=0, make rc=0, zero failures** — `lib/nanosleep.c` (PQ-17's
line) compiles silently now that F18 is published. The single binary
builds 108 program names (104 symlinks + `[` + `arch`/`hostname`/`kill`/
`uptime` via the install-program flag + `ginstall`).

Two findings about the default configuration, both classified
**build-system default, not a phcc defect**:

- A default configure — under **gcc and phcc alike** — does not build
  `arch`, `hostname`, `kill`, `uptime` at all (coreutils keeps them out
  of the default install set; they duplicate other system providers).
  `--enable-install-program=arch,hostname,kill,uptime` is coreutils' own
  supported switch and restores them in both builds.
- In multicall mode the six digest programs (`md5sum`, `sha1sum`,
  `sha224sum`, `sha256sum`, `sha384sum`, `sha512sum`) are served by the
  shared cksum machinery, so their `--help`/`--version` metadata is the
  multicall table's (e.g. `--version` credits the cksum authors). The box
  is **byte-identical to a gcc-built single binary** on all 12 of those
  checks (smoke A2) — upstream multicall behaviour, faithfully carried.

## Integration shape

**One bundle behind the existing dispatcher** (design §5's anticipated
reduction). Coreutils' single-binary build already organises itself as
`src/coreutils-coreutils.o` (the multicall main) + one
`src/libsinglebin_<prog>.a` per program + `libver.a` + `libcoreutils.a`;
the bundle is `ld -r` over exactly those inputs (archive groups repeated
as in upstream's own link line), then objcopy localizes every global
except `main`, renamed `coreutils_main` (800 symbols localized). The
bundle's undefined-symbol set was compared against the standalone single
binary's: the only extras are libc/libgcc/crt names a final link resolves
(`optarg`, `environ`, `__va_arg`, the `__fixuns*` float helpers, …) — no
coreutils-internal member was dropped by the extraction. The dispatcher
(`src/phccbox11.c`) hands `coreutils_main` argv with argv[0] set to the
applet name; coreutils' own main switch dispatches from there, including
`[`, the name variants `dir`/`vdir`, and `install` (the build tree calls
it `ginstall`; both names dispatch in the multicall table, the box
registers the canonical `install`). The v1 custom `true`/`false`/`yes`
stand-ins are removed — the GNU coreutils versions replace them, as the
v1 design intended. The v1 bundles for sed/grep/awk/sh/bzip2/gzip were
reused (grep/awk/bzip2/gzip re-bundled from the same gate/fresh trees
with the phase-3f inventories after the v1 bundle files were found
pruned; gzip's bundle excludes `libgzip_a-xalloc-die.o`, whose symbol
gzip's own `util.o` already provides — the loose-objects equivalent of
archive extraction). Final link by phcc @ 32674fc3:
`-lselinux -lpcre2-8 -lm -lgmp -lcrypto`.

**Deployment-name rule (measured):** the dispatcher enters CLI mode only
when argv[0]'s basename is literally `phccbox`; a copy named anything
else is treated as an applet name and exits 127. The v1.1 binary is
therefore staged in its own directory under the canonical name.

## Binary

| Binary | Bytes | SHA-256 | Applets | Where |
|---|---|---|---|---|
| phccbox (v1.1) | 5,136,620 | `a47222050482a12f55984c357ab49f557ce3fdc26ea39f673454644da932d1a2` | 122 in `--list` | ranch `~/phccbox/v1.1/phccbox`; verified local copy `~/workspace/phccbox/phccbox-v11` (hash-identical after transfer) |
| libstdbuf.so | 31,101 | `cf87217ab2b6cb60288805557025095bfbd2e4ec347b02b6cac58986ac1c47c9` | stdbuf's companion | ranch `~/phccbox/v1.1/libstdbuf.so` (colocated with the binary) |

`--list` (122): the 13 kept v1 names (awk ash bzip2 bunzip2 bzcat egrep
fgrep grep gunzip gzip sed sh zcat) + 108 coreutils names + `coreutils`
(its own multicall entry). `vi` remains the v1 `phccbox-vi` variant,
unchanged.

**`stdbuf` packaging note:** stdbuf finds `libstdbuf.so` by searching
beside the executable and in the configured pkglibexecdir. With no
colocated copy the applet reports `stdbuf: failed to find
'libstdbuf.so'`; with the phcc-built `.so` shipped beside the binary it
works (`stdbuf -oL echo hi` → `hi`, rc 0). Same requirement the
standalone tool has — its build tree just happens to satisfy it
implicitly. Not a compiler defect; a companion file the package carries.

## Coverage recount (BusyBox map)

Coreutils contributes 108 names; **90 are in the 403-applet BusyBox
defconfig inventory** (87 of them new over v1 — the other 3 are
true/false/yes, already counted as v1 customs and now GNU). 18 coreutils
names are not BusyBox applets and ride along as extras: b2sum basenc
chcon csplit dir dircolors fmt join numfmt pathchk pinky pr ptx runcon
sha224sum sha384sum stdbuf vdir.

| Scope | Applets | Share of 403 |
|---|---|---|
| v1 shipped | 17 | 4.2% |
| **v1.1 shipped** | **104** | **25.8%** |
| Full map ceiling (design §2) | 259 | 64.3% |

No coreutils applet failed to build; none is silently dropped.

## Smoke — all green

References: a gcc build of the same coreutils 9.12 (plain per-program
build, same `--enable-install-program` flag) for A/B, a gcc single-binary
build for A2, system tools for C. All runs under `LC_ALL=C TZ=UTC`;
references invoked with `exec -a <name>` so usage lines compare honestly
(coreutils' `usage()` prints argv[0] verbatim — the first A run's 113
"failures" were that invocation artifact plus the digest-oracle
question, both resolved as above; two B harness bugs — a leading `;`
after PRE and shell job-death notices — were likewise found and fixed in
the harness, not the binary).

| Suite | Result | Content |
|---|---|---|
| A: --help/--version | 205/217 vs per-program gcc build | every diff is in the digest family, settled by A2 |
| A2: digest family | **12/12** vs gcc single-binary build | byte-identical stdout/stderr/rc |
| B: functional | **54/54** | per-family cases below; stdout+stderr+rc+resulting-file-manifest (paths, modes, SHA-256) byte-compared |
| C: v1 regression + farm | **27/27** | sed/grep/awk/sh vs system tools, gzip/bzip2 roundtrips vs system decompressors, GNU true/false/yes, unknown→127, `--install` farm = 122 links, farm `ls`/`wc`/`[`/`sha256sum` verified |

B families (54 cases): cat, head/tail, wc, sort, uniq, cut, tr,
comm/join, tsort, paste, split, csplit, fmt/fold, expand/unexpand, nl,
od, cksum, all seven digest tools (incl. `md5sum -c`), base64/base32/
basenc, seq (incl. float steps), expr, factor (incl. a 13-digit
semiprime — the `-lgmp` path), numfmt, date (fixed epochs), printf
(float formats), echo/yes, test/`[` exit-code battery, basename/dirname/
readlink/realpath/pathchk, ls/dir/vdir on a fixed-mtime tree, cp/mv/ln/
link/unlink with link counts, mkdir/rmdir/rm, touch/stat/truncate,
install (modes), mktemp (patterns normalized to fixed names), mkfifo +
mknod error parity, chmod/chgrp/chown, df/du, dd, tee, env/printenv,
id/groups/whoami/logname/hostname/arch/uname/hostid/tty, nproc,
who/users/uptime (-p/-s forms), timeout (rc 0/124, TERM and KILL),
nice, stty error parity, dircolors (incl. `--print-database` hash),
stdbuf (with colocated .so), chroot error parity, runcon/chcon error
parity (no SELinux in this environment — both sides fail identically),
shred -z content, pr/ptx, kill (-l table + live SIGTERM delivery),
pwd, sleep, sync.

Three B cases initially diffed on harness-inherent noise and were
re-run with the noise removed, outputs otherwise identical: mktemp's
random suffixes, dd's bytes-copied timing/rate line, and the shell's
"Killed" job notice (contains the PID and the literal command line) in
the timeout case.

**Misbehaviour found: none.** Every applet that built behaves at parity
with the gcc reference on the suite above. Known boundaries carried
from the design, unchanged: gawk extension modules (§8), `vi` as a
separate variant, no static binary (PQ-23).

## Build notes

- Thermal discipline held throughout (guard at 73 °C in the build
  scripts; observed peak 65.1 °C, shared with a parallel agent's work).
- The link.sh function-form bundling pass silently produced empty
  symbol lists for grep/awk/bzip2 on its first run (the same commands
  run directly succeed; root cause not isolated — the bundles were
  rebuilt by direct execution and verified by symbol counts and the
  full smoke). Recorded here rather than smoothed over.


## Binaries

| Binary | Bytes | SHA-256 | Applets | Where |
|---|---|---|---|---|
| phccbox | 2,498,796 | `fb86c90dd5ec20619b3ca67f816eb341666f166ee0847782921db32b0bbeb1a1` | 16 | ranch `~/phccbox/phccbox`; verified local copy `~/workspace/phccbox/phccbox` (hash-identical after transfer) |
| phccbox-vi | 7,962,860 | `0165f2bbf9152813dd717ae7dd587644cccb41680902ff92417a4242723b3c5f` | 17 (adds `vi`) | ranch `~/phccbox/phccbox-vi` only |
| phccbox-static | — | — | — | **not achieved**: `tcc -static` link fails — see design §7 |

`phccbox --list` (core): awk ash bzip2 bunzip2 bzcat egrep false fgrep grep
gunzip gzip sed sh true yes zcat.

## Smoke corpus — 35 pass / 1 fail, the 1 fail a harness artifact

Full log: `receipts/phase4-smoke.log`. Every applet exercised through an
argv[0] symlink, stdout bytes + exit code compared against the standalone
reference tool.

| Applet | Cases | Result | Reference |
|---|---|---|---|
| sed | 4 | 4/4 | gate standalone GNU sed 4.10 |
| grep | 4 | 4/4 | gate standalone GNU grep 3.12 |
| egrep | 2 | 2/2 | standalone grep -E (flag-form) |
| fgrep | 2 | 2/2 | standalone grep -F |
| awk | 4 | 4/4 | gate standalone gawk 5.4.1 |
| sh | 4 | 3/4 → 4/4 on re-test | standalone dash 0.5.12 |
| ash | 1 | 1/1 | standalone dash via `ash` name |
| bzip2 | 2 | 2/2 | standalone bzip2 1.0.8 |
| bunzip2 | 1 | 1/1 | standalone bzip2 -d |
| bzcat | 1 | 1/1 | standalone bzip2 -dc |
| gzip | 2 | 2/2 | standalone GNU gzip 1.14 (-n for determinism) |
| gunzip | 1 | 1/1 | standalone gzip -d |
| zcat | 1 | 1/1 | standalone gzip -dc |
| true | 1 | 1/1 | system /usr/bin/true (exit code) |
| false | 1 | 1/1 | system /usr/bin/false (exit code) |
| yes | 2 | 2/2 | system /usr/bin/yes (through head) |
| vi | 1 | 1/1 | gate standalone Vim 9.2, ex-mode edit, result files byte-compared |
| dispatch | 2 | 2/2 | `phccbox sed …` form; unknown applet → exit 127 |

The `sh/name0` case compared `sh -c 'echo $0'` outputs where box and
reference were invoked by *different absolute paths* — both printed their own
invocation path, correctly. Re-run with matched argv[0] (`exec -a sh` on both
sides): outputs identical (`name=sh`), PASS. Recorded as a harness artifact,
not a divergence.

## Symlink farm demonstration

`phccbox --install` created 16 links; with `PATH` pointed at the farm,
unqualified `sed`, `awk`, `true` resolved into the farm
(`command -v` shows the farm paths) and produced correct output
(log in `receipts/phase4-smoke.log`, farm section).

## Build notes (evidence in receipts/phase*.log)

- dash 0.5.12, gzip 1.14, bzip2 1.0.8: `./configure CC=<phcc>` + `make`,
  rc=0 each, first attempt (phase2 log).
- coreutils 9.12 with `--enable-single-binary=symlinks`: configure rc=0
  under phcc; make fails at exactly one line —
  `lib/nanosleep.c:60: error: constant exceeds 32 bit` (PQ-17; fix F18 in
  flight in a parallel tree). Coreutils deferred to v1.1 per the design.
- Bundling: `ld -r` per program + `objcopy --localize-symbols` (all globals
  except `main`) + `--redefine-sym main=<applet>_main`; dispatcher compiled
  and final link done by phcc itself.
- Thermal discipline held: guards paused work at Tctl ≥75 °C (two holds in
  phase 2, peak seen 79.4 °C during a parallel gcc reference build).

---

# Exocortical family — receipts (task 35.6.8/8.7.12/6.5.9)

Native C ports of every feature in **wbic16/exocortical @ 0872ca2a**
(2026-11-07 17:35 -0600), inventoried in `exocortical-inventory.md`
(52 files → 40 features, 12 asset/vendored/config files).

## Binary

- `phccbox-exo` (ranch: `~/phccbox-work/phccbox-exo`, staged as
  `~/phccbox-work/exo-stage/phccbox`): **2,654,444 bytes**,
  SHA-256 `a9413cd05e6c3905e6dbddc66874be51f8ffdb682380bf3e9ed4dc36e3a4c15f`.
- 56 applets: the 16 v1 core applets + **40 Exocortical applets** (the v1
  dispatcher only enters CLI mode when argv[0] is literally `phccbox`).
- Everything compiled and linked **by phcc** (`~/phcc-gate-run/phcc/tcc`,
  base 9d070f26) via the v1 bundle mechanism: `ld -r` + objcopy keeping
  only the 40 `exo_applet_*` globals (43 internal symbols localised).
  libcurl is linked as `/usr/lib/x86_64-linux-gnu/libcurl.so.4` (the node
  has the runtime but no `-dev` symlink/headers; the port hand-declares
  the small API subset it uses).
- Port design: every system-changing applet honours **EXO_SYSROOT**
  (prefixes absolute file paths; /proc and /dev excepted), **EXO_DRYRUN**
  (transcript only) and **EXO_TRANSCRIPT** (every intended command /
  file write logged). With none set, behaviour is the original's literal
  paths and commands. User-level originals that self-elevate via
  `sudo tee` are matched by delegating through `sudo tee` in production
  mode (same spelling per script: `-a` vs `--append`).

## Safety method (hard rule held)

No original and no port was ever executed against a real node's state.

- **Originals** ran as unprivileged `wbic16` on aletheia-core with a stub
  PATH (every effect command — sudo, apt, systemctl, curl, git, rsync,
  ollama, … — log-only with canned output; `date` fixed), a fake HOME and
  a scratch cwd. Direct writes to real system paths fail EACCES, which
  truncates `set -e` scripts at a recorded point; the port's transcript
  is then compared against that prefix plus static source sequence.
- **Not run at all** (receipt = port run + static behaviour table):
  `setup` and `ssh-setup` (their flows reach the real
  `/opt/exopy/bin/pip3` — real network installs), `patch-libuv` (its
  `./autogen.sh` would write into the node's real `/source/libuv`),
  `benchmark` (real `/opt/beebjit` build scripts), `install` (writes the
  node's real `/usr/local/exocortical`).
- **Ports** ran only under EXO_SYSROOT scratch roots, EXO_DRYRUN, or the
  same stub harness. `unshare -rm` is unavailable on the node (uid_map),
  so scratch roots + stubs are the sandbox.
- One near-miss, disclosed: an early squid-server port run passed the
  *unprefixed* path to the node's real `security_file_certgen`. It failed
  on permissions (the db dir is proxy-owned) and changed nothing —
  `/var/lib/squid/ssl_db` mtime verified 2026-03-31 before and after.
  The argument is sysroot-prefixed now.

## Verification classes

- **A/B full**: original and port in the harness above; stdout, exit
  code and operation stream compared byte-for-byte.
- **Prefix + full**: original truncates (EACCES / missing binary); port
  completes under its scratch root; shared prefix must match.
- **Byte-proof**: file bodies the port wrote are diffed against the
  original's heredoc rendered by bash itself (`exo-test/proof.py`).
  **15/15 BYTE-IDENTICAL**: squid-client ×4 (apt proxy, ollama
  proxy.conf, profile.d, pip.conf), squid.conf (template *and* the file
  the port wrote), tb-fabric unit, usb sysctl + journald confs, setup
  fallback DNS, NFS exports + fstab blocks, LLM sysctl conf + udev rule,
  browser-sandbox profile (both ABI variants).
- **Golden/stub-server**: exo-console export diffed against a golden
  built from exo.js's `generateMarkdownExport` source (byte-identical;
  store JSON re-parsed under node/V8); exo-agentmail driven against a
  local stub HTTP server (request method/path/auth/JSON captured);
  ports URLs diffed against the page's own formula computed independently
  in Python (byte-identical).

## Per-feature results (40/40 ported)

Legend: ✓ = pass as classed; asymmetries are listed once below the table.

| Applet | Class | Method | Result |
|---|---|---|---|
| benchmark | A | port, seeded markers | ✓ (orig not run: real /opt/beebjit) |
| browse | A | A/B full ×2 (rc 0, rc 5) | ✓ identical |
| cluster-status | A | A/B full | ✓ identical |
| rag | A(D) | A/B full ×2 | ✓ identical (ops: note a) |
| rpush | A | A/B full ×3 | ✓ identical (usage: note b) |
| squid-cache-monitor | A | A/B prod (read-only) + seeded | ✓ identical; seeded run exercises found-branches + ANSI colour |
| exo-console | A | golden export | ✓ byte-identical export; tessera 1323 (ceil rule) |
| ports | A | formula matrix | ✓ URLs byte-identical; unknown host/persona now rejected rc 1 |
| enable-browser-sandbox | B | refusal A/B + dry-run + proof | ✓ rc 1 both (note b); profile byte-proof ✓ |
| enable-npm-proxy | B | A/B usage + no-CA; port full | ✓ identical; full flow op-exact |
| enable-virtual-memory | B | guard A/B; port full | ✓ identical; fstab line + done file verified |
| enable-zram | B | A/B full | ✓ identical; disksize = MemTotal×1.5 = 46959568896 |
| fast-go-setup | B | A/B full ×2 seeds | ✓ identical |
| hwe-enable | B | A/B full | ✓ identical |
| install | B | port only | ✓ (orig not run: real target dir) |
| install-keys | B | A/B full ×2 runs | ✓ identical incl. home trees, modes 700/600, idempotence |
| lemonade | B | A/B full | ✓ identical |
| maintenance | B | A/B full | ✓ identical |
| nfs-provision-client | B | prefix + port full + quirk run | ✓ prefix identical; **quirk reproduced** (below) |
| nfs-provision-server | B | prefix + port full | ✓ prefix identical; exports byte-proof ✓ |
| nvidia-setup | B | A/B full | ✓ identical |
| nvme-expand-disk | B | guard A/B | ✓ identical (ops: note c) |
| optimize-llm-inference | B | guard A/B; port full | ✓ guard identical (note b); full flow: tier 32 from real MemTotal, grub/override/5b pinning coherent; 2 file byte-proofs ✓ |
| patch-amd-gpu | B | A/B prod + rooted files | ✓ stdout identical; pin file byte-exact (quirk below); ops: note d |
| patch-libuv | B | port only | ✓ op sequence matches source (orig not run: real checkout) |
| provision-squid-client | B | prefix + port full + fail variant | ✓ prefix identical; 4 file byte-proofs ✓; verify-fail footer path ✓ |
| provision-squid-server | B | prefix + port full | ✓ ops identical through [2/7] incl. apache2 quirk branch; port completes, squid.conf byte-identical |
| repair-openclaw | B | A/B full | ✓ stdout + rc 127 identical (ops: note e) |
| setup | B | port gate + port full | ✓ gate exits 1 at exo-ready; full run: re-exec ×2 configs, 34 pkgs, 45 clones, DNS byte-proof ✓ (orig not run: pip hazard) |
| squid-uninstall-client | B | A/B full ×2 | ✓ identical |
| ssh-setup | B | port only | ✓ rc 127, op sequence matches source (orig not run: pip hazard) |
| sync-ollama-models | B | A/B full ×2 | ✓ identical (no-arg stderr: note f) |
| tailscale | B | A/B full ×2 (Y/n) | ✓ identical incl. installed-file bytes |
| tb-auth | B | A/B prod | ✓ identical |
| tb-setup | B | A/B full | ✓ stdout identical; unit byte-proof ✓ (ops: note g) |
| usb-ssd-fixup | B | A/B + rooted files | ✓ 2 file byte-proofs ✓; fstab edit matches real sed byte-for-byte |
| wifi-optimizations | B | A/B rooted + prod | ✓ identical both modes (rooted ops: note g) |
| exo-agentmail | D | stub HTTP server | ✓ GET-before-parse (incl. --help), POST JSON `{to,subject,text}` + Bearer captured (orig: note h) |
| hello-phext | D | A/B full, seeded server | ✓ identical (ops: note i) |
| setup-phext-mcp | D | A/B full | ✓ stdout + rc 127 identical; latent-bug behaviour reproduced (below) |

Classified asymmetries (not divergences):
a) the port's transcript records commands the stub log cannot see
   (absolute-path `/opt/exopy/bin/python`); b) usage/refusal lines embed
   argv[0] — originals invoked by path, applets by name; c) transcript
   records real read-only commands (`df -h`) the stub log never sees;
d) pipelines are one RUN-SH transcript line vs one stub-log line per
   pipeline element; e) rooted-mode native MKDIR/CHOWN replace sudo
   lines in the op stream; f) bash's `${1:?}` diagnostic prefix is the
   script path, the port's is the applet name (message body identical);
g) rooted-mode native writes (TEE/WRITE ops) replace `sudo tee` lines —
   production-mode runs show identical op streams; h) the original
   cannot run here (agentmail package absent — ImportError captured) and
   prints the SDK object repr where the port prints the JSON body (same
   request, presentation divergence, documented in the inventory);
i) EXEC transcript line has no stub-log counterpart (exec replaces the
   process).

## Original quirks reproduced (not fixed)

- **nfs-provision-client**: `((MOUNTED++))` under `set -e` kills the
  script after the first mounted share — the port reproduces the silent
  exit 1 at exactly that point (dedicated quirk run).
- **patch-amd-gpu**: the pin file contains literal `\n` sequences
  (the original's `echo` lacks `-e`) — byte-verified with od.
- **provision-squid-server §1.1**: the apache2 check captures stdout
  only, so the "already disabled" branch is nearly dead — verified live:
  both original and port take the stop/disable/hold branch.
- **setup-phext-mcp**: `mcphost-config.yml` is copied from the wrong
  directory (latent bug) — cp fails, "Installed default mcphost config"
  prints anyway; port matches, including a cp-style diagnostic.
- **setup**: git configs and `git pull` run twice (before and after the
  self re-exec) — reproduced.

## Port bugs found by the receipts and fixed

`restrict` used as a variable name (C99 keyword); dispatcher entries
first landed inside the WITH_VIM guard; `exo_tee` did not delegate to
`sudo tee` in production mode; wifi-optimizations op order (both
sysctls precede both appends); rag usage text; five applets now tolerate
failed `cd` like their no-`set -e` originals; stdout buffering let child
output overtake buffered printf (fflush before fork — this one also
reordered the optimize guard and squid-server header); CHK mapped write
failures to exit 255 where bash exits 1; nfs-server passed unprefixed
paths to bare chown/chmod; tb-setup's post-loop probe is unconditional;
squid.conf template kept heredoc line-continuations the original's bash
collapses (generator fix — the written squid.conf is now byte-identical);
squid-uninstall edited /etc/environment natively in production mode
instead of via `sudo sed`; certgen's `-s` path not sysrooted; ports
accepted unlisted hosts/personas.

## Coverage

Exocortical family: **40/40 features ported, 0 unported**; 12 files are
assets/vendored/config (evidence per file in the inventory). This family
is separate from the BusyBox map (17/403) — see the design doc §2.
Harness: `~/workspace/phccbox/exo-test/` (driver.sh, cases1–4.sh,
stub.sh, proof.py); raw evidence on the ranch in
`~/phccbox-work/exo-test/{R1..R4,PROOF}.txt` and `cases/<case>/`.
