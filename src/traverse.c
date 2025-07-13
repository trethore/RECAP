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
#include <regex.h>

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

static int match_regex_list(const regex_ctx* ctx, const char* str) {
    for (int i = 0; i < ctx->count; i++) {
        if (regexec(&ctx->compiled[i], str, 0, NULL, 0) == 0) {
            return 1;
        }
    }
    return 0;
}

static int match_fnmatch_list(const fnmatch_ctx* ctx, const char* path_to_check) {
    for (int i = 0; i < ctx->count; i++) {
        const char* pattern = ctx->patterns[i];
        if (fnmatch(pattern, path_to_check, FNM_PATHNAME | FNM_PERIOD) == 0) {
            return 1;
        }

        size_t pattern_len = strlen(pattern);
        if (pattern_len > 0 && pattern[pattern_len - 1] == '/') {
            char dir_pattern[MAX_PATH_SIZE];
            strncpy(dir_pattern, pattern, pattern_len - 1);
            dir_pattern[pattern_len - 1] = '\0';
            if (fnmatch(dir_pattern, path_to_check, FNM_PATHNAME | FNM_PERIOD) == 0) {
                struct stat st;
                if (stat(path_to_check, &st) == 0 && S_ISDIR(st.st_mode)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int should_be_skipped(const char* rel_path, const recap_context* ctx) {

    if (!ctx->output.use_stdout && ctx->output.relative_output_path[0] != '\0') {
        if (strcmp(rel_path, ctx->output.relative_output_path) == 0) {
            return 1;
        }
    }

    if (match_fnmatch_list(&ctx->fnmatch_exclude_filters, rel_path)) {
        return 1;
    }

    if (ctx->exclude_filters.count > 0 && match_regex_list(&ctx->exclude_filters, rel_path)) {
        return 1;
    }

    if (ctx->include_filters.count > 0) {
        if (!match_regex_list(&ctx->include_filters, rel_path)) {
            return 1;
        }
    }

    return 0;
}


int should_show_content(const char* rel_path, const char* full_path, const recap_context* ctx) {
    if (ctx->content_exclude_filters.count > 0 && match_regex_list(&ctx->content_exclude_filters, rel_path)) {
        return 0;
    }

    if (ctx->content_include_filters.count > 0) {
        if (match_regex_list(&ctx->content_include_filters, rel_path)) {
            return is_text_file(full_path);
        }
    }

    return 0;
}


void print_indent(int depth, FILE* output) {
    for (int i = 0; i < depth; i++) {
        fputc('\t', output);
    }
}

void write_file_content_inline(const char* filepath, int depth, recap_context* ctx) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Warning: Could not open file %s to read content: %s\n", filepath, strerror(errno));
        print_indent(depth, ctx->output_stream);
        fprintf(ctx->output_stream, "[Error reading file content]\n");
        return;
    }

    char line[4096];
    int header_found = 0;


    if (ctx->strip_until_regex_is_set) {
        while (fgets(line, sizeof(line), f)) {

            if (regexec(&ctx->strip_until_regex, line, 0, NULL, 0) == 0) {
                header_found = 1;
                break;
            }
        }
        if (!header_found) {

            fclose(f);
            return;
        }
    }


    int needs_newline = 0;
    while (fgets(line, sizeof(line), f)) {
        print_indent(depth, ctx->output_stream);
        fputs(line, ctx->output_stream);
        size_t len = strlen(line);
        needs_newline = (len > 0 && line[len - 1] != '\n');
    }

    if (ferror(f)) {
        fprintf(stderr, "Warning: I/O error reading content from %s\n", filepath);
    }
    else if (needs_newline) {
        fputc('\n', ctx->output_stream);
    }

    fclose(f);
}

static void remove_dot_slash_prefix(char* str) {
    if (strncmp(str, "./", 2) == 0) {
        memmove(str, str + 2, strlen(str) - 1);
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
        if (strncmp(normalized_full_path, "/", 1) == 0 && strlen(normalized_full_path) > 1) {
            snprintf(rel_path_out, size, "%s", normalized_full_path + 1);
        }
        else {
            strncpy(rel_path_out, normalized_full_path, size - 1);
            rel_path_out[size - 1] = '\0';
        }
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

int start_traversal(const char* initial_path, recap_context* ctx) {
    char path_normalized[MAX_PATH_SIZE];
    char rel_path[MAX_PATH_SIZE];

    strncpy(path_normalized, initial_path, sizeof(path_normalized) - 1);
    path_normalized[sizeof(path_normalized) - 1] = '\0';
    normalize_path(path_normalized);

    get_relative_path(path_normalized, rel_path, sizeof(rel_path));


    if (should_be_skipped(rel_path, ctx)) {
        fprintf(stderr, "Info: Start path '%s' is excluded by a filter, skipping.\n", initial_path);
        return 0;
    }

    struct stat st;
    if (stat(path_normalized, &st) != 0) {
        perror("stat include path");
        fprintf(stderr, "Warning: Could not stat start path: %s. Skipping.\n", initial_path);
        return 0;
    }

    const char* basename_part = strrchr(path_normalized, '/');
    basename_part = basename_part ? basename_part + 1 : path_normalized;

    int initial_depth = 0;

    if (S_ISDIR(st.st_mode)) {

        if (strcmp(basename_part, ".") != 0 || strcmp(initial_path, ".") == 0 || strcmp(initial_path, "./") == 0) {
            fprintf(ctx->output_stream, "%s/\n", basename_part);
        }
        traverse_directory(path_normalized, initial_depth + 1, ctx);
    }
    else if (S_ISREG(st.st_mode)) {
        if (should_show_content(rel_path, path_normalized, ctx)) {
            fprintf(ctx->output_stream, "%s:\n", basename_part);
            write_file_content_inline(path_normalized, initial_depth + 1, ctx);
        }
        else {
            fprintf(ctx->output_stream, "%s\n", basename_part);
        }
    }
    else {
        fprintf(stderr, "Warning: Start path %s is not a regular file or directory. Skipping.\n", initial_path);
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


        if (should_be_skipped(rel_path, ctx)) {
            continue;
        }

        int is_dir = 0, is_reg = 0, is_link = 0;
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

        print_indent(depth, ctx->output_stream);

        if (is_link) {
            fprintf(ctx->output_stream, "%s@\n", entry->d_name);
            continue;
        }

        if (is_dir) {
            fprintf(ctx->output_stream, "%s/\n", entry->d_name);
            traverse_directory(full_path, depth + 1, ctx);
        }
        else if (is_reg) {

            if (should_show_content(rel_path, full_path, ctx)) {
                fprintf(ctx->output_stream, "%s:\n", entry->d_name);
                write_file_content_inline(full_path, depth + 1, ctx);
            }
            else {
                fprintf(ctx->output_stream, "%s\n", entry->d_name);
            }
        }
    }

    closedir(dir);
}