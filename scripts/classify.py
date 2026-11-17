#!/usr/bin/env python3
"""Classify all 403 BusyBox 1.37 defconfig applets for the phccbox coverage map."""
import pathlib

R = pathlib.Path.home() / "workspace/phccbox/receipts"
applets = [l.strip() for l in open("/tmp/applets.txt") if l.strip()]
# /tmp/applets.txt = 402 names from the defconfig build's --list + "tc",
# a defconfig applet excluded from that build only because the host's
# kernel headers dropped CBQ (see design doc).
cu = set(l.strip() for l in open(R / "coreutils-binprograms.txt") if l.strip())

V1 = {
 "sed": "GNU sed 4.10", "grep": "GNU grep 3.12", "egrep": "GNU grep 3.12 (-E wrapper)",
 "fgrep": "GNU grep 3.12 (-F wrapper)", "awk": "GNU Awk 5.4.1",
 "sh": "dash 0.5.12", "ash": "dash 0.5.12",
 "bzip2": "bzip2 1.0.8", "bunzip2": "bzip2 1.0.8 (-d wrapper)", "bzcat": "bzip2 1.0.8 (-dc wrapper)",
 "gzip": "GNU gzip 1.14", "gunzip": "GNU gzip 1.14 (-d wrapper)", "zcat": "GNU gzip 1.14 (-dc wrapper)",
 "vi": "Vim 9.2 (phccbox-vi variant)",
 "true": "custom (complete; coreutils replaces in v1.1)",
 "false": "custom (complete; coreutils replaces in v1.1)",
 "yes": "custom (complete; coreutils replaces in v1.1)",
}
GNU2 = {  # later slice: a full upstream project, same bundling mechanism
 "find": "findutils", "xargs": "findutils",
 "diff": "diffutils", "cmp": "diffutils", "diff3": "diffutils", "sdiff": "diffutils",
 "tar": "GNU tar", "cpio": "GNU cpio", "ed": "GNU ed", "patch": "GNU patch",
 "bc": "GNU bc", "dc": "GNU dc", "less": "Less", "unzip": "Info-ZIP unzip",
 "ar": "GNU binutils", "strings": "GNU binutils",
 "lzma": "XZ Utils", "unlzma": "XZ Utils", "xz": "XZ Utils", "unxz": "XZ Utils",
 "lzcat": "XZ Utils", "xzcat": "XZ Utils",
 "uncompress": "GNU gzip (name variant; table entry only)",
 "chattr": "e2fsprogs", "lsattr": "e2fsprogs", "getfattr": "attr", "setfattr": "attr",
 "fatattr": "fatattr", "dos2unix": "dos2unix", "unix2dos": "dos2unix",
 "lzop": "lzop", "mt": "GNU cpio", "time": "GNU time", "tree": "tree",
 "ts": "moreutils", "uudecode": "GNU sharutils", "uuencode": "GNU sharutils",
 "[" : "coreutils 9.12 (test, bracket form — v1.1 with CU)",
}
UL = {  # later slice: util-linux (one project, many applets)
 "more": "util-linux", "hexdump": "util-linux", "rev": "util-linux", "cal": "util-linux",
 "setsid": "util-linux", "setarch": "util-linux", "taskset": "util-linux", "chrt": "util-linux",
 "ionice": "util-linux", "fallocate": "util-linux", "flock": "util-linux", "getopt": "util-linux",
 "look": "util-linux", "col": "util-linux", "colcrt": "util-linux", "colrm": "util-linux",
 "column": "util-linux", "mesg": "util-linux", "wall": "util-linux", "write": "util-linux",
 "lsblk": "util-linux", "findmnt": "util-linux", "namei": "util-linux", "whereis": "util-linux",
 "script": "util-linux", "renice": "util-linux", "partx": "util-linux", "unshare": "util-linux",
 "nsenter": "util-linux", "setpriv": "util-linux", "blkid": "util-linux (read-only probe)",
 "dmesg": "util-linux", "killall5": "procps", "pidof": "procps", "pgrep": "procps",
 "pkill": "procps", "ps": "procps", "top": "procps", "free": "procps", "vmstat": "procps",
 "watch": "procps", "powertop": "powertop", "iostat": "sysstat", "mpstat": "sysstat",
 "fuser": "psmisc", "pstree": "psmisc", "lsof": "lsof", "start-stop-daemon": "dpkg (debianutils part)",
 "which": "debianutils", "run-parts": "debianutils", "tempfile": "debianutils",
 "rpm": "rpm.org", "rpm2cpio": "rpm.org", "dpkg": "dpkg", "dpkg-deb": "dpkg",
 "xxd": "Vim tree (xxd ships with Vim)",
 "eject": "util-linux", "fdflush": "util-linux", "fdformat": "util-linux",
 "findfs": "util-linux", "fstrim": "util-linux", "hd": "util-linux (hexdump name variant)",
 "ipcrm": "util-linux", "ipcs": "util-linux", "last": "util-linux", "logger": "util-linux",
 "linux32": "util-linux (setarch)", "linux64": "util-linux (setarch)",
 "mountpoint": "util-linux", "nologin": "util-linux", "rtcwake": "util-linux",
 "scriptreplay": "util-linux", "killall": "procps", "pmap": "procps", "pwdx": "procps",
 "w": "procps", "lspci": "pciutils", "lsusb": "usbutils", "netstat": "net-tools",
}
NET = set("""wget nc ping ping6 traceroute traceroute6 nslookup arp arping brctl nameif
ifup ifdown udhcpc zcip ether-wake telnet ftpget ftpput tftp ssl_client whois ipcalc
ip ipaddr iplink iproute iprule iptunnel ipneigh ifconfig route rdate tc ifenslave
vconfig slattach udhcpc6 pscan""".split())
SRV = set("""httpd ftpd telnetd tftpd dnsd udhcpd ntpd crond syslogd klogd inetd tcpsvd udpsvd
fakeidentd acpid ifplugd watchdog svlogd bootchartd sendmail popmaildir crontab
dhcprelay dumpleases logread lpd lpq lpr""".split())
INIT = set("""init getty login sulogin runlevel halt poweroff reboot runsv runsvdir sv svok
switch_root pivot_root vlock chpst envdir envuidgid setuidgid softlimit svc linuxrc
run-init""".split())
AUTH = set("""su passwd chpasswd adduser addgroup deluser delgroup add-shell remove-shell
cryptpw mkpasswd""".split())
KERN = set("""mount umount insmod rmmod lsmod modprobe depmod losetup mkswap swapon swapoff
blkdiscard blockdev fdisk sfdisk freeramdisk hdparm devmem mdev uevent setserial adjtimex
hwclock readprofile raidautorun rdev sysctl mkfs.ext2 mkfs.vfat mkfs.minix fsck fsck.minix
mke2fs mkdosfs tune2fs resize2fs fsfreeze makedevs modinfo nbd-client partprobe resume
setlogcons tunctl""".split())
HW = set("""fbset loadfont setfont kbd_mode dumpkmap loadkmap chvt deallocvt fgconsole conspy
openvt setconsole beep i2cdetect i2cdump i2cget i2cset rfkill setkeycodes showkey microcom
fbsplash gpio lsscsi volname i2ctransfer rx nanddump nandwrite ubiattach ubidetach
ubimkvol ubirename ubirmvol ubirsvol ubiupdatevol""".split())
MISC = {  # no full upstream home; custom candidates for later slices
 "chat": "custom (expect-style serial chat)", "clear": "custom (terminfo clear)",
 "reset": "custom (terminal reset)", "resize": "custom (terminal size)",
 "crc32": "custom (trivial)", "dnsdomainname": "custom (trivial)",
 "fsync": "custom (trivial)", "hexedit": "custom (hex editor)",
 "makemime": "custom (mail tools)", "reformime": "custom (mail tools)",
 "man": "custom (needs a man-db decision)", "readahead": "custom (trivial)",
 "seedrng": "custom (security-sensitive)", "sha3sum": "custom (hash tool)",
 "usleep": "custom (trivial)", "cttyhack": "custom (trivial)", "ttysize": "custom (trivial)",
}
XBB = {"ascii": "BusyBox-only novelty", "bbconfig": "BusyBox-only (its own config)",
 "mim": "BusyBox-only", "nmeter": "BusyBox-only", "pipe_progress": "BusyBox-only",
 "smemcap": "BusyBox-only"}
XSHELL = {"hush": "BusyBox's second shell; phccbox's shell answer is dash",
 "[[": "BusyBox ash extension; dash has no [[ — needs a shell decision"}

rows, counts = [], {}
def put(name, cls, src):
    rows.append((name, cls, src)); counts[cls] = counts.get(cls, 0) + 1

for a in sorted(set(applets)):
    if a in V1: put(a, "V1 — in phccbox v1", V1[a])
    elif a in cu: put(a, "CU — coreutils 9.12, v1.1 (F18-gated)", "coreutils 9.12")
    elif a in GNU2: put(a, "L-GNU — later slice, full upstream project", GNU2[a])
    elif a in UL: put(a, "L-UL — later slice, util-linux/procps family", UL[a])
    elif a in NET: put(a, "L-NET — later slice, network clients", "small project / custom")
    elif a in SRV: put(a, "X-SRV — deferred: daemon/server lifecycle", "—")
    elif a in INIT: put(a, "X-INIT — non-goal v1.x: init/session lifecycle", "—")
    elif a in AUTH: put(a, "X-AUTH — deferred: account/credential mutation", "—")
    elif a in KERN: put(a, "X-KERN — deferred: kernel-interface, privileged/destructive", "—")
    elif a in HW: put(a, "X-HW — deferred: hardware/console, no ranch test target", "—")
    elif a in XBB: put(a, "X-BB — BusyBox-only, no GNU-world counterpart", XBB[a])
    elif a in XSHELL: put(a, "X-SHELL — shell-definition question", XSHELL[a])
    elif a in MISC: put(a, "L-CUST — later slice, custom candidate", MISC[a])
    else: put(a, "UNCLASSIFIED", "?")

total = len(rows)
v1 = counts.get("V1 — in phccbox v1", 0)
cu_n = counts.get("CU — coreutils 9.12, v1.1 (F18-gated)", 0)
later = sum(v for k, v in counts.items() if k.startswith("L-"))
print(f"total={total} v1={v1} ({100*v1/total:.1f}%) cu={cu_n} v1+cu={v1+cu_n} ({100*(v1+cu_n)/total:.1f}%) later-slices={later} full-map={v1+cu_n+later} ({100*(v1+cu_n+later)/total:.1f}%)")
for k in sorted(counts): print(f"  {counts[k]:4d}  {k}")
unc = [n for n, c, s in rows if c == "UNCLASSIFIED"]
print("UNCLASSIFIED:", unc)
with open(R / "coverage-map.md", "w") as f:
    f.write("| BusyBox applet | phccbox class | Source |\n|---|---|---|\n")
    for n, c, s in rows: f.write(f"| {n} | {c} | {s} |\n")
print("wrote", R / "coverage-map.md")
