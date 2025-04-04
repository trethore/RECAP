#include "ctf.h"

// --- Utility Functions ---

int is_compiled_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        // Check if the file is in the exception list
        for (int i = 0; content_exceptions[i] != NULL; i++) {
            if (strcmp(filename, content_exceptions[i]) == 0) {
                return 0; // Not a compiled file
            }
        }
        return 1; // If no extension and not in exception list, it's a compiled file
    }
    ext++;
    for (int i = 0; compiled_exts[i] != NULL; i++) {
        if (strcmp(ext, compiled_exts[i]) == 0)
            return 1;
    }
    return 0;
}

int is_text_file(const char *filename) {
    // Basic check: not a known compiled type
    // More sophisticated checks (e.g., reading initial bytes) could be added.
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
    if (!content_flag) return 0; // --content not set at all

    // Check exceptions first
    const char *base_filename = strrchr(filename, '/');
    base_filename = base_filename ? base_filename + 1 : filename; // Get basename
    for (int i = 0; content_exceptions[i] != NULL; i++) {
        if (strcmp(base_filename, content_exceptions[i]) == 0)
            return 1; // Always show if in exception list
    }

    // If --content was given but no specific types, show content for all non-compiled files
    if (content_type_count == 0) {
        return !is_compiled_file(filename);
    }

    // If specific types were given, check the extension
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0; // Don't show content for files without extensions (unless exception)
    ext++;
    for (int i = 0; i < content_type_count; i++) {
        if (strcmp(ext, content_types[i]) == 0)
            return 1; // Show if extension matches specified types
    }
    return 0; // Don't show otherwise
}

char *get_output_filename(void) {
    char *filename = malloc(MAX_PATH_SIZE);
    if (!filename) return NULL;

    int required_len = -1;

    if (strlen(output_name) > 0) {
        // Calculate required length for custom name
        required_len = snprintf(NULL, 0, "%s/%s.txt", output_dir, output_name);
        if (required_len >= 0 && required_len < MAX_PATH_SIZE) {
            snprintf(filename, MAX_PATH_SIZE, "%s/%s.txt", output_dir, output_name);
        } else {
            fprintf(stderr, "Error: Constructed output path is too long: %s/%s.txt\n", output_dir, output_name);
            free(filename);
            return NULL;
        }
    } else {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "ctf-output-%Y%m%d-%H%M%S.txt", t);

        // Calculate required length for timestamped name
        required_len = snprintf(NULL, 0, "%s/%s", output_dir, timestamp);
        if (required_len >= 0 && required_len < MAX_PATH_SIZE) {
            snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, timestamp);
        } else {
            fprintf(stderr, "Error: Constructed output path is too long: %s/%s\n", output_dir, timestamp);
            free(filename);
            return NULL;
        }
    }
    normalize_path(filename); // Ensure consistent separators
    return filename;
}
