/* phccbox v1.1 — a multicall binary that replaces BusyBox with full GNU
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
 * v1.1 adds the GNU coreutils 9.12 family as ONE bundle: the objects of
 * coreutils' own --enable-single-binary build, with its multicall main
 * renamed coreutils_main.  The dispatcher hands it argv with argv[0] set
 * to the applet name, and coreutils' own main switch does the rest —
 * including "[", and the name variants dir/vdir (ls driven by name).
 * The v1 custom true/false/yes stand-ins are gone: the GNU coreutils
 * versions replace them, as the v1 design intended.
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
int coreutils_main(int argc, char **argv);
#ifdef WITH_VIM
int vim_main(int argc, char **argv);
#endif

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
    { "[", coreutils_main },
    { "arch", coreutils_main },
    { "ash", sh_main },
    { "awk", awk_main },
    { "b2sum", coreutils_main },
    { "base32", coreutils_main },
    { "base64", coreutils_main },
    { "basename", coreutils_main },
    { "basenc", coreutils_main },
    { "bunzip2", bunzip2_main },
    { "bzcat", bzcat_main },
    { "bzip2", bzip2_main },
    { "cat", coreutils_main },
    { "chcon", coreutils_main },
    { "chgrp", coreutils_main },
    { "chmod", coreutils_main },
    { "chown", coreutils_main },
    { "chroot", coreutils_main },
    { "cksum", coreutils_main },
    { "comm", coreutils_main },
    { "coreutils", coreutils_main },
    { "cp", coreutils_main },
    { "csplit", coreutils_main },
    { "cut", coreutils_main },
    { "date", coreutils_main },
    { "dd", coreutils_main },
    { "df", coreutils_main },
    { "dir", coreutils_main },
    { "dircolors", coreutils_main },
    { "dirname", coreutils_main },
    { "du", coreutils_main },
    { "echo", coreutils_main },
    { "egrep", egrep_main },
    { "env", coreutils_main },
    { "expand", coreutils_main },
    { "expr", coreutils_main },
    { "factor", coreutils_main },
    { "false", coreutils_main },
    { "fgrep", fgrep_main },
    { "fmt", coreutils_main },
    { "fold", coreutils_main },
    { "grep", grep_main },
    { "groups", coreutils_main },
    { "gunzip", gunzip_main },
    { "gzip", gzip_main },
    { "head", coreutils_main },
    { "hostid", coreutils_main },
    { "hostname", coreutils_main },
    { "id", coreutils_main },
    { "install", coreutils_main },
    { "join", coreutils_main },
    { "kill", coreutils_main },
    { "link", coreutils_main },
    { "ln", coreutils_main },
    { "logname", coreutils_main },
    { "ls", coreutils_main },
    { "md5sum", coreutils_main },
    { "mkdir", coreutils_main },
    { "mkfifo", coreutils_main },
    { "mknod", coreutils_main },
    { "mktemp", coreutils_main },
    { "mv", coreutils_main },
    { "nice", coreutils_main },
    { "nl", coreutils_main },
    { "nohup", coreutils_main },
    { "nproc", coreutils_main },
    { "numfmt", coreutils_main },
    { "od", coreutils_main },
    { "paste", coreutils_main },
    { "pathchk", coreutils_main },
    { "pinky", coreutils_main },
    { "pr", coreutils_main },
    { "printenv", coreutils_main },
    { "printf", coreutils_main },
    { "ptx", coreutils_main },
    { "pwd", coreutils_main },
    { "readlink", coreutils_main },
    { "realpath", coreutils_main },
    { "rm", coreutils_main },
    { "rmdir", coreutils_main },
    { "runcon", coreutils_main },
    { "sed", sed_main },
    { "seq", coreutils_main },
    { "sh", sh_main },
    { "sha1sum", coreutils_main },
    { "sha224sum", coreutils_main },
    { "sha256sum", coreutils_main },
    { "sha384sum", coreutils_main },
    { "sha512sum", coreutils_main },
    { "shred", coreutils_main },
    { "shuf", coreutils_main },
    { "sleep", coreutils_main },
    { "sort", coreutils_main },
    { "split", coreutils_main },
    { "stat", coreutils_main },
    { "stdbuf", coreutils_main },
    { "stty", coreutils_main },
    { "sum", coreutils_main },
    { "sync", coreutils_main },
    { "tac", coreutils_main },
    { "tail", coreutils_main },
    { "tee", coreutils_main },
    { "test", coreutils_main },
    { "timeout", coreutils_main },
    { "touch", coreutils_main },
    { "tr", coreutils_main },
    { "true", coreutils_main },
    { "truncate", coreutils_main },
    { "tsort", coreutils_main },
    { "tty", coreutils_main },
    { "uname", coreutils_main },
    { "unexpand", coreutils_main },
    { "uniq", coreutils_main },
    { "unlink", coreutils_main },
    { "uptime", coreutils_main },
    { "users", coreutils_main },
    { "vdir", coreutils_main },
    { "wc", coreutils_main },
    { "who", coreutils_main },
    { "whoami", coreutils_main },
    { "yes", coreutils_main },
    { "zcat", zcat_main },
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
