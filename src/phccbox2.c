/* phccbox — a multicall binary that replaces BusyBox with full GNU
 * implementations, compiled by phcc.
 *
 * Each applet is the real program's main() renamed at bundle time
 * (ld -r + objcopy --redefine-sym, all other globals localized), so the
 * code inside an applet is byte-for-byte the code of the standalone
 * phcc-built tool.  Applets whose standalone program varies behaviour by
 * argv[0] are driven through the applet name; applets whose program does
 * NOT look at argv[0] for the variant (measured: GNU grep for egrep/
 * fgrep) get the equivalent option injected by a wrapper instead.
 *
 * Dispatch:
 *   argv[0] basename == applet name          -> run it
 *   phccbox <applet> [args...]               -> run it
 *   phccbox --list                           -> applet names, one per line
 *   phccbox --install DIR                    -> symlink farm in DIR
 *   unknown name                             -> error, exit 127
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <limits.h>

int sed_main(int argc, char **argv);
int grep_main(int argc, char **argv);
int awk_main(int argc, char **argv);
int sh_main(int argc, char **argv);
int bzip2_main(int argc, char **argv);
int gzip_main(int argc, char **argv);
#ifdef WITH_VIM
int vim_main(int argc, char **argv);


#endif

/* ---- Exocortical family (task 35.6.8): native C ports of wbic16/exocortical
 * @ 0872ca2a. Inventory: ~/workspace/phccbox/exocortical-inventory.md -------- */
int exo_applet_benchmark(int argc, char **argv);
int exo_applet_browse(int argc, char **argv);
int exo_applet_cluster_status(int argc, char **argv);
int exo_applet_rpush(int argc, char **argv);
int exo_applet_rag(int argc, char **argv);
int exo_applet_hwe_enable(int argc, char **argv);
int exo_applet_lemonade(int argc, char **argv);
int exo_applet_maintenance(int argc, char **argv);
int exo_applet_tb_auth(int argc, char **argv);
int exo_applet_wifi_optimizations(int argc, char **argv);
int exo_applet_hello_phext(int argc, char **argv);
int exo_applet_install(int argc, char **argv);
int exo_applet_enable_npm_proxy(int argc, char **argv);
int exo_applet_install_keys(int argc, char **argv);
int exo_applet_fast_go_setup(int argc, char **argv);
int exo_applet_sync_ollama_models(int argc, char **argv);
int exo_applet_enable_zram(int argc, char **argv);
int exo_applet_enable_virtual_memory(int argc, char **argv);
int exo_applet_nvme_expand_disk(int argc, char **argv);
int exo_applet_usb_ssd_fixup(int argc, char **argv);
int exo_applet_provision_squid_client(int argc, char **argv);
int exo_applet_provision_squid_server(int argc, char **argv);
int exo_applet_squid_uninstall_client(int argc, char **argv);
int exo_applet_squid_cache_monitor(int argc, char **argv);
int exo_applet_enable_browser_sandbox(int argc, char **argv);
int exo_applet_nfs_provision_client(int argc, char **argv);
int exo_applet_nfs_provision_server(int argc, char **argv);
int exo_applet_tb_setup(int argc, char **argv);
int exo_applet_setup(int argc, char **argv);
int exo_applet_ssh_setup(int argc, char **argv);
int exo_applet_setup_phext_mcp(int argc, char **argv);
int exo_applet_patch_libuv(int argc, char **argv);
int exo_applet_repair_openclaw(int argc, char **argv);
int exo_applet_nvidia_setup(int argc, char **argv);
int exo_applet_patch_amd_gpu(int argc, char **argv);
int exo_applet_tailscale(int argc, char **argv);
int exo_applet_optimize_llm_inference(int argc, char **argv);
int exo_applet_exo_agentmail(int argc, char **argv);
int exo_applet_exo_console(int argc, char **argv);
int exo_applet_ports(int argc, char **argv);


/* ---- custom applets: behaviour small enough to be complete ---------- */
static int true_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int false_main(int argc, char **argv) { (void)argc; (void)argv; return 1; }

static int yes_main(int argc, char **argv)
{
    /* GNU yes: operands joined by single spaces, or "y"; line + '\n',
       repeated forever.  SIGPIPE keeps its default disposition, so a
       closed pipe kills the process exactly as it does for GNU yes. */
    char *line;
    size_t len = 0, i;
    if (argc > 1) {
        for (i = 1; i < (size_t)argc; i++) len += strlen(argv[i]) + 1;
        line = malloc(len + 1);
        if (!line) return 1;
        line[0] = '\0';
        for (i = 1; i < (size_t)argc; i++) {
            if (i > 1) strcat(line, " ");
            strcat(line, argv[i]);
        }
    } else {
        line = (char *)"y";
    }
    for (;;) {
        if (puts(line) == EOF) return 1;   /* e.g. EIO / ENOSPC on redirect */
    }
}

/* ---- option-injecting wrapper (egrep, fgrep, gunzip, zcat, ...) ----- */
typedef int (*main_fn)(int, char **);
struct wrapper { main_fn fn; const char *opt; };
static int wrap_call(main_fn fn, const char *opt, int argc, char **argv)
{
    char **nv = malloc(sizeof(char *) * (argc + 2));
    int i;
    if (!nv) return 1;
    nv[0] = argv[0];
    nv[1] = (char *)opt;
    for (i = 1; i < argc; i++) nv[i + 1] = argv[i];
    nv[argc + 1] = NULL;
    return fn(argc + 1, nv);
}
static int egrep_main(int argc, char **argv)  { return wrap_call(grep_main,  "-E",  argc, argv); }
static int fgrep_main(int argc, char **argv)  { return wrap_call(grep_main,  "-F",  argc, argv); }
static int gunzip_main(int argc, char **argv) { return wrap_call(gzip_main,  "-d",  argc, argv); }
static int zcat_main(int argc, char **argv)   { return wrap_call(gzip_main,  "-dc", argc, argv); }
static int bunzip2_main(int argc, char **argv){ return wrap_call(bzip2_main, "-d",  argc, argv); }
static int bzcat_main(int argc, char **argv)  { return wrap_call(bzip2_main, "-dc", argc, argv); }

/* ---- applet table ----------------------------------------------------- */
struct applet { const char *name; main_fn fn; };
static const struct applet applets[] = {
    { "awk",     awk_main },
    { "ash",     sh_main },
    { "bzip2",   bzip2_main },
    { "bunzip2", bunzip2_main },
    { "bzcat",   bzcat_main },
    { "egrep",   egrep_main },
    { "false",   false_main },
    { "fgrep",   fgrep_main },
    { "grep",    grep_main },
    { "gunzip",  gunzip_main },
    { "gzip",    gzip_main },
    { "sed",     sed_main },
    { "sh",      sh_main },
    { "true",    true_main },
    { "yes",     yes_main },
    { "zcat",    zcat_main },
    /* ---- Exocortical family ---- */
    { "benchmark",               exo_applet_benchmark },
    { "browse",                  exo_applet_browse },
    { "cluster-status",          exo_applet_cluster_status },
    { "rpush",                   exo_applet_rpush },
    { "rag",                     exo_applet_rag },
    { "hwe-enable",              exo_applet_hwe_enable },
    { "lemonade",                exo_applet_lemonade },
    { "maintenance",             exo_applet_maintenance },
    { "tb-auth",                 exo_applet_tb_auth },
    { "wifi-optimizations",      exo_applet_wifi_optimizations },
    { "hello-phext",             exo_applet_hello_phext },
    { "install",                 exo_applet_install },
    { "enable-npm-proxy",        exo_applet_enable_npm_proxy },
    { "install-keys",            exo_applet_install_keys },
    { "fast-go-setup",           exo_applet_fast_go_setup },
    { "sync-ollama-models",      exo_applet_sync_ollama_models },
    { "enable-zram",             exo_applet_enable_zram },
    { "enable-virtual-memory",   exo_applet_enable_virtual_memory },
    { "nvme-expand-disk",        exo_applet_nvme_expand_disk },
    { "usb-ssd-fixup",           exo_applet_usb_ssd_fixup },
    { "provision-squid-client",  exo_applet_provision_squid_client },
    { "provision-squid-server",  exo_applet_provision_squid_server },
    { "squid-uninstall-client",  exo_applet_squid_uninstall_client },
    { "squid-cache-monitor",     exo_applet_squid_cache_monitor },
    { "enable-browser-sandbox",  exo_applet_enable_browser_sandbox },
    { "nfs-provision-client",    exo_applet_nfs_provision_client },
    { "nfs-provision-server",    exo_applet_nfs_provision_server },
    { "tb-setup",                exo_applet_tb_setup },
    { "setup",                   exo_applet_setup },
    { "ssh-setup",               exo_applet_ssh_setup },
    { "setup-phext-mcp",         exo_applet_setup_phext_mcp },
    { "patch-libuv",             exo_applet_patch_libuv },
    { "repair-openclaw",         exo_applet_repair_openclaw },
    { "nvidia-setup",            exo_applet_nvidia_setup },
    { "patch-amd-gpu",           exo_applet_patch_amd_gpu },
    { "tailscale",               exo_applet_tailscale },
    { "optimize-llm-inference",  exo_applet_optimize_llm_inference },
    { "exo-agentmail",           exo_applet_exo_agentmail },
    { "exo-console",             exo_applet_exo_console },
    { "ports",                   exo_applet_ports },
#ifdef WITH_VIM
    { "vi",      vim_main },
#endif
    { NULL, NULL }
};
#define NAPPLETS ((int)(sizeof(applets) / sizeof(applets[0])) - 1)

static const struct applet *find_applet(const char *name)
{
    int i;
    for (i = 0; applets[i].name; i++)
        if (strcmp(applets[i].name, name) == 0) return &applets[i];
    return NULL;
}

static const char *base_name(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void usage(FILE *f)
{
    fprintf(f,
        "Usage: phccbox <applet> [arguments...]\n"
        "       phccbox --list\n"
        "       phccbox --install DIR\n"
        "or invoke through a link named after an applet.\n"
        "%d applets carried.\n", NAPPLETS);
}

static int do_list(void)
{
    int i;
    for (i = 0; applets[i].name; i++) puts(applets[i].name);
    return 0;
}

static int do_install(const char *dir)
{
    char self[PATH_MAX], path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    int i, made = 0, skipped = 0;
    if (n < 0) { perror("phccbox: readlink /proc/self/exe"); return 1; }
    self[n] = '\0';
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "phccbox: cannot create %s: %s\n", dir, strerror(errno));
        return 1;
    }
    for (i = 0; applets[i].name; i++) {
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", dir, applets[i].name);
        if (lstat(path, &st) == 0) {
            char tgt[PATH_MAX]; ssize_t m;
            if (S_ISLNK(st.st_mode) &&
                (m = readlink(path, tgt, sizeof(tgt) - 1)) >= 0) {
                tgt[m] = '\0';
                if (strcmp(tgt, self) == 0) { skipped++; continue; }
            }
            fprintf(stderr, "phccbox: %s exists, not ours — skipped\n", path);
            skipped++;
            continue;
        }
        if (symlink(self, path) != 0) {
            fprintf(stderr, "phccbox: symlink %s: %s\n", path, strerror(errno));
            return 1;
        }
        made++;
    }
    printf("phccbox: %d applet links installed in %s (%d already present)\n",
           made, dir, skipped);
    return 0;
}

int main(int argc, char **argv)
{
    const char *prog = argc > 0 ? base_name(argv[0]) : "phccbox";
    const struct applet *ap;

    if (strcmp(prog, "phccbox") != 0) {
        ap = find_applet(prog);
        if (ap) return ap->fn(argc, argv);
        fprintf(stderr, "phccbox: unknown applet '%s'\n", prog);
        return 127;
    }
    if (argc < 2) { usage(stderr); return 1; }
    if (strcmp(argv[1], "--list") == 0) return do_list();
    if (strcmp(argv[1], "--install") == 0) {
        if (argc < 3) { fprintf(stderr, "phccbox: --install needs a directory\n"); return 1; }
        return do_install(argv[2]);
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout); return 0;
    }
    ap = find_applet(argv[1]);
    if (!ap) {
        fprintf(stderr, "phccbox: unknown applet '%s'\n", argv[1]);
        return 127;
    }
    return ap->fn(argc - 1, argv + 1);
}
