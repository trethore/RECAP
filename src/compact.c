#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static int is_ident_char(int c) {
    return isalnum((unsigned char)c) || c == '_';
}

static char* shrink_buffer(char* buf, size_t len) {
    char* r = realloc(buf, len + 1);
    return r ? r : buf;
}

static char* compact_c_like(const char* input, int allow_line, int allow_block) {
    size_t n = strlen(input);
    char* out = malloc(n + 1);
    if (!out) return strdup("");

    size_t o = 0;
    int in_line = 0, in_block = 0, in_str = 0, in_chr = 0, esc = 0;
    int pending_space = 0;
    int last_ident = 0;
    char quote = 0;

    for (size_t i = 0; i < n; i++) {
        char c = input[i];

        if (in_line) {
            if (c == '\n') {
                out[o++] = '\n';
                in_line = 0;
                last_ident = 0;
            }
            continue;
        }
        if (in_block) {
            if (c == '*' && i + 1 < n && input[i + 1] == '/') {
                i++;
                in_block = 0;
                continue;
            }
            if (c == '\n') {
                out[o++] = '\n';
                last_ident = 0;
            }
            continue;
        }
        if (in_str) {
            out[o++] = c;
            if (!esc) {
                if (c == '\\') {
                    esc = 1;
                } else if (c == quote) {
                    in_str = 0;
                }
            } else {
                esc = 0;
            }
            continue;
        }
        if (in_chr) {
            out[o++] = c;
            if (!esc) {
                if (c == '\\') {
                    esc = 1;
                } else if (c == '\'') {
                    in_chr = 0;
                }
            } else {
                esc = 0;
            }
            continue;
        }

        if (allow_block && c == '/' && i + 1 < n && input[i + 1] == '*') {
            i++;
            in_block = 1;
            continue;
        }
        if (allow_line && c == '/' && i + 1 < n && input[i + 1] == '/') {
            i++;
            in_line = 1;
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\r') {
            pending_space = 1;
            continue;
        }
        if (c == '\n') {
            out[o++] = '\n';
            pending_space = 0;
            last_ident = 0;
            continue;
        }

        if (pending_space) {
            int curr_ident = is_ident_char((unsigned char)c);
            if (last_ident && (curr_ident || c == '"' || c == '<')) {
                out[o++] = ' ';
            }
            pending_space = 0;
        }

        if (c == '"') {
            out[o++] = c;
            in_str = 1;
            quote = '"';
            esc = 0;
            last_ident = 0;
            continue;
        }
        if (c == '\'') {
            out[o++] = c;
            in_chr = 1;
            esc = 0;
            last_ident = 0;
            continue;
        }

        out[o++] = c;
        last_ident = is_ident_char((unsigned char)c);
    }
    out[o] = '\0';
    return shrink_buffer(out, o);
}

static char* compact_hash_style(const char* input) {
    size_t n = strlen(input);
    char* out = malloc(n + 1);
    if (!out) return strdup("");
    size_t o = 0;
    int in_str = 0, triple = 0, esc = 0;
    char quote = 0;

    for (size_t i = 0; i < n; i++) {
        char c = input[i];
        if (in_str) {
            out[o++] = c;
            if (triple) {
                if (c == quote && i + 2 < n && input[i + 1] == quote && input[i + 2] == quote) {
                    out[o++] = input[i + 1];
                    out[o++] = input[i + 2];
                    i += 2;
                    in_str = 0;
                    triple = 0;
                }
            } else {
                if (!esc) {
                    if (c == '\\') esc = 1;
                    else if (c == quote) in_str = 0;
                } else {
                    esc = 0;
                }
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            if (i + 2 < n && input[i + 1] == c && input[i + 2] == c) {
                out[o++] = c; out[o++] = c; out[o++] = c;
                i += 2;
                in_str = 1; triple = 1; quote = c; esc = 0;
            } else {
                out[o++] = c;
                in_str = 1; triple = 0; quote = c; esc = 0;
            }
            continue;
        }

        if (c == '#') {
            while (i < n && input[i] != '\n') i++;
            if (i < n && input[i] == '\n') out[o++] = '\n';
            continue;
        }

        out[o++] = c;
    }
    out[o] = '\0';
    return shrink_buffer(out, o);
}

static char* compact_json_minify(const char* input) {
    size_t n = strlen(input);
    char* out = malloc(n + 1);
    if (!out) return strdup("");
    size_t o = 0;
    int in_str = 0, esc = 0;
    for (size_t i = 0; i < n; i++) {
        char c = input[i];
        if (in_str) {
            out[o++] = c;
            if (!esc) {
                if (c == '\\') esc = 1;
                else if (c == '"') in_str = 0;
            } else {
                esc = 0;
            }
            continue;
        }
        if (c == '"') {
            out[o++] = c;
            in_str = 1; esc = 0;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        out[o++] = c;
    }
    out[o] = '\0';
    return shrink_buffer(out, o);
}

char* apply_compact_transformations(const char* content, const char* filename) {
    char* intermediate = NULL;
    const char* ext = strrchr(filename, '.');

    if (ext) {
        if (strcmp(ext, ".json") == 0) {
            intermediate = compact_json_minify(content);
        } else if (
            strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 || strcmp(ext, ".cpp") == 0 ||
            strcmp(ext, ".hpp") == 0 || strcmp(ext, ".java") == 0 || strcmp(ext, ".js") == 0 ||
            strcmp(ext, ".ts") == 0 || strcmp(ext, ".go") == 0) {
            intermediate = compact_c_like(content, 1, 1);
        } else if (strcmp(ext, ".css") == 0) {
            intermediate = compact_c_like(content, 0, 1);
        } else if (
            strcmp(ext, ".py") == 0 || strcmp(ext, ".sh") == 0 || strcmp(ext, ".rb") == 0 || strcmp(ext, ".pl") == 0) {
            intermediate = compact_hash_style(content);
        }
    }

    if (!intermediate) {
        intermediate = strdup(content);
    }

    const char* p = intermediate;
    size_t original_len = strlen(p);
    char* result_buffer = malloc(original_len + 1);
    if (!result_buffer) {
        free(intermediate);
        return strdup("");
    }
    char* q = result_buffer;

    while (*p) {
        const char* start_of_line = p;
        const char* end_of_line = strchr(p, '\n');
        size_t line_len = end_of_line ? (size_t)(end_of_line - p) : strlen(p);

        const char* line_content_end = p + line_len;
        while (line_content_end > start_of_line && isspace((unsigned char)*(line_content_end - 1))) {
            line_content_end--;
        }

        size_t trimmed_len = line_content_end - start_of_line;

        if (trimmed_len > 0) {
            memcpy(q, start_of_line, trimmed_len);
            q += trimmed_len;
            *q++ = '\n';
        }

        if (end_of_line) {
            p = end_of_line + 1;
        } else {
            break;
        }
    }
    *q = '\0';
    free(intermediate);

    char* final_result = realloc(result_buffer, strlen(result_buffer) + 1);
    return final_result ? final_result : result_buffer;
}
