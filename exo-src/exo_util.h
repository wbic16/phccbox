/* exo_util.h -- shared layer for the Exocortical applet family in phccbox.
 *
 * Safety/test hooks (documented in exocortical-inventory.md):
 *   EXO_SYSROOT=<dir>  prefix every absolute system path (not /proc, /dev)
 *   EXO_DRYRUN=1       perform nothing; transcribe commands and writes
 *   EXO_TRANSCRIPT=<f> append one line per external command / file effect
 */
#ifndef EXO_UTIL_H
#define EXO_UTIL_H

#include <stddef.h>
#include <sys/types.h>

int         exo_dry(void);
const char *exo_root(void);                    /* "" when unset */
char       *exo_p(const char *abs);            /* malloc: sysrooted path */
void        exo_log(const char *line);         /* one transcript line */
void        exo_log_cmd(const char *kind, char *const argv[]);

/* process execution */
int  exo_run(char *const argv[]);                       /* returns exit status */
int  exo_vrun(const char *a0, ...);                     /* NULL-terminated */
int  exo_run_redir(char *const argv[], const char *in_path,
                   const char *out_path, int out_append, int err_to_null);
int  exo_sh(const char *cmd);                           /* /bin/sh -c */
char *exo_cap(char *const argv[], int *rc);             /* malloc: stdout */
int  exo_which(const char *name);                       /* PATH lookup */
const char *exo_user(void);
const char *exo_home(void);

/* file access (paths are logical absolute paths; sysroot applied inside) */
char *exo_read(const char *abs);               /* malloc or NULL */
char *exo_read_raw(const char *path);          /* no sysroot */
int   exo_exists(const char *abs);
int   exo_isdir(const char *abs);
int   exo_mkdir_p(const char *abs);
int   exo_write(const char *abs, const char *data, long len);
int   exo_append(const char *abs, const char *data, long len);
int   exo_tee(const char *abs, const char *data, long len, int append);
int   exo_cp(const char *src, const char *dst_abs);     /* src as given */
int   exo_rm_f(const char *abs);
int   exo_rmdir(const char *abs);
int   exo_symlink(const char *target, const char *link_abs);
int   exo_chmod(const char *abs, mode_t mode);
long  exo_memtotal_kb(void);                   /* from /proc/meminfo, -1 err */

/* tiny string builder */
typedef struct { char *buf; size_t len, cap; } exo_sb;
void  exo_sb_init(exo_sb *s);
void  exo_sb_addn(exo_sb *s, const char *p, size_t n);
void  exo_sb_add(exo_sb *s, const char *p);
void  exo_sb_addf(exo_sb *s, const char *fmt, ...);
char *exo_sb_steal(exo_sb *s);                 /* malloc'd contents */
void  exo_sb_free(exo_sb *s);

/* helpers */
char *exo_strdup(const char *s);
char *exo_strndup(const char *s, size_t n);
int   exo_contains(const char *hay, const char *needle);        /* substring */
int   exo_contains_ci(const char *hay, const char *needle);
long  exo_count_substr(const char *hay, const char *needle);    /* line-free count */
long  exo_count_lines_containing(const char *text, const char *needle);
char *exo_replace_all(const char *s, const char *from, const char *to);
char *exo_trim(char *s);                                        /* in place */
/* expand a %%VAR%% template using name/value pairs (NULL-terminated arrays) */
char *exo_tmpl(const char *tmpl, const char *const *names,
               const char *const *vals);

#endif
char *exo_cap_err(char *const argv[], int *rc);  /* stdout+stderr merged */
