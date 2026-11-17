/* applets_g.c -- Exocortical family: exo-agentmail (libcurl), exo-console
 * (exo.js behaviour), ports (ports.html submitter behaviour). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include "exo_util.h"
#include "gen/lit_ports.h"
#include "gen/lit_exoconsole.h"

/* ============================================================ libcurl */
/* Hand-declared minimal libcurl surface (no dev headers on the ranch). */
typedef void CURL;
typedef int CURLcode;
typedef struct curl_slist { char *data; struct curl_slist *next; } curl_slist;
extern CURL *curl_easy_init(void);
extern CURLcode curl_easy_setopt(CURL *handle, int option, ...);
extern CURLcode curl_easy_perform(CURL *handle);
extern void curl_easy_cleanup(CURL *handle);
extern char *curl_easy_escape(CURL *handle, const char *string, int length);
extern void curl_free(void *p);
extern curl_slist *curl_slist_append(curl_slist *list, const char *string);
extern void curl_slist_free_all(curl_slist *list);
#define CURLOPT_URL 10002
#define CURLOPT_HTTPHEADER 10023
#define CURLOPT_WRITEFUNCTION 20011
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_POSTFIELDS 10015
#define CURLOPT_POST 47L

static size_t curl_sink(void *ptr, size_t size, size_t nmemb, void *ud) {
    exo_sb *s = (exo_sb *)ud;
    exo_sb_addn(s, (const char *)ptr, size * nmemb);
    return size * nmemb;
}
static char *http_do(const char *url, const char *auth, const char *post_body) {
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    exo_sb body; exo_sb_init(&body);
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, (void *)curl_sink);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_slist *hdrs = NULL;
    char authbuf[1200];
    snprintf(authbuf, sizeof authbuf, "Authorization: Bearer %s", auth);
    hdrs = curl_slist_append(hdrs, authbuf);
    if (post_body) {
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, post_body);
    }
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    if (rc != 0) { exo_sb_free(&body); return NULL; }
    return exo_sb_steal(&body);
}
static char *json_escape(const char *s) {
    exo_sb b; exo_sb_init(&b);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"': exo_sb_add(&b, "\\\""); break;
            case '\\': exo_sb_add(&b, "\\\\"); break;
            case '\n': exo_sb_add(&b, "\\n"); break;
            case '\r': exo_sb_add(&b, "\\r"); break;
            case '\t': exo_sb_add(&b, "\\t"); break;
            default:
                if (*p < 0x20) exo_sb_addf(&b, "\\u%04x", *p);
                else exo_sb_addn(&b, (const char *)p, 1);
        }
    }
    return exo_sb_steal(&b);
}

/* ============================================================ agentmail */
static void load_dotenv(const char *path) {
    char *content = exo_read_raw(path);
    if (!content) return;
    char *copy = exo_strdup(content);
    char *save = NULL;
    for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *t = exo_trim(ln);
        if (!*t || *t == '#') continue;
        if (strncmp(t, "export ", 7) == 0) t += 7;
        char *eq = strchr(t, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = exo_trim(t);
        char *val = exo_trim(eq + 1);
        size_t vl = strlen(val);
        if (vl >= 2 && ((val[0] == '"' && val[vl - 1] == '"') || (val[0] == '\'' && val[vl - 1] == '\''))) {
            val[vl - 1] = 0; val++;
        }
        if (!getenv(key)) setenv(key, val, 1);   /* dotenv does not override */
    }
    free(copy);
    free(content);
}
int exo_applet_exo_agentmail(int argc, char **argv) {
    load_dotenv(".env");
    /* node identity from /etc/mirrorborn.phext (k=v lines) */
    char email[512] = "", name[512] = "";
    {
        char *cfg = exo_read("/etc/mirrorborn.phext");
        if (cfg) {
            char *copy = exo_strdup(cfg);
            char *save = NULL;
            for (char *ln = strtok_r(copy, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
                char *eq = strchr(ln, '=');
                if (!eq) continue;
                char *k = exo_strndup(ln, (size_t)(eq - ln));
                char *kt = exo_trim(k);
                char *v = exo_trim(eq + 1);
                if (strcmp(kt, "email") == 0) snprintf(email, sizeof email, "%s", v);
                if (strcmp(kt, "name") == 0) snprintf(name, sizeof name, "%s", v);
                free(k);
            }
            free(copy);
            free(cfg);
        }
    }
    const char *key = getenv("AGENTMAIL_API_KEY");
    if (!key || !*key) {
        fprintf(stderr, "exo-agentmail: AGENTMAIL_API_KEY is not set\n");
        return 1;
    }
    const char *base = getenv("AGENTMAIL_BASE_URL");
    if (!base || !*base) base = "https://api.agentmail.to/v0";
    CURL *esc = curl_easy_init();
    char *email_esc = esc ? curl_easy_escape(esc, email, 0) : exo_strdup(email);
    /* 1. GET the inbox and print it (precedes argument parsing, as in the original) */
    {
        exo_sb u; exo_sb_init(&u);
        exo_sb_add(&u, base); exo_sb_add(&u, "/inboxes/"); exo_sb_add(&u, email_esc);
        char *url = exo_sb_steal(&u);
        exo_log_cmd("GET", (char *[]){ url, NULL });
        if (!exo_dry()) {
            char *resp = http_do(url, key, NULL);
            if (resp) {
                fputs(resp, stdout);
                if (resp[0] && resp[strlen(resp) - 1] != '\n') putchar('\n');
                free(resp);
            }
        }
        free(url);
    }
    /* 2. argparse-equivalent parsing */
    const char *recipient = "will@phext.io";
    exo_sb sd; exo_sb_init(&sd); exo_sb_add(&sd, "Message from "); exo_sb_add(&sd, name);
    char *subject_default = exo_sb_steal(&sd);
    const char *subject = subject_default;
    const char *message = "";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char **target = NULL;
        const char *val = NULL;
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf("usage: exo-agentmail.py [-h] [--recipient RECIPIENT] [--subject SUBJECT]\n");
            printf("                         [--message MESSAGE]\n");
            free(subject_default);
            if (esc) curl_easy_cleanup(esc);
            return 0;
        } else if (strcmp(a, "--recipient") == 0) { target = &recipient; }
        else if (strcmp(a, "--subject") == 0) { target = &subject; }
        else if (strcmp(a, "--message") == 0) { target = &message; }
        else if (strncmp(a, "--recipient=", 12) == 0) { recipient = a + 12; continue; }
        else if (strncmp(a, "--subject=", 10) == 0) { subject = a + 10; continue; }
        else if (strncmp(a, "--message=", 10) == 0) { message = a + 10; continue; }
        else {
            fprintf(stderr, "usage: exo-agentmail.py [-h] [--recipient RECIPIENT] [--subject SUBJECT]\n");
            fprintf(stderr, "                         [--message MESSAGE]\n");
            fprintf(stderr, "exo-agentmail.py: error: unrecognized arguments: %s\n", a);
            free(subject_default);
            if (esc) curl_easy_cleanup(esc);
            return 2;
        }
        if (target) {
            if (i + 1 >= argc) {
                fprintf(stderr, "exo-agentmail.py: error: argument %s: expected one argument\n", a);
                free(subject_default);
                if (esc) curl_easy_cleanup(esc);
                return 2;
            }
            val = argv[++i];
            *target = val;
        }
    }
    /* 3. send only when all three are non-empty */
    if (recipient[0] && subject[0] && message[0]) {
        char *er = json_escape(recipient), *es = json_escape(subject), *em = json_escape(message);
        exo_sb jb; exo_sb_init(&jb);
        exo_sb_addf(&jb, "{\"to\": \"%s\", \"subject\": \"%s\", \"text\": \"%s\"}", er, es, em);
        char *payload = exo_sb_steal(&jb);
        free(er); free(es); free(em);
        exo_sb u; exo_sb_init(&u);
        exo_sb_add(&u, base); exo_sb_add(&u, "/inboxes/"); exo_sb_add(&u, email_esc); exo_sb_add(&u, "/messages/send");
        char *url = exo_sb_steal(&u);
        exo_log_cmd("POST", (char *[]){ url, payload, NULL });
        if (!exo_dry()) {
            char *resp = http_do(url, key, payload);
            if (resp) {
                fputs(resp, stdout);
                if (resp[0] && resp[strlen(resp) - 1] != '\n') putchar('\n');
                free(resp);
            }
        }
        free(url);
        free(payload);
    } else {
        printf("Usage: exo-agentmail.py <recipient> <subject> <message>\n");
    }
    free(subject_default);
    if (esc) { curl_free(email_esc); curl_easy_cleanup(esc); }
    return 0;
}

/* ============================================================ tiny JSON */
typedef enum { JNULL, JBOOL, JNUM, JSTR, JARR, JOBJ } jtype;
typedef struct jval jval;
typedef struct { char *key; jval *val; } jpair;
struct jval {
    jtype t;
    double num; char *raw;          /* numbers keep their raw text */
    char *str;
    jval **items; size_t n, cap;    /* array */
    jpair *pairs; size_t np, pcap;  /* object */
};
static jval *jnew(jtype t) { jval *v = calloc(1, sizeof(jval)); v->t = t; return v; }
static void jfree(jval *v) {
    if (!v) return;
    free(v->raw); free(v->str);
    for (size_t i = 0; i < v->n; i++) jfree(v->items[i]);
    free(v->items);
    for (size_t i = 0; i < v->np; i++) { free(v->pairs[i].key); jfree(v->pairs[i].val); }
    free(v->pairs);
    free(v);
}
static void jarr_push(jval *a, jval *item) {
    if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 8; a->items = realloc(a->items, a->cap * sizeof(jval *)); }
    a->items[a->n++] = item;
}
static void jobj_put(jval *o, const char *key, jval *val) {
    for (size_t i = 0; i < o->np; i++)
        if (strcmp(o->pairs[i].key, key) == 0) { jfree(o->pairs[i].val); o->pairs[i].val = val; return; }
    if (o->np == o->pcap) { o->pcap = o->pcap ? o->pcap * 2 : 8; o->pairs = realloc(o->pairs, o->pcap * sizeof(jpair)); }
    o->pairs[o->np].key = exo_strdup(key);
    o->pairs[o->np].val = val;
    o->np++;
}
static jval *jobj_get(jval *o, const char *key) {
    if (!o || o->t != JOBJ) return NULL;
    for (size_t i = 0; i < o->np; i++) if (strcmp(o->pairs[i].key, key) == 0) return o->pairs[i].val;
    return NULL;
}
typedef struct { const char *p; } jparse;
static void jskip(jparse *s) { while (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r') s->p++; }
static jval *jparse_val(jparse *s);
static char *jparse_str_raw(jparse *s) {
    /* s->p at opening quote; returns unescaped string */
    s->p++;
    exo_sb b; exo_sb_init(&b);
    while (*s->p && *s->p != '"') {
        if (*s->p == '\\' && s->p[1]) {
            s->p++;
            switch (*s->p) {
                case 'n': exo_sb_add(&b, "\n"); break;
                case 't': exo_sb_add(&b, "\t"); break;
                case 'r': exo_sb_add(&b, "\r"); break;
                case 'b': exo_sb_add(&b, "\b"); break;
                case 'f': exo_sb_add(&b, "\f"); break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4 && isxdigit((unsigned char)s->p[1]); i++) {
                        cp = cp * 16 + (isdigit((unsigned char)s->p[1]) ? s->p[1] - '0' : (tolower((unsigned char)s->p[1]) - 'a' + 10));
                        s->p++;
                    }
                    char u8[4]; int n = 0;
                    if (cp < 0x80) u8[n++] = (char)cp;
                    else if (cp < 0x800) { u8[n++] = (char)(0xC0 | (cp >> 6)); u8[n++] = (char)(0x80 | (cp & 63)); }
                    else { u8[n++] = (char)(0xE0 | (cp >> 12)); u8[n++] = (char)(0x80 | ((cp >> 6) & 63)); u8[n++] = (char)(0x80 | (cp & 63)); }
                    exo_sb_addn(&b, u8, (size_t)n);
                    break;
                }
                default: exo_sb_addn(&b, s->p, 1);
            }
            s->p++;
        } else {
            exo_sb_addn(&b, s->p, 1);
            s->p++;
        }
    }
    if (*s->p == '"') s->p++;
    return exo_sb_steal(&b);
}
static jval *jparse_val(jparse *s) {
    jskip(s);
    if (*s->p == '{') {
        s->p++;
        jval *o = jnew(JOBJ);
        jskip(s);
        if (*s->p == '}') { s->p++; return o; }
        while (1) {
            jskip(s);
            char *k = jparse_str_raw(s);
            jskip(s);
            if (*s->p == ':') s->p++;
            jval *v = jparse_val(s);
            jobj_put(o, k, v);
            free(k);
            jskip(s);
            if (*s->p == ',') { s->p++; continue; }
            if (*s->p == '}') s->p++;
            break;
        }
        return o;
    }
    if (*s->p == '[') {
        s->p++;
        jval *a = jnew(JARR);
        jskip(s);
        if (*s->p == ']') { s->p++; return a; }
        while (1) {
            jarr_push(a, jparse_val(s));
            jskip(s);
            if (*s->p == ',') { s->p++; continue; }
            if (*s->p == ']') s->p++;
            break;
        }
        return a;
    }
    if (*s->p == '"') { jval *v = jnew(JSTR); v->str = jparse_str_raw(s); return v; }
    if (strncmp(s->p, "true", 4) == 0) { s->p += 4; jval *v = jnew(JBOOL); v->num = 1; return v; }
    if (strncmp(s->p, "false", 5) == 0) { s->p += 5; return jnew(JBOOL); }
    if (strncmp(s->p, "null", 4) == 0) { s->p += 4; return jnew(JNULL); }
    const char *start = s->p;
    while (*s->p && (isdigit((unsigned char)*s->p) || strchr("-+.eE", *s->p))) s->p++;
    jval *v = jnew(JNUM);
    v->raw = exo_strndup(start, (size_t)(s->p - start));
    v->num = strtod(v->raw, NULL);
    return v;
}
static void jser_str(exo_sb *b, const char *s) {
    exo_sb_add(b, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"': exo_sb_add(b, "\\\""); break;
            case '\\': exo_sb_add(b, "\\\\"); break;
            case '\n': exo_sb_add(b, "\\n"); break;
            case '\r': exo_sb_add(b, "\\r"); break;
            case '\t': exo_sb_add(b, "\\t"); break;
            case '\b': exo_sb_add(b, "\\b"); break;
            case '\f': exo_sb_add(b, "\\f"); break;
            default:
                if (*p < 0x20) exo_sb_addf(b, "\\u%04x", *p);
                else exo_sb_addn(b, (const char *)p, 1);
        }
    }
    exo_sb_add(b, "\"");
}
static void jser(exo_sb *b, jval *v) {
    switch (v->t) {
        case JNULL: exo_sb_add(b, "null"); break;
        case JBOOL: exo_sb_add(b, v->num ? "true" : "false"); break;
        case JNUM: {
            /* JS prints integer-valued numbers without a decimal point */
            if (v->raw && strpbrk(v->raw, ".eE") == NULL) exo_sb_add(b, v->raw);
            else exo_sb_addf(b, "%.17g", v->num);
            break;
        }
        case JSTR: jser_str(b, v->str); break;
        case JARR:
            exo_sb_add(b, "[");
            for (size_t i = 0; i < v->n; i++) { if (i) exo_sb_add(b, ","); jser(b, v->items[i]); }
            exo_sb_add(b, "]");
            break;
        case JOBJ:
            exo_sb_add(b, "{");
            for (size_t i = 0; i < v->np; i++) {
                if (i) exo_sb_add(b, ",");
                jser_str(b, v->pairs[i].key);
                exo_sb_add(b, ":");
                jser(b, v->pairs[i].val);
            }
            exo_sb_add(b, "}");
            break;
    }
}
static jval *jstr_new(const char *s) { jval *v = jnew(JSTR); v->str = exo_strdup(s); return v; }
static jval *jnum_new_ll(long long x) {
    jval *v = jnew(JNUM);
    char buf[32]; snprintf(buf, sizeof buf, "%lld", x);
    v->raw = exo_strdup(buf); v->num = (double)x;
    return v;
}

/* ============================================================ exo-console */
/* "now" may be pinned with EXO_CONSOLE_NOW=<epoch-seconds> (test hook). */
static time_t console_now(void) {
    const char *e = getenv("EXO_CONSOLE_NOW");
    if (e && *e) return (time_t)strtoll(e, NULL, 10);
    return time(NULL);
}
static void fmt_utc_date(time_t t, char *buf, size_t n) {
    struct tm tmv; gmtime_r(&t, &tmv);
    strftime(buf, n, "%Y-%m-%d", &tmv);
}
static void fmt_iso_ms(time_t t, char *buf, size_t n) {
    struct tm tmv; gmtime_r(&t, &tmv);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);
}
static long long tessera_day(time_t now) {
    struct tm start; memset(&start, 0, sizeof start);
    start.tm_year = 2023 - 1900; start.tm_mon = 2; start.tm_mday = 26;
    time_t s = timegm(&start);
    double diff = difftime(now, s);
    if (diff < 0) diff = -diff;
    return (long long)(diff / 86400.0 + 0.999999999);  /* ceil for positive */
}
static const char *console_store_path(void) {
    static char buf[4096];
    const char *e = getenv("EXO_CONSOLE_STORE");
    if (e && *e) return e;
    snprintf(buf, sizeof buf, "%s/.exocortex-2130.json", exo_home());
    return buf;
}
static jval *console_load(void) {
    char *content = exo_read_raw(console_store_path());
    jval *root;
    if (content) {
        jparse p = { content };
        root = jparse_val(&p);
        free(content);
        if (!root) root = jnew(JOBJ);
    } else {
        root = jnew(JOBJ);
    }
    if (!jobj_get(root, "logs")) jobj_put(root, "logs", jnew(JARR));
    if (!jobj_get(root, "weeklyLogs")) jobj_put(root, "weeklyLogs", jnew(JOBJ));
    /* loadState filters state.logs to today (UTC) */
    char today[16]; fmt_utc_date(console_now(), today, sizeof today);
    jval *logs = jobj_get(root, "logs");
    jval *filtered = jnew(JARR);
    for (size_t i = 0; i < logs->n; i++) {
        jval *d = jobj_get(logs->items[i], "date");
        if (d && d->t == JSTR && strcmp(d->str, today) == 0) jarr_push(filtered, logs->items[i]);
        else jfree(logs->items[i]);
    }
    free(logs->items); logs->items = filtered->items; logs->n = filtered->n; logs->cap = filtered->cap;
    free(filtered);
    return root;
}
static void console_save(jval *root) {
    exo_sb b; exo_sb_init(&b);
    jser(&b, root);
    char *out = exo_sb_steal(&b);
    const char *path = console_store_path();
    exo_sb l; exo_sb_init(&l); exo_sb_add(&l, "WRITE "); exo_sb_add(&l, path);
    char *line = exo_sb_steal(&l); exo_log(line); free(line);
    if (!exo_dry()) {
        FILE *f = fopen(path, "w");
        if (f) { fputs(out, f); fclose(f); }
    }
    free(out);
}
static void console_usage(void) {
    fprintf(stderr, "usage: exo-console <log|list|distribution|week|tessera|slot|quote|export>\n");
    fprintf(stderr, "  log --category dev|ops|sales [--persona X]... <text...>\n");
}
int exo_applet_exo_console(int argc, char **argv) {
    if (argc < 2) { console_usage(); return 1; }
    const char *cmd = argv[1];
    time_t now = console_now();
    char today[16]; fmt_utc_date(now, today, sizeof today);

    if (strcmp(cmd, "log") == 0) {
        const char *category = NULL;
        const char *personas[64]; int np = 0;
        exo_sb text; exo_sb_init(&text);
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--category") == 0 && i + 1 < argc) category = argv[++i];
            else if (strncmp(argv[i], "--category=", 11) == 0) category = argv[i] + 11;
            else if (strcmp(argv[i], "--persona") == 0 && i + 1 < argc && np < 64) personas[np++] = argv[++i];
            else if (strncmp(argv[i], "--persona=", 10) == 0 && np < 64) personas[np++] = argv[i] + 10;
            else { if (text.len) exo_sb_add(&text, " "); exo_sb_add(&text, argv[i]); }
        }
        char *txt = exo_sb_steal(&text);
        char *trimmed = exo_trim(txt);
        if (!*trimmed) { printf("Write something first!\n"); free(txt); return 1; }
        if (!category || (strcmp(category, "dev") && strcmp(category, "ops") && strcmp(category, "sales"))) {
            printf("Select DEV, OPS, or SALES\n"); free(txt); return 1;
        }
        jval *root = console_load();
        struct tm local; localtime_r(&now, &local);
        int slot_min = (local.tm_min / 15) * 15;
        char slot[8]; snprintf(slot, sizeof slot, "%02d:%02d", local.tm_hour, slot_min);
        char iso[40]; fmt_iso_ms(now, iso, sizeof iso);
        jval *log = jnew(JOBJ);
        jobj_put(log, "id", jnum_new_ll((long long)now * 1000));
        jobj_put(log, "date", jstr_new(today));
        jobj_put(log, "slotId", jstr_new(slot));
        jobj_put(log, "time", jstr_new(iso));
        jobj_put(log, "category", jstr_new(category));
        jobj_put(log, "text", jstr_new(trimmed));
        jval *parr = jnew(JARR);
        for (int i = 0; i < np; i++) jarr_push(parr, jstr_new(personas[i]));
        jobj_put(log, "personas", parr);
        jarr_push(jobj_get(root, "logs"), log);
        jval *weekly = jobj_get(root, "weeklyLogs");
        jval *day = jobj_get(weekly, today);
        if (!day) { day = jnew(JARR); jobj_put(weekly, today, day); }
        /* weeklyLogs holds a second reference in JS; the store serialises a copy */
        exo_sb b; exo_sb_init(&b); jser(&b, log);
        char *lj = exo_sb_steal(&b);
        jparse p = { lj };
        jval *log_copy = jparse_val(&p);
        free(lj);
        jarr_push(day, log_copy);
        console_save(root);
        jfree(root);
        free(txt);
        return 0;
    }
    if (strcmp(cmd, "list") == 0) {
        jval *root = console_load();
        jval *logs = jobj_get(root, "logs");
        /* newest first by time (ISO strings sort chronologically) */
        for (size_t i = 1; i < logs->n; i++) {
            jval *tmp = logs->items[i]; size_t j = i;
            jval *tt = jobj_get(tmp, "time");
            while (j > 0) {
                jval *pt = jobj_get(logs->items[j - 1], "time");
                if (pt && tt && strcmp(pt->str, tt->str) <= 0) break;
                logs->items[j] = logs->items[j - 1]; j--;
            }
            logs->items[j] = tmp;
        }
        for (size_t i = 0; i < logs->n; i++) {
            jval *l = logs->items[i];
            jval *slot = jobj_get(l, "slotId"), *cat = jobj_get(l, "category"), *tx = jobj_get(l, "text");
            char up[16] = ""; 
            if (cat) { snprintf(up, sizeof up, "%s", cat->str); for (char *q = up; *q; q++) *q = toupper((unsigned char)*q); }
            printf("%s [%s] %s", slot ? slot->str : "", up, tx ? tx->str : "");
            jval *ps = jobj_get(l, "personas");
            if (ps && ps->n) {
                printf(" *Personas: ");
                for (size_t k = 0; k < ps->n; k++) printf("%s%s", k ? ", " : "", ps->items[k]->str);
                printf("*");
            }
            printf("\n");
        }
        jfree(root);
        return 0;
    }
    if (strcmp(cmd, "distribution") == 0) {
        jval *root = console_load();
        jval *logs = jobj_get(root, "logs");
        long counts[3] = { 0, 0, 0 };
        const char *cats[3] = { "dev", "ops", "sales" };
        for (size_t i = 0; i < logs->n; i++) {
            jval *c = jobj_get(logs->items[i], "category");
            if (c) for (int k = 0; k < 3; k++) if (strcmp(c->str, cats[k]) == 0) counts[k]++;
        }
        long total = (long)logs->n;
        long denom = total ? total : 1;
        for (int k = 0; k < 3; k++)
            printf("%s: %ld (%.1f%%)\n", cats[k], counts[k], counts[k] * 100.0 / denom);
        jfree(root);
        return 0;
    }
    if (strcmp(cmd, "week") == 0) {
        jval *root = console_load();
        jval *weekly = jobj_get(root, "weeklyLogs");
        struct tm local; localtime_r(&now, &local);
        int dow = local.tm_wday; /* 0 = Sunday */
        int back = (dow == 0) ? 6 : dow - 1;
        time_t monday = now - (time_t)back * 86400;
        const char *letters = "MTWTFSS";
        long total = 0;
        for (int i = 0; i < 7; i++) {
            time_t d = monday + (time_t)i * 86400;
            char ds[16]; fmt_utc_date(d, ds, sizeof ds);
            jval *day = jobj_get(weekly, ds);
            long n = day ? (long)day->n : 0;
            total += n;
            printf("%c %s: %ld%s\n", letters[i], ds, n, strcmp(ds, today) == 0 ? " (today)" : "");
        }
        printf("Total: %ld logs, %.1f hours\n", total, total * 15.0 / 60.0);
        jfree(root);
        return 0;
    }
    if (strcmp(cmd, "tessera") == 0) {
        printf("Tessera Day: %lld\n", tessera_day(now));
        return 0;
    }
    if (strcmp(cmd, "slot") == 0) {
        struct tm local; localtime_r(&now, &local);
        int slot_min = (local.tm_min / 15) * 15;
        printf("Current slot: %02d:%02d\n", local.tm_hour, slot_min);
        printf("%s\n", local.tm_hour >= 21 ? "Exo time (21:00+)" : "Pre-exo time");
        return 0;
    }
    if (strcmp(cmd, "quote") == 0) {
        int nq = 0; while (EXO_QUOTES[nq]) nq++;
        srand((unsigned)(now ^ getpid()));
        printf("%s\n", EXO_QUOTES[nq ? rand() % nq : 0]);
        return 0;
    }
    if (strcmp(cmd, "export") == 0) {
        jval *root = console_load();
        jval *logs = jobj_get(root, "logs");
        long counts[3] = { 0, 0, 0 };
        const char *cats[3] = { "dev", "ops", "sales" };
        for (size_t i = 0; i < logs->n; i++) {
            jval *c = jobj_get(logs->items[i], "category");
            if (c) for (int k = 0; k < 3; k++) if (strcmp(c->str, cats[k]) == 0) counts[k]++;
        }
        printf("# Exocortex 2130 - Nightly Log\n\n");
        printf("**Date:** %s\n", today);
        printf("**Tessera Day:** %lld\n\n", tessera_day(now));
        printf("## Summary\n\n");
        printf("- **Total Logs:** %zu\n", logs->n);
        printf("- **DEV:** %ld\n", counts[0]);
        printf("- **OPS:** %ld\n", counts[1]);
        printf("- **SALES:** %ld\n\n", counts[2]);
        printf("## Logs\n\n");
        for (size_t i = 0; i < logs->n; i++) {
            jval *l = logs->items[i];
            jval *slot = jobj_get(l, "slotId"), *cat = jobj_get(l, "category"), *tx = jobj_get(l, "text");
            char up[16] = "";
            if (cat) { snprintf(up, sizeof up, "%s", cat->str); for (char *q = up; *q; q++) *q = toupper((unsigned char)*q); }
            printf("### %s [%s]\n\n", slot ? slot->str : "", up);
            printf("%s\n\n", tx ? tx->str : "");
            jval *ps = jobj_get(l, "personas");
            if (ps && ps->n) {
                printf("*Personas: ");
                for (size_t k = 0; k < ps->n; k++) printf("%s%s", k ? ", " : "", ps->items[k]->str);
                printf("*\n\n");
            }
        }
        printf("---\n*Build the future from 2130*\n");
        jfree(root);
        return 0;
    }
    console_usage();
    return 1;
}

/* ============================================================ ports */
static char *form_encode(const char *s) {
    exo_sb b; exo_sb_init(&b);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '.' || *p == '_' || *p == '*') exo_sb_addn(&b, (const char *)p, 1);
        else if (*p == ' ') exo_sb_add(&b, "+");
        else exo_sb_addf(&b, "%%%02X", *p);
    }
    return exo_sb_steal(&b);
}
static char *ports_build_url(const char *host, long n, const char *p, const char *c, const char *s) {
    char *ep = form_encode(p), *ec = form_encode(c), *es = form_encode(s);
    exo_sb u; exo_sb_init(&u);
    exo_sb_addf(&u, "http://%s:%ld/api/v2/select?p=%s&c=%s&s=%s", host, 11000 + n, ep, ec, es);
    free(ep); free(ec); free(es);
    return exo_sb_steal(&u);
}
int exo_applet_ports(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "list") == 0) {
        printf("Hosts:\n");
        for (int i = 0; PORTS_HOSTS[i]; i += 2)
            printf("  %s (%s)\n", PORTS_HOSTS[i], PORTS_HOSTS[i + 1]);
        printf("Personas (port = 11000 + N):\n");
        for (int i = 0; PORTS_PERSONAS[i]; i += 2)
            printf("  #%s %s\n", PORTS_PERSONAS[i], PORTS_PERSONAS[i + 1]);
        return 0;
    }
    if (argc >= 4 && (strcmp(argv[1], "url") == 0 || strcmp(argv[1], "fetch") == 0)) {
        const char *host = argv[2];
        long n = strtol(argv[3], NULL, 10);
        /* the page's form only ever submits a listed host and persona;
           the CLI keeps the same boundary */
        int hok = 0;
        for (int i = 0; PORTS_HOSTS[i]; i += 2)
            if (strcmp(PORTS_HOSTS[i], host) == 0) hok = 1;
        if (!hok || n < 1 || n > 101) {
            fprintf(stderr, "ports: unknown host or persona (see: ports list)\n");
            return 1;
        }
        const char *p = argc > 4 ? argv[4] : "index";
        const char *c = argc > 5 ? argv[5] : "1.1.1/1.1.1/1.1.1";
        const char *s = argc > 6 ? argv[6] : "";
        char *url = ports_build_url(host, n, p, c, s);
        if (strcmp(argv[1], "url") == 0) {
            printf("%s\n", url);
        } else {
            exo_log_cmd("GET", (char *[]){ url, NULL });
            if (!exo_dry()) {
                CURL *cu = curl_easy_init();
                if (cu) {
                    exo_sb body; exo_sb_init(&body);
                    curl_easy_setopt(cu, CURLOPT_URL, url);
                    curl_easy_setopt(cu, CURLOPT_WRITEFUNCTION, (void *)curl_sink);
                    curl_easy_setopt(cu, CURLOPT_WRITEDATA, &body);
                    if (curl_easy_perform(cu) == 0) {
                        char *out = exo_sb_steal(&body);
                        fputs(out, stdout);
                        free(out);
                    } else exo_sb_free(&body);
                    curl_easy_cleanup(cu);
                }
            }
        }
        free(url);
        return 0;
    }
    fprintf(stderr, "usage: ports list | ports url <host> <persona#> [phext] [coordinate] [message] | ports fetch ...\n");
    fprintf(stderr, "  hosts: logos-prime, halcyon-vector, aletheia-core, chrysalis-hub, aurora-continuum, luma\n");
    return 1;
}
