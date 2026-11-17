/* applets_b.c -- Exocortical family: proxy/npm, keys, go, model sync,
 * memory (zram/swap), disk expansion, ssd fixup. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include "exo_util.h"
#include "gen/lit_usb.h"

#define ROOTED (exo_root()[0] || exo_dry())

/* ---------------------------------------------------------- npm proxy */
/* enable-npm-proxy.sh <squid-ip> */
int exo_applet_enable_npm_proxy(int argc, char **argv) {
    if (argc < 2 || !argv[1][0]) {
        printf("Usage: %s <squid-ip>\n", argv[0]);
        return 1;
    }
    const char *ca = "/usr/local/share/ca-certificates/ranch-choir-squid-ca.crt";
    if (!exo_exists(ca)) {
        printf("ERROR: Install the Squid CA first.\n");
        return 1;
    }
    char ph[300], ps[300];
    snprintf(ph, sizeof ph, "http://%s:3128", argv[1]);
    snprintf(ps, sizeof ps, "http://%s:3129", argv[1]);
    exo_vrun("npm", "config", "set", "proxy", ph, "--global", NULL);
    exo_vrun("npm", "config", "set", "proxy", ph, NULL);
    exo_vrun("npm", "config", "set", "https-proxy", ps, "--global", NULL);
    exo_vrun("npm", "config", "set", "https-proxy", ps, NULL);
    exo_vrun("npm", "config", "set", "cafile", (char *)ca, NULL);
    exo_vrun("npm", "config", "get", "proxy", NULL);
    exo_vrun("npm", "config", "get", "https-proxy", NULL);
    exo_vrun("sudo", "update-ca-certificates", NULL);
    return exo_vrun("npm", "view", "openclaw", "version", NULL);
}

/* ---------------------------------------------------------- install-keys */
#include "gen/lit_keys.h"
static void raw_mkdir_log(const char *path) {
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "MKDIR "); exo_sb_add(&l, path);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (!exo_dry()) {
        /* mkdir -p for a single-level home subdir */
        char *tmp = exo_strdup(path);
        for (char *q = tmp + 1; ; q++) {
            if (*q == '/' || *q == 0) { char s = *q; *q = 0; mkdir(tmp, 0777); *q = s; if (!s) break; }
        }
        free(tmp);
    }
}
static void raw_chmod_log(const char *path, mode_t m) {
    exo_sb l; exo_sb_init(&l); exo_sb_addf(&l, "CHMOD %o ", (unsigned)m); exo_sb_add(&l, path);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (!exo_dry()) chmod(path, m);
}
/* install-keys.sh: 12 Mirrorborn keys, matched on the key blob (field 2) */
int exo_applet_install_keys(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_sb sp; exo_sb_init(&sp); exo_sb_add(&sp, exo_home()); exo_sb_add(&sp, "/.ssh");
    char *sshdir = exo_sb_steal(&sp);
    exo_sb ap; exo_sb_init(&ap); exo_sb_add(&ap, sshdir); exo_sb_add(&ap, "/authorized_keys");
    char *auth = exo_sb_steal(&ap);
    raw_mkdir_log(sshdir);
    raw_chmod_log(sshdir, 0700);
    /* touch */
    if (!exo_dry()) { FILE *f = fopen(auth, "a"); if (f) fclose(f); }
    raw_chmod_log(auth, 0600);
    char *content = exo_read_raw(auth);
    if (!content) content = exo_strdup("");
    int added = 0, skipped = 0, total = 0;
    for (int i = 0; MIRRORBORN_KEYS[i]; i++) {
        total++;
        const char *key = MIRRORBORN_KEYS[i];
        /* field 2 = blob */
        const char *b = strchr(key, ' ');
        const char *e = b ? strchr(b + 1, ' ') : NULL;
        char *blob = e ? exo_strndup(b + 1, (size_t)(e - (b + 1))) : exo_strdup(b ? b + 1 : "");
        if (strstr(content, blob)) { skipped++; }
        else {
            if (!exo_dry()) {
                FILE *f = fopen(auth, "a");
                if (f) { fprintf(f, "%s\n", key); fclose(f); }
            }
            exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "APPEND "); exo_sb_add(&l, auth);
            char *line = exo_sb_steal(&l); exo_log(line); free(line);
            added++;
        }
        free(blob);
    }
    printf("install-keys: %d added, %d already present (%d total)\n", added, skipped, total);
    free(content); free(sshdir); free(auth);
    return 0;
}

/* ---------------------------------------------------------- fast-go-setup */
int exo_applet_fast_go_setup(int argc, char **argv) {
    (void)argc; (void)argv;
    static const char *RELEASE = "go1.25.6.linux-amd64.tar.gz";
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    exo_sb ap; exo_sb_init(&ap); exo_sb_add(&ap, cwd); exo_sb_add(&ap, "/"); exo_sb_add(&ap, RELEASE);
    char *archive = exo_sb_steal(&ap);
    if (access(archive, F_OK) != 0) {
        exo_sb u; exo_sb_init(&u); exo_sb_add(&u, "https://go.dev/dl/"); exo_sb_add(&u, RELEASE);
        char *url = exo_sb_steal(&u);
        exo_vrun("wget", url, NULL);
        free(url);
    }
    char *dest = exo_p("/usr/local");
    char save[PATH_MAX];
    if (getcwd(save, sizeof save)) {
        if (chdir(dest) == 0) {
            struct stat st;
            if (stat("go", &st) != 0) exo_vrun("tar", "zxvf", archive, NULL);
            chdir(save);
        }
    }
    free(dest);
    exo_sb bp; exo_sb_init(&bp); exo_sb_add(&bp, exo_home()); exo_sb_add(&bp, "/.bashrc");
    char *bashrc_path = exo_sb_steal(&bp);
    char *bashrc = exo_read_raw(bashrc_path);
    free(bashrc_path);
    if (!bashrc) bashrc = exo_strdup("");
    long have_go_bin = exo_count_lines_containing(bashrc, "$HOME/go/bin");
    if (have_go_bin == 0) {
        exo_sb gp; exo_sb_init(&gp); exo_sb_add(&gp, exo_home()); exo_sb_add(&gp, "/go/bin");
        char *gb = exo_sb_steal(&gp);
        struct stat st;
        if (stat(gb, &st) == 0 && S_ISDIR(st.st_mode))
            printf("Warning: You have a user-specific Go bin folder, but it isn't in your path yet.\n");
        free(gb);
    }
    long have_go = exo_count_lines_containing(bashrc, "usr/local/go/bin");
    if (have_go == 1) {
        char *av[] = { "go", NULL };
        int rc;
        char *out = exo_cap_err(av, &rc);
        (void)rc;
        if (exo_count_lines_containing(out, "Go is a tool for managing Go source code") == 1)
            printf("Go ready for use.\n");
        else
            printf("You probably need to add /usr/local/go/bin to your PATH\n");
        free(out);
    } else {
        printf("You need to patch $HOME/.bashrc to include the line below\n");
        printf("\n");
        printf("export PATH=\"$PATH:/usr/local/go/bin:$HOME/go/bin\"\n");
    }
    free(bashrc); free(archive);
    return 0;
}

/* ---------------------------------------------------------- sync-ollama-models */
int exo_applet_sync_ollama_models(int argc, char **argv) {
    if (argc < 2 || !argv[1][0]) {
        fprintf(stderr, "%s: 1: Usage: ollama-sync.sh <remote-host>\n", argv[0]);
        return 1;
    }
    const char *remote = argv[1];
    const char *models = "/usr/share/ollama/.ollama/models/";
    printf("=== Terminating ollama instances ===\n");
    exo_vrun("sudo", "killall", "ollama", NULL);
    exo_vrun("ssh", (char *)remote, "sudo killall ollama", NULL);
    char owner[128];
    snprintf(owner, sizeof owner, "%s:%s", exo_user(), exo_user());
    char ssh_chown[256];
    snprintf(ssh_chown, sizeof ssh_chown, "sudo chown -R %s /usr/share/ollama", owner);
    exo_vrun("ssh", (char *)remote, ssh_chown, NULL);
    exo_vrun("sudo", "chown", "-R", owner, "/usr/share/ollama", NULL);
    printf("=== Pulling from %s ===\n", remote);
    exo_sb rp; exo_sb_init(&rp); exo_sb_addf(&rp, "%s:%s", remote, models);
    char *rsrc = exo_sb_steal(&rp);
    exo_vrun("rsync", "-av", "--progress", rsrc, (char *)models, NULL);
    printf("=== Pushing to %s ===\n", remote);
    exo_vrun("rsync", "-av", "--progress", (char *)models, rsrc, NULL);
    free(rsrc);
    printf("=== Fixing ownership ===\n");
    exo_vrun("sudo", "chown", "-R", "ollama:ollama", "/usr/share/ollama", NULL);
    exo_vrun("ssh", (char *)remote, "sudo chown -R ollama:ollama /usr/share/ollama", NULL);
    printf("Done.\n");
    return 0;
}

/* ---------------------------------------------------------- enable-zram */
int exo_applet_enable_zram(int argc, char **argv) {
    (void)argc; (void)argv;
    char *mods = exo_read_raw("/proc/modules");
    long loaded = mods ? exo_count_lines_containing(mods, "zram") : 0;
    free(mods);
    if (loaded != 0) {
        exo_vrun("swapoff", "/dev/zram0", NULL);
        exo_vrun("rmmod", "zram", NULL);
    }
    exo_vrun("modprobe", "zram", NULL);
    long kb = exo_memtotal_kb();
    if (kb < 0) kb = 0;
    long long mem = (long long)kb * 1024 * 15 / 10;
    printf("Allocated %lld GB\n", (long long)kb / 1024 / 1024);
    char num[32];
    snprintf(num, sizeof num, "%lld", mem);
    exo_write("/sys/block/zram0/disksize", num, -1);
    exo_vrun("mkswap", "-fq", "/dev/zram0", NULL);
    exo_vrun("swapon", "-p", "5", "/dev/zram0", NULL);
    return exo_vrun("zramctl", NULL);
}

/* ---------------------------------------------------------- enable-virtual-memory */
int exo_applet_enable_virtual_memory(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *done = "/etc/exocortical-virtual-memory.done";
    if (exo_exists(done)) {
        printf("Virtual memory already enabled\n");
        char *av[] = { "top", "-bn1", NULL };
        int rc; char *out = exo_cap(av, &rc); (void)rc;
        char *save = NULL;
        for (char *ln = strtok_r(out, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save))
            if (exo_contains_ci(ln, "swap")) printf("%s\n", ln);
        free(out);
        return 0;
    }
    if (!exo_isdir("/var/cache/swap")) {
        if (ROOTED) {
            exo_mkdir_p("/var/cache/swap");
            printf("mkdir: created directory '/var/cache/swap'\n");
        } else {
            exo_vrun("sudo", "mkdir", "-v", "/var/cache/swap", NULL);
        }
    }
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    char *dir = exo_p("/var/cache/swap");
    if (chdir(dir) != 0) { free(dir); return 1; }
    free(dir);
    struct stat st;
    if (stat("swapfile", &st) != 0) {
        exo_vrun("sudo", "dd", "if=/dev/zero", "of=swapfile", "bs=4K", "count=20M", NULL);
        exo_vrun("sudo", "chmod", "600", "swapfile", NULL);
        exo_vrun("sudo", "mkswap", "swapfile", NULL);
    }
    exo_vrun("sudo", "swapon", "swapfile", NULL);
    exo_vrun("sudo", "swaplabel", "swapfile", NULL);
    if (cwd[0]) chdir(cwd);
    char *fstab = exo_read("/etc/fstab");
    long n = fstab ? exo_count_lines_containing(fstab, "/var/cache/swap/swapfile") : 0;
    free(fstab);
    if (n == 0) {
        exo_tee("/etc/fstab", "/var/cache/swap/swapfile none swap sw 0 0\n", -1, 1);
        printf("Added swapfile to fstab\n");
    }
    exo_tee(done, "Done\n", -1, 0);
    return 0;
}

/* ---------------------------------------------------------- nvme-expand-disk */
int exo_applet_nvme_expand_disk(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *done = "/etc/exocortical-nvme.done";
    if (!exo_exists(done)) {
        exo_vrun("sudo", "growpart", "/dev/nvme0n1", "3", NULL);
        exo_vrun("sudo", "lsblk", "/dev/nvme0n1p3", NULL);
        exo_vrun("sudo", "lvextend", "-l+100%FREE", "/dev/ubuntu-vg/ubuntu-lv", NULL);
        exo_vrun("sudo", "resize2fs", "/dev/mapper/ubuntu--vg-ubuntu--lv", NULL);
        if (ROOTED) exo_write(done, "", 0);
        else exo_vrun("sudo", "touch", (char *)done, NULL);
    }
    exo_vrun("sudo", "pvs", NULL);
    exo_vrun("sudo", "vgs", NULL);
    return exo_vrun("df", "-h", NULL);
}

/* ---------------------------------------------------------- usb-ssd-fixup */
int exo_applet_usb_ssd_fixup(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!exo_exists("/etc/sysctl.d/99-minimize-disk-io.conf")) {
        exo_tee("/etc/sysctl.d/99-minimize-disk-io.conf", USB_SYSCTL_CONF, -1, 0); /* orig: cat <<EOF | sudo tee (echoes) */
        exo_vrun("sudo", "sysctl", "--system", NULL);
    }
    exo_vrun("sudo", "systemctl", "disable", "--now", "unattended-upgrades",
             "apt-daily.timer", "apt-daily-upgrade.timer", NULL);
    exo_tee("/sys/block/sda/queue/scheduler", "none\n", -1, 0);
    /* sed -i: first occurrence per line, / and /boot ext4 lines */
    char *fstab = exo_read("/etc/fstab");
    if (fstab) {
        exo_sb out; exo_sb_init(&out);
        char *copy = exo_strdup(fstab);
        char *save = NULL;
        for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
            /* sed s/// without g: first occurrence only, per line */
            const char *pats[2] = { "/ ext4 defaults 0", "/boot ext4 defaults 0" };
            const char *reps[2] = { "/ ext4 defaults,noatime 0", "/boot ext4 defaults,noatime 0" };
            char *cur = exo_strdup(ln);
            for (int i = 0; i < 2; i++) {
                char *hit = strstr(cur, pats[i]);
                if (hit) {
                    exo_sb b; exo_sb_init(&b);
                    exo_sb_addn(&b, cur, (size_t)(hit - cur));
                    exo_sb_add(&b, reps[i]);
                    exo_sb_add(&b, hit + strlen(pats[i]));
                    free(cur);
                    cur = exo_sb_steal(&b);
                }
            }
            exo_sb_add(&out, cur);
            exo_sb_add(&out, "\n");
            free(cur);
        }
        free(copy);
        char *newf = exo_sb_steal(&out);
        /* preserve absence of trailing newline like sed would (sed keeps line structure;
           our rebuild adds one -- acceptable if original ended with newline, which fstab does) */
        exo_write("/etc/fstab", newf, -1);
        free(newf);
        free(fstab);
    }
    if (!exo_exists("/etc/systemd/journald.conf.d/volatile.conf")) {
        if (ROOTED) exo_mkdir_p("/etc/systemd/journald.conf.d");
        else exo_vrun("sudo", "mkdir", "-p", "/etc/systemd/journald.conf.d", NULL);
        exo_tee("/etc/systemd/journald.conf.d/volatile.conf", USB_JOURNALD_CONF, -1, 0);
        exo_vrun("sudo", "systemctl", "restart", "systemd-journald", NULL);
    }
    return 0;
}
