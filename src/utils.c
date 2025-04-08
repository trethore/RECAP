// its what you expect from a utils file

#include "ctf.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int is_text_file(const char* full_path) {
    FILE* file = fopen(full_path, "rb");
    if (!file)
        return 0;

    unsigned char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    if (bytes_read == 0)
        return 1;

    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == 0)
            return 0;
    }

    return 1;
}

void normalize_path(char* path) {
    if (!path)
        return;

    int n = (int)strlen(path);
    if (n == 0) {
        strcpy(path, ".");
        return;
    }

    int write = 0;
    int start = 0;
    int last_slash = 0;

    if (path[0] == '/') {
        path[write++] = '/';
        last_slash = 1;
        start = 1;
    }

    for (int i = start; i < n; i++) {
        char c = path[i];

        if (c == '\\' || c == '/') {
            if (!last_slash) {
                path[write++] = '/';
                last_slash = 1;
            }
            continue;
        }

        if (c == '.') {
            if (i + 1 < n && path[i + 1] == '.' &&
                ((i + 2 == n) || (path[i + 2] == '/' || path[i + 2] == '\\'))) {
                i++;
                if (write > start) {
                    write--;
                    while (write > start && path[write - 1] != '/')
                        write--;
                    if (write > start)
                        write--;
                    last_slash = 1;
                }
                continue;
            }
            else if (i + 1 == n || (path[i + 1] == '/' || path[i + 1] == '\\')) {
                last_slash = 1;
                continue;
            }
        }

        path[write++] = c;
        last_slash = 0;
    }

    if (write > 1 && path[write - 1] == '/')
        write--;

    if (write == 0) {
        path[write++] = '.';
    }

    path[write] = '\0';
}

int should_show_content(const char* filename, const char* full_path, content_ctx* content_context) {
    if (!content_context->content_flag)
        return 0;

    if (content_context->content_specifier_count == 0)
        return is_text_file(full_path);

    const char* base_filename = strrchr(filename, '/');
    base_filename = base_filename ? base_filename + 1 : filename;

    const char* dot = strrchr(base_filename, '.');
    const char* ext = (dot && dot != base_filename) ? dot + 1 : NULL;

    for (int i = 0; i < content_context->content_specifier_count; i++) {
        const char* spec = content_context->content_specifiers[i];

        if (strcmp(base_filename, spec) == 0)
            return 1;

        if (strcmp(spec, "null") == 0 && !ext && is_text_file(full_path))
            return 1;

        if (ext && !strchr(spec, '.') && !strchr(spec, '/')) {
            if (strcmp(ext, spec) == 0)
                return 1;
        }
    }

    return 0;
}

char* get_output_filename(FILE* output, output_ctx* output_context) {
    (void)output;

    char* filename = malloc(MAX_PATH_SIZE);
    if (!filename) {
        fprintf(stderr, "Error: Failed to allocate memory for output filename.\n");
        return NULL;
    }

    const char* output_dir = (strlen(output_context->output_dir) > 0) ?
        output_context->output_dir : ".";
    int required_len = 0;

    if (strlen(output_context->output_name) > 0) {

        required_len = snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, output_context->output_name);
    }
    else {

        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timestamp[64];
        if (!t || strftime(timestamp, sizeof(timestamp), "ctf-output-%Y%m%d-%H%M%S.txt", t) == 0) {
            fprintf(stderr, "Error: Failed to generate timestamp.\n");
            free(filename);
            return NULL;
        }
        required_len = snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, timestamp);
    }

    if (required_len < 0 || required_len >= MAX_PATH_SIZE) {
        fprintf(stderr, "Error: Constructed output path is too long or encoding error.\n");
        free(filename);
        return NULL;
    }

    normalize_path(filename);

    if (strcmp(filename, ".") == 0 || strcmp(filename, "/") == 0) {
        fprintf(stderr, "Error: Invalid output filename generated: %s\n", filename);
        free(filename);
        return NULL;
    }

    return filename;
}

