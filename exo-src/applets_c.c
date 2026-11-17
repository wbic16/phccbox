/* applets_c.c -- Exocortical family: squid client/server/uninstall,
 * cache monitor, browser sandbox. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include "exo_util.h"
#include "gen/lit_squid.h"
#include "gen/lit_squidclient.h"
#include "gen/lit_sandbox.h"

#define ROOTED (exo_root()[0] || exo_dry())
#define CHK(x) do { int _r = (x); if (_r != 0) return _r; } while (0)
/* write-family calls return -1 on failure; the shell's exit for a failed
   redirection is 1 */
#define CHKW(x) do { int _r = (x); if (_r != 0) return 1; } while (0)

/* helper: delete lines of a text whose line matches pred; returns new text */
static int line_has_proxy_eq(const char *ln) {
    /* sed pattern: .*_[Pp][Rr][Oo][Xx][Yy]=  (any case mix) */
    for (const char *p = ln; *p; p++) {
        if (*p == '_' && (p[1] == 'p' || p[1] == 'P') && (p[2] == 'r' || p[2] == 'R') &&
            (p[3] == 'o' || p[3] == 'O') && (p[4] == 'x' || p[4] == 'X') &&
            (p[5] == 'y' || p[5] == 'Y') && p[6] == '=') return 1;
    }
    return 0;
}
static char *delete_lines(char *text, int (*pred)(const char *)) {
    exo_sb out; exo_sb_init(&out);
    char *copy = exo_strdup(text);
    char *p = copy;
    while (*p) {
        char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *ln = exo_strndup(p, len);
        if (!pred(ln)) { exo_sb_addn(&out, p, len); if (e) exo_sb_add(&out, "\n"); }
        free(ln);
        if (!e) break;
        p = e + 1;
    }
    free(copy);
    return exo_sb_steal(&out);
}

/* ========================================================== squid client */
int exo_applet_provision_squid_client(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: sudo %s <SQUID_SERVER_IP>\n", argv[0]);
        printf("  Example: sudo %s 192.168.1.10\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    char ph[300], ps[300];
    snprintf(ph, sizeof ph, "http://%s:3128", ip);
    snprintf(ps, sizeof ps, "http://%s:3129", ip);
    printf("============================================\n");
    printf("  Ranch Choir Client Proxy Setup\n");
    printf("============================================\n");
    printf("  Squid Server: %s\n", ip);
    printf("  HTTP Proxy:   %s\n", ph);
    printf("  SSL Proxy:    %s\n", ps);
    printf("============================================\n");
    printf("\n");

    const char *ca_local = "/usr/local/share/ca-certificates/ranch-choir-squid-ca.crt";
    printf("[1/6] Installing Squid CA certificate...\n");
    char ca_url[300];
    snprintf(ca_url, sizeof ca_url, "http://%s/squid-ca-cert.crt", ip);
    char *ca_out = exo_p(ca_local);
    int rc = exo_vrun("curl", "-sf", ca_url, "-o", ca_out, NULL);
    free(ca_out);
    if (rc != 0) {
        printf("  ERROR: Could not download CA cert from %s\n", ca_url);
        printf("  Make sure the Squid server is provisioned and nginx is running.\n");
        return 1;
    }
    CHK(exo_vrun("update-ca-certificates", NULL));
    printf("  -> CA certificate trusted.\n");

    printf("[2/6] Configuring apt proxy...\n");
    {
        const char *names[] = { "PROXY_HTTP", "PROXY_SSL", 0 };
        const char *vals[] = { ph, ps, 0 };
        char *body = exo_tmpl(SQ_APT_PROXY_TMPL, names, vals);
        CHKW(exo_write("/etc/apt/apt.conf.d/02ranch-proxy", body, -1));
        free(body);
    }
    printf("  -> apt proxy configured.\n");

    printf("[3/6] Configuring Ollama proxy...\n");
    CHKW(exo_mkdir_p("/etc/systemd/system/ollama.service.d"));
    {
        const char *names[] = { "PROXY_HTTP", "PROXY_SSL", "SQUID_IP", 0 };
        const char *vals[] = { ph, ps, ip, 0 };
        char *body = exo_tmpl(SQ_OLLAMA_PROXY_TMPL, names, vals);
        CHKW(exo_write("/etc/systemd/system/ollama.service.d/proxy.conf", body, -1));
        free(body);
    }
    {
        char *av[] = { "systemctl", "is-active", "--quiet", "ollama", NULL };
        char *devnull = NULL; (void)devnull;
        int arc = exo_run_redir(av, NULL, NULL, 0, 1);
        if (arc == 0) {
            CHK(exo_vrun("systemctl", "daemon-reload", NULL));
            CHK(exo_vrun("systemctl", "restart", "ollama", NULL));
            printf("  -> Ollama service restarted with proxy.\n");
        } else {
            CHK(exo_vrun("systemctl", "daemon-reload", NULL));
            printf("  -> Ollama systemd override written (service not currently running).\n");
        }
    }

    printf("[4/6] Setting system-wide proxy environment...\n");
    {
        const char *names[] = { "PROXY_HTTP", "PROXY_SSL", "SQUID_IP", 0 };
        const char *vals[] = { ph, ps, ip, 0 };
        char *body = exo_tmpl(SQ_PROFILED_TMPL, names, vals);
        CHKW(exo_write("/etc/profile.d/ranch-proxy.sh", body, -1));
        free(body);
    }
    CHK(exo_chmod("/etc/profile.d/ranch-proxy.sh", 0644));
    {
        char *env = exo_read("/etc/environment");
        if (env && strstr(env, "http_proxy=")) {
            char *cleaned = delete_lines(env, line_has_proxy_eq);
            CHKW(exo_write("/etc/environment", cleaned, -1));
            free(cleaned);
        }
        free(env);
        const char *names[] = { "PROXY_HTTP", "PROXY_SSL", "SQUID_IP", 0 };
        const char *vals[] = { ph, ps, ip, 0 };
        char *body = exo_tmpl(SQ_ENVIRON_TMPL, names, vals);
        CHKW(exo_append("/etc/environment", body, -1));
        free(body);
    }
    printf("  -> System environment configured.\n");

    printf("[5/6] Configuring pip and npm...\n");
    CHKW(exo_mkdir_p("/etc/pip"));
    {
        const char *names[] = { "PROXY_SSL", 0 };
        const char *vals[] = { ps, 0 };
        char *body = exo_tmpl(SQ_PIP_CONF_TMPL, names, vals);
        CHKW(exo_write("/etc/pip/pip.conf", body, -1));
        free(body);
    }
    printf("  -> pip configured.\n");
    if (exo_which("npm")) {
        char *a1[] = { "npm", "config", "set", "proxy", ph, "--global", NULL };
        exo_run_redir(a1, NULL, NULL, 0, 1);
        char *a2[] = { "npm", "config", "set", "https-proxy", ps, "--global", NULL };
        exo_run_redir(a2, NULL, NULL, 0, 1);
        CHK(exo_vrun("npm", "config", "set", "cafile", (char *)ca_local, NULL));
        printf("  -> npm configured.\n");
    } else {
        printf("  -> npm not found, skipping.\n");
    }

    printf("[6/6] Verifying proxy connectivity...\n");
    setenv("http_proxy", ph, 1);
    setenv("https_proxy", ps, 1);
    int verify_ok = 1;
    {
        char *av[] = { "curl", "-sf", "--proxy", ph, "-o", "/dev/null",
                       "http://archive.ubuntu.com/ubuntu/dists/noble/Release", NULL };
        int r = exo_run_redir(av, NULL, NULL, 0, 1);
        if (r == 0) printf("  -> apt repos (HTTP): OK\n");
        else printf("  -> apt repos (HTTP): FAILED (non-fatal, may need different Ubuntu version)\n");
    }
    {
        char *av[] = { "curl", "-sf", "--proxy", ps, "-o", "/dev/null",
                       "https://registry.ollama.ai/v2/library/qwen3/manifests/latest", NULL };
        int r = exo_run_redir(av, NULL, NULL, 0, 1);
        if (r == 0) printf("  -> Ollama registry (HTTPS): OK\n");
        else {
            printf("  -> Ollama registry (HTTPS): FAILED\n");
            printf("     Check CA cert trust and Squid SSL bump config.\n");
            verify_ok = 0;
        }
    }
    {
        char *av[] = { "curl", "-sf", "--proxy", ps, "-o", "/dev/null",
                       "https://pypi.org/simple/", NULL };
        int r = exo_run_redir(av, NULL, NULL, 0, 1);
        if (r == 0) printf("  -> PyPI (HTTPS): OK\n");
        else printf("  -> PyPI (HTTPS): SKIPPED\n");
    }

    printf("\n");
    printf("============================================\n");
    printf("  Client Provisioned Successfully\n");
    printf("============================================\n");
    printf("\n");
    printf("  This node now routes through the cache:\n");
    printf("    apt install  -> cached .deb packages\n");
    printf("    ollama pull  -> cached model blobs\n");
    printf("    pip install  -> cached wheels\n");
    printf("    npm install  -> cached tarballs\n");
    printf("\n");
    printf("  To verify cache hits, on the server run:\n");
    printf("    tail -f /var/log/squid/access.log | grep HIT\n");
    printf("\n");
    if (!verify_ok) {
        printf("  WARNING: Some connectivity checks failed.\n");
        printf("  You may need to reboot or re-source your shell.\n");
    }
    printf("  Tip: log out and back in (or reboot) to pick\n");
    printf("  up the proxy environment variables everywhere.\n");
    printf("============================================\n");
    return 0;
}

/* ========================================================== squid server */
int exo_applet_provision_squid_server(int argc, char **argv) {
    const char *ip = argc > 1 ? argv[1] : "0.0.0.0";
    const char *ca_dir = "/etc/squid/ssl";
    const char *ca_cert = "/etc/squid/ssl/squid-ca-cert.pem";
    const char *ca_key = "/etc/squid/ssl/squid-ca-key.pem";
    const char *ca_der = "/etc/squid/ssl/squid-ca-cert.der";
    const char *ssl_db = "/var/lib/squid/ssl_db";
    const char *export_cert = "/var/www/html/squid-ca-cert.crt";
    const char *cache_dir = "/var/spool/squid";

    printf("============================================\n");
    printf("  Ranch Choir Squid Cache Server Setup\n");
    printf("============================================\n");
    printf("Listen IP:       %s\n", ip);
    printf("HTTP Port:       3128\n");
    printf("SSL Bump Port:   3129\n");
    printf("Disk Cache:      2097152 MB\n");
    printf("RAM Cache:       2048 MB\n");
    printf("Max Object Size: 102400 MB\n");
    printf("============================================\n");

    printf("[1/7] Installing squid and dependencies...\n");
    CHK(exo_vrun("apt-get", "update", "-qq", NULL));
    CHK(exo_vrun("apt-get", "install", "-y", "squid-openssl", "openssl", "nginx-light", "apparmor-utils", NULL));
    CHK(exo_vrun("sudo", "aa-complain", "/etc/apparmor.d/usr.sbin.squid", NULL));
    {
        /* quirk preserved: compares stdout only with the not-found text */
        char *av[] = { "sudo", "systemctl", "status", "apache2", NULL };
        int arc; char *have = exo_cap(av, &arc); (void)arc;
        char *have_t = exo_strdup(exo_trim(have));
        free(have);
        if (strcmp(have_t, "Unit apache2.service could not be found.") == 0) {
            printf("apache2 already disabled\n");
        } else {
            exo_vrun("sudo", "systemctl", "stop", "apache2", NULL);
            exo_vrun("sudo", "systemctl", "disable", "apache2", NULL);
            exo_vrun("sudo", "apt-mark", "hold", "apache2", NULL);
        }
        free(have_t);
    }
    CHK(exo_vrun("sudo", "systemctl", "restart", "nginx", NULL));
    CHK(exo_vrun("sudo", "systemctl", "status", "nginx", NULL));

    printf("[2/7] Generating CA certificate for SSL interception...\n");
    CHKW(exo_mkdir_p(ca_dir));
    if (!exo_exists(ca_key)) {
        char *keyout = exo_p(ca_key), *certout = exo_p(ca_cert);
        int rc = exo_vrun("openssl", "req", "-new", "-newkey", "rsa:4096", "-sha256",
                          "-days", "3650", "-nodes", "-x509", "-extensions", "v3_ca",
                          "-keyout", keyout, "-out", certout, "-subj",
                          "/C=US/ST=Nebraska/L=Ranch/O=RanchChoir/OU=ExocortexCache/CN=Ranch Choir Squid CA", NULL);
        free(keyout); free(certout);
        CHK(rc);
        char *cin = exo_p(ca_cert), *dout = exo_p(ca_der);
        rc = exo_vrun("openssl", "x509", "-in", cin, "-outform", "DER", "-out", dout, NULL);
        free(cin); free(dout);
        CHK(rc);
        printf("  -> New CA generated.\n");
    } else {
        printf("  -> Existing CA found, reusing.\n");
    }
    CHKW(exo_mkdir_p("/var/www/html"));
    if (ROOTED) exo_mkdir_p("/var/lib/squid");
    else CHK(exo_vrun("sudo", "mkdir", "-p", "/var/lib/squid", NULL));
    CHKW(exo_cp(ca_cert, export_cert));
    CHK(exo_chmod(export_cert, 0644));

    printf("[3/7] Initializing SSL certificate database (%s)...\n", ssl_db);
    {
        if (ROOTED) {
            exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "RM-RF "); exo_sb_add(&l, ssl_db);
            char *line = exo_sb_steal(&l); exo_log(line); free(line);
            if (!exo_dry()) { char *p = exo_p(ssl_db); exo_sb c; exo_sb_init(&c); exo_sb_add(&c, "rm -rf "); exo_sb_add(&c, p); char *cmd = exo_sb_steal(&c); exo_sh(cmd); free(cmd); free(p); }
        } else {
            CHK(exo_vrun("rm", "-rf", (char *)ssl_db, NULL));
        }
    }
    { char *dbp = exo_p(ssl_db);
        CHK(exo_vrun("/usr/lib/squid/security_file_certgen", "-c", "-s", dbp, "-M", "64MB", NULL));
        free(dbp); }
    CHK(exo_vrun("chown", "-R", "proxy:proxy", (char *)ssl_db, NULL));

    printf("[4/7] Preparing cache directory...\n");
    CHKW(exo_mkdir_p(cache_dir));
    CHK(exo_vrun("chown", "-R", "proxy:proxy", (char *)cache_dir, NULL));

    printf("[5/7] Writing squid.conf...\n");
    {
        const char *names[] = { "LISTEN_IP", "SQUID_PORT", "SQUID_SSL_PORT", "CA_CERT", "CA_KEY",
                                "SSL_DB", "CACHE_DIR", "CACHE_DISK_MB", "CACHE_MEM_MB",
                                "MAX_OBJ_SIZE_MB", "MAX_OBJ_MEM_MB", 0 };
        const char *vals[] = { ip, "3128", "3129", ca_cert, ca_key, ssl_db, cache_dir,
                               "2097152", "2048", "102400", "256", 0 };
        char *body = exo_tmpl(SQUID_CONF_TMPL, names, vals);
        CHKW(exo_write("/etc/squid/squid.conf", body, -1));
        free(body);
    }

    printf("[6/7] Initializing squid cache and starting service...\n");
    {
        char *av[] = { "squid", "-z", "--foreground", NULL };
        exo_run_redir(av, NULL, NULL, 0, 1);
    }
    CHK(exo_vrun("systemctl", "restart", "squid", NULL));
    CHK(exo_vrun("systemctl", "enable", "squid", NULL));
    CHK(exo_vrun("systemctl", "restart", "nginx", NULL));
    CHK(exo_vrun("systemctl", "enable", "nginx", NULL));

    printf("[7/7] Configuring firewall...\n");
    {
        int ufw_active = 0;
        if (exo_which("ufw")) {
            int urc; char *av[] = { "ufw", "status", NULL };
            char *out = exo_cap(av, &urc); (void)urc;
            if (out && strstr(out, "active")) ufw_active = 1;
            free(out);
        }
        if (ufw_active) {
            CHK(exo_vrun("ufw", "allow", "3128/tcp", "comment", "Squid HTTP proxy", NULL));
            CHK(exo_vrun("ufw", "allow", "3129/tcp", "comment", "Squid SSL bump proxy", NULL));
            CHK(exo_vrun("ufw", "allow", "80/tcp", "comment", "Nginx CA cert download", NULL));
            printf("  -> UFW rules added.\n");
        } else {
            printf("  -> UFW not active, skipping.\n");
        }
    }

    char server_ip[128] = "";
    {
        int hrc; char *av[] = { "hostname", "-I", NULL };
        char *out = exo_cap(av, &hrc); (void)hrc;
        if (out) {
            char *t = exo_trim(out);
            char *sp = strchr(t, ' ');
            if (sp) *sp = 0;
            snprintf(server_ip, sizeof server_ip, "%s", t);
            free(out);
        }
    }
    printf("\n");
    printf("============================================\n");
    printf("  Squid Cache Server is LIVE\n");
    printf("============================================\n");
    printf("\n");
    printf("  Server IP:    %s\n", server_ip);
    printf("  HTTP Proxy:   http://%s:3128\n", server_ip);
    printf("  SSL Proxy:    http://%s:3129\n", server_ip);
    printf("  CA Cert URL:  http://%s/squid-ca-cert.crt\n", server_ip);
    printf("\n");
    printf("  Cache stats:  squidclient -h localhost mgr:info\n");
    printf("  Hit ratio:    squidclient -h localhost mgr:utilization\n");
    printf("\n");
    printf("  Next: run provision-squid-client.sh on each node.\n");
    printf("============================================\n");
    return 0;
}

/* ========================================================== squid uninstall */
int exo_applet_squid_uninstall_client(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("Removing Ranch Choir proxy config...\n");
    if (ROOTED) {
        exo_rm_f("/usr/local/share/ca-certificates/ranch-choir-squid-ca.crt");
        CHK(exo_vrun("sudo", "update-ca-certificates", NULL));
        exo_rm_f("/etc/apt/apt.conf.d/02ranch-proxy");
        exo_rm_f("/etc/systemd/system/ollama.service.d/proxy.conf");
        exo_rmdir("/etc/systemd/system/ollama.service.d");
    } else {
        CHK(exo_vrun("sudo", "rm", "-f", "/usr/local/share/ca-certificates/ranch-choir-squid-ca.crt", NULL));
        CHK(exo_vrun("sudo", "update-ca-certificates", NULL));
        CHK(exo_vrun("sudo", "rm", "-f", "/etc/apt/apt.conf.d/02ranch-proxy", NULL));
        CHK(exo_vrun("sudo", "rm", "-f", "/etc/systemd/system/ollama.service.d/proxy.conf", NULL));
        exo_vrun("sudo", "rmdir", "/etc/systemd/system/ollama.service.d", NULL);
    }
    CHK(exo_vrun("sudo", "systemctl", "daemon-reload", NULL));
    {
        char *av[] = { "systemctl", "is-active", "--quiet", "ollama", NULL };
        int arc = exo_run_redir(av, NULL, NULL, 0, 1);
        if (arc == 0) CHK(exo_vrun("sudo", "systemctl", "restart", "ollama", NULL));
    }
    if (ROOTED) exo_rm_f("/etc/profile.d/ranch-proxy.sh");
    else CHK(exo_vrun("sudo", "rm", "-f", "/etc/profile.d/ranch-proxy.sh", NULL));
    if (ROOTED) {
        char *env = exo_read("/etc/environment");
        if (env) {
            char *cleaned = delete_lines(env, line_has_proxy_eq);
            exo_write("/etc/environment", cleaned, -1);
            free(cleaned);
            free(env);
        }
    } else {
        /* production mode: the original edits in place via sudo sed */
        exo_vrun("sudo", "sed", "-i", "/.*_[Pp][Rr][Oo][Xx][Yy]=/d", "/etc/environment", NULL);
    }
    if (ROOTED) exo_rm_f("/etc/pip/pip.conf");
    else CHK(exo_vrun("sudo", "rm", "-f", "/etc/pip/pip.conf", NULL));
    if (exo_which("npm")) {
        char *a1[] = { "npm", "config", "delete", "proxy", "--global", NULL };
        exo_run_redir(a1, NULL, NULL, 0, 1);
        char *a2[] = { "npm", "config", "delete", "https-proxy", "--global", NULL };
        exo_run_redir(a2, NULL, NULL, 0, 1);
    }
    printf("Done. Log out and back in to clear env vars from this session.\n");
    return 0;
}

/* ========================================================== cache monitor */
static char *colourise(const char *line) {
    char *a = exo_replace_all(line, "TCP_HIT", "\x1b[32mTCP_HIT\x1b[0m");
    char *b = exo_replace_all(a, "TCP_MEM_HIT", "\x1b[32;1mTCP_MEM_HIT\x1b[0m");
    free(a);
    char *c = exo_replace_all(b, "TCP_MISS", "\x1b[31mTCP_MISS\x1b[0m");
    free(b);
    char *d = exo_replace_all(c, "TCP_TUNNEL", "\x1b[33mTCP_TUNNEL\x1b[0m");
    free(c);
    return d;
}
int exo_applet_squid_cache_monitor(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("============================================\n");
    printf("  Ranch Choir Cache Status\n");
    {
        int rc; char *av[] = { "date", NULL };
        char *d = exo_cap(av, &rc); (void)rc;
        printf("  %s\n", exo_trim(d));
        free(d);
    }
    printf("============================================\n");
    printf("\n");
    printf("--- Cache Hit Ratio (5 min / 60 min) ---\n");
    {
        int rc; char *av[] = { "squidclient", "-h", "localhost", "mgr:5min", NULL };
        char *out = exo_cap(av, &rc); (void)rc;
        int any = 0;
        if (out) {
            char *copy = exo_strdup(out);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                if (strstr(ln, "client_http.hits") || strstr(ln, "client_http.requests") ||
                    strstr(ln, "client_http.kbytes")) { printf("%s\n", ln); any = 1; }
            }
            free(copy);
            free(out);
        }
        if (!any) printf("  (squidclient not available)\n");
    }
    printf("\n");
    printf("--- Disk Cache Usage ---\n");
    if (exo_isdir("/var/spool/squid")) {
        char *dir = exo_p("/var/spool/squid");
        int rc; char *av[] = { "du", "-sh", dir, NULL };
        char *out = exo_cap(av, &rc); (void)rc;
        free(dir);
        if (out) {
            char *t = exo_trim(out);
            char *sp = strpbrk(t, " \t");
            if (sp) *sp = 0;
            printf("  Cache directory: %s used\n", t);
            free(out);
        }
    }
    printf("\n");
    printf("--- Recent Large Objects Cached ---\n");
    if (exo_exists("/var/log/squid/store.log")) {
        char *log = exo_read("/var/log/squid/store.log");
        long count = 0;
        exo_sb hits; exo_sb_init(&hits);
        if (log) {
            char *copy = exo_strdup(log);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                /* awk whitespace fields */
                char *f[16]; int nf = 0;
                char *p = ln;
                while (*p && nf < 16) {
                    while (*p == ' ' || *p == '\t') p++;
                    if (!*p) break;
                    f[nf++] = p;
                    while (*p && *p != ' ' && *p != '\t') p++;
                    if (*p) *p++ = 0;
                }
                if (nf >= 5) {
                    double sz = strtod(f[4], NULL);
                    if (sz > 104857600.0) {
                        count++;
                        exo_sb_addf(&hits, "  %s  %.0f MB  %s\n", f[0], sz / 1048576.0, nf >= 13 ? f[12] : "");
                    }
                }
            }
            free(copy);
            free(log);
        }
        /* tail -10 of the formatted lines */
        char *h = exo_sb_steal(&hits);
        long lines = exo_count_substr(h, "\n");
        const char *p = h;
        for (long i = 0; i < lines - 10 && *p; i++) { const char *e = strchr(p, '\n'); if (e) p = e + 1; }
        fputs(p, stdout);
        free(h);
        printf("  (%ld objects > 100 MB in store log)\n", count);
    } else {
        printf("  (store.log not found)\n");
    }
    printf("\n");
    printf("--- Active Connections ---\n");
    {
        int rc; char *av[] = { "squidclient", "-h", "localhost", "mgr:active_requests", NULL };
        char *out = exo_cap(av, &rc); (void)rc;
        if (out) {
            int n = 0;
            char *copy = exo_strdup(out);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln && n < 5; ln = strtok_r(NULL, "\n", &save), n++)
                printf("%s\n", ln);
            free(copy);
            free(out);
        }
    }
    printf("\n");
    printf("--- Memory Usage ---\n");
    {
        int rc; char *av[] = { "squidclient", "-h", "localhost", "mgr:mem", NULL };
        char *out = exo_cap(av, &rc); (void)rc;
        if (out) {
            int n = 0;
            char *copy = exo_strdup(out);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                if (n < 5 && (strstr(ln, "Total") || strstr(ln, "Free") || strstr(ln, "Cached"))) {
                    printf("%s\n", ln); n++;
                }
            }
            free(copy);
            free(out);
        }
    }
    printf("\n");
    printf("============================================\n");
    printf("  Live access log (Ctrl+C to stop):\n");
    printf("============================================\n");
    printf("\n");
    fflush(stdout);
    /* tail -f with colourisation; missing log: tail's error went to /dev/null
       in the original and the pipeline exits 0 silently */
    char *logpath = exo_p("/var/log/squid/access.log");
    FILE *f = fopen(logpath, "r");
    free(logpath);
    if (!f) return 0;
    /* last 10 lines first (tail default) */
    exo_sb all; exo_sb_init(&all);
    char buf[8192]; size_t nr;
    while ((nr = fread(buf, 1, sizeof buf, f)) > 0) exo_sb_addn(&all, buf, nr);
    char *content = exo_sb_steal(&all);
    {
        long lines = exo_count_substr(content, "\n");
        const char *p = content;
        for (long i = 0; i < lines - 10 && *p; i++) { const char *e = strchr(p, '\n'); if (e) p = e + 1; }
        /* print complete lines from p, colourised; remember consumed offset */
        const char *q = p;
        while (1) {
            const char *e = strchr(q, '\n');
            if (!e) break;
            char *ln = exo_strndup(q, (size_t)(e - q));
            char *col = colourise(ln);
            printf("%s\n", col);
            free(ln); free(col);
            q = e + 1;
        }
        fflush(stdout);
        /* follow: poll for appended data (partial line handling mirrors tail -f + sed line buffering closely enough) */
        exo_sb partial; exo_sb_init(&partial);
        exo_sb_add(&partial, q);
        free(content);
        for (;;) {
            clearerr(f);
            char rb[8192];
            size_t rn = fread(rb, 1, sizeof rb, f);
            if (rn > 0) {
                exo_sb_addn(&partial, rb, rn);
                char *cur = partial.buf;
                char *scan = cur;
                while (1) {
                    char *e = strchr(scan, '\n');
                    if (!e) break;
                    char *ln = exo_strndup(scan, (size_t)(e - scan));
                    char *col = colourise(ln);
                    printf("%s\n", col);
                    free(ln); free(col);
                    scan = e + 1;
                }
                fflush(stdout);
                /* keep the partial remainder */
                exo_sb rest; exo_sb_init(&rest);
                exo_sb_add(&rest, scan);
                exo_sb_free(&partial);
                partial = rest;
            } else {
                usleep(250000);
            }
        }
    }
    return 0; /* unreachable: the follow loop is the behaviour */
}

/* ========================================================== browser sandbox */
int exo_applet_enable_browser_sandbox(int argc, char **argv) {
    const char *profile_name = "playwright-chromium";
    const char *profile_file = "/etc/apparmor.d/playwright-chromium";
    const char *setup_file = "/etc/exocortical-browser-sandbox.done";
    if (getuid() != 0) {
        exo_sb m; exo_sb_init(&m);
        exo_sb_add(&m, "Run as root: sudo ");
        exo_sb_add(&m, argv[0]);
        for (int i = 1; i < argc; i++) { exo_sb_add(&m, " "); exo_sb_add(&m, argv[i]); }
        char *msg = exo_sb_steal(&m);
        printf("%s\n", msg);
        free(msg);
        return 1;
    }
    if (argc > 1 && strcmp(argv[1], "--undo") == 0) {
        if (exo_exists(profile_file))
            exo_vrun("apparmor_parser", "-R", (char *)profile_file, NULL);
        if (ROOTED) { exo_rm_f(profile_file); exo_rm_f(setup_file); }
        else { exo_vrun("rm", "-f", (char *)profile_file, NULL); exo_vrun("rm", "-f", (char *)setup_file, NULL); }
        printf("Removed %s. Chrome's sandbox is blocked again on this machine.\n", profile_name);
        return 0;
    }
    if (exo_exists(profile_file) && exo_exists(setup_file)) {
        printf("Already set up (%s). Use --undo to remove.\n", setup_file);
        return 0;
    }
    if (!exo_which("apparmor_parser")) {
        printf("apparmor_parser not found. Nothing to do on a machine without AppArmor.\n");
        return 1;
    }
    char *rstr = exo_read_raw("/proc/sys/kernel/apparmor_restrict_unprivileged_userns");
    char *rval = rstr ? exo_trim(rstr) : "";
    printf("apparmor_restrict_unprivileged_userns=%s\n", rstr ? rval : "absent");
    if (!rstr || strcmp(rval, "1") != 0) {
        printf("The userns restriction is not active here, so this is probably not your problem.\n");
        printf("Nothing installed.\n");
        if (rstr) free(rstr);
        return 1;
    }
    free(rstr);
    const char *abi = exo_exists("/etc/apparmor.d/abi/4.0") ? "abi <abi/4.0>," : "";
    {
        const char *names[] = { "ABI_LINE", "PROFILE_NAME", 0 };
        const char *vals[] = { abi, profile_name, 0 };
        char *body = exo_tmpl(SANDBOX_PROFILE_TMPL, names, vals);
        exo_write(profile_file, body, -1);
        free(body);
    }
    if (exo_vrun("apparmor_parser", "-r", (char *)profile_file, NULL) != 0) {
        printf("Profile failed to load; removing it.\n");
        if (ROOTED) exo_rm_f(profile_file);
        else exo_vrun("rm", "-f", (char *)profile_file, NULL);
        return 1;
    }
    printf("Loaded %s\n", profile_file);
    const char *su = getenv("SUDO_USER");
    const char *target = (su && *su) ? su : exo_user();
    exo_sb gp; exo_sb_init(&gp);
    exo_sb_addf(&gp, "/home/%s/.cache/ms-playwright/chromium-*/chrome-linux*/chrome", target);
    char *pattern = exo_sb_steal(&gp);
    glob_t g;
    char *chrome = NULL;
    if (glob(pattern, 0, NULL, &g) == 0 && g.gl_pathc > 0)
        chrome = exo_strdup(g.gl_pathv[g.gl_pathc - 1]);
    globfree(&g);
    free(pattern);
    if (!chrome) {
        printf("No Playwright Chrome found for %s; profile installed but not verified.\n", target);
    } else {
        char *av[] = { "sudo", "-u", (char *)target, "timeout", "30", chrome,
                       "--headless=new", "--disable-gpu", "--dump-dom", "about:blank", NULL };
        int rc; char *out = exo_cap_err(av, &rc); (void)rc;
        if (out && strstr(out, "<html")) {
            printf("Verified: Chrome launches with its sandbox enabled (%s)\n", chrome);
        } else {
            printf("Profile loaded but Chrome still failed:\n");
            if (out) {
                int n = 0;
                char *copy = exo_strdup(out);
                char *save = NULL;
                for (char *ln = strtok_r(copy, "\n", &save); ln && n < 5; ln = strtok_r(NULL, "\n", &save), n++)
                    printf("%s\n", ln);
                free(copy);
            }
            free(out); free(chrome);
            return 1;
        }
        free(out);
        free(chrome);
    }
    exo_tee(setup_file, "Done\n", -1, 0);
    printf("Next: openclaw browser start\n");
    return 0;
}
