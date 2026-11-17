#!/usr/bin/env python3
"""Heredoc byte-proof: render the ORIGINAL scripts' heredoc bodies with bash
(the original's own expansion engine) and compare against (a) files the C
ports wrote under their scratch roots, and (b) the ports' embedded templates
(rendered with the same values). Run on the ranch from exo-test/."""
import re, subprocess, sys, os

FRESH = os.path.expanduser("~/exocortical-fresh")
CASES = os.path.expanduser("~/phccbox-work/exo-test/cases")
GEN = os.path.expanduser("~/phccbox-work/exo-build/exo-src/gen")
out = []

def note(s): out.append(s); print(s, flush=True)

def extract_heredoc(script, needle):
    """Find the heredoc whose command line contains `needle`;
    return (body, quoted) — quoted means the original used <<'EOF' (no expansion)."""
    lines = open(os.path.join(FRESH, script)).read().split("\n")
    for i, ln in enumerate(lines):
        m = re.search(r"<<-?\s*('|\")?([A-Za-z_][A-Za-z0-9_]*)\1?", ln)
        if m and needle in ln:
            quoted = bool(m.group(1))
            delim = m.group(2)
            body = []
            for j in range(i + 1, len(lines)):
                if lines[j].strip() == delim:
                    return "\n".join(body) + "\n", quoted
                body.append(lines[j])
            raise SystemExit(f"unterminated heredoc in {script} for {needle}")
    raise SystemExit(f"heredoc not found: {script} needle {needle}")

def render_bash(body, env, quoted):
    """Render the body the way bash would inside the original heredoc."""
    d = "<<'PROOFEOF'" if quoted else "<<PROOFEOF"
    p = subprocess.run(["bash", "-c", f"cat {d}\n" + body + "PROOFEOF"],
                       capture_output=True, text=True,
                       env={**os.environ, **env})
    return p.stdout

def read_c_template(header, varname):
    txt = open(os.path.join(GEN, header)).read()
    m = re.search(varname + r'\s*(\[\])?\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', txt)
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2))
    s = "".join(parts)
    return s.encode().decode("unicode_escape")

def expand(tmpl, vals):
    for k, v in vals.items():
        tmpl = tmpl.replace("%%" + k + "%%", v)
    return tmpl

def cmp(name, a, b):
    note(f"PROOF {name}: {'BYTE-IDENTICAL' if a == b else 'DIFF'} ({len(a)} vs {len(b)} bytes)")
    if a != b:
        al, bl = a.split("\n"), b.split("\n")
        for i, (x, y) in enumerate(zip(al, bl)):
            if x != y:
                note(f"  first diff line {i}: orig={x!r} port={y!r}")
                break

def port_file(case, rel):
    p = os.path.join(CASES, case, "port/root", rel)
    return open(p).read() if os.path.exists(p) else None

# ---- file-body proofs from the rooted port runs ----
checks = [
    ("squid-client apt proxy", "provision-squid-client.sh", "/etc/apt/apt.conf.d/02ranch-proxy",
     {"SQUID_IP": "192.168.86.50", "PROXY_HTTP": "http://192.168.86.50:3128", "PROXY_SSL": "http://192.168.86.50:3129"},
     "provision-squid-client", "etc/apt/apt.conf.d/02ranch-proxy", "file"),
    ("squid-client ollama proxy.conf", "provision-squid-client.sh", "/proxy.conf",
     {"SQUID_IP": "192.168.86.50", "PROXY_HTTP": "http://192.168.86.50:3128", "PROXY_SSL": "http://192.168.86.50:3129"},
     "provision-squid-client", "etc/systemd/system/ollama.service.d/proxy.conf", "file"),
    ("squid-client profile.d", "provision-squid-client.sh", "profile.d/ranch-proxy.sh",
     {"SQUID_IP": "192.168.86.50", "PROXY_HTTP": "http://192.168.86.50:3128", "PROXY_SSL": "http://192.168.86.50:3129"},
     "provision-squid-client", "etc/profile.d/ranch-proxy.sh", "file"),
    ("squid-client pip.conf", "provision-squid-client.sh", "/pip.conf",
     {"SQUID_IP": "192.168.86.50", "PROXY_HTTP": "http://192.168.86.50:3128", "PROXY_SSL": "http://192.168.86.50:3129"},
     "provision-squid-client", "etc/pip/pip.conf", "file"),
    ("tb-fabric unit", "tb-setup.sh", "tb-fabric.service",
     {}, "tb-setup", "etc/systemd/system/tb-fabric.service", "file"),
    ("usb sysctl conf", "usb-ssd-fixup.sh", "99-minimize-disk-io.conf",
     {}, "usb-ssd-fixup", "etc/sysctl.d/99-minimize-disk-io.conf", "file"),
    ("usb journald conf", "usb-ssd-fixup.sh", "volatile.conf",
     {}, "usb-ssd-fixup", "etc/systemd/journald.conf.d/volatile.conf", "file"),
    ("setup fallback DNS", "setup.sh", "fallback.conf",
     {}, "setup-full", "etc/systemd/resolved.conf.d/fallback.conf", "file"),
]
for name, script, needle, env, case, rel, kind in checks:
    body, quoted = extract_heredoc(script, needle)
    rendered = render_bash(body, env, quoted)
    pf = port_file(case, rel)
    if pf is None:
        note(f"PROOF {name}: port file MISSING ({case}/{rel})")
    else:
        cmp(name, rendered, pf)

# appended blocks: port file must equal seed + rendered block
def append_check(name, script, needle, env, case, rel, seed):
    body, quoted = extract_heredoc(script, needle)
    rendered = render_bash(body, env, quoted)
    pf = port_file(case, rel)
    if pf is None:
        note(f"PROOF {name}: port file MISSING")
        return
    cmp(name, seed + rendered, pf)

append_check("nfs exports block", "nfs-provision-server.sh", "/etc/exports",
             {"EXPORT_ROOT": "/srv/mirrorborn", "MB_SUBNET": "192.168.86.0/24"},
             "nfs-provision-server", "etc/exports", "# exports seed\n")
append_check("nfs fstab block", "nfs-provision-client.sh", "/etc/fstab",
             {"NFS_SERVER": "orchid-flow", "NFS_ROOT": "/srv/mirrorborn", "MOUNT_BASE": "/mnt/mirrorborn"},
             "nfs-provision-client", "etc/fstab", "# fstab\nUUID=x / ext4 defaults 0 1\n")

# LLM files from the optimize-full rooted run: values depend on node RAM tier;
# read what the port used from its own outputs is fragile — instead verify the
# sysctl + udev bodies (tier-independent) here, override checked in receipts.
for name, needle, rel in [
    ("llm sysctl conf", "SYSCTL_CONF", "etc/sysctl.d/90-exocortical-llm.conf"),
    ("llm udev rule", "UDEV_RULE", "etc/udev/rules.d/60-exocortical-iosched.rules"),
]:
    body, quoted = extract_heredoc("optimize-llm-inference.sh", needle)
    rendered = render_bash(body, {}, quoted)
    pf = port_file("optimize-full", rel)
    if pf is None:
        note(f"PROOF {name}: port file MISSING")
    else:
        cmp(name, rendered, pf)

# ---- template proofs (no port file exists: squid.conf, sandbox profile) ----
squid_env = {"LISTEN_IP": "0.0.0.0", "SQUID_PORT": "3128", "SQUID_SSL_PORT": "3129",
             "CA_DIR": "/etc/squid/ssl", "CA_CERT": "/etc/squid/ssl/squid-ca-cert.pem",
             "CA_KEY": "/etc/squid/ssl/squid-ca-key.pem",
             "SSL_DB": "/var/lib/squid/ssl_db", "CACHE_DIR": "/var/spool/squid",
             "CACHE_DISK_MB": "2097152", "CACHE_MEM_MB": "2048",
             "MAX_OBJ_SIZE_MB": "102400", "MAX_OBJ_MEM_MB": "256"}
body, squoted = extract_heredoc("provision-squid-server.sh", "/etc/squid/squid.conf")
rendered = render_bash(body, squid_env, squoted)
tmpl = read_c_template("lit_squid.h", "SQUID_CONF_TMPL")
note("squid.conf heredoc vars in original: " + ",".join(sorted(set(re.findall(r"\$\{?([A-Z_]+)", body)))))
cmp("squid.conf template", rendered, expand(tmpl, squid_env))

for abi in ["  abi <abi/4.0>,\n", ""]:
    env = {"PROFILE_NAME": "playwright-chromium", "ABI_LINE": abi}
    body, bquoted = extract_heredoc("enable-browser-sandbox.sh", "PROFILE_FILE")
    rendered = render_bash(body, env, bquoted)
    tmpl = read_c_template("lit_sandbox.h", "SANDBOX_PROFILE_TMPL")
    cmp(f"sandbox profile (abi={'yes' if abi else 'no'})", rendered, expand(tmpl, env))

print("\n".join(out))
