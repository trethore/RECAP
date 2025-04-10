// this file add some useful functions for traversing directories and files
#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <limits.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// types de données

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif

#ifndef DT_DIR
#define DT_DIR 4
#endif

#ifndef DT_REG
#define DT_REG 8
#endif

static inline void remove_dot_prefix(char* str) {
    if (strncmp(str, "./", 2) == 0) {
        size_t len = strlen(str);
        memmove(str, str + 2, len - 1);
    }
}

void print_indent(int depth, FILE* output) {
    for (int i = 0; i < depth; i++) {
        fputc('\t', output);
    }
}


void write_file_content_inline(const char* filepath, int depth, FILE* output, content_ctx* content_context) {
    (void)content_context;
    FILE* f = fopen(filepath, "r");
    if (!f)
        return;

    char line[1024];
    char last_char = '\0';
    while (fgets(line, sizeof(line), f)) {
        print_indent(depth, output);
        fputs(line, output);
        size_t len = strlen(line);
        if (len > 0)
            last_char = line[len - 1];
    }
    if (last_char != '\n' && last_char != '\0') {
        fputc('\n', output);
    }
    fclose(f);
}

void get_relative_path(const char* full_path, char* rel_path, size_t size) {
    char clean_path[MAX_PATH_SIZE];
    snprintf(clean_path, sizeof(clean_path), "%s", full_path);
    normalize_path(clean_path);
    if (clean_path[0] != '/') {
        remove_dot_prefix(clean_path);
        snprintf(rel_path, size, "%s", clean_path);
    }
    else {
        char cwd[MAX_PATH_SIZE];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            normalize_path(cwd);
            size_t cwd_len = strlen(cwd);
            if (strncmp(clean_path, cwd, cwd_len) == 0) {
                if (clean_path[cwd_len] == '/' || clean_path[cwd_len] == '\0') {
                    char* relative_part = clean_path + cwd_len;
                    if (*relative_part == '/')
                        relative_part++;
                    snprintf(rel_path, size, "%s", relative_part);
                    if (rel_path[0] == '\0' && strcmp(clean_path, cwd) == 0) {
                        snprintf(rel_path, size, ".");
                    }
                }
                else {
                    snprintf(rel_path, size, "%s", clean_path);
                }
            }
            else {
                snprintf(rel_path, size, "%s", clean_path);
            }
        }
        else {
            perror("getcwd");
            snprintf(rel_path, size, "%s", clean_path);
        }
    }
    if (rel_path[0] == '\0' && strcmp(full_path, ".") == 0) {
        snprintf(rel_path, size, ".");
    }
}


int is_excluded(FILE* output, const char* rel_path, exclude_patterns_ctx* exclude_ctx, output_ctx* output_context) {
    char path_to_check[MAX_PATH_SIZE];
    snprintf(path_to_check, sizeof(path_to_check), "%s", rel_path);
    normalize_path(path_to_check);
    remove_dot_prefix(path_to_check);

    if (path_to_check[0] == '\0') {
        snprintf(path_to_check, sizeof(path_to_check), ".");
    }

    static char relative_output_path[MAX_PATH_SIZE] = "";
    static int output_path_calculated = 0;
    if (!output_path_calculated && output_context->output_dir[0] != '\0') {
        char* output_filename_ptr = get_output_filename(output, output_context);
        if (output_filename_ptr) {
            get_relative_path(output_filename_ptr, relative_output_path, sizeof(relative_output_path));
            remove_dot_prefix(relative_output_path);
            free(output_filename_ptr);
            output_path_calculated = 1;
        }
    }
    if (output_path_calculated && strcmp(path_to_check, relative_output_path) == 0) {
        return 1;
    }

    const char* base = strrchr(path_to_check, '/');
    base = base ? base + 1 : path_to_check;
    if (strcmp(base, "recap") == 0 || strcmp(base, "recap.exe") == 0)
        return 1;

    for (int i = 0; i < exclude_ctx->exclude_count; i++) {
        const char* pattern = exclude_ctx->exclude_patterns[i];
        char clean_pattern[MAX_PATH_SIZE];
        snprintf(clean_pattern, sizeof(clean_pattern), "%s", pattern);
        remove_dot_prefix(clean_pattern);

        int match_flags = FNM_PATHNAME | FNM_NOESCAPE | FNM_PERIOD;
        if (fnmatch(clean_pattern, path_to_check, match_flags) == 0) {
            return 1;
        }

        size_t pattern_len = strlen(clean_pattern);
        if (pattern_len > 0 && clean_pattern[pattern_len - 1] == '/') {
            char pattern_base[MAX_PATH_SIZE];
            snprintf(pattern_base, sizeof(pattern_base), "%.*s", (int)(pattern_len - 1), clean_pattern);
            if (strcmp(path_to_check, pattern_base) == 0 ||
                (strncmp(path_to_check, clean_pattern, pattern_len) == 0)) {
                return 1;
            }
        }
    }

    return 0;
}


void traverse_directory(const char* base_path, int depth, FILE* output, exclude_patterns_ctx* exclude_ctx, output_ctx* output_context, content_ctx* content_context) {
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

        if (is_excluded(output, rel_path, exclude_ctx, output_context)) {
            continue;
        }

        int is_dir = 0, is_reg = 0;
        struct stat st;
        if (entry->d_type == DT_UNKNOWN) {
            if (stat(full_path, &st) != 0) {
                fprintf(stderr, "Warning: Could not stat entry %s: ", full_path);
                perror(NULL);
                continue;
            }
            is_dir = S_ISDIR(st.st_mode);
            is_reg = S_ISREG(st.st_mode);
        }
        else {
            is_dir = (entry->d_type == DT_DIR);
            is_reg = (entry->d_type == DT_REG);
        }

        print_indent(depth, output);
        if (is_dir) {
            fprintf(output, "%s/\n", entry->d_name);
            traverse_directory(full_path, depth + 1, output, exclude_ctx, output_context, content_context);
        }
        else if (is_reg) {
            if (should_show_content(entry->d_name, full_path, content_context)) {
                fprintf(output, "%s:\n", entry->d_name);
                write_file_content_inline(full_path, depth + 1, output, content_context);
            }
            else {
                fprintf(output, "%s\n", entry->d_name);
            }
        }
    }
    closedir(dir);
}


int print_path_hierarchy(const char* path, FILE* output) {
    char copy[MAX_PATH_SIZE];
    char original_path_normalized[MAX_PATH_SIZE];
    snprintf(original_path_normalized, sizeof(original_path_normalized), "%s", path);
    normalize_path(original_path_normalized);
    snprintf(copy, sizeof(copy), "%s", original_path_normalized);

    if (strcmp(copy, ".") == 0) {
        return 0;
    }

    struct stat st;
    if (stat(original_path_normalized, &st) != 0) {
        return 0;
    }

    if (S_ISREG(st.st_mode)) {
        char* last_slash = strrchr(copy, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';
        }
        else {
            return 0;
        }
    }

    int depth = 0;
    char* token = strtok(copy, "/");
    while (token != NULL) {
        print_indent(depth, output);
        fprintf(output, "%s/\n", token);
        depth++;
        token = strtok(NULL, "/");
    }
    return depth;
}