#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

int is_text_file(const char* full_path) {
    FILE* file = fopen(full_path, "rb");
    if (!file) {
        perror("fopen in is_text_file");
        return 0;
    }

    unsigned char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);

    if (ferror(file)) {
        fprintf(stderr, "Warning: I/O error reading %s\n", full_path);
        fclose(file);
        return 0;
    }
    fclose(file);

    if (bytes_read == 0) {
        return 1;
    }

    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\0') {
            return 0;
        }
    }

    return 1;
}

void normalize_path(char* path) {
    if (!path) return;

    size_t n = strlen(path);
    if (n == 0) {
        strcpy(path, ".");
        return;
    }

    char* result = path;
    size_t write_idx = 0;
    size_t read_idx = 0;
    int is_absolute = (path[0] == '/');
    int segments = 0;

    if (is_absolute) {
        result[write_idx++] = '/';
        read_idx++;
        while (read_idx < n && (path[read_idx] == '/' || path[read_idx] == '\\')) {
            read_idx++;
        }
    }

    while (read_idx < n) {
        if (path[read_idx] == '/' || path[read_idx] == '\\') {
            while (read_idx < n && (path[read_idx] == '/' || path[read_idx] == '\\')) {
                read_idx++;
            }
            if (write_idx > 0 && result[write_idx - 1] != '/') {
                if (write_idx == 1 && is_absolute) {
                }
                else {
                    result[write_idx++] = '/';
                }
            }
            continue;
        }

        if (path[read_idx] == '.' && (read_idx + 1 == n || path[read_idx + 1] == '/' || path[read_idx + 1] == '\\')) {
            read_idx++;
            while (read_idx < n && (path[read_idx] == '/' || path[read_idx] == '\\')) {
                read_idx++;
            }
            continue;
        }

        if (path[read_idx] == '.' && read_idx + 1 < n && path[read_idx + 1] == '.' &&
            (read_idx + 2 == n || path[read_idx + 2] == '/' || path[read_idx + 2] == '\\')) {
            read_idx += 2;
            while (read_idx < n && (path[read_idx] == '/' || path[read_idx] == '\\')) {
                read_idx++;
            }

            if (write_idx > 0) {
                if (result[write_idx - 1] == '/') {
                    write_idx--;
                }

                size_t backtrack_limit = is_absolute ? 1 : 0;
                while (write_idx > backtrack_limit && result[write_idx - 1] != '/') {
                    write_idx--;
                }
                if ((!is_absolute && segments > 0) || (is_absolute && write_idx > 1)) {
                    if (write_idx > backtrack_limit && result[write_idx - 1] == '/') {
                        write_idx--;
                    }
                    segments--;
                }
                else if (!is_absolute && segments == 0) {
                    if (write_idx > 0 && result[write_idx - 1] != '/') result[write_idx++] = '/'; // Ensure separator
                    result[write_idx++] = '.';
                    result[write_idx++] = '.';
                    segments++;
                }
                if (write_idx > backtrack_limit && result[write_idx - 1] != '/') {
                    result[write_idx++] = '/';
                }
            }
            else if (!is_absolute) {
                result[write_idx++] = '.';
                result[write_idx++] = '.';
                if (read_idx < n) result[write_idx++] = '/';
                segments++;
            }
            continue;
        }

        if (write_idx > 0 && result[write_idx - 1] != '/') {
            if (write_idx == 1 && is_absolute) {
            }
            else {
                result[write_idx++] = '/';
            }
        }
        while (read_idx < n && path[read_idx] != '/' && path[read_idx] != '\\') {
            result[write_idx++] = path[read_idx++];
        }
        segments++;
    }

    if (write_idx > 1 && result[write_idx - 1] == '/' && is_absolute) {
        write_idx--;
    }
    else if (write_idx > 0 && result[write_idx - 1] == '/' && !is_absolute) {
        write_idx--;
    }

    if (write_idx == 0) {
        result[write_idx++] = '.';
    }

    result[write_idx] = '\0';
}

int should_show_content(const char* filename, const char* full_path, const content_ctx* content_context) {
    if (!content_context->content_flag) {
        return 0;
    }

    if (content_context->content_specifier_count == 0) {
        return is_text_file(full_path);
    }

    const char* base_filename = strrchr(filename, '/');
    base_filename = base_filename ? base_filename + 1 : filename;

    const char* dot = strrchr(base_filename, '.');
    const char* ext = (dot && dot != base_filename && dot[1] != '\0') ? dot + 1 : NULL; // Ensure dot is not last char

    for (int i = 0; i < content_context->content_specifier_count; i++) {
        const char* spec = content_context->content_specifiers[i];

        if (strcmp(base_filename, spec) == 0) {
            return is_text_file(full_path);
        }

        if (strcmp(spec, "null") == 0 && !ext) {
            return is_text_file(full_path);
        }

        if (ext && strchr(spec, '.') == NULL && strchr(spec, '/') == NULL) {
            if (strcmp(ext, spec) == 0) {
                return is_text_file(full_path);
            }
        }
    }

    return 0;
}

int generate_output_filename(output_ctx* ctx) {
    char combined_path[MAX_PATH_SIZE];
    const char* dir = ".";

    if (strlen(ctx->output_dir) > 0) {
        dir = ctx->output_dir;
        struct stat dir_st;
        if (stat(dir, &dir_st) != 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "Error: Output directory '%s' does not exist.\n", dir);
            }
            else {
                perror("stat output directory");
                fprintf(stderr, "Error: Cannot access output directory '%s'.\n", dir);
            }
            return -1;
        }
        if (!S_ISDIR(dir_st.st_mode)) {
            fprintf(stderr, "Error: Specified output directory path '%s' is not a directory.\n", dir);
            return -1;
        }
    }

    if (strlen(ctx->output_name) > 0) {
        int len = snprintf(combined_path, sizeof(combined_path), "%s/%s", dir, ctx->output_name);
        if (len < 0 || (size_t)len >= sizeof(combined_path)) {
            fprintf(stderr, "Error: Constructed output path is too long or encoding error (%s/%s).\n", dir, ctx->output_name);
            return -1;
        }
    }
    else {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timestamp[64];
        if (!t || strftime(timestamp, sizeof(timestamp), "recap-output-%Y%m%d-%H%M%S.txt", t) == 0) {
            fprintf(stderr, "Error: Failed to generate timestamp for output filename.\n");
            return -1;
        }
        int len = snprintf(combined_path, sizeof(combined_path), "%s/%s", dir, timestamp);
        if (len < 0 || (size_t)len >= sizeof(combined_path)) {
            fprintf(stderr, "Error: Constructed output path is too long or encoding error (%s/%s).\n", dir, timestamp);
            return -1;
        }
    }

    normalize_path(combined_path);

    if (strcmp(combined_path, ".") == 0 || strcmp(combined_path, "..") == 0 || strcmp(combined_path, "/") == 0) {
        fprintf(stderr, "Error: Invalid output filename generated: '%s'. Resolves to a directory-like path.\n", combined_path);
        return -1;
    }

    struct stat st;
    if (stat(combined_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Output path '%s' points to an existing directory. Please specify a file path.\n", combined_path);
        return -1;
    }

    strncpy(ctx->calculated_output_path, combined_path, MAX_PATH_SIZE - 1);
    ctx->calculated_output_path[MAX_PATH_SIZE - 1] = '\0';

    get_relative_path(ctx->calculated_output_path, ctx->relative_output_path, sizeof(ctx->relative_output_path));

    return 0;
}