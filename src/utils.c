// its what you expect from a utils file
#include "ctf.h"
#include <ctype.h>   
#include <stdio.h>   
#include <stdlib.h> 
#include <string.h>  
#include <time.h>   

int is_text_file(const char* full_path) {
    FILE* file = fopen(full_path, "rb");
    if (!file) {
        return 0;
    }

    unsigned char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    if (bytes_read == 0) {
        return 1;
    }

    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == 0) {
            return 0;
        }
    }

    return 1;
}

void normalize_path(char* path) {
    int len = strlen(path);
    int write_idx = 0;
    int last_was_slash = 0;
    int start_idx = 0;

    if (path[0] == '/') {
        path[write_idx++] = '/';
        last_was_slash = 1;
        start_idx = 1;
    }

    for (int i = start_idx; i < len; i++) {
        if (path[i] == '\\' || path[i] == '/') {
            if (!last_was_slash) {
                path[write_idx++] = '/';
                last_was_slash = 1;
            }
        }
        else if (path[i] == '.') {
            if (i + 1 < len && path[i + 1] == '.' && (path[i + 2] == '/' || path[i + 2] == '\\' || path[i + 2] == '\0')) {
                i++;

                if (write_idx > start_idx) {

                    write_idx--;
                    while (write_idx > start_idx && path[write_idx - 1] != '/') {
                        write_idx--;
                    }

                    if (write_idx > start_idx) {
                        write_idx--;
                    }

                    last_was_slash = 1;
                }
            }
            else if (path[i + 1] == '/' || path[i + 1] == '\\' || path[i + 1] == '\0') {


                last_was_slash = 1;
            }
            else {

                path[write_idx++] = path[i];
                last_was_slash = 0;
            }
        }
        else {
            path[write_idx++] = path[i];
            last_was_slash = 0;
        }
    }

    if (write_idx > 1 && path[write_idx - 1] == '/') {
        write_idx--;
    }

    if (write_idx == 0 && start_idx == 0) {
        path[write_idx++] = '.';
    }
    path[write_idx] = '\0';
}


int should_show_content(const char* filename, const char* full_path) {
    if (!content_flag) return 0;

    if (content_specifier_count == 0) {
        return is_text_file(full_path);
    }

    const char* base_filename = strrchr(filename, '/');
    base_filename = base_filename ? base_filename + 1 : filename;

    const char* ext = strrchr(base_filename, '.');

    if (ext != NULL && ext != base_filename) {
        ext++;
    }
    else {
        ext = NULL;
    }

    for (int i = 0; i < content_specifier_count; i++) {
        const char* spec = content_specifiers[i];

        if (strcmp(base_filename, spec) == 0) {
            return 1;
        }

        if (strcmp(spec, "null") == 0 && ext == NULL && is_text_file(full_path)) {
            return 1;
        }

        if (ext != NULL && strchr(spec, '.') == NULL && strchr(spec, '/') == NULL) {
            if (strcmp(ext, spec) == 0) {
                return 1;
            }
        }
    }

    return 0;
}


char* get_output_filename(void) {
    char* filename = malloc(MAX_PATH_SIZE);
    if (!filename) {
        fprintf(stderr, "Error: Failed to allocate memory for output filename.\n");
        return NULL;
    }

    int required_len = -1;

    if (strlen(output_name) > 0) {

        required_len = snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, output_name);
    }
    else {

        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timestamp[64];

        strftime(timestamp, sizeof(timestamp), "ctf-output-%Y%m%d-%H%M%S.txt", t);

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
