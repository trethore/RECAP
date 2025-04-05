// its what you expect from a utils file
#include "ctf.h"

int is_compiled_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) {

        for (int i = 0; content_exceptions[i] != NULL; i++) {
            if (strcmp(filename, content_exceptions[i]) == 0) {
                return 0;
            }
        }
        return 1;
    }
    ext++;
    for (int i = 0; compiled_exts[i] != NULL; i++) {
        if (strcmp(ext, compiled_exts[i]) == 0)
            return 1;
    }
    return 0;
}

int is_text_file(const char *filename) {

    return !is_compiled_file(filename);
}

void normalize_path(char *path) {
    for (int i = 0; path[i]; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

int should_show_content(const char *filename) {
    if (!content_flag) return 0;


    const char *base_filename = strrchr(filename, '/');
    base_filename = base_filename ? base_filename + 1 : filename;
    for (int i = 0; content_exceptions[i] != NULL; i++) {
        if (strcmp(base_filename, content_exceptions[i]) == 0)
            return 1;
    }


    if (content_type_count == 0) {
        return !is_compiled_file(filename);
    }


    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    ext++;
    for (int i = 0; i < content_type_count; i++) {
        if (strcmp(ext, content_types[i]) == 0)
            return 1;
    }
    return 0;
}

char *get_output_filename(void) {
    char *filename = malloc(MAX_PATH_SIZE);
    if (!filename) return NULL;

    int required_len = -1;

    if (strlen(output_name) > 0) {
        required_len = snprintf(NULL, 0, "%s/%s", output_dir, output_name);
        if (required_len >= 0 && required_len < MAX_PATH_SIZE) {
            snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, output_name);
        } else {
            fprintf(stderr, "Error: Constructed output path is too long: %s/%s\n", output_dir, output_name);
            free(filename);
            return NULL;
        }
    } else {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "ctf-output-%Y%m%d-%H%M%S.txt", t);

        required_len = snprintf(NULL, 0, "%s/%s", output_dir, timestamp);
        if (required_len >= 0 && required_len < MAX_PATH_SIZE) {
            snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, timestamp);
        } else {
            fprintf(stderr, "Error: Constructed output path is too long: %s/%s\n", output_dir, timestamp);
            free(filename);
            return NULL;
        }
    }
    normalize_path(filename);
    return filename;
}
