// this file add some useful functions for traversing directories and files

#include "ctf.h"
#include <limits.h> // For PATH_MAX if needed, though MAX_PATH_SIZE is defined
#include <stdlib.h> // For realpath if used (not currently)
#include <fnmatch.h> // For is_excluded
#include <string.h> // For string functions
#include <unistd.h> // For getcwd
#include <sys/stat.h> // For stat in traverse_directory
#include <dirent.h> // For opendir/readdir

void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(output, "  ");
    }
}

void write_file_content_inline(const char *filepath, int depth) {
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        print_indent(depth);
        fprintf(output, "%s", line);
    }

    fseek(f, -1, SEEK_END);
    if (fgetc(f) != '\n') {
        fprintf(output, "\n");
    }
    fclose(f);
}

// New function: get_relative_path
void get_relative_path(const char *full_path, char *rel_path, size_t size) {
    char clean_path[MAX_PATH_SIZE];
    strncpy(clean_path, full_path, size -1);
    clean_path[size-1] = '\0';
    normalize_path(clean_path);

    // If it's already relative (doesn't start with /), use it directly
    // This assumes CWD is the intended base, which is typical for this tool
    if (clean_path[0] != '/') {
         // Remove leading "./" if present for consistency
        if (strncmp(clean_path, "./", 2) == 0) {
            memmove(clean_path, clean_path + 2, strlen(clean_path) - 1); // -1 to include null terminator move
        }
        strncpy(rel_path, clean_path, size -1);
        rel_path[size-1] = '\0';
    } else {
        // It's an absolute path, we need CWD
        char cwd[MAX_PATH_SIZE];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            normalize_path(cwd);
            // Very basic relative path calculation: if clean_path starts with cwd
            size_t cwd_len = strlen(cwd);
            if (strncmp(clean_path, cwd, cwd_len) == 0) {
                // Check if the match is at a directory boundary
                if (clean_path[cwd_len] == '/' || clean_path[cwd_len] == '\0') {
                    char *relative_part = clean_path + cwd_len;
                    if (*relative_part == '/') { // Skip the leading slash
                        relative_part++;
                    }
                     strncpy(rel_path, relative_part, size - 1);
                     rel_path[size - 1] = '\0';

                     // If path was exactly CWD, rel_path is now empty, should be "."
                     if (rel_path[0] == '\0' && strcmp(clean_path, cwd) == 0) {
                        strcpy(rel_path, ".");
                     }

                } else {
                     // Absolute path doesn't share CWD prefix correctly, use absolute?
                     // For exclusion matching, using the non-relative path is problematic.
                     // Fallback to using the absolute path, hoping patterns don't rely on relativity.
                     strncpy(rel_path, clean_path, size - 1);
                     rel_path[size - 1] = '\0';
                }
            } else {
                 // Absolute path outside CWD, use it directly for matching
                 strncpy(rel_path, clean_path, size - 1);
                 rel_path[size - 1] = '\0';
            }
        } else {
             // Failed to get CWD, use the absolute path
             perror("getcwd");
             strncpy(rel_path, clean_path, size - 1);
             rel_path[size - 1] = '\0';
        }
    }
     // Final check: if result is empty, but original was '.', set to "."
     if (rel_path[0] == '\0' && strcmp(full_path, ".") == 0) {
        strcpy(rel_path, ".");
     }
}

// Updated function: is_excluded
int is_excluded(const char *rel_path) {
    char path_to_check[MAX_PATH_SIZE];
    strncpy(path_to_check, rel_path, MAX_PATH_SIZE -1);
    path_to_check[MAX_PATH_SIZE - 1] = '\0';
    // Normalization should have happened in get_relative_path, but double check
    normalize_path(path_to_check);

    // Ensure path doesn't start with './' after normalization/relativization for matching
    if (strncmp(path_to_check, "./", 2) == 0) {
        memmove(path_to_check, path_to_check + 2, strlen(path_to_check) - 1); // Move null term too
    }
     // If path becomes empty after removing ./, treat as "."
     if (path_to_check[0] == '\0') {
        strcpy(path_to_check, ".");
     }

    // Exclude the output file itself
    static char relative_output_path[MAX_PATH_SIZE] = "";
    static int output_path_calculated = 0;
    if (!output_path_calculated && output) { // Check if output file is open
        char *output_filename_ptr = get_output_filename();
        if (output_filename_ptr) {
            get_relative_path(output_filename_ptr, relative_output_path, sizeof(relative_output_path));
            // Clean up relative output path as well
            if (strncmp(relative_output_path, "./", 2) == 0) {
                 memmove(relative_output_path, relative_output_path + 2, strlen(relative_output_path) - 1);
            }
             if (relative_output_path[0] == '\0') {
                // If output file resolves to CWD, its relative path is "."
                // This case needs careful handling if output is in CWD.
                // Let's assume get_output_filename gives a non-"." name.
                // If output_name is not set, it will have a timestamp.
                // If output_name is set, it might be just a filename.
                // If output_dir is "." and output_name is "out.txt", rel path is "out.txt".
                // If output_dir is "." and output_name is not set, rel path is "ctf-output-...".
                // It shouldn't become "." unless the output file is literally named "."
                // which is unlikely and disallowed by get_output_filename logic.
             }
            free(output_filename_ptr);
            output_path_calculated = 1;
        }
    }
     if (output_path_calculated && strcmp(path_to_check, relative_output_path) == 0) {
        return 1; // Exclude the output file
     }

    // Exclude the ctf executable itself (common exclusion)
    const char *base = strrchr(path_to_check, '/');
    base = base ? base + 1 : path_to_check;
    if (strcmp(base, "ctf") == 0 || strcmp(base, "ctf.exe") == 0) return 1;


    for (int i = 0; i < exclude_count; i++) {
        const char *pattern = exclude_patterns[i];
        char clean_pattern[MAX_PATH_SIZE];
        strncpy(clean_pattern, pattern, MAX_PATH_SIZE -1);
        clean_pattern[MAX_PATH_SIZE -1] = '\0';
        // Don't normalize patterns here, gitignore patterns have specific meanings
        // e.g., trailing spaces, leading slashes.
        // However, we *should* handle backslashes if on Windows?
        // For now, assume POSIX paths in patterns.

         // Remove leading "./" from pattern if present for consistency with path_to_check
         if (strncmp(clean_pattern, "./", 2) == 0) {
            memmove(clean_pattern, clean_pattern + 2, strlen(clean_pattern) - 1);
         }

        int match_flags = FNM_PATHNAME | FNM_NOESCAPE;
        // FNM_PERIOD: Special handling for '.' at start of filename component.
        // Gitignore usually matches leading dots unless explicitly specified.
        // Let's add FNM_PERIOD for stricter matching like shell globs.
        match_flags |= FNM_PERIOD;

        // fnmatch compare against the relative path
        if (fnmatch(clean_pattern, path_to_check, match_flags) == 0) {
            return 1; // Pattern matches path
        }

        // Handle directory patterns ending in '/' specifically
        // fnmatch with FNM_PATHNAME should handle this, but gitignore has nuances.
        // If pattern is `dir/`, it should match `dir` and `dir/anything`.
         size_t pattern_len = strlen(clean_pattern);
         if (pattern_len > 0 && clean_pattern[pattern_len - 1] == '/') {
             // Check if path_to_check is exactly the pattern (minus slash) or starts with it + slash
             char pattern_base[MAX_PATH_SIZE];
             strncpy(pattern_base, clean_pattern, pattern_len - 1);
             pattern_base[pattern_len - 1] = '\0';

             if (strcmp(path_to_check, pattern_base) == 0 ||
                 (strncmp(path_to_check, clean_pattern, pattern_len) == 0)) {
                 return 1;
             }
         }
         // Also handle case where pattern is "dir" and path is "dir/file"
         // This is implicitly handled by gitignore rules: "dir" matches file `dir` and directory `dir`.
         // If it matches directory `dir`, it implicitly matches everything inside.
         // fnmatch(pattern, path, FNM_PATHNAME) should work if path is `dir/file.txt`
         // Let's test if fnmatch alone is sufficient.
         // Consider pattern `build` and path `build/output.o`.
         // fnmatch("build", "build/output.o", FNM_PATHNAME) == 0 -> Correct.
         // Consider pattern `*.log` and path `logs/app.log`.
         // fnmatch("*.log", "logs/app.log", FNM_PATHNAME) == FNM_NOMATCH -> Correct.
         // Consider pattern `logs/*.log` and path `logs/app.log`.
         // fnmatch("logs/*.log", "logs/app.log", FNM_PATHNAME) == 0 -> Correct.
    }

    return 0; // Not excluded by any pattern
}

// Updated function: traverse_directory
void traverse_directory(const char *base_path, int depth) {
    DIR *dir = opendir(base_path);
    if (!dir) {
        // Don't print perror here, could be permissions. Let main handle top-level errors.
        // Maybe log to stderr if a verbose flag is added later.
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[MAX_PATH_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        // Normalization happens inside get_relative_path now
        // normalize_path(full_path); // Normalize early - moved

        char rel_path[MAX_PATH_SIZE];
        get_relative_path(full_path, rel_path, sizeof(rel_path));

        if (is_excluded(rel_path)) {
            continue;
        }

        struct stat st;
        // Use stat() for now. If symlink handling is needed, switch to lstat()
        // and add checks for S_ISLNK(st.st_mode).
        if (stat(full_path, &st) != 0) {
            // Print stat errors for entries *inside* traversal, might indicate issues.
            fprintf(stderr, "Warning: Could not stat entry %s: ", full_path);
            perror(NULL);
            continue;
        }

        print_indent(depth);
        if (S_ISDIR(st.st_mode)) {
            fprintf(output, "%s/\n", entry->d_name);
            traverse_directory(full_path, depth + 1); // Recurse with the non-normalized full_path
        } else if (S_ISREG(st.st_mode)) {
            if (should_show_content(entry->d_name, full_path)) {
                fprintf(output, "%s:\n", entry->d_name);
                write_file_content_inline(full_path, depth + 1);
            } else {
                fprintf(output, "%s\n", entry->d_name);
            }
        }
        // Ignore other types (sockets, pipes, symlinks for now)
    }
    closedir(dir);
}

// Updated function: print_path_hierarchy
int print_path_hierarchy(const char *path) {
    char copy[MAX_PATH_SIZE];
    char original_path_normalized[MAX_PATH_SIZE]; // Keep a normalized copy of original

    strncpy(original_path_normalized, path, MAX_PATH_SIZE -1);
    original_path_normalized[MAX_PATH_SIZE -1] = '\0';
    normalize_path(original_path_normalized);


    strncpy(copy, original_path_normalized, MAX_PATH_SIZE -1);
    copy[MAX_PATH_SIZE - 1] = '\0';

    // Handle edge case: path is just "."
    if (strcmp(copy, ".") == 0) {
        return 0; // No hierarchy for current directory itself
    }

    struct stat st;
    if (stat(original_path_normalized, &st) != 0) {
         // Path doesn't exist or error, cannot print hierarchy
         return 0;
    }

    char *print_limit = copy + strlen(copy); // Point to end initially

    // If it's a file, find the last slash and set the limit there to print only parent dirs
    if (S_ISREG(st.st_mode)) {
        char *last_slash = strrchr(copy, '/');
        if (last_slash != NULL) {
            print_limit = last_slash; // Point to the last slash (exclusive end)
        } else {
            // File in the current directory relative to CWD, no hierarchy to print
            return 0;
        }
    }
    // If it's a directory, print_limit remains at the end, print the full path hierarchy

    int depth = 0;
    char *start = copy;
    char *end;

    // Handle absolute paths correctly
    if (*start == '/') {
        // How to represent the root? Maybe skip printing just "/"
        start++; // Move past the initial slash
    }

    while (start < print_limit && *start != '\0') {
         // Skip multiple leading slashes if any (shouldn't happen after normalize)
         while (*start == '/' && start < print_limit) {
             start++;
         }
         if (start >= print_limit) break; // Reached the limit

        // Find the next separator or the print limit
        end = strchr(start, '/');
        if (end == NULL || end >= print_limit) {
            end = print_limit; // Process up to the limit
        }

        if (end > start) { // Ensure there's a component name
            char component[MAX_PATH_SIZE];
            size_t component_len = end - start;
             if (component_len >= MAX_PATH_SIZE) component_len = MAX_PATH_SIZE - 1; // Prevent overflow
            strncpy(component, start, component_len);
            component[component_len] = '\0';

            // Avoid printing "." as a component unless it's the only thing?
            // Normalization should remove intermediate "." components.
            if (strcmp(component, ".") != 0) {
                print_indent(depth);
                fprintf(output, "%s/\n", component);
                depth++;
            }
        }

        if (end == print_limit) break; // Reached the limit

        start = end + 1; // Move to the character after '/'
    }

    return depth;
}
