/* applets_d.c -- Exocortical family: NFS client/server, Thunderbolt fabric. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "exo_util.h"
#include "gen/lit_nfs.h"
#include "gen/lit_tb.h"

#define CHK(x) do { int _r = (x); if (_r != 0) return _r; } while (0)
static const char *SHARES[] = { "models", "shared", "exocortex", "mirrorborn", "bliss", "backups", 0 };

static char *hostname_cap(int short_form) {
    char *av[] = { "hostname", NULL, NULL };
    if (short_form) av[1] = "-s";
    int rc; char *out = exo_cap(av, &rc); (void)rc;
    if (!out) return exo_strdup("");
    char *t = exo_strdup(exo_trim(out));
    free(out);
    return t;
}
static void timestamp(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, n, "%Y%m%d%H%M%S", &tmv);
}

/* ========================================================== nfs client */
int exo_applet_nfs_provision_client(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *server = "orchid-flow", *root = "/srv/mirrorborn", *base = "/mnt/mirrorborn";
    char *hn = hostname_cap(0);
    printf("=== NFS Client Setup: %s ===\n", hn);
    printf("Server: %s\n", server);
    printf("Mount point: %s\n", base);
    printf("\n");
    printf("[1/4] Installing NFS client...\n");
    CHK(exo_vrun("apt-get", "update", "-qq", NULL));
    CHK(exo_vrun("apt-get", "install", "-y", "-qq", "nfs-common", NULL));
    printf("\n");
    printf("[2/4] Creating mount points...\n");
    for (int i = 0; SHARES[i]; i++) {
        exo_sb p; exo_sb_init(&p); exo_sb_add(&p, base); exo_sb_add(&p, "/"); exo_sb_add(&p, SHARES[i]);
        char *path = exo_sb_steal(&p);
        CHK(exo_mkdir_p(path));
        free(path);
    }
    printf("  Created %s/{models,shared,exocortex,mirrorborn,bliss,backups}\n", base);
    printf("\n");
    printf("[3/4] Configuring /etc/fstab...\n");
    {
        char ts[32]; timestamp(ts, sizeof ts);
        exo_sb bp; exo_sb_init(&bp); exo_sb_add(&bp, "/etc/fstab.bak."); exo_sb_add(&bp, ts);
        char *bak = exo_sb_steal(&bp);
        CHK(exo_cp("/etc/fstab", bak));
        free(bak);
    }
    {
        char *fstab = exo_read("/etc/fstab");
        if (fstab && strstr(fstab, "orchid-flow:/srv/mirrorborn")) {
            printf("  Entries already present in fstab, skipping\n");
        } else {
            const char *names[] = { "NFS_SERVER", "NFS_ROOT", "MOUNT_BASE", 0 };
            const char *vals[] = { server, root, base, 0 };
            char *body = exo_tmpl(NFS_FSTAB_TMPL, names, vals);
            CHK(exo_append("/etc/fstab", body, -1));
            free(body);
            printf("  Added 6 entries to /etc/fstab\n");
        }
        free(fstab);
    }
    printf("\n");
    printf("[4/4] Mounting shares...\n");
    CHK(exo_vrun("mount", "-a", NULL));
    int mounted = 0;
    for (int i = 0; SHARES[i]; i++) {
        exo_sb p; exo_sb_init(&p); exo_sb_add(&p, base); exo_sb_add(&p, "/"); exo_sb_add(&p, SHARES[i]);
        char *path = exo_sb_steal(&p);
        char *av[] = { "mountpoint", "-q", path, NULL };
        int mrc = exo_run_redir(av, NULL, NULL, 0, 1);
        if (mrc == 0) {
            printf("  ✓ %s\n", path);
            /* faithful quirk: the original runs under set -e and
               ((MOUNTED++)) yields status 1 when the old value is 0,
               so it exits silently right here on the first mounted share */
            int old = mounted;
            mounted++;
            free(path);
            if (old == 0) { free(hn); return 1; }
        } else {
            printf("  ✗ %s -- not mounted\n", path);
            free(path);
        }
    }
    printf("\n");
    printf("  Mounted: %d/6\n", mounted);
    printf("\n");
    /* Ollama symlink convenience block */
    const char *omd = "/usr/share/ollama/.ollama/models";
    {
        char *lp = exo_p(omd);
        struct stat lst;
        int is_link = lstat(lp, &lst) == 0 && S_ISLNK(lst.st_mode);
        free(lp);
        if (exo_isdir("/usr/share/ollama") && !is_link) {
            printf("Ollama detected. Symlink models dir?\n");
            printf("  %s -> %s/models/ollama\n", omd, base);
            printf("\n");
            printf("  Create symlink? [y/N] ");
            fflush(stdout);
            char line[64];
            if (fgets(line, sizeof line, stdin) && (line[0] == 'y' || line[0] == 'Y') && (line[1] == '\n' || line[1] == 0)) {
                exo_sb tp; exo_sb_init(&tp); exo_sb_add(&tp, base); exo_sb_add(&tp, "/models/ollama");
                char *target = exo_sb_steal(&tp);
                CHK(exo_mkdir_p(target));
                if (exo_isdir(omd)) {
                    printf("  Moving existing models to NFS...\n");
                    exo_sb s1; exo_sb_init(&s1); exo_sb_add(&s1, omd); exo_sb_add(&s1, "/");
                    char *src = exo_sb_steal(&s1);
                    exo_sb s2; exo_sb_init(&s2); exo_sb_add(&s2, target); exo_sb_add(&s2, "/");
                    char *dst = exo_sb_steal(&s2);
                    CHK(exo_vrun("rsync", "-a", src, dst, NULL));
                    free(src); free(dst);
                    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "RM-RF "); exo_sb_add(&l, omd);
                    char *lg = exo_sb_steal(&l); exo_log(lg); free(lg);
                    if (!exo_dry()) exo_vrun("rm", "-rf", (char *)omd, NULL);
                }
                /* ln -sf target link */
                {
                    char *lp2 = exo_p(omd);
                    if (!exo_dry()) { unlink(lp2); symlink(target, lp2); }
                    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "SYMLINK "); exo_sb_add(&l, target); exo_sb_add(&l, " "); exo_sb_add(&l, omd);
                    char *lg = exo_sb_steal(&l); exo_log(lg); free(lg);
                    free(lp2);
                }
                printf("  ✓ Symlink created\n");
                free(target);
            }
        }
    }
    CHK(exo_vrun("sudo", "systemctl", "daemon-reload", NULL));
    printf("\n");
    printf("=== NFS client setup complete on %s ===\n", hn);
    printf("\n");
    printf("Verify: df -h %s/models\n", base);
    printf("\n");
    free(hn);
    return 0;
}

/* ========================================================== nfs server */
int exo_applet_nfs_provision_server(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *root = "/srv/mirrorborn", *subnet = "192.168.86.0/24";
    printf("=== NFS Server Setup: orchid-flow ===\n");
    printf("Export root: %s\n", root);
    printf("Allowed subnet: %s\n", subnet);
    printf("\n");
    printf("[1/5] Installing NFS kernel server...\n");
    CHK(exo_vrun("apt-get", "update", "-qq", NULL));
    CHK(exo_vrun("apt-get", "install", "-y", "-qq", "nfs-kernel-server", "nfs-common", NULL));
    printf("\n");
    printf("[2/5] Creating export directories...\n");
    for (int i = 0; SHARES[i]; i++) {
        exo_sb p; exo_sb_init(&p); exo_sb_add(&p, root); exo_sb_add(&p, "/"); exo_sb_add(&p, SHARES[i]);
        char *path = exo_sb_steal(&p);
        CHK(exo_mkdir_p(path));
        free(path);
    }
    { char *rp = exo_p(root);
      CHK(exo_vrun("chown", "-R", "wbic16:wbic16", rp, NULL));
      CHK(exo_vrun("chmod", "-R", "755", rp, NULL));
      free(rp); }
    printf("  Created: models, shared, exocortex, mirrorborn, bliss, backups\n");
    printf("\n");
    printf("[3/5] Configuring /etc/exports...\n");
    if (exo_exists("/etc/exports")) {
        char ts[32]; timestamp(ts, sizeof ts);
        exo_sb bp; exo_sb_init(&bp); exo_sb_add(&bp, "/etc/exports.bak."); exo_sb_add(&bp, ts);
        char *bak = exo_sb_steal(&bp);
        CHK(exo_cp("/etc/exports", bak));
        free(bak);
    }
    {
        const char *names[] = { "EXPORT_ROOT", "MB_SUBNET", 0 };
        const char *vals[] = { root, subnet, 0 };
        char *body = exo_tmpl(NFS_EXPORTS_TMPL, names, vals);
        CHK(exo_append("/etc/exports", body, -1));
        free(body);
    }
    printf("  Exports written to /etc/exports\n");
    printf("\n");
    printf("[4/5] Applying exports and starting NFS...\n");
    CHK(exo_vrun("exportfs", "-ra", NULL));
    CHK(exo_vrun("systemctl", "enable", "nfs-kernel-server", NULL));
    CHK(exo_vrun("systemctl", "restart", "nfs-kernel-server", NULL));
    printf("  NFS server running\n");
    printf("\n");
    printf("[5/5] Checking firewall...\n");
    {
        int active = 0;
        if (exo_which("ufw")) {
            int rc; char *av[] = { "ufw", "status", NULL };
            char *out = exo_cap(av, &rc); (void)rc;
            if (out && strstr(out, "active")) active = 1;
            free(out);
        }
        if (active) {
            CHK(exo_vrun("ufw", "allow", "from", (char *)subnet, "to", "any", "port", "nfs", NULL));
            CHK(exo_vrun("ufw", "allow", "from", (char *)subnet, "to", "any", "port", "111", NULL));
            CHK(exo_vrun("ufw", "allow", "from", (char *)subnet, "to", "any", "port", "2049", NULL));
            printf("  Opened NFS ports for %s\n", subnet);
        } else {
            printf("  ufw not active, skipping firewall rules\n");
        }
    }
    printf("\n");
    printf("=== orchid-flow NFS server ready ===\n");
    printf("\n");
    printf("Verify exports:\n");
    printf("  exportfs -v\n");
    printf("\n");
    printf("Shares available:\n");
    {
        int rc; char *av[] = { "exportfs", "-v", NULL };
        char *out = exo_cap(av, &rc); (void)rc;
        if (out) {
            char *copy = exo_strdup(out);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save))
                printf("  %s\n", ln);
            free(copy);
            free(out);
        }
    }
    printf("\n");
    printf("Next: run nfs-provision-client.sh on each node.\n");
    printf("\n");
    return 0;
}

/* ========================================================== tb-setup */
int exo_applet_tb_setup(int argc, char **argv) {
    (void)argc; (void)argv;
    char *hn = hostname_cap(1);
    const char *addr = NULL;
    for (int i = 0; TB_MAP[i]; i += 2)
        if (strcmp(TB_MAP[i], hn) == 0) { addr = TB_MAP[i + 1]; break; }
    if (!addr) {
        printf("ERROR: hostname '%s' not found in TB map\n", hn);
        free(hn);
        return 1;
    }
    const char *iface = "thunderbolt0";
    {
        char *a1[] = { "modprobe", "thunderbolt", NULL };
        exo_run_redir(a1, NULL, NULL, 0, 1);
        char *a2[] = { "modprobe", "thunderbolt_net", NULL };
        exo_run_redir(a2, NULL, NULL, 0, 1);
    }
    int found = 0;
    for (int i = 1; i <= 5; i++) {
        char *av[] = { "ip", "link", "show", (char *)iface, NULL };
        if (exo_run_redir(av, NULL, NULL, 0, 1) == 0) { found = 1; break; }
        printf("Waiting for %s... (%d/5)\n", iface, i);
        exo_vrun("sleep", "2", NULL);
    }
    /* the original re-checks unconditionally after the loop (its line 60
       error gate), so a successful wait still costs a second probe */
    {
        char *av[] = { "ip", "link", "show", (char *)iface, NULL };
        found = (exo_run_redir(av, NULL, NULL, 0, 1) == 0);
    }
    if (!found) {
        printf("ERROR: %s not found\n", iface);
        free(hn);
        return 1;
    }
    CHK(exo_vrun("ip", "addr", "flush", "dev", (char *)iface, NULL));
    {
        char *av[] = { "ip", "link", "add", "name", "tbfab", "type", "bridge", NULL };
        exo_run_redir(av, NULL, NULL, 0, 1);
    }
    CHK(exo_vrun("ip", "link", "set", "tbfab", "up", NULL));
    CHK(exo_vrun("ip", "link", "set", "tbfab", "mtu", "65520", NULL));
    /* enslave every thunderbolt* interface in /sys/class/net */
    int ports = 0;
    {
        char *netdir = exo_p("/sys/class/net");
        DIR *d = opendir(netdir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (strncmp(e->d_name, "thunderbolt", 11) == 0) {
                    ports++;
                    CHK(exo_vrun("ip", "link", "set", e->d_name, "up", "mtu", "65520", NULL));
                    CHK(exo_vrun("ip", "link", "set", e->d_name, "master", "tbfab", NULL));
                    CHK(exo_vrun("ip", "link", "set", e->d_name, "promisc", "on", NULL));
                }
            }
            closedir(d);
        }
        free(netdir);
    }
    CHK(exo_vrun("ip", "addr", "flush", "dev", "tbfab", NULL));
    {
        exo_sb a; exo_sb_init(&a); exo_sb_add(&a, addr); exo_sb_add(&a, "/16");
        char *cidr = exo_sb_steal(&a);
        CHK(exo_vrun("ip", "addr", "add", cidr, "dev", "tbfab", NULL));
        free(cidr);
    }
    printf("[%s] tbfab -> %s/16 with %d ports\n", hn, addr, ports);
    CHK(exo_vrun("ip", "link", "set", (char *)iface, "up", NULL));
    printf("OK: %s -> %s @ %s/24\n", hn, iface, addr);
    if (!exo_exists("/etc/systemd/system/tb-fabric.service")) {
        exo_tee("/etc/systemd/system/tb-fabric.service", TB_FABRIC_UNIT, -1, 0);
        CHK(exo_vrun("sudo", "systemctl", "daemon-reload", NULL));
        CHK(exo_vrun("sudo", "systemctl", "enable", "tb-fabric.service", NULL));
        printf("[tb-fabric]: Installed\n");
    } else {
        printf("[tb-fabric]: OK\n");
    }
    free(hn);
    return 0;
}
