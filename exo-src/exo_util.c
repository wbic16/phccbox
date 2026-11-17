/* exo_util.c -- see exo_util.h */
#define _GNU_SOURCE
#include "exo_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>

char *exo_strdup(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }
char *exo_strndup(const char *s, size_t n) { char *p = malloc(n + 1); memcpy(p, s, n); p[n] = 0; return p; }

int exo_dry(void) { const char *d = getenv("EXO_DRYRUN"); return d && *d; }

const char *exo_root(void) { const char *r = getenv("EXO_SYSROOT"); return r ? r : ""; }

char *exo_p(const char *abs) {
    const char *r = exo_root();
    exo_sb s; exo_sb_init(&s);
    if (r[0] && abs[0] == '/' &&
        strncmp(abs, "/proc", 5) != 0 && strncmp(abs, "/dev", 4) != 0) {
        exo_sb_add(&s, r);
        /* avoid double slash if root ends with / */
        if (s.len && s.buf[s.len - 1] == '/') s.buf[--s.len] = 0;
    }
    exo_sb_add(&s, abs);
    return exo_sb_steal(&s);
}

void exo_log(const char *line) {
    const char *t = getenv("EXO_TRANSCRIPT");
    if (!t || !*t) return;
    FILE *f = fopen(t, "a");
    if (!f) return;
    fputs(line, f);
    fputc('\n', f);
    fclose(f);
}

void exo_log_cmd(const char *kind, char *const argv[]) {
    if (!getenv("EXO_TRANSCRIPT")) return;
    exo_sb s; exo_sb_init(&s);
    exo_sb_add(&s, kind);
    for (int i = 0; argv[i]; i++) { exo_sb_add(&s, " "); exo_sb_add(&s, argv[i]); }
    char *line = exo_sb_steal(&s);
    exo_log(line);
    free(line);
}

static int wait_status(pid_t pid) {
    int st;
    while (waitpid(pid, &st, 0) < 0) if (errno != EINTR) return 127;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return 127;
}

int exo_run(char *const argv[]) {
    exo_log_cmd("RUN", argv);
    if (exo_dry()) return 0;
    fflush(stdout);   /* keep our buffered printf ahead of the child's output */
    pid_t pid = fork();
    if (pid == 0) { execvp(argv[0], argv); _exit(127); }
    if (pid < 0) return 127;
    return wait_status(pid);
}

int exo_vrun(const char *a0, ...) {
    char *argv[64]; int n = 0;
    va_list ap; va_start(ap, a0);
    argv[n++] = (char *)a0;
    const char *a;
    while ((a = va_arg(ap, const char *)) && n < 63) argv[n++] = (char *)a;
    argv[n] = NULL;
    va_end(ap);
    return exo_run(argv);
}

int exo_run_redir(char *const argv[], const char *in_path, const char *out_path,
                  int out_append, int err_to_null) {
    exo_log_cmd("RUN", argv);
    if (exo_dry()) return 0;
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        if (in_path) { int fd = open(in_path, O_RDONLY); if (fd >= 0) { dup2(fd, 0); close(fd); } }
        if (out_path) {
            int fd = open(out_path, O_WRONLY | O_CREAT | (out_append ? O_APPEND : O_TRUNC), 0666);
            if (fd >= 0) { dup2(fd, 1); close(fd); }
        }
        if (err_to_null) { int fd = open("/dev/null", O_WRONLY); if (fd >= 0) { dup2(fd, 2); close(fd); } }
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid < 0) return 127;
    return wait_status(pid);
}

int exo_sh(const char *cmd) {
    if (getenv("EXO_TRANSCRIPT")) {
        exo_sb s; exo_sb_init(&s); exo_sb_add(&s, "RUN-SH "); exo_sb_add(&s, cmd);
        char *l = exo_sb_steal(&s); exo_log(l); free(l);
    }
    if (exo_dry()) return 0;
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) { execl("/bin/sh", "sh", "-c", cmd, (char *)0); _exit(127); }
    if (pid < 0) return 127;
    return wait_status(pid);
}

char *exo_cap(char *const argv[], int *rc) {
    exo_log_cmd("CAP", argv);
    if (exo_dry()) { if (rc) *rc = 0; return exo_strdup(""); }
    int fds[2];
    if (pipe(fds) < 0) { if (rc) *rc = 127; return exo_strdup(""); }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fds[1], 1);
        close(fds[0]); close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    exo_sb s; exo_sb_init(&s);
    char buf[4096]; ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) exo_sb_addn(&s, buf, (size_t)n);
    close(fds[0]);
    int st = (pid > 0) ? wait_status(pid) : 127;
    if (rc) *rc = st;
    return exo_sb_steal(&s);
}

int exo_which(const char *name) {
    if (strchr(name, '/')) return access(name, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    char *copy = exo_strdup(path), *save = NULL, *tok = strtok_r(copy, ":", &save);
    int found = 0;
    while (tok) {
        exo_sb s; exo_sb_init(&s);
        exo_sb_add(&s, tok[0] ? tok : "."); exo_sb_add(&s, "/"); exo_sb_add(&s, name);
        char *p = exo_sb_steal(&s);
        if (access(p, X_OK) == 0) { found = 1; free(p); break; }
        free(p);
        tok = strtok_r(NULL, ":", &save);
    }
    free(copy);
    return found;
}

const char *exo_user(void) {
    const char *u = getenv("USER");
    if (u && *u) return u;
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_name : "root";
}

const char *exo_home(void) {
    const char *h = getenv("HOME");
    if (h && *h) return h;
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/";
}

char *exo_read_raw(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    exo_sb s; exo_sb_init(&s);
    char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) exo_sb_addn(&s, buf, n);
    fclose(f);
    return exo_sb_steal(&s);
}

char *exo_read(const char *abs) {
    char *p = exo_p(abs);
    char *r = exo_read_raw(p);
    free(p);
    return r;
}

int exo_exists(const char *abs) {
    char *p = exo_p(abs); struct stat st; int ok = stat(p, &st) == 0; free(p); return ok;
}
int exo_isdir(const char *abs) {
    char *p = exo_p(abs); struct stat st; int ok = stat(p, &st) == 0 && S_ISDIR(st.st_mode); free(p); return ok;
}

int exo_mkdir_p(const char *abs) {
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "MKDIR "); exo_sb_add(&l, abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *p = exo_p(abs);
    /* iterative mkdir -p */
    for (char *q = p + 1; ; q++) {
        if (*q == '/' || *q == 0) {
            char save = *q; *q = 0;
            mkdir(p, 0777);
            *q = save;
            if (!save) break;
        }
    }
    struct stat st; int ok = stat(p, &st) == 0 && S_ISDIR(st.st_mode);
    free(p);
    return ok ? 0 : -1;
}

static int write_common(const char *abs, const char *data, long len, int append, const char *kind) {
    if (len < 0) len = (long)strlen(data);
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, kind); exo_sb_add(&l, " "); exo_sb_add(&l, abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *p = exo_p(abs);
    int fd = open(p, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0666);
    free(p);
    if (fd < 0) return -1;
    long off = 0;
    while (off < len) { ssize_t w = write(fd, data + off, (size_t)(len - off)); if (w <= 0) { close(fd); return -1; } off += w; }
    close(fd);
    return 0;
}

int exo_write(const char *abs, const char *data, long len)  { return write_common(abs, data, len, 0, "WRITE"); }
int exo_append(const char *abs, const char *data, long len) { return write_common(abs, data, len, 1, "APPEND"); }

/* the originals spell tee's append flag both ways (-a in wifi / virtual
   memory, --append in patch-amd-gpu); the delegate uses this spelling */
static const char *exo_tee_append_flag = "-a";
void exo_tee_set_append_flag(const char *f) { exo_tee_append_flag = f; }

int exo_tee(const char *abs, const char *data, long len, int append) {
    if (len < 0) len = (long)strlen(data);
    if (!getenv("EXO_SYSROOT") && !exo_dry() && getuid() != 0) {
        /* user-level script semantics: the originals pipe through
           `sudo tee [-a] <path>`; do exactly that (tee echoes to stdout). */
        exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "RUN sudo tee ");
        if (append) { exo_sb_add(&l, exo_tee_append_flag); exo_sb_add(&l, " "); }
        exo_sb_add(&l, abs);
        char *line = exo_sb_steal(&l); exo_log(line); free(line);
        int fds[2];
        if (pipe(fds) != 0) return -1;
        pid_t pid = fork();
        if (pid == 0) {
            dup2(fds[0], 0); close(fds[0]); close(fds[1]);
            char *args[5]; int i = 0;
            args[i++] = "sudo"; args[i++] = "tee";
            if (append) args[i++] = (char *)exo_tee_append_flag;
            args[i++] = (char *)abs; args[i] = NULL;
            execvp("sudo", args);
            _exit(127);
        }
        close(fds[0]);
        long off = 0;
        while (off < len) { ssize_t w = write(fds[1], data + off, (size_t)(len - off)); if (w <= 0) break; off += w; }
        close(fds[1]);
        int status = 0; waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    int rc = write_common(abs, data, len, append, append ? "TEE-A" : "TEE");
    /* tee echoes its input to stdout in every mode */
    fwrite(data, 1, (size_t)len, stdout);
    fflush(stdout);
    return rc;
}

int exo_cp(const char *src, const char *dst_abs) {
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "COPY "); exo_sb_add(&l, src);
    exo_sb_add(&l, " "); exo_sb_add(&l, dst_abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *content = exo_read_raw(src);
    if (!content) {
        /* src may itself be a logical absolute path */
        content = exo_read(src);
        if (!content) {
            /* match cp's diagnostic for the common missing-source case */
            fprintf(stderr, "cp: cannot stat '%s': No such file or directory\n", src);
            return -1;
        }
    }
    char *dp = exo_p(dst_abs);
    struct stat st;
    const char *dst = dp;
    char *joined = NULL;
    if (stat(dp, &st) == 0 && S_ISDIR(st.st_mode)) {
        const char *base = strrchr(src, '/');
        exo_sb s; exo_sb_init(&s); exo_sb_add(&s, dp); exo_sb_add(&s, "/");
        exo_sb_add(&s, base ? base + 1 : src);
        joined = exo_sb_steal(&s);
        dst = joined;
    }
    FILE *f = fopen(dst, "wb");
    if (!f) { free(content); free(dp); free(joined); return -1; }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    free(content); free(dp); free(joined);
    return 0;
}

int exo_rm_f(const char *abs) {
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "RM "); exo_sb_add(&l, abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *p = exo_p(abs); int rc = unlink(p); free(p); return rc;
}

int exo_rmdir(const char *abs) {
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "RMDIR "); exo_sb_add(&l, abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *p = exo_p(abs); int rc = rmdir(p); free(p); return rc;
}

int exo_symlink(const char *target, const char *link_abs) {
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "SYMLINK "); exo_sb_add(&l, target);
    exo_sb_add(&l, " "); exo_sb_add(&l, link_abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *p = exo_p(link_abs); int rc = symlink(target, p); free(p); return rc;
}

int exo_chmod(const char *abs, mode_t mode) {
    exo_sb l; exo_sb_init(&l); exo_sb_addf(&l, "CHMOD %o ", (unsigned)mode); exo_sb_add(&l, abs);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (exo_dry()) return 0;
    char *p = exo_p(abs); int rc = chmod(p, mode); free(p); return rc;
}

long exo_memtotal_kb(void) {
    char *m = exo_read_raw("/proc/meminfo");
    if (!m) return -1;
    long kb = -1;
    const char *p = strstr(m, "MemTotal:");
    if (p) kb = strtol(p + 9, NULL, 10);
    free(m);
    return kb;
}

/* string builder */
void exo_sb_init(exo_sb *s) { s->cap = 256; s->len = 0; s->buf = malloc(s->cap); s->buf[0] = 0; }
static void sb_grow(exo_sb *s, size_t need) {
    if (s->len + need + 1 > s->cap) {
        while (s->len + need + 1 > s->cap) s->cap *= 2;
        s->buf = realloc(s->buf, s->cap);
    }
}
void exo_sb_addn(exo_sb *s, const char *p, size_t n) { sb_grow(s, n); memcpy(s->buf + s->len, p, n); s->len += n; s->buf[s->len] = 0; }
void exo_sb_add(exo_sb *s, const char *p) { exo_sb_addn(s, p, strlen(p)); }
void exo_sb_addf(exo_sb *s, const char *fmt, ...) {
    char tmp[4096];
    va_list ap; va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    exo_sb_add(s, tmp);
}
char *exo_sb_steal(exo_sb *s) { char *b = s->buf; s->buf = NULL; s->len = s->cap = 0; return b; }
void exo_sb_free(exo_sb *s) { free(s->buf); s->buf = NULL; s->len = s->cap = 0; }

int exo_contains(const char *hay, const char *needle) { return hay && strstr(hay, needle) != NULL; }
int exo_contains_ci(const char *hay, const char *needle) { return hay && strcasestr(hay, needle) != NULL; }
long exo_count_substr(const char *hay, const char *needle) {
    if (!hay || !*needle) return 0;
    long n = 0; size_t nl = strlen(needle); const char *p = hay;
    while ((p = strstr(p, needle))) { n++; p += nl; }
    return n;
}
long exo_count_lines_containing(const char *text, const char *needle) {
    if (!text) return 0;
    long n = 0;
    const char *p = text;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *line = exo_strndup(p, len);
        if (strstr(line, needle)) n++;
        free(line);
        if (!e) break;
        p = e + 1;
    }
    return n;
}
char *exo_replace_all(const char *s, const char *from, const char *to) {
    exo_sb b; exo_sb_init(&b);
    size_t fl = strlen(from);
    const char *p = s;
    while (1) {
        const char *hit = strstr(p, from);
        if (!hit) { exo_sb_add(&b, p); break; }
        exo_sb_addn(&b, p, (size_t)(hit - p));
        exo_sb_add(&b, to);
        p = hit + fl;
    }
    return exo_sb_steal(&b);
}
char *exo_trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0;
    return s;
}
char *exo_tmpl(const char *tmpl, const char *const *names, const char *const *vals) {
    exo_sb b; exo_sb_init(&b);
    const char *p = tmpl;
    while (*p) {
        if (p[0] == '%' && p[1] == '%') {
            const char *e = strstr(p + 2, "%%");
            if (e) {
                size_t nl = (size_t)(e - (p + 2));
                const char *rep = NULL;
                for (int i = 0; names[i]; i++)
                    if (strlen(names[i]) == nl && strncmp(names[i], p + 2, nl) == 0) { rep = vals[i]; break; }
                if (rep) { exo_sb_add(&b, rep); p = e + 2; continue; }
            }
        }
        exo_sb_addn(&b, p, 1);
        p++;
    }
    return exo_sb_steal(&b);
}

char *exo_cap_err(char *const argv[], int *rc) {
    exo_log_cmd("CAP", argv);
    if (exo_dry()) { if (rc) *rc = 0; return exo_strdup(""); }
    int fds[2];
    if (pipe(fds) < 0) { if (rc) *rc = 127; return exo_strdup(""); }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fds[1], 1); dup2(fds[1], 2);
        close(fds[0]); close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    exo_sb s; exo_sb_init(&s);
    char buf[4096]; ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) exo_sb_addn(&s, buf, (size_t)n);
    close(fds[0]);
    int st = (pid > 0) ? wait_status(pid) : 127;
    if (rc) *rc = st;
    return exo_sb_steal(&s);
}
