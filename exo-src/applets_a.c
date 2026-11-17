/* applets_a.c -- Exocortical family: small utility applets.
 * benchmark, browse, cluster-status, rpush, rag, hwe-enable, lemonade,
 * maintenance, tb-auth, wifi-optimizations, hello-phext, install.
 * Behaviour contracts: ~/workspace/phccbox/exocortical-inventory.md */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <glob.h>
#include "exo_util.h"

#define ROOTED (exo_root()[0] || exo_dry())

/* ------------------------------------------------------------------ */
/* benchmark.sh: if /opt/beebjit exists, cd there, ./build.sh, ./benchmark.sh */
int exo_applet_benchmark(int argc, char **argv) {
    (void)argc; (void)argv;
    if (exo_isdir("/opt/beebjit")) {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
        char *d = exo_p("/opt/beebjit");
        chdir(d);   /* a failed cd leaves the shell in cwd; the runs still happen */
        exo_vrun("./build.sh", NULL);
        exo_vrun("./benchmark.sh", NULL);
        if (cwd[0]) chdir(cwd);
        free(d);
    }
    return 0;
}

/* browse.sh: avahi-browse _exocortex_exocortical._tcp */
int exo_applet_browse(int argc, char **argv) {
    (void)argc; (void)argv;
    return exo_vrun("avahi-browse", "_exocortex_exocortical._tcp", NULL);
}

/* cluster-status.sh: per host, ping lines with "ms" + "bytes from",
   excluding "packet loss" lines */
int exo_applet_cluster_status(int argc, char **argv) {
    (void)argc; (void)argv;
    static const char *hosts[] = {
        "logos-prime", "halcyon-vector", "aletheia-core",
        "chrysalis-hub", "aurora-continuum", 0
    };
    for (int i = 0; hosts[i]; i++) {
        char *av[] = { "ping", "-c", "1", (char *)hosts[i], NULL };
        int rc;
        char *out = exo_cap(av, &rc);
        (void)rc;
        char *save = NULL;
        for (char *ln = strtok_r(out, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
            if (strstr(ln, "ms") && strstr(ln, "bytes from") && !strstr(ln, "packet loss"))
                printf("%s\n", ln);
        }
        free(out);
    }
    return 0;
}

/* rpush.sh: rsync -varzP <dir> <acct>@<server>:<dir>/ */
int exo_applet_rpush(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <dir> <server> (<account>)\n", argv[0]);
        return 1;
    }
    const char *acct = argc > 3 ? argv[3] : "wbic16";
    exo_sb d; exo_sb_init(&d);
    exo_sb_addf(&d, "%s@%s:%s/", acct, argv[2], argv[1]);
    char *dest = exo_sb_steal(&d);
    int rc = exo_vrun("rsync", "-varzP", argv[1], dest, NULL);
    free(dest);
    return rc;
}

/* rag.sh: /opt/exopy/bin/python rag.py <src> <model> <query>, stderr discarded */
int exo_applet_rag(int argc, char **argv) {
    if (argc < 2 || !argv[1][0]) {
        printf("Usage: %s <query> (optional: <directory>)\n", argv[0]);
        /* the usage line embeds argv[0], exactly like the original's $0 */
        return 1;
    }
    const char *src = argc > 2 ? argv[2] : "/usr/local/exollama";
    const char *model = argc > 3 ? argv[3] : "llama3.2";
    char *av[] = { "/opt/exopy/bin/python", "rag.py", (char *)src, (char *)model, argv[1], NULL };
    return exo_run_redir(av, NULL, NULL, 0, 1);
}

/* hwe-enable.sh: sudo apt install linux-generic-hwe-24.04 */
int exo_applet_hwe_enable(int argc, char **argv) {
    (void)argc; (void)argv;
    return exo_vrun("sudo", "apt", "install", "linux-generic-hwe-24.04", NULL);
}

/* lemonade.sh */
int exo_applet_lemonade(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_vrun("sudo", "add-apt-repository", "ppa:lemonade-team/stable", NULL);
    return exo_vrun("sudo", "apt", "install", "lemonade-server", NULL);
}

/* maintenance.sh: openclaw doctor; apt update && upgrade; git status; openclaw update */
int exo_applet_maintenance(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_vrun("openclaw", "doctor", NULL);
    int rc = exo_vrun("sudo", "apt", "update", NULL);
    if (rc == 0) exo_vrun("sudo", "apt", "upgrade", NULL);
    exo_vrun("git", "status", NULL);
    return exo_vrun("openclaw", "update", NULL);
}

/* tb-auth.sh: udev rule via sudo tee (tee echoes the rule to stdout) */
int exo_applet_tb_auth(int argc, char **argv) {
    (void)argc; (void)argv;
    static const char rule[] =
        "ACTION==\"add\", SUBSYSTEM==\"thunderbolt\", ATTR{authorized}==\"0\", ATTR{authorized}=\"1\"\n";
    exo_tee("/etc/udev/rules.d/99-thunderbolt-auto.rules", rule, -1, 0);
    exo_vrun("sudo", "udevadm", "control", "--reload-rules", NULL);
    return exo_vrun("sudo", "udevadm", "trigger", "--subsystem-match=thunderbolt", NULL);
}

/* wifi-optimizations.sh */
int exo_applet_wifi_optimizations(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_vrun("sudo", "sysctl", "-w", "net.core.default_qdisc=fq", NULL);
    exo_vrun("sudo", "sysctl", "-w", "net.ipv4.tcp_congestion_control=bbr", NULL);
    exo_tee("/etc/sysctl.conf", "net.core.default_qdisc=fq\n", -1, 1);
    exo_tee("/etc/sysctl.conf", "net.ipv4.tcp_congestion_control=bbr\n", -1, 1);
    return 0;
}

/* hello-phext.sh: ROCKET_ADDRESS=0.0.0.0; exec ~/.cargo/bin/hello-phext */
int exo_applet_hello_phext(int argc, char **argv) {
    (void)argc; (void)argv;
    setenv("ROCKET_ADDRESS", "0.0.0.0", 1);
    exo_sb p; exo_sb_init(&p);
    exo_sb_add(&p, exo_home()); exo_sb_add(&p, "/.cargo/bin/hello-phext");
    char *path = exo_sb_steal(&p);
    if (getenv("EXO_TRANSCRIPT")) {
        exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "EXEC "); exo_sb_add(&l, path);
        char *line = exo_sb_steal(&l); exo_log(line); free(line);
    }
    if (exo_dry()) { free(path); return 0; }
    execl(path, "hello-phext", (char *)0);
    fprintf(stderr, "hello-phext: cannot exec %s\n", path);
    free(path);
    return 127;
}

/* install.sh: install rag.* from cwd into /usr/local/exocortical */
int exo_applet_install(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *dest = "/usr/local/exocortical";
    if (!exo_isdir(dest)) {
        if (ROOTED) {
            exo_mkdir_p(dest);
        } else {
            exo_vrun("sudo", "mkdir", (char *)dest, NULL);
            exo_sb co; exo_sb_init(&co);
            exo_sb_addf(&co, "%s:%s", exo_user(), exo_user());
            char *owner = exo_sb_steal(&co);
            exo_vrun("sudo", "chown", owner, (char *)dest, NULL);
            free(owner);
        }
        if (ROOTED && getenv("EXO_TRANSCRIPT")) {
            exo_sb l; exo_sb_init(&l);
            exo_sb_addf(&l, "CHOWN %s:%s %s", exo_user(), exo_user(), dest);
            char *line = exo_sb_steal(&l); exo_log(line); free(line);
        }
    }
    /* glob rag.* in cwd, like the shell */
    char *matches[256]; int n = 0;
    {
        glob_t g;
        if (glob("rag.*", 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc && n < 255; i++)
                matches[n++] = exo_strdup(g.gl_pathv[i]);
            globfree(&g);
        }
    }
    if (n == 0) {
        fprintf(stderr, "cp: cannot stat 'rag.*': No such file or directory\n");
        return 1;
    }
    if (ROOTED) {
        int rc = 0;
        for (int i = 0; i < n; i++) if (exo_cp(matches[i], dest) != 0) rc = 1;
        for (int i = 0; i < n; i++) free(matches[i]);
        return rc;
    }
    char *av[260]; int k = 0;
    av[k++] = "cp";
    for (int i = 0; i < n; i++) av[k++] = matches[i];
    av[k++] = "/usr/local/exocortical/";
    av[k] = NULL;
    int rc = exo_run(av);
    for (int i = 0; i < n; i++) free(matches[i]);
    return rc;
}
