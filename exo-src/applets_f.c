/* applets_f.c -- Exocortical family: optimize-llm-inference. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include "exo_util.h"
#include "gen/lit_llm.h"

#define CHK(x) do { int _r = (x); if (_r != 0) return _r; } while (0)

static char *extract_quoted(const char *line, const char *key) {
    /* first key="..." match on the line */
    exo_sb pat; exo_sb_init(&pat); exo_sb_add(&pat, key); exo_sb_add(&pat, "=\"");
    char *p = exo_sb_steal(&pat);
    const char *hit = strstr(line, p);
    free(p);
    if (!hit) return NULL;
    hit += strlen(key) + 2;
    const char *e = strchr(hit, '"');
    if (!e) return NULL;
    return exo_strndup(hit, (size_t)(e - hit));
}
static int line_has_num_gpu_0(const char *ln) {
    if (strncmp(ln, "num_gpu", 7) != 0) return 0;
    const char *p = ln + 7;
    while (*p == ' ') p++;
    return p[0] == '0' && p[1] == 0;
}

int exo_applet_optimize_llm_inference(int argc, char **argv) {
    const char *done = "/etc/exocortical-llm-optimized.done";
    const char *force = getenv("FORCE");
    if (exo_exists(done) && !(force && strcmp(force, "1") == 0)) {
        printf("LLM inference already optimized.\n");
        printf("--- current config ---\n");
        char *sw = exo_read_raw("/proc/sys/vm/swappiness");
        if (sw) { fputs(sw, stdout); if (sw[0] && sw[strlen(sw) - 1] != '\n') putchar('\n'); free(sw); }
        { char *av[] = { "ollama", "list", NULL }; exo_run_redir(av, NULL, NULL, 0, 1); }
        printf("Re-apply with: sudo FORCE=1 %s\n", argv[0]);
        return 0;
    }
    long kb = exo_memtotal_kb();
    long total_mb = kb > 0 ? kb / 1024 : 0;
    const char *tier = total_mb >= 80000 ? "96" : total_mb >= 24000 ? "32" : "16";
    const char *shell_env = getenv("EXOCORTEX_SHELL");
    int shell = shell_env ? atoi(shell_env) : 0;
    printf("[detect] %ld MB RAM -> tier %s GB\n", total_mb, tier);
    printf("[detect] Shell assignment: %d\n", shell);

    /* phase 1 */
    printf("[phase 1] Swap parameters...\n");
    if (!exo_exists("/etc/sysctl.d/90-exocortical-llm.conf")) {
        exo_tee("/etc/sysctl.d/90-exocortical-llm.conf", LLM_SYSCTL_CONF, -1, 0);
        CHK(exo_vrun("sudo", "sysctl", "--system", "--quiet", NULL));
        printf("[phase 1] Sysctl tuned.\n");
    } else {
        printf("[phase 1] Sysctl already configured.\n");
    }

    /* phase 2 */
    printf("[phase 2] NVMe I/O scheduler...\n");
    if (!exo_exists("/etc/udev/rules.d/60-exocortical-iosched.rules")) {
        exo_tee("/etc/udev/rules.d/60-exocortical-iosched.rules", LLM_UDEV_RULE, -1, 0);
        char *pat = exo_p("/sys/block/nvme*");
        glob_t g;
        if (glob(pat, 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) {
                struct stat st;
                if (stat(g.gl_pathv[i], &st) == 0 && S_ISDIR(st.st_mode)) {
                    /* $dev is the sysrooted path here; the logical write is
                       /sys/block/<name>/queue/scheduler */
                    const char *base = strrchr(g.gl_pathv[i], '/');
                    exo_sb lp; exo_sb_init(&lp); exo_sb_add(&lp, "/sys/block/");
                    exo_sb_add(&lp, base ? base + 1 : ""); exo_sb_add(&lp, "/queue/scheduler");
                    char *logical = exo_sb_steal(&lp);
                    exo_write(logical, "mq-deadline\n", -1);
                    free(logical);
                }
            }
            globfree(&g);
        }
        free(pat);
        printf("[phase 2] NVMe scheduler set to mq-deadline.\n");
    } else {
        printf("[phase 2] NVMe scheduler already configured.\n");
    }

    /* phase 3 */
    printf("[phase 3] AMD 780M iGPU tuning...\n");
    int gtt_mb = strcmp(tier, "96") == 0 ? 32768 : strcmp(tier, "32") == 0 ? 16384 : 8192;
    int grub_updated = 0;
    {
        char *grub = exo_read("/etc/default/grub");
        if (!grub) grub = exo_strdup("");
        char *work = exo_strdup(grub);
        if (!strstr(work, "amdgpu.gttsize=")) {
            printf("[phase 3] Adding amdgpu.gttsize=%d to GRUB...\n", gtt_mb);
            exo_sb ins; exo_sb_init(&ins); exo_sb_addf(&ins, "amdgpu.gttsize=%d ", gtt_mb);
            char *ins_s = exo_sb_steal(&ins);
            char *nw = NULL;
            /* sed: at each line starting GRUB_CMDLINE_LINUX_DEFAULT=" insert after the quote */
            exo_sb out; exo_sb_init(&out);
            char *copy = exo_strdup(work);
            char *save = NULL;
            int changed = 0;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                const char *pfx = "GRUB_CMDLINE_LINUX_DEFAULT=\"";
                if (strncmp(ln, pfx, strlen(pfx)) == 0) {
                    exo_sb_addn(&out, ln, strlen(pfx)); exo_sb_add(&out, ins_s); exo_sb_add(&out, ln + strlen(pfx));
                    changed = 1;
                } else exo_sb_add(&out, ln);
                exo_sb_add(&out, "\n");
            }
            free(copy);
            nw = exo_sb_steal(&out);
            free(ins_s);
            /* bash sets GRUB_UPDATED=1 whenever the grep failed and sed ran,
               even if no line matched */
            grub_updated = 1;
            if (changed) { exo_write("/etc/default/grub", nw, -1); free(work); work = nw; }
            else free(nw);
        }
        if (!strstr(work, "iommu=pt")) {
            printf("[phase 3] Adding iommu=pt to GRUB...\n");
            exo_sb out; exo_sb_init(&out);
            char *copy = exo_strdup(work);
            char *save = NULL;
            int changed = 0;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                const char *pfx = "GRUB_CMDLINE_LINUX_DEFAULT=\"";
                if (strncmp(ln, pfx, strlen(pfx)) == 0) {
                    exo_sb_addn(&out, ln, strlen(pfx)); exo_sb_add(&out, "iommu=pt "); exo_sb_add(&out, ln + strlen(pfx));
                    changed = 1;
                } else exo_sb_add(&out, ln);
                exo_sb_add(&out, "\n");
            }
            free(copy);
            char *nw = exo_sb_steal(&out);
            grub_updated = 1;   /* sed ran (grep failed), as in the original */
            if (changed) exo_write("/etc/default/grub", nw, -1);
            free(nw);
        }
        free(work);
        free(grub);
    }
    if (grub_updated) {
        CHK(exo_vrun("sudo", "update-grub", NULL));
        printf("[phase 3] GRUB updated. Reboot required for GPU changes.\n");
    } else {
        printf("[phase 3] GRUB already configured.\n");
    }

    /* phase 4 */
    printf("[phase 4] Ollama service tuning...\n");
    if (!exo_isdir("/etc/systemd/system/ollama.service.d")) {
        if (exo_root()[0] || exo_dry()) exo_mkdir_p("/etc/systemd/system/ollama.service.d");
        else CHK(exo_vrun("sudo", "mkdir", "-p", "/etc/systemd/system/ollama.service.d", NULL));
    }
    int max_models = strcmp(tier, "96") == 0 ? 3 : strcmp(tier, "32") == 0 ? 2 : 1;
    int num_ctx;
    {
        char key[16];
        snprintf(key, sizeof key, "%s-%d", tier, shell);
        if (!strcmp(key, "96-1")) num_ctx = 65536;
        else if (!strcmp(key, "96-2")) num_ctx = 8192;
        else if (!strcmp(key, "32-1")) num_ctx = 16384;
        else if (!strcmp(key, "32-2")) num_ctx = 16384;
        else if (!strcmp(key, "16-1")) num_ctx = 4096;
        else if (!strcmp(key, "16-2")) num_ctx = 32768;
        else num_ctx = 8192;
    }
    /* Vulkan/ggml restore probe -- reads the live ollama install, as the
       original does (these paths are the install's own, not sysrooted) */
    char *ollama_lib = NULL;
    {
        /* command -v ollama via PATH search, then readlink -f */
        char binpath[4096] = "";
        const char *path = getenv("PATH");
        if (path) {
            char *copy = exo_strdup(path), *save = NULL;
            for (char *tok = strtok_r(copy, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
                char cand[4096];
                snprintf(cand, sizeof cand, "%s/ollama", tok[0] ? tok : ".");
                if (access(cand, X_OK) == 0) { snprintf(binpath, sizeof binpath, "%s", cand); break; }
            }
            free(copy);
        }
        if (binpath[0]) {
            char resolved[4096];
            if (realpath(binpath, resolved)) {
                char *d1 = strrchr(resolved, '/');
                if (d1) {
                    *d1 = 0;
                    char *d2 = strrchr(resolved, '/');
                    if (d2) {
                        *d2 = 0;
                        exo_sb lb; exo_sb_init(&lb); exo_sb_add(&lb, resolved); exo_sb_add(&lb, "/lib/ollama");
                        ollama_lib = exo_sb_steal(&lb);
                    }
                }
            }
        }
    }
    if (ollama_lib) {
        int lrc; char *lav[] = { "lspci", "-d", "1002:", NULL };
        char *lout = exo_cap(lav, &lrc); (void)lrc;
        int has_amd = lout && strstr(lout, "1002") != NULL ? 1 : (lout && lout[0] ? 1 : 0);
        /* grep -q . == any line at all (non-empty output) */
        has_amd = lout && lout[0] ? 1 : 0;
        free(lout);
        exo_sb vp; exo_sb_init(&vp); exo_sb_add(&vp, ollama_lib); exo_sb_add(&vp, "/vulkan/libggml-vulkan.so");
        char *vso = exo_sb_steal(&vp);
        exo_sb bp; exo_sb_init(&bp); exo_sb_add(&bp, ollama_lib); exo_sb_add(&bp, "/libggml-base.so*");
        char *bpat = exo_sb_steal(&bp);
        struct stat st;
        int missing_vulkan = stat(vso, &st) != 0;
        glob_t g;
        int has_base = glob(bpat, 0, NULL, &g) == 0 && g.gl_pathc > 0;
        globfree(&g);
        free(vso); free(bpat);
        if (has_amd && (missing_vulkan || !has_base)) {
            int vrc; char *vav[] = { "ollama", "-v", NULL };
            char *vout = exo_cap_err(vav, &vrc); (void)vrc;
            char *ver = exo_strdup("");
            if (vout) {
                char *t = exo_trim(vout);
                char *last = strrchr(t, ' ');
                ver = exo_strdup(last ? last + 1 : t);
                free(vout);
            }
            printf("[phase 4] Ollama %s is missing its Vulkan/ggml libs; restoring from release...\n", ver);
            /* prefix = dirname(dirname(OLLAMA_LIB)) */
            char *libcopy = exo_strdup(ollama_lib);
            char *s1 = strrchr(libcopy, '/'); if (s1) *s1 = 0;
            char *s2 = strrchr(libcopy, '/'); if (s2) *s2 = 0;
            exo_sb cmd; exo_sb_init(&cmd);
            exo_sb_addf(&cmd, "curl -fsSL \"https://github.com/ollama/ollama/releases/download/v%s/ollama-linux-amd64.tar.zst\" | sudo tar --zstd -x -C \"%s\" --exclude='lib/ollama/cuda_*' lib/ollama", ver, libcopy);
            char *cmds = exo_sb_steal(&cmd);
            exo_sh(cmds);
            free(cmds);
            free(libcopy);
            free(ver);
        }
    }
    {
        char mm[16], nc[16];
        snprintf(mm, sizeof mm, "%d", max_models);
        snprintf(nc, sizeof nc, "%d", num_ctx);
        const char *names[] = { "MAX_MODELS", "NUM_CTX", 0 };
        const char *vals[] = { mm, nc, 0 };
        char *body = exo_tmpl(LLM_OLLAMA_OVERRIDE_TMPL, names, vals);
        exo_tee("/etc/systemd/system/ollama.service.d/exocortical.conf", body, -1, 0);
        free(body);
    }
    CHK(exo_vrun("sudo", "systemctl", "daemon-reload", NULL));
    {
        char *av[] = { "sudo", "systemctl", "restart", "ollama", NULL };
        exo_run_redir(av, NULL, NULL, 0, 1);
    }
    exo_vrun("sleep", "10", NULL);
    {
        int rc; char *av[] = { "sudo", "journalctl", "-u", "ollama", "--since", "-30s", "--no-pager", NULL };
        char *log = exo_cap(av, &rc); (void)rc;
        if (!log) log = exo_strdup("");
        /* last line containing 'inference compute' and not 'library=cpu' */
        char *gpu_line = NULL;
        {
            char *copy = exo_strdup(log);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                if (strstr(ln, "inference compute") && !strstr(ln, "library=cpu")) {
                    free(gpu_line);
                    gpu_line = exo_strdup(ln);
                }
            }
            free(copy);
        }
        if (gpu_line) {
            char *desc = extract_quoted(gpu_line, "description");
            char *tot = extract_quoted(gpu_line, "total");
            printf("[phase 4] GPU OK: %s %s\n", desc ? desc : "", tot ? tot : "");
            free(desc); free(tot);
            free(gpu_line);
        } else {
            fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            fprintf(stderr, "[phase 4] WARNING: Ollama found NO GPU and is running on CPU only.\n");
            if (strstr(log, "dropping integrated GPU")) {
                fprintf(stderr, "  Ollama dropped the iGPU: OLLAMA_IGPU_ENABLE=1 is not reaching the service.\n");
                fprintf(stderr, "  check: systemctl show ollama -p Environment | tr ' ' '\\n' | grep IGPU\n");
            }
            fprintf(stderr, "  check: ls %s/vulkan; journalctl -u ollama | grep -E 'inference compute|dropping'\n", ollama_lib ? ollama_lib : "");
            fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        }
        free(log);
    }
    printf("[phase 4] Ollama configured: max_models=%d ctx=%d vulkan=1 igpu=1\n", max_models, num_ctx);

    /* phase 5 */
    printf("[phase 5] Model pulls...\n");
    if (shell == 1) {
        printf("[phase 5] Pulling nemotron-cascade-2:30b...\n");
        CHK(exo_vrun("ollama", "pull", "nemotron-cascade-2:30b", NULL));
    } else if (shell == 2) {
        const char *m = strcmp(tier, "96") == 0 ? "qwen3.5:122b" : strcmp(tier, "32") == 0 ? "qwen3.5:27b" : "qwen3.5:9b";
        printf("[phase 5] Pulling %s...\n", m);
        CHK(exo_vrun("ollama", "pull", m, NULL));
    } else {
        printf("[phase 5] No shell assigned (EXOCORTEX_SHELL=0). Skipping model pulls.\n");
        printf("  Re-run with: EXOCORTEX_SHELL=1 %s  (mirrorborn)\n", argv[0]);
        printf("           or: EXOCORTEX_SHELL=2 %s  (utility fleet)\n", argv[0]);
    }

    /* phase 5b */
    long long gtt_gb = 0;
    {
        char *pat = exo_p("/sys/class/drm/card*/device/mem_info_gtt_total");
        glob_t g;
        long long mx = 0;
        if (glob(pat, 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) {
                char *c = exo_read_raw(g.gl_pathv[i]);
                if (c) { long long v = strtoll(c, NULL, 10); if (v > mx) mx = v; free(c); }
            }
            globfree(&g);
        }
        free(pat);
        gtt_gb = mx / 1073741824LL;
    }
    if (gtt_gb <= 0) gtt_gb = gtt_mb / 1024;
    const char *pin_env = getenv("PIN_CPU_OVER_GB");
    long long pin = pin_env ? strtoll(pin_env, NULL, 10) : gtt_gb * 3 / 4;
    if (pin > 0 && exo_which("ollama")) {
        printf("[phase 5b] Pinning models larger than %lld GB to CPU (GTT %lld GB)...\n", pin, gtt_gb);
        int rc; char *av[] = { "ollama", "list", NULL };
        char *list = exo_cap(av, &rc); (void)rc;
        if (list) {
            char *copy = exo_strdup(list);
            char *save = NULL;
            int lineno = 0;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                lineno++;
                if (lineno == 1) continue;
                /* fields: $1 name, $3 size, $4 unit */
                char *f[8]; int nf = 0;
                char *p = ln;
                while (*p && nf < 8) {
                    while (*p == ' ' || *p == '\t') p++;
                    if (!*p) break;
                    f[nf++] = p;
                    while (*p && *p != ' ' && *p != '\t') p++;
                    if (*p) *p++ = 0;
                }
                if (nf >= 4 && strcmp(f[3], "GB") == 0 && strtod(f[2], NULL) > (double)pin) {
                    const char *m = f[0];
                    int src; char *sav[] = { "ollama", "show", "--parameters", (char *)m, NULL };
                    char *params = exo_cap(sav, &src); (void)src;
                    int pinned = 0;
                    if (params) {
                        char *pc = exo_strdup(params);
                        char *psave = NULL;
                        for (char *pl = strtok_r(pc, "\n", &psave); pl; pl = strtok_r(NULL, "\n", &psave))
                            if (line_has_num_gpu_0(pl)) { pinned = 1; break; }
                        free(pc);
                        free(params);
                    }
                    if (pinned) {
                        printf("[phase 5b]   %s already pinned\n", m);
                        continue;
                    }
                    char tmpname[] = "/tmp/exo-pin-XXXXXX";
                    int fd = mkstemp(tmpname);
                    if (fd >= 0) {
                        exo_sb content; exo_sb_init(&content);
                        exo_sb_addf(&content, "FROM %s\nPARAMETER num_gpu 0\n", m);
                        char *cs = exo_sb_steal(&content);
                        if (write(fd, cs, strlen(cs)) < 0) { }
                        close(fd);
                        free(cs);
                        exo_log_cmd("MKTEMP", (char *[]){ tmpname, NULL });
                        char *cav[] = { "ollama", "create", (char *)m, "-f", tmpname, NULL };
                        int crc = exo_run_redir(cav, NULL, "/dev/null", 0, 1);
                        if (crc == 0) printf("[phase 5b]   %s -> num_gpu 0\n", m);
                        else fprintf(stderr, "[phase 5b]   WARNING: could not pin %s\n", m);
                        unlink(tmpname);
                    }
                }
            }
            free(copy);
            free(list);
        }
    } else {
        printf("[phase 5b] Skipping CPU pinning.\n");
    }

    /* phase 6 */
    printf("[phase 6] Firewall rules...\n");
    if (exo_which("ufw")) {
        char *a1[] = { "sudo", "ufw", "allow", "from", "192.168.0.0/16", "to", "any", "port", "11434", "proto", "tcp", NULL };
        exo_run_redir(a1, NULL, NULL, 0, 1);
        char *a2[] = { "sudo", "ufw", "allow", "from", "192.168.0.0/16", "to", "any", "port", "1337", "proto", "tcp", NULL };
        exo_run_redir(a2, NULL, NULL, 0, 1);
        printf("[phase 6] Firewall rules applied.\n");
    } else {
        printf("[phase 6] ufw not found, skipping.\n");
    }

    /* footer */
    long long swap_bytes = 0;
    {
        int rc; char *av[] = { "swapon", "--show", "--noheadings", "--bytes", NULL };
        char *out = exo_cap(av, &rc); (void)rc;
        if (out) {
            char *copy = exo_strdup(out);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                char *f[8]; int nf = 0;
                char *p = ln;
                while (*p && nf < 8) {
                    while (*p == ' ' || *p == '\t') p++;
                    if (!*p) break;
                    f[nf++] = p;
                    while (*p && *p != ' ' && *p != '\t') p++;
                    if (*p) *p++ = 0;
                }
                if (nf >= 3) swap_bytes += strtoll(f[2], NULL, 10);
            }
            free(copy);
            free(out);
        }
    }
    printf("\n");
    printf("=== Exocortical LLM Optimization Complete ===\n");
    printf("  RAM tier:    %s GB\n", tier);
    printf("  Shell:       %d\n", shell);
    printf("  Ollama:      vulkan=1 igpu=1 flash_attn=1 ctx=%d\n", num_ctx);
    printf("  Max models:  %d\n", max_models);
    printf("  GTT size:    %d MB (reboot to apply)\n", gtt_mb);
    printf("  Swap:        %.0f GB\n", (double)swap_bytes / 1073741824.0);
    printf("\n");
    if (grub_updated) printf("*** REBOOT REQUIRED for amdgpu.gttsize and iommu changes ***\n");
    exo_write(done, "Done\n", -1); /* original: echo Done | sudo tee >/dev/null */
    free(ollama_lib);
    return 0;
}
