// its what you expect from a utils file
#include "ctf.h"
#include <ctype.h>   // For isprint (potentially, not used currently), needed by args.c
#include <stdio.h>   // For FILE, fopen, fread, fclose, snprintf, fprintf
#include <stdlib.h>  // For malloc, free
#include <string.h>  // For strrchr, strcmp, strncmp
#include <time.h>    // For time, localtime, strftime

int is_text_file(const char *full_path)  {
    FILE *file = fopen(full_path, "rb"); // Open in binary mode
    if (!file) {
        // Cannot open, assume not text or inaccessible
        return 0;
    }

    unsigned char buffer[1024]; // Read a chunk to check for binary indicators
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    if (bytes_read == 0) {
        return 1; // Empty file is considered text
    }

    // Simple heuristic: Check for null bytes, often indicates binary data
    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == 0) {
            return 0; // Found null byte, likely binary
        }
        // Could add more checks here, e.g., for control characters
        // or high percentage of non-printable ASCII, but keep it simple.
    }

    // No null bytes found in the first chunk, assume text
    return 1;
}

void normalize_path(char *path) {
    int len = strlen(path);
    int write_idx = 0;
    int last_was_slash = 0;
    int start_idx = 0;

    // Skip leading slashes for relative path calculation later if needed,
    // but keep one if it's an absolute path.
    if (path[0] == '/') {
        path[write_idx++] = '/';
        last_was_slash = 1;
        start_idx = 1;
    }

    for (int i = start_idx; i < len; i++) {
        if (path[i] == '\\' || path[i] == '/') { // Treat backslash as slash
            if (!last_was_slash) { // Avoid multiple slashes
                path[write_idx++] = '/';
                last_was_slash = 1;
            }
        } else if (path[i] == '.') {
            // Handle "." and ".."
            if (i + 1 < len && path[i+1] == '.' && (path[i+2] == '/' || path[i+2] == '\\' || path[i+2] == '\0')) {
                // Found ".."
                i++; // Consume the second dot
                // Go up one level
                if (write_idx > start_idx) { // Ensure not at the beginning (or root for absolute)
                    // Move back before the last slash
                    write_idx--; // Move back past the potential slash
                    while (write_idx > start_idx && path[write_idx - 1] != '/') {
                        write_idx--;
                    }
                    // If we are at the start (e.g. /..), write_idx might be start_idx
                    if (write_idx > start_idx) {
                         write_idx--; // Move back past the slash itself
                    }
                    // After moving back, the next char should be a slash
                    last_was_slash = 1;
                }
            } else if (path[i+1] == '/' || path[i+1] == '\\' || path[i+1] == '\0') {
                // Found "." component, just skip it
                // Ensure the next char is treated as a slash start
                last_was_slash = 1;
            } else {
                 // Dot is part of a filename
                 path[write_idx++] = path[i];
                 last_was_slash = 0;
            }
        } else {
            path[write_idx++] = path[i];
            last_was_slash = 0;
        }
    }

    // Remove trailing slash unless it's the only character "/"
    if (write_idx > 1 && path[write_idx - 1] == '/') {
        write_idx--;
    }
    // Handle case where path becomes empty (e.g. "./" normalized)
    if (write_idx == 0 && start_idx == 0) {
        path[write_idx++] = '.'; // Represent CWD as "."
    }
    path[write_idx] = '\0'; // Null-terminate the normalized path
}


int should_show_content(const char *filename, const char *full_path) {
    if (!content_flag) return 0; // --content flag was not used

    // If --content was used but no specific specifiers provided, default to heuristic
    if (content_specifier_count == 0) {
        return is_text_file(full_path);
    }

    // Check against explicit specifiers provided with --content
    const char *base_filename = strrchr(filename, '/'); // Get base name just in case
    base_filename = base_filename ? base_filename + 1 : filename;

    const char *ext = strrchr(base_filename, '.');
    // Handle dotfiles correctly: ".bashrc" -> ext is NULL, base_filename is ".bashrc"
    // Handle "file.tar.gz" -> ext is ".gz"
    if (ext != NULL && ext != base_filename) { // Make sure '.' is not the first char
         ext++; // Point after the dot
    } else {
         ext = NULL; // No extension found or it's a dotfile starting with '.'
    }


    for (int i = 0; i < content_specifier_count; i++) {
        const char *spec = content_specifiers[i];

        // Check 1: Exact filename match (case-sensitive)
        if (strcmp(base_filename, spec) == 0) {
            return 1;
        }

        // Check 2: Extension match (case-sensitive)
        // Specifier should not contain '.' or '/' if it's meant as an extension
        if (ext != NULL && strchr(spec, '.') == NULL && strchr(spec, '/') == NULL) {
             if (strcmp(ext, spec) == 0) {
                 return 1;
             }
        }
        // Potential Check 3: Glob pattern match against filename?
        // Requires fnmatch. For now, only exact name and extension.
    }

    return 0; // No specifier matched
}


char *get_output_filename(void) {
    char *filename = malloc(MAX_PATH_SIZE);
    if (!filename) {
        fprintf(stderr, "Error: Failed to allocate memory for output filename.\n");
        return NULL;
    }

    int required_len = -1;

    if (strlen(output_name) > 0) {
        // User specified a name with --name
        required_len = snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, output_name);
    } else {
        // Generate timestamped filename
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[64];
        // Format: ctf-output-YYYYMMDD-HHMMSS.txt
        strftime(timestamp, sizeof(timestamp), "ctf-output-%Y%m%d-%H%M%S.txt", t);

        required_len = snprintf(filename, MAX_PATH_SIZE, "%s/%s", output_dir, timestamp);
    }

    if (required_len < 0 || required_len >= MAX_PATH_SIZE) {
         fprintf(stderr, "Error: Constructed output path is too long or encoding error.\n");
         free(filename);
         return NULL;
    }

    // Normalize the final path *after* construction
    // This handles cases like --dir . --name ../output.txt correctly
    // Although, allowing .. in output name might be risky.
    // Consider adding checks to prevent escaping the intended output dir.
    normalize_path(filename);

    // Final check: ensure filename is not just "." or "/"
    if (strcmp(filename, ".") == 0 || strcmp(filename, "/") == 0) {
        fprintf(stderr, "Error: Invalid output filename generated: %s\n", filename);
        free(filename);
        return NULL;
    }

    return filename; // Remember caller must free
}
