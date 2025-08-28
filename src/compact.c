#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char* remove_matches(const char* input, const char* pattern_str, uint32_t options) {
    pcre2_code* re;
    int error_code;
    PCRE2_SIZE error_offset;

    re = pcre2_compile((PCRE2_SPTR)pattern_str, PCRE2_ZERO_TERMINATED, options, &error_code, &error_offset, NULL);
    if (re == NULL) {
        return strdup(input);
    }

    PCRE2_SIZE outlen = 0;
    int rc = pcre2_substitute(re, (PCRE2_SPTR)input, PCRE2_ZERO_TERMINATED, 0, PCRE2_SUBSTITUTE_GLOBAL, NULL, NULL, (PCRE2_SPTR)"", 0, NULL, &outlen);

    if (rc < 0 && rc != PCRE2_ERROR_NOMEMORY) {
        pcre2_code_free(re);
        return strdup(input);
    }

    PCRE2_UCHAR* output_buffer = malloc(outlen);
    if (!output_buffer) {
        pcre2_code_free(re);
        return strdup(input);
    }

    rc = pcre2_substitute(re, (PCRE2_SPTR)input, PCRE2_ZERO_TERMINATED, 0, PCRE2_SUBSTITUTE_GLOBAL, NULL, NULL, (PCRE2_SPTR)"", 0, output_buffer, &outlen);
    pcre2_code_free(re);

    if (rc < 0) {
        free(output_buffer);
        return strdup(input);
    }

    char* result = strdup((char*)output_buffer);
    free(output_buffer);
    return result ? result : strdup("");
}


char* apply_compact_transformations(const char* content, const char* filename) {
    char* temp1 = NULL;
    char* temp2 = NULL;

    const char* ext = strrchr(filename, '.');
    if (ext) {
        if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 || strcmp(ext, ".cpp") == 0 ||
            strcmp(ext, ".hpp") == 0 || strcmp(ext, ".java") == 0 || strcmp(ext, ".js") == 0 ||
            strcmp(ext, ".ts") == 0 || strcmp(ext, ".css") == 0 || strcmp(ext, ".go") == 0) {

            temp1 = remove_matches(content, "/\\*.*?\\*/", PCRE2_DOTALL);
            temp2 = remove_matches(temp1, "//.*", 0);
            free(temp1);
        }
        else if (strcmp(ext, ".py") == 0 || strcmp(ext, ".sh") == 0 || strcmp(ext, ".rb") == 0 || strcmp(ext, ".pl") == 0) {
            temp2 = remove_matches(content, "#.*", 0);
        }
        else {
            temp2 = strdup(content);
        }
    }
    else {
        temp2 = strdup(content);
    }

    const char* p = temp2;
    size_t original_len = strlen(p);
    char* result_buffer = malloc(original_len + 1);
    if (!result_buffer) {
        free(temp2);
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
        }
        else {
            break;
        }
    }
    *q = '\0';
    free(temp2);

    char* final_result = realloc(result_buffer, strlen(result_buffer) + 1);
    return final_result ? final_result : result_buffer;
}