// this file add some useful functions for traversing directories and files

#include "ctf.h"
#include <limits.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(output, "  ");
    }
}

void write_file_content_inline(const char* filepath, int depth) {
    FILE* f = fopen(filepath, "r");
    if (!f)
        return;

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

void get_relative_path(const char* full_path, char* rel_path, size_t size) {
    char clean_path[MAX_PATH_SIZE];
    strncpy(clean_path, full_path, size - 1);
    clean_path[size - 1] = '\0';
    normalize_path(clean_path);
    if (clean_path[0] != '/') {
        if (strncmp(clean_path, "./", 2) == 0) {
            memmove(clean_path, clean_path + 2, strlen(clean_path) - 1); // -1 to include null terminator move
        }
        strncpy(rel_path, clean_path, size - 1);
        rel_path[size - 1] = '\0';
    }
    else {
        char cwd[MAX_PATH_SIZE];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            normalize_path(cwd);
            size_t cwd_len = strlen(cwd);
            if (strncmp(clean_path, cwd, cwd_len) == 0) {
                if (clean_path[cwd_len] == '/' || clean_path[cwd_len] == '\0') {
                    char* relative_part = clean_path + cwd_len;
                    if (*relative_part == '/') {
                        relative_part++;
                    }
                    strncpy(rel_path, relative_part, size - 1);
                    rel_path[size - 1] = '\0';

                    if (rel_path[0] == '\0' && strcmp(clean_path, cwd) == 0) {
                        strcpy(rel_path, ".");
                    }
                }
                else {
                    strncpy(rel_path, clean_path, size - 1);
                    rel_path[size - 1] = '\0';
                }
            }
            else {
                strncpy(rel_path, clean_path, size - 1);
                rel_path[size - 1] = '\0';
            }
        }
        else {
            perror("getcwd");
            strncpy(rel_path, clean_path, size - 1);
            rel_path[size - 1] = '\0';
        }
    }
    if (rel_path[0] == '\0' && strcmp(full_path, ".") == 0) {
        strcpy(rel_path, ".");
    }
}

int is_excluded(const char* rel_path) {
    char path_to_check[MAX_PATH_SIZE];
    strncpy(path_to_check, rel_path, MAX_PATH_SIZE - 1);
    path_to_check[MAX_PATH_SIZE - 1] = '\0';
    normalize_path(path_to_check);

    if (strncmp(path_to_check, "./", 2) == 0) {
        memmove(path_to_check, path_to_check + 2, strlen(path_to_check) - 1); // Move null term too
    }
    if (path_to_check[0] == '\0') {
        strcpy(path_to_check, ".");
    }

    static char relative_output_path[MAX_PATH_SIZE] = "";
    static int output_path_calculated = 0;
    if (!output_path_calculated && output) { // Check if output file is open
        char* output_filename_ptr = get_output_filename();
        if (output_filename_ptr) {
            get_relative_path(output_filename_ptr, relative_output_path, sizeof(relative_output_path));
            if (strncmp(relative_output_path, "./", 2) == 0) {
                memmove(relative_output_path, relative_output_path + 2, strlen(relative_output_path) - 1);
            }
            if (relative_output_path[0] == '\0') {
            }
            free(output_filename_ptr);
            output_path_calculated = 1;
        }
    }
    if (output_path_calculated && strcmp(path_to_check, relative_output_path) == 0) {
        return 1;
    }

    const char* base = strrchr(path_to_check, '/');
    base = base ? base + 1 : path_to_check;
    if (strcmp(base, "ctf") == 0 || strcmp(base, "ctf.exe") == 0)
        return 1;

    for (int i = 0; i < exclude_count; i++) {
        const char* pattern = exclude_patterns[i];
        char clean_pattern[MAX_PATH_SIZE];
        strncpy(clean_pattern, pattern, MAX_PATH_SIZE - 1);
        clean_pattern[MAX_PATH_SIZE - 1] = '\0';

        if (strncmp(clean_pattern, "./", 2) == 0) {
            memmove(clean_pattern, clean_pattern + 2, strlen(clean_pattern) - 1);
        }

        int match_flags = FNM_PATHNAME | FNM_NOESCAPE;
        match_flags |= FNM_PERIOD;

        if (fnmatch(clean_pattern, path_to_check, match_flags) == 0) {
            return 1;
        }

        size_t pattern_len = strlen(clean_pattern);
        if (pattern_len > 0 && clean_pattern[pattern_len - 1] == '/') {
            char pattern_base[MAX_PATH_SIZE];
            strncpy(pattern_base, clean_pattern, pattern_len - 1);
            pattern_base[pattern_len - 1] = '\0';

            if (strcmp(path_to_check, pattern_base) == 0 ||
                (strncmp(path_to_check, clean_pattern, pattern_len) == 0)) {
                return 1;
            }
        }
    }

    return 0;
}

void traverse_directory(const char* base_path, int depth) {
    DIR* dir = opendir(base_path);
    if (!dir) {

        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[MAX_PATH_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        char rel_path[MAX_PATH_SIZE];
        get_relative_path(full_path, rel_path, sizeof(rel_path));

        if (is_excluded(rel_path)) {
            continue;
        }

        struct stat st;

        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Warning: Could not stat entry %s: ", full_path);
            perror(NULL);
            continue;
        }

        print_indent(depth);
        if (S_ISDIR(st.st_mode)) {
            fprintf(output, "%s/\n", entry->d_name);
            traverse_directory(full_path, depth + 1);
        }
        else if (S_ISREG(st.st_mode)) {
            if (should_show_content(entry->d_name, full_path)) {
                fprintf(output, "%s:\n", entry->d_name);
                write_file_content_inline(full_path, depth + 1);
            }
            else {
                fprintf(output, "%s\n", entry->d_name);
            }
        }
    }
    closedir(dir);
}

int print_path_hierarchy(const char* path) {
    char copy[MAX_PATH_SIZE];
    char original_path_normalized[MAX_PATH_SIZE];

    strncpy(original_path_normalized, path, MAX_PATH_SIZE - 1);
    original_path_normalized[MAX_PATH_SIZE - 1] = '\0';
    normalize_path(original_path_normalized);

    strncpy(copy, original_path_normalized, MAX_PATH_SIZE - 1);
    copy[MAX_PATH_SIZE - 1] = '\0';

    if (strcmp(copy, ".") == 0) {
        return 0;
    }

    struct stat st;
    if (stat(original_path_normalized, &st) != 0) {
        return 0;
    }

    char* print_limit = copy + strlen(copy);

    if (S_ISREG(st.st_mode)) {
        char* last_slash = strrchr(copy, '/');
        if (last_slash != NULL) {
            print_limit = last_slash;
        }
        else {
            return 0;
        }
    }

    int depth = 0;
    char* start = copy;
    char* end;

    if (*start == '/') {
        start++;
    }

    while (start < print_limit && *start != '\0') {
        while (*start == '/' && start < print_limit) {
            start++;
        }
        if (start >= print_limit)
            break;

        end = strchr(start, '/');
        if (end == NULL || end >= print_limit) {
            end = print_limit;
        }

        if (end > start) {
            char component[MAX_PATH_SIZE];
            size_t component_len = end - start;
            if (component_len >= MAX_PATH_SIZE)
                component_len = MAX_PATH_SIZE - 1;
            strncpy(component, start, component_len);
            component[component_len] = '\0';

            if (strcmp(component, ".") != 0) {
                print_indent(depth);
                fprintf(output, "%s/\n", component);
                depth++;
            }
        }

        if (end == print_limit)
            break;

        start = end + 1;
    }

    return depth;
}
