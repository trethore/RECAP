/* fnmatch.c */
#include "fnmatch.h"

#if defined(_WIN32)

#include <ctype.h>
#include <string.h>
#include "fnmatch.h"

/*
 * Helper: match a [...] range. Returns:
 *   0 if test is in the set,
 *   1 if not,
 *  -1 on malformed pattern.
 * Advances *newp to just past the closing ']'.
 */
static int rangematch(const char *pattern, char test, int flags, const char **newp)
{
    int negate = 0, match = 0;
    const char *p = pattern;

    if (*p == '!' || *p == '^') {
        negate = 1;
        p++;
    }

    /* Empty class is a literal '[' */
    if (*p == ']' || *p == '\0')
        return -1;

    for ( ; *p && *p != ']'; p++) {
        char lo = *p, hi = lo;

        if (lo == '\\' && !(flags & FNM_NOESCAPE) && p[1]) {
            lo = p[1];
            p++;
        }

        if (p[1] == '-' && p[2] && p[2] != ']') {
            hi = p[2];
            p += 2;
        }

        if (flags & FNM_CASEFOLD) {
            lo = tolower((unsigned char)lo);
            hi = tolower((unsigned char)hi);
            test = tolower((unsigned char)test);
        }

        if (lo <= test && test <= hi)
            match = 1;
    }

    if (*p != ']')
        return -1;  /* no closing ] */

    *newp = p + 1;
    return negate ? !match : match ? 0 : 1;
}

int fnmatch(const char *pattern, const char *string, int flags)
{
    const char *p = pattern;
    const char *s = string;

    /* Leading period must match literally unless FNM_PERIOD is off */
    if (*s == '.' && !(flags & FNM_PERIOD)) {
        if (*p != '.' && *p != '[')
            return FNM_NOMATCH;
    }

    while (*p) {
        char c = *p++;

        switch (c) {
        case '?':
            if (!*s) 
                return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && (*s == '/' || *s == '\\'))
                return FNM_NOMATCH;
            s++;
            break;

        case '*': {
            /* collapse multiple '*' */
            while (*p == '*') p++;

            /* If trailing '*', match rest */
            if (!*p) {
                if (flags & FNM_PATHNAME) {
                    if (strchr(s, '/') || strchr(s, '\\'))
                        return FNM_NOMATCH;
                }
                return 0;
            }

            /* Try to match at each position */
            while (*s) {
                if ((flags & FNM_PATHNAME) && (*s == '/' || *s == '\\'))
                    break;
                if (fnmatch(p, s, flags) == 0)
                    return 0;
                s++;
            }
            return FNM_NOMATCH;
        }

        case '[': {
            if (!*s)
                return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && (*s == '/' || *s == '\\'))
                return FNM_NOMATCH;

            const char *newp;
            int rm = rangematch(p, *s, flags, &newp);
            if (rm < 0 || rm > 0)
                return FNM_NOMATCH;

            p = newp;
            s++;
            break;
        }

        case '\\':
            if (!(flags & FNM_NOESCAPE) && *p)
                c = *p++;
            [[fallthrough]];

        default:
            if (flags & FNM_CASEFOLD) {
                if (tolower((unsigned char)c) != tolower((unsigned char)*s))
                    return FNM_NOMATCH;
            } else if (c != *s) {
                return FNM_NOMATCH;
            }
            s++;
            break;
        }
    }

    return *s ? FNM_NOMATCH : 0;
}

#else 

#include <fnmatch.h>

#endif /* _WIN32 */
