#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <limits.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// types de données (Keep these for DT_ defines if not standardly available)
#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#ifndef DT_LNK 
#define DT_LNK 10
#endif


void print_indent(int depth, FILE* output) {
    for (int i = 0; i < depth; i++) {
        fputc('\t', output);
    }
}


void write_file_content_inline(const char* filepath, int depth, recap_context* ctx) {
    FILE* f = fopen(filepath, "r"); // Use "r" for text mode read
    if (!f) {
        fprintf(stderr, "Warning: Could not open file %s to read content: %s\n", filepath, strerror(errno));
        print_indent(depth, ctx->output_stream);
        fprintf(ctx->output_stream, "[Error reading file content]\n");
        return;
    }


    char line[1024 * 4];
    int first_line = 1;
    int needs_newline = 0;

    while (fgets(line, sizeof(line), f)) {
        print_indent(depth, ctx->output_stream);
        fputs(line, ctx->output_stream);
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n') {
            needs_newline = 1;
        }
        else {
            needs_newline = 0;
        }
        first_line = 0;
    }

    if (ferror(f)) {
        fprintf(stderr, "Warning: I/O error reading content from %s\n", filepath);
    }
    else if (needs_newline && !first_line) {
        fputc('\n', ctx->output_stream);
    }


    fclose(f);
}

static void remove_dot_slash_prefix(char* str) {
    if (strncmp(str, "./", 2) == 0) {
        memmove(str, str + 2, strlen(str) - 2 + 1);
    }
}


void get_relative_path(const char* full_path, char* rel_path_out, size_t size) {
    char normalized_full_path[MAX_PATH_SIZE];
    char cwd[MAX_PATH_SIZE];

    strncpy(normalized_full_path, full_path, sizeof(normalized_full_path) - 1);
    normalized_full_path[sizeof(normalized_full_path) - 1] = '\0';
    normalize_path(normalized_full_path);

    if (strcmp(normalized_full_path, ".") == 0) {
        snprintf(rel_path_out, size, ".");
        return;
    }

    if (normalized_full_path[0] != '/') {
        strncpy(rel_path_out, normalized_full_path, size - 1);
        rel_path_out[size - 1] = '\0';
        remove_dot_slash_prefix(rel_path_out);
        if (rel_path_out[0] == '\0') {
            strcpy(rel_path_out, ".");
        }
        return;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd in get_relative_path");
        strncpy(rel_path_out, normalized_full_path, size - 1);
        rel_path_out[size - 1] = '\0';
        return;
    }
    normalize_path(cwd);

    size_t cwd_len = strlen(cwd);

    if (strcmp(cwd, "/") == 0) {
        if (strncmp(normalized_full_path, "/", 1) == 0) {
            snprintf(rel_path_out, size, "%s", normalized_full_path + 1);
            return;
        }
        strncpy(rel_path_out, normalized_full_path, size - 1);
        rel_path_out[size - 1] = '\0';
        return;
    }


    if (strncmp(normalized_full_path, cwd, cwd_len) == 0 &&
        (normalized_full_path[cwd_len] == '/' || normalized_full_path[cwd_len] == '\0')) {
        const char* relative_part = normalized_full_path + cwd_len;
        if (*relative_part == '/') {
            relative_part++;
        }
        if (*relative_part == '\0') {
            snprintf(rel_path_out, size, ".");
        }
        else {
            snprintf(rel_path_out, size, "%s", relative_part);
        }
        return;
    }
    strncpy(rel_path_out, normalized_full_path, size - 1);
    rel_path_out[size - 1] = '\0';

}

int is_excluded(const char* current_rel_path, const recap_context* ctx) {
    char path_to_check[MAX_PATH_SIZE];

    strncpy(path_to_check, current_rel_path, sizeof(path_to_check) - 1);
    path_to_check[sizeof(path_to_check) - 1] = '\0';
    normalize_path(path_to_check);

    if (strcmp(path_to_check, ".") == 0) {
        for (int i = 0; i < ctx->excludes.exclude_count; i++) {
            const char* pattern = ctx->excludes.exclude_patterns[i];
            if (strcmp(pattern, ".") == 0 || strcmp(pattern, "./") == 0) return 1;
        }
    }

    if (!ctx->output.use_stdout && ctx->output.relative_output_path[0] != '\0') {
        if (strcmp(path_to_check, ctx->output.relative_output_path) == 0) {
            return 1;
        }
    }

    const char* base = strrchr(path_to_check, '/');
    base = base ? base + 1 : path_to_check;
    if (strcmp(base, "recap") == 0 || strcmp(base, "recap.exe") == 0) {
        return 1;
    }

    for (int i = 0; i < ctx->excludes.exclude_count; i++) {
        const char* pattern = ctx->excludes.exclude_patterns[i];
        char normalized_pattern[MAX_PATH_SIZE];

        strncpy(normalized_pattern, pattern, sizeof(normalized_pattern) - 1);
        normalized_pattern[sizeof(normalized_pattern) - 1] = '\0';

        int pattern_ends_with_slash = 0;
        size_t pattern_len = strlen(normalized_pattern);
        if (pattern_len > 0 && normalized_pattern[pattern_len - 1] == '/') {
            pattern_ends_with_slash = 1;
            normalized_pattern[pattern_len - 1] = '\0';
        }

        int match_flags = FNM_PATHNAME | FNM_PERIOD;

        if (fnmatch(normalized_pattern, path_to_check, match_flags) == 0) {
            if (pattern_ends_with_slash) {
                size_t normalized_pattern_len = strlen(normalized_pattern);
                if (strncmp(path_to_check, normalized_pattern, normalized_pattern_len) == 0) {
                    if (path_to_check[normalized_pattern_len] == '\0' || path_to_check[normalized_pattern_len] == '/') {
                        return 1;
                    }
                }

            }
            else {
                return 1;
            }
        }
        if (pattern_ends_with_slash) {
            normalized_pattern[pattern_len - 1] = '/';
        }

    }

    return 0;
}


int start_traversal(const char* initial_path, recap_context* ctx) {
    char path_normalized[MAX_PATH_SIZE];
    char rel_path[MAX_PATH_SIZE];

    strncpy(path_normalized, initial_path, sizeof(path_normalized) - 1);
    path_normalized[sizeof(path_normalized) - 1] = '\0';
    normalize_path(path_normalized);

    get_relative_path(path_normalized, rel_path, sizeof(rel_path));

    if (is_excluded(rel_path, ctx)) {
        if (strcmp(rel_path, ".") == 0) {
            fprintf(stderr, "Warning: Root include path '.' is excluded, this include target will be skipped.\n");
        }
        return 0;
    }

    struct stat st;
    if (stat(path_normalized, &st) != 0) {
        perror("stat include path");
        fprintf(stderr, "Warning: Could not stat include path: %s. Skipping.\n", initial_path);
        return 0;
    }

    const char* basename_part = strrchr(path_normalized, '/');
    basename_part = basename_part ? basename_part + 1 : path_normalized;
    if ((strcmp(initial_path, ".") == 0 || strcmp(initial_path, "./") == 0) && strcmp(basename_part, ".") != 0) {
        get_relative_path(path_normalized, rel_path, sizeof(rel_path));
        basename_part = rel_path;
    }


    int initial_depth = 0;

    if (S_ISDIR(st.st_mode)) {
        if (strcmp(basename_part, ".") != 0 || strcmp(initial_path, ".") == 0 || strcmp(initial_path, "./") == 0) { // Print "." if it was the input
            print_indent(initial_depth, ctx->output_stream);
            fprintf(ctx->output_stream, "%s/\n", basename_part);
        }
        traverse_directory(path_normalized, initial_depth + 1, ctx);
    }
    else if (S_ISREG(st.st_mode)) {
        print_indent(initial_depth, ctx->output_stream);
        if (should_show_content(basename_part, path_normalized, &ctx->content)) {
            fprintf(ctx->output_stream, "%s:\n", basename_part);
            write_file_content_inline(path_normalized, initial_depth + 1, ctx);
        }
        else {
            fprintf(ctx->output_stream, "%s\n", basename_part);
        }
    }
    else {
        fprintf(stderr, "Warning: Input path %s is not a regular file or directory. Skipping.\n", initial_path);
        return 0;
    }

    return 0;
}


void traverse_directory(const char* base_path, int depth, recap_context* ctx) {
    DIR* dir = opendir(base_path);
    if (!dir) {
        fprintf(stderr, "Warning: Could not open directory %s: %s\n", base_path, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH_SIZE];
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        if (len < 0 || (size_t)len >= sizeof(full_path)) {
            fprintf(stderr, "Warning: Path too long, skipping entry: %s/%s\n", base_path, entry->d_name);
            continue;
        }

        char rel_path[MAX_PATH_SIZE];
        get_relative_path(full_path, rel_path, sizeof(rel_path));

        if (is_excluded(rel_path, ctx)) {
            continue;
        }

        int is_dir = 0;
        int is_reg = 0;
        int is_link = 0;

#ifdef _DIRENT_HAVE_D_TYPE
        if (entry->d_type != DT_UNKNOWN) {
            is_dir = (entry->d_type == DT_DIR);
            is_reg = (entry->d_type == DT_REG);
            is_link = (entry->d_type == DT_LNK);
        }
        else
#endif
        {
            struct stat st;
            if (lstat(full_path, &st) != 0) {
                fprintf(stderr, "Warning: Could not lstat entry %s: %s. Skipping.\n", full_path, strerror(errno));
                continue;
            }
            is_dir = S_ISDIR(st.st_mode);
            is_reg = S_ISREG(st.st_mode);
            is_link = S_ISLNK(st.st_mode);
        }

        // Handle symlinks
        if (is_link) {
            print_indent(depth, ctx->output_stream);
            fprintf(ctx->output_stream, "%s@\n", entry->d_name);
            continue;
        }


        print_indent(depth, ctx->output_stream);
        if (is_dir) {
            fprintf(ctx->output_stream, "%s/\n", entry->d_name);
            traverse_directory(full_path, depth + 1, ctx);
        }
        else if (is_reg) {
            if (should_show_content(entry->d_name, full_path, &ctx->content)) {
                fprintf(ctx->output_stream, "%s:\n", entry->d_name);
                write_file_content_inline(full_path, depth + 1, ctx);
            }
            else {
                fprintf(ctx->output_stream, "%s\n", entry->d_name);
            }
        }
    }

    if (errno != 0 && entry == NULL) {
        //Could be useful later idc
        //perror("readdir error");
        //fprintf(stderr, "Warning: Error reading directory %s\n", base_path);
    }

    closedir(dir);
}

