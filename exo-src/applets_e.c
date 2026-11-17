/* applets_e.c -- Exocortical family: setup epic, ssh-setup,
 * setup-phext-mcp, patch-libuv, repair-openclaw, nvidia-setup,
 * patch-amd-gpu, tailscale. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "exo_util.h"
#include "gen/lit_setup.h"

#define CHK(x) do { int _r = (x); if (_r != 0) return _r; } while (0)
#define ROOTED (exo_root()[0] || exo_dry())

static void sudo_mkdir_p(const char *path) {
    if (ROOTED) { exo_mkdir_p(path); return; }
    exo_vrun("sudo", "mkdir", "-p", (char *)path, NULL);
}
static void sudo_chown_user(const char *path) {
    char owner[128];
    snprintf(owner, sizeof owner, "%s:%s", exo_user(), exo_user());
    if (ROOTED) {
        exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "CHOWN "); exo_sb_add(&l, owner);
        exo_sb_add(&l, " "); exo_sb_add(&l, path);
        char *line = exo_sb_steal(&l); exo_log(line); free(line);
        return;
    }
    exo_vrun("sudo", "chown", owner, (char *)path, NULL);
}
static void wait_all(void) { int st; while (waitpid(-1, &st, 0) > 0) { } }

/* ================================================================ setup */
static int setup_phases(void) {
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    const char *mp_env = getenv("MAX_PARALLEL");
    int max_parallel = mp_env ? atoi(mp_env) : 8;
    if (max_parallel < 1) max_parallel = 8;

    printf("[phase 1] System packages...\n");
    CHK(exo_vrun("sudo", "apt", "update", "-y", NULL));
    {
        char *av[64]; int n = 0;
        av[n++] = "sudo"; av[n++] = "apt"; av[n++] = "install"; av[n++] = "-y";
        av[n++] = "--no-install-recommends";
        for (int i = 0; SETUP_PKGS[i] && n < 62; i++) av[n++] = (char *)SETUP_PKGS[i];
        av[n] = NULL;
        CHK(exo_run(av));
    }
    {
        char *av[] = { "snap", "list", "docker", NULL };
        if (exo_run_redir(av, NULL, NULL, 0, 1) != 0)
            CHK(exo_vrun("sudo", "snap", "install", "docker", NULL));
    }
    if (!exo_exists("/etc/exo-uv-ready"))
        CHK(exo_vrun("./patch-libuv.sh", NULL));

    if (!exo_exists("/etc/avahi/services/exocortex.service")) {
        printf("[phase 2] Avahi advertisement...\n");
        char *av[] = { "snap", "list", "avahi", NULL };
        if (exo_run_redir(av, NULL, NULL, 0, 1) != 0)
            CHK(exo_vrun("sudo", "snap", "install", "avahi", NULL));
        if (ROOTED) CHK(exo_cp("exocortex.service", "/etc/avahi/services/"));
        else CHK(exo_vrun("sudo", "cp", "exocortex.service", "/etc/avahi/services/", NULL));
        CHK(exo_vrun("sudo", "service", "avahi-daemon", "restart", NULL));
    }

    printf("[phase 3] Ollama...\n");
    if (!exo_which("ollama"))
        CHK(exo_sh("curl -fsSL https://ollama.com/install.sh | sudo bash"));
    /* LLM_AGENT=ollama: the micro branch is dead, as in the original */

    printf("[phase 4] Rust toolchain...\n");
    {
        char *a1[] = { "sudo", "rustup", "default", "stable", NULL };
        exo_run_redir(a1, NULL, NULL, 0, 1);
        char *a2[] = { "rustup", "default", "stable", NULL };
        exo_run_redir(a2, NULL, NULL, 0, 1);
    }
    {
        exo_sb bp; exo_sb_init(&bp); exo_sb_add(&bp, exo_home()); exo_sb_add(&bp, "/.bashrc");
        char *bashrc_path = exo_sb_steal(&bp);
        char *bashrc = exo_read_raw(bashrc_path);
        long in_path = bashrc ? exo_count_lines_containing(bashrc, ".cargo/bin") : 0;
        if (in_path == 0) {
            printf("Adding Rust programs to PATH - login again to activate\n");
            if (!exo_dry()) {
                FILE *f = fopen(bashrc_path, "a");
                if (f) { fprintf(f, "export PATH=\"$HOME/.cargo/bin:$PATH\"\n"); fclose(f); }
            }
            exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "APPEND "); exo_sb_add(&l, bashrc_path);
            char *line = exo_sb_steal(&l); exo_log(line); free(line);
        }
        free(bashrc);
        free(bashrc_path);
        exo_sb np; exo_sb_init(&np);
        exo_sb_add(&np, exo_home()); exo_sb_add(&np, "/.cargo/bin:");
        const char *old = getenv("PATH");
        exo_sb_add(&np, old ? old : "");
        char *newpath = exo_sb_steal(&np);
        setenv("PATH", newpath, 1);
        free(newpath);
    }
    printf("[phase 4] Cargo installs (parallel)...\n");
    for (int i = 0; SETUP_CRATES[i]; i++) {
        exo_log_cmd("RUN", (char *[]){ "cargo", "install", (char *)SETUP_CRATES[i], NULL });
        if (!exo_dry()) {
            pid_t pid = fork();
            if (pid == 0) { execlp("cargo", "cargo", "install", SETUP_CRATES[i], (char *)0); _exit(127); }
        }
    }
    wait_all();
    printf("[phase 4] Cargo installs complete.\n");

    printf("[phase 5] Python environment...\n");
    if (!exo_isdir("/opt/exopy")) {
        sudo_mkdir_p("/opt/exopy");
        sudo_chown_user("/opt/exopy");
        char *vd = exo_p("/opt/exopy");
        CHK(exo_vrun("python3", "-m", "venv", vd, NULL));
        free(vd);
    }
    {
        char *vd = exo_p("/opt/exopy");
        exo_sb p3; exo_sb_init(&p3); exo_sb_add(&p3, vd); exo_sb_add(&p3, "/bin/pip3");
        char *pip3 = exo_sb_steal(&p3);
        char *av[] = { pip3, "install", "--quiet",
                       "llvmlite", "numba", "torch", "tensorflow", "shap",
                       "llama-index", "openai", "tf-keras",
                       "llama-index-embeddings-huggingface", "llama-index-llms-ollama",
                       "tensorflow[and-cuda]", NULL };
        CHK(exo_run(av));
        exo_sb pi; exo_sb_init(&pi); exo_sb_add(&pi, vd); exo_sb_add(&pi, "/bin/pip");
        char *pip = exo_sb_steal(&pi);
        CHK(exo_vrun(pip, "install", "--quiet", "-U", "openai-whisper", NULL));
        free(pip3); free(pip); free(vd);
    }
    {
        char *av[] = { "pip", "install", "--quiet", "agentmail", "python-dotenv", NULL };
        exo_run_redir(av, NULL, NULL, 0, 1);
    }

    /* phase 6: ssh key + exo-ready gate */
    {
        exo_sb pp; exo_sb_init(&pp); exo_sb_add(&pp, exo_home()); exo_sb_add(&pp, "/.ssh/id_ed25519.pub");
        char *pub = exo_sb_steal(&pp);
        struct stat st;
        if (stat(pub, &st) != 0) {
            printf("No SSH Identity found...generating one.\n");
            exo_sb kp; exo_sb_init(&kp); exo_sb_add(&kp, exo_home()); exo_sb_add(&kp, "/.ssh/id_ed25519");
            char *key = exo_sb_steal(&kp);
            CHK(exo_vrun("ssh-keygen", "-t", "ed25519", "-N", "", "-f", key, NULL));
            free(key);
        }
        if (!exo_exists("/etc/exo-ready")) {
            printf("\n");
            printf("You need to touch /etc/exo-ready after uploading your public key to GitHub.\n");
            printf("\n");
            char *content = exo_read_raw(pub);
            if (content) { fputs(content, stdout); free(content); }
            printf("\n");
            free(pub);
            return 1;
        }
        free(pub);
    }

    printf("[phase 7] External repos...\n");
    if (!exo_isdir("/opt/phoronix")) {
        sudo_mkdir_p("/opt/phoronix");
        sudo_chown_user("/opt/phoronix");
        exo_log("RUN-BG cd /opt/phoronix; wget+apt phoronix");
        if (!exo_dry()) {
            pid_t pid = fork();
            if (pid == 0) {
                char *d = exo_p("/opt/phoronix");
                chdir(d);
                exo_vrun("wget", "-q", "https://github.com/phoronix-test-suite/phoronix-test-suite/releases/download/v10.8.4/phoronix-test-suite_10.8.4_all.deb", NULL);
                exo_vrun("sudo", "apt", "install", "-y", "./phoronix-test-suite_10.8.4_all.deb", NULL);
                _exit(0);
            }
        }
    }
    if (!exo_isdir("/opt/ROCm")) {
        sudo_mkdir_p("/opt/ROCm");
        sudo_chown_user("/opt/ROCm");
        exo_log("RUN-BG cd /opt/ROCm; git clone ROCm; checkout roc-6.3.x; pull");
        if (!exo_dry()) {
            pid_t pid = fork();
            if (pid == 0) {
                char *d = exo_p("/opt/ROCm");
                chdir(d);
                exo_vrun("git", "clone", "git@github.com:ROCm/ROCm.git", ".", NULL);
                exo_vrun("git", "checkout", "roc-6.3.x", NULL);
                exo_vrun("git", "pull", NULL);
                _exit(0);
            }
        }
    }
    if (!exo_isdir("/opt/beebjit")) {
        sudo_mkdir_p("/opt/beebjit");
        sudo_chown_user("/opt/beebjit");
        char *bd = exo_p("/opt/beebjit");
        exo_log_cmd("RUN-BG", (char *[]){ "git", "clone", "git@github.com:wbic16/beebjit.git", bd, NULL });
        if (!exo_dry()) {
            pid_t pid = fork();
            if (pid == 0) { execlp("git", "git", "clone", "git@github.com:wbic16/beebjit.git", bd, (char *)0); _exit(127); }
        }
        free(bd);
    }
    wait_all();
    printf("[phase 7] External repos complete.\n");

    printf("[phase 8] Exocortex source trees...\n");
    if (!exo_isdir("/source")) {
        sudo_mkdir_p("/source");
        sudo_chown_user("/source");
    }
    {
        int running = 0;
        char *src_root = exo_p("/source");
        for (int i = 0; SETUP_REPOS[i]; i++) {
            exo_sb dp; exo_sb_init(&dp); exo_sb_add(&dp, src_root); exo_sb_add(&dp, "/"); exo_sb_add(&dp, SETUP_REPOS[i]);
            char *dir = exo_sb_steal(&dp);
            struct stat st;
            if (stat(dir, &st) != 0) {
                exo_sb up; exo_sb_init(&up); exo_sb_add(&up, "git@github.com:wbic16/"); exo_sb_add(&up, SETUP_REPOS[i]); exo_sb_add(&up, ".git");
                char *url = exo_sb_steal(&up);
                exo_log_cmd("RUN-BG", (char *[]){ "git", "clone", url, dir, NULL });
                if (!exo_dry()) {
                    pid_t pid = fork();
                    if (pid == 0) { execlp("git", "git", "clone", url, dir, (char *)0); _exit(127); }
                }
                free(url);
                running++;
                if (running >= max_parallel) { wait_all(); running = 0; }
            }
            free(dir);
        }
        free(src_root);
    }
    wait_all();
    printf("[phase 8] Source trees complete.\n");
    if (!exo_isdir("/source/the-book-of-secret-knowledge")) {
        char *d = exo_p("/source/the-book-of-secret-knowledge");
        int rc = exo_vrun("git", "clone", "https://github.com/wbic16/the-book-of-secret-knowledge.git", d, NULL);
        free(d);
        CHK(rc);
    }

    /* phase 9: micro-agent branch is dead (LLM_AGENT=ollama) */

    if (!exo_exists("/etc/systemd/resolved.conf.d/fallback.conf")) {
        printf("Installing fallback DNS...\n");
        sudo_mkdir_p("/etc/systemd/resolved.conf.d");
        exo_tee("/etc/systemd/resolved.conf.d/fallback.conf", SETUP_DNS_CONF, -1, 0);
        /* NOTE: the original pipes the heredoc to `sudo tee` and tee echoes it;
           exo_tee reproduces the echo. */
        CHK(exo_vrun("sudo", "systemctl", "restart", "systemd-resolved", NULL));
    }
    CHK(exo_vrun("sudo", "apt", "upgrade", "-y", NULL));

    printf("[phase 11] Proof tools (Charon/Aeneas/Lean)...\n");
    {
        char *sp = exo_p("/source/libphext-rs/scripts/install-proof-tools.sh");
        if (access(sp, X_OK) == 0) CHK(exo_vrun(sp, NULL));
        else printf("[phase 11] /source/libphext-rs not present - skipping proof tools.\n");
        free(sp);
    }
    printf("Setup Complete.\n");
    return 0;
}

int exo_applet_setup(int argc, char **argv) {
    /* git config + pull run in both passes, exactly as the original */
    exo_vrun("git", "config", "--global", "init.defaultBranch", "exo", NULL);
    exo_vrun("git", "config", "pull.rebase", "true", NULL);
    int prc = exo_vrun("git", "pull", NULL);
    if (!(argc > 1 && strcmp(argv[1], "exec") == 0)) {
        /* re-run self with "exec"; parent exits with the child's status.
           set -e note: a failing git pull above aborts the original before
           the re-exec; mirror that. */
        if (prc != 0) return prc;
        exo_log("REEXEC setup exec");
        if (exo_dry()) return 0;
        pid_t pid = fork();
        if (pid == 0) {
            execl("/proc/self/exe", argv[0], "exec", (char *)0);
            _exit(127);
        }
        int st;
        while (waitpid(pid, &st, 0) < 0) { }
        if (WIFEXITED(st)) return WEXITSTATUS(st);
        return 127;
    }
    if (prc != 0) return prc;   /* set -e: git pull failure aborts */
    printf("Ready to update.\n");
    return setup_phases();
}

/* ================================================================ ssh-setup */
int exo_applet_ssh_setup(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_vrun("sudo", "visudo", NULL);
    if (ROOTED) exo_mkdir_p("/source");
    else exo_vrun("sudo", "mkdir", "/source", NULL);
    {
        char owner[128];
        snprintf(owner, sizeof owner, "%s:%s", exo_user(), exo_user());
        if (!ROOTED) exo_vrun("sudo", "chown", owner, "/source", NULL);
        else {
            exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "CHOWN "); exo_sb_add(&l, owner); exo_sb_add(&l, " /source");
            char *line = exo_sb_steal(&l); exo_log(line); free(line);
        }
    }
    {
        exo_sb kp; exo_sb_init(&kp); exo_sb_add(&kp, exo_home()); exo_sb_add(&kp, "/.ssh/id_ed25519");
        char *key = exo_sb_steal(&kp);
        struct stat st;
        if (stat(key, &st) != 0) exo_vrun("ssh-keygen", NULL);
        free(key);
    }
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    char *src = exo_p("/source");
    chdir(src);   /* no set -e in the original: a failed cd leaves us in cwd */
    free(src);
    struct stat st;
    if (stat("exocortical", &st) != 0)
        exo_vrun("git", "clone", "git@github.com:wbic16/exocortical.git", NULL);
    chdir("exocortical");
    exo_vrun("./enable-virtual-memory.sh", NULL);
    int rc = exo_vrun("./setup.sh", NULL);
    if (cwd[0]) chdir(cwd);
    return rc;
}

/* ================================================================ setup-phext-mcp */
int exo_applet_setup_phext_mcp(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_vrun("go", "install", "github.com/mark3labs/mcphost@latest", NULL);
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    char *src = exo_p("/source");
    chdir(src);
    free(src);
    struct stat st;
    if (stat("phext-mcp", &st) != 0)
        exo_vrun("git", "clone", "git@github.com:wbic16/phext-mcp.git", NULL);
    chdir("phext-mcp");
    exo_vrun("cargo", "build", "--release", NULL);
    exo_sb yp; exo_sb_init(&yp); exo_sb_add(&yp, exo_home()); exo_sb_add(&yp, "/.mcphost.yml");
    char *yml = exo_sb_steal(&yp);
    if (stat(yml, &st) != 0) {
        /* quirk preserved: the original copies mcphost-config.yml relative
           to the phext-mcp cwd, where it does not exist; the copy fails and
           the message prints anyway (no set -e) */
        exo_cp("mcphost-config.yml", yml);
        printf("Installed default mcphost config\n");
    } else {
        printf("Review mcphost-config.yml\n");
    }
    free(yml);
    exo_sb mp; exo_sb_init(&mp); exo_sb_add(&mp, exo_home()); exo_sb_add(&mp, "/go/bin/mcphost");
    char *mcphost = exo_sb_steal(&mp);
    exo_log_cmd("EXEC", (char *[]){ mcphost, "-m", "ollama:llama4:scout", NULL });
    int rc = 0;
    if (!exo_dry()) {
        execl(mcphost, mcphost, "-m", "ollama:llama4:scout", (char *)0);
        rc = 127;
    }
    free(mcphost);
    if (cwd[0]) chdir(cwd);
    return rc;
}

/* ================================================================ patch-libuv */
int exo_applet_patch_libuv(int argc, char **argv) {
    (void)argc; (void)argv;
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    char *src = exo_p("/source");
    chdir(src);
    free(src);
    struct stat st;
    if (stat("libuv", &st) != 0)
        exo_vrun("git", "clone", "https://github.com/libuv/libuv.git", "--depth", "1", NULL);
    chdir("libuv");
    exo_vrun("git", "pull", "--depth", "100", NULL);
    exo_vrun("git", "checkout", "v1.51.0", NULL);
    exo_vrun("sudo", "apt", "install", "automake", "libtool", "-y", NULL);
    exo_vrun("./autogen.sh", NULL);
    exo_vrun("./configure", NULL);
    int rc = exo_vrun("make", NULL);
    if (rc == 0) rc = exo_vrun("sudo", "make", "install", NULL);
    if (cwd[0]) chdir(cwd);
    return rc;
}

/* ================================================================ repair-openclaw */
int exo_applet_repair_openclaw(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_sb op; exo_sb_init(&op); exo_sb_add(&op, exo_home()); exo_sb_add(&op, "/.openclaw");
    char *oc = exo_sb_steal(&op);
    struct stat st;
    if (stat(oc, &st) == 0 && S_ISDIR(st.st_mode)) {
        exo_sb np_; exo_sb_init(&np_); exo_sb_add(&np_, oc); exo_sb_add(&np_, "-historic-snapshot");
        char *snap = exo_sb_steal(&np_);
        /* mv ~/.openclaw ~/.openclaw-historic-snapshot : note the original's
           snapshot path is sibling: ~/.openclaw-historic-snapshot */
        free(snap);
        exo_sb np2; exo_sb_init(&np2); exo_sb_add(&np2, exo_home()); exo_sb_add(&np2, "/.openclaw-historic-snapshot");
        char *snap2 = exo_sb_steal(&np2);
        exo_log_cmd("RUN", (char *[]){ "mv", oc, snap2, NULL });
        if (!exo_dry()) rename(oc, snap2);
        free(snap2);
    }
    free(oc);
    if (ROOTED) { exo_mkdir_p("/opt/openclaw"); sudo_chown_user("/opt/openclaw"); }
    else {
        exo_vrun("sudo", "mkdir", "-p", "/opt/openclaw", NULL);
        char owner[128];
        snprintf(owner, sizeof owner, "%s:%s", exo_user(), exo_user());
        exo_vrun("sudo", "chown", "-R", owner, "/opt/openclaw", NULL);
    }
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    char *dir = exo_p("/opt/openclaw");
    chdir(dir);   /* original has no set -e: failed cd leaves us in cwd */
    free(dir);
    const char *deb = "OpenClaw-2026.10.7-amd64.deb";
    if (stat(deb, &st) != 0) {
        exo_sb u; exo_sb_init(&u);
        exo_sb_add(&u, "https://github.com/openclaw/openclaw/releases/download/v2026.10.7/");
        exo_sb_add(&u, deb);
        char *url = exo_sb_steal(&u);
        exo_vrun("wget", url, NULL);
        free(url);
    }
    exo_sb dp; exo_sb_init(&dp); exo_sb_add(&dp, "./"); exo_sb_add(&dp, deb);
    char *debarg = exo_sb_steal(&dp);
    exo_vrun("sudo", "apt", "install", debarg, NULL);
    free(debarg);
    int rc = exo_sh("curl -fsSL https://openclaw.ai/install.sh | bash");
    if (cwd[0]) chdir(cwd);
    return rc;   /* the original's exit code is its last command's */
}

/* ================================================================ nvidia-setup */
int exo_applet_nvidia_setup(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_vrun("wget", "https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb", NULL);
    exo_vrun("sudo", "dpkg", "-i", "cuda-keyring_1.1-1_all.deb", NULL);
    exo_vrun("sudo", "apt-get", "update", NULL);
    exo_vrun("sudo", "apt-get", "-y", "install", "cuda-toolkit-12-6", NULL);
    return exo_vrun("sudo", "apt-get", "install", "-y", "nvidia-open", "nvidia-driver-565-open", "nvidia-kernel-common-565", NULL);
}

/* ================================================================ patch-amd-gpu */
int exo_applet_patch_amd_gpu(int argc, char **argv) {
    (void)argc; (void)argv;
    exo_sh("wget -qO - https://repo.radeon.com/rocm/rocm.gpg.key | sudo apt-key add -");
    if (ROOTED) exo_mkdir_p("/etc/apt/keyrings");
    else exo_vrun("sudo", "mkdir", "--parents", "--mode=0755", "/etc/apt/keyrings", NULL);
    if (ROOTED) {
        /* the pipeline's effect is the keyring file; content comes from the
           network in the original -- in sysroot mode we transcribe it */
        exo_log("RUN-SH wget https://repo.radeon.com/rocm/rocm.gpg.key -O - | gpg --dearmor | sudo tee /etc/apt/keyrings/rocm.gpg > /dev/null");
    } else {
        exo_sh("wget https://repo.radeon.com/rocm/rocm.gpg.key -O - | gpg --dearmor | sudo tee /etc/apt/keyrings/rocm.gpg > /dev/null");
    }
    exo_vrun("sudo", "apt-get", "update", NULL);
    exo_vrun("sudo", "apt-get", "install", "rocm-dkms", "-y", NULL);
    exo_tee("/etc/apt/sources.list.d/amdgpu.list",
            "deb [arch=amd64 signed-by=/etc/apt/keyrings/rocm.gpg] https://repo.radeon.com/amdgpu/6.3.1/ubuntu noble main\n", -1, 0);
    exo_vrun("sudo", "apt", "update", "-y", NULL);
    exo_tee_set_append_flag("--append");   /* this script spells it long */
    exo_tee("/etc/apt/sources.list.d/rocm.list",
            "deb [arch=amd64 signed-by=/etc/apt/keyrings/rocm.gpg] https://repo.radeon.com/rocm/apt/6.3.1 noble main\n", -1, 1);
    exo_tee_set_append_flag("-a");
    /* quirk preserved byte-for-byte: echo without -e wrote literal \n */
    exo_tee("/etc/apt/preferences.d/rocm-pin-600",
            "Package: *\\nPin: release o=repo.radeon.com\\nPin-Priority: 600\n", -1, 0);
    exo_vrun("sudo", "apt", "update", "-y", NULL);
    exo_vrun("sudo", "apt", "install", "amdgpu-dkms", "-y", NULL);
    return exo_vrun("sudo", "apt", "install", "rocm", "-y", NULL);
}

/* ================================================================ tailscale */
int exo_applet_tailscale(int argc, char **argv) {
    (void)argc; (void)argv;
    {
        glob_t g;
        char *av[300]; int n = 0;
        av[n++] = "sudo"; av[n++] = "chown";
        exo_sb o; exo_sb_init(&o); exo_sb_addf(&o, "%s:%s", exo_user(), exo_user());
        char *owner = exo_sb_steal(&o);
        av[n++] = owner;
        int matched = 0;
        if (glob("tailscale*.sh", 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc && n < 295; i++) { av[n++] = g.gl_pathv[i]; matched = 1; }
        }
        if (!matched) av[n++] = "tailscale*.sh";
        av[n] = NULL;
        exo_run(av);
        if (matched) globfree(&g);
        free(owner);
    }
    {
        char *av[] = { "curl", "-fsSL", "https://tailscale.com/install.sh", NULL };
        exo_run_redir(av, NULL, "tailscale_install.sh", 0, 0);
    }
    exo_vrun("git", "diff", NULL);
    exo_vrun("git", "status", NULL);
    printf("Review the upstream changes, press Y to proceed\n");
    fflush(stdout);
    char line[256];
    const char *go = "n";
    if (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = 0;
        go = line;
    }
    if (strcmp(go, "Y") == 0) {
        struct stat st;
        if (stat("tailscale_install.sh", &st) == 0) chmod("tailscale_install.sh", st.st_mode | 0111);
        exo_log("CHMOD +x tailscale_install.sh");
        exo_vrun("sudo", "./tailscale_install.sh", NULL);
    }
    exo_vrun("git", "add", "tailscale_install.sh", NULL);
    return exo_vrun("git", "status", NULL);
}
