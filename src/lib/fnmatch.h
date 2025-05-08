/* fnmatch.h */
#ifndef FNMATCH_H
#define FNMATCH_H

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

/* Match flags */
#define FNM_PATHNAME  (1<<0)  /* Slash must be matched by slash */
#define FNM_NOESCAPE  (1<<1)  /* Backslashes don't quote special chars */
#define FNM_PERIOD    (1<<2)  /* Period must be matched by period */
#define FNM_CASEFOLD  (1<<3)  /* Case-insensitive match */

/* Return code */
#define FNM_NOMATCH   1

/**
 * fnmatch() - compare string to pattern
 * @pattern: shell-style pattern
 * @string:  string to test against
 * @flags:   bitmask of FNM_* flags
 *
 * Returns 0 on match, FNM_NOMATCH on mismatch.
 */
int fnmatch(const char *pattern, const char *string, int flags);

#ifdef __cplusplus
}
#endif

#else  /* POSIX */

#include <fnmatch.h>

#endif /* _WIN32 */

#endif /* FNMATCH_H */
