// its what you expect from a utils file
#include "ctf.h"
#include <ctype.h> // Include for tolower

// pls if u see this reach out to me on discord i dont know what im doing
int is_text_file(const char *full_path)  {
    FILE *file = fopen(full_path, "rb");
    if (!file) {
        perror("Failed to open file");
        return 0; 
    }

    int c;
    int is_text = 1;
    int count = 0;

    while ((c = fgetc(file)) != EOF && count < 1024) {
        if ((c < 32 || c > 126) && c != '\n' && c != '\r' && c != '\t') {
            is_text = 0; 
            break;
        }
        count++;
    }

    fclose(file);
    return is_text;
}



void normalize_path(char *path) {
    for (int i = 0; path[i]; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

int should_show_content(const char *filename, const char *full_path) {
    if (!content_flag) return 0;

    if (content_type_count == 0) {
        // Pass only full_path to is_text_file
        return is_text_file(full_path);
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
