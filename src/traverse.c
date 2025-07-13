#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int match_regex_list(const regex_ctx* ctx, const char* str) {
    for (int i = 0; i < ctx->count; i++) {
        if (regexec(&ctx->compiled[i], str, 0, NULL, 0) == 0) return 1;
    }
    return 0;
}

static int match_fnmatch_list(const fnmatch_ctx* ctx, const char* path_to_check) {
    for (int i = 0; i < ctx->count; i++) {
        const char* pattern = ctx->patterns[i];
        if (fnmatch(pattern, path_to_check, FNM_PATHNAME | FNM_PERIOD) == 0) return 1;
        size_t len = strlen(pattern);
        if (len > 0 && pattern[len - 1] == '/') {
            if (strncmp(path_to_check, pattern, len) == 0) return 1;
        }
    }
    return 0;
}

static int should_be_skipped(const char* rel_path, const struct stat* st, recap_context* ctx) {
    if (!ctx->output.use_stdout && strcmp(rel_path, ctx->output.relative_output_path) == 0) return 1;
    if (match_fnmatch_list(&ctx->fnmatch_exclude_filters, rel_path)) return 1;
    if (ctx->exclude_filters.count > 0 && match_regex_list(&ctx->exclude_filters, rel_path)) return 1;

    if (ctx->include_filters.count > 0) {
        if (match_regex_list(&ctx->include_filters, rel_path)) return 0;
        char temp_path[MAX_PATH_SIZE];
        strncpy(temp_path, rel_path, sizeof(temp_path) - 1);
        temp_path[sizeof(temp_path) - 1] = '\0';
        for (char* p = strrchr(temp_path, '/'); p; p = strrchr(temp_path, '/')) {
            *p = '\0';
            if (match_regex_list(&ctx->include_filters, temp_path)) return 0;
        }
        return S_ISDIR(st->st_mode) ? 0 : 1;
    }
    return 0;
}

int should_show_content(const char* rel_path, const char* full_path, recap_context* ctx) {
    if (ctx->content_exclude_filters.count > 0 && match_regex_list(&ctx->content_exclude_filters, rel_path)) return 0;
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

void write_file_content(const char* filepath, int depth, recap_context* ctx) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        print_indent(depth, ctx->output_stream);
        fprintf(ctx->output_stream, "[Error reading file content]\n");
        return;
    }

    char line[4096];
    int header_stripped = !ctx->strip_regex_is_set;
    int previous_line_was_blank = 0;

    while (fgets(line, sizeof(line), f)) {
        if (!header_stripped) {
            if (regexec(&ctx->strip_regex, line, 0, NULL, 0) == 0) {
                header_stripped = 1;
            }
            continue;
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) {
            if (!previous_line_was_blank) {
                fputc('\n', ctx->output_stream);
                previous_line_was_blank = 1;
            }
        }
        else {
            previous_line_was_blank = 0;
            print_indent(depth, ctx->output_stream);
            fputs(line, ctx->output_stream);
            fputc('\n', ctx->output_stream);
        }
    }
    fclose(f);
}

void traverse_directory(const char* base_path, const char* rel_path_prefix, int depth, recap_context* ctx) {
    DIR* dir = opendir(base_path);
    if (!dir) return;

    struct dirent** namelist;
    int n = scandir(base_path, &namelist, NULL, alphasort);
    if (n < 0) {
        closedir(dir);
        return;
    }

    for (int i = 0; i < n; i++) {
        struct dirent* entry = namelist[i];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(namelist[i]);
            continue;
        }

        char full_path[MAX_PATH_SIZE], rel_path[MAX_PATH_SIZE];
        int len;

        len = snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        if (len < 0 || (size_t)len >= sizeof(full_path)) {
            free(namelist[i]);
            continue;
        }
        len = snprintf(rel_path, sizeof(rel_path), "%s%s", rel_path_prefix, entry->d_name);
        if (len < 0 || (size_t)len >= sizeof(rel_path)) {
            free(namelist[i]);
            continue;
        }
        struct stat st;
        if (lstat(full_path, &st) != 0) {
            free(namelist[i]);
            continue;
        }
        if (should_be_skipped(rel_path, &st, ctx)) {
            free(namelist[i]);
            continue;
        }

        ctx->items_processed_count++;
        print_indent(depth, ctx->output_stream);

        if (S_ISDIR(st.st_mode)) {
            fprintf(ctx->output_stream, "%s/\n", entry->d_name);
            char next_rel_prefix[MAX_PATH_SIZE];
            len = snprintf(next_rel_prefix, sizeof(next_rel_prefix), "%s/", rel_path);
            if (len >= 0 && (size_t)len < sizeof(next_rel_prefix)) {
                traverse_directory(full_path, next_rel_prefix, depth + 1, ctx);
            }
        }
        else if (S_ISLNK(st.st_mode)) {
            fprintf(ctx->output_stream, "%s@\n", entry->d_name);
        }
        else if (S_ISREG(st.st_mode)) {
            if (should_show_content(rel_path, full_path, ctx)) {
                fprintf(ctx->output_stream, "%s:\n", entry->d_name);
                write_file_content(full_path, depth + 1, ctx);
            }
            else {
                fprintf(ctx->output_stream, "%s\n", entry->d_name);
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    closedir(dir);
}

int start_traversal(recap_context* ctx) {
    for (int i = 0; i < ctx->start_path_count; i++) {
        char path[MAX_PATH_SIZE], rel_path[MAX_PATH_SIZE];
        strncpy(path, ctx->start_paths[i], sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        normalize_path(path);

        get_relative_path(path, ctx->cwd, rel_path, sizeof(rel_path));

        struct stat st;
        if (lstat(path, &st) != 0) {
            fprintf(stderr, "Warning: Could not stat start path: %s\n", ctx->start_paths[i]);
            continue;
        }

        if (should_be_skipped(rel_path, &st, ctx)) continue;

        ctx->items_processed_count++;
        const char* basename = strrchr(path, '/');
        basename = basename ? basename + 1 : path;

        if (S_ISDIR(st.st_mode)) {
            fprintf(ctx->output_stream, "%s/\n", strcmp(path, ".") == 0 ? "." : basename);
            char rel_prefix[MAX_PATH_SIZE];
            int len;
            if (strcmp(rel_path, ".") == 0) {
                rel_prefix[0] = '\0';
                len = 0;
            }
            else {
                len = snprintf(rel_prefix, sizeof(rel_prefix), "%s/", rel_path);
            }

            if (len >= 0 && (size_t)len < sizeof(rel_prefix)) {
                traverse_directory(path, rel_prefix, 1, ctx);
            }
        }
        else if (S_ISREG(st.st_mode)) {
            if (should_show_content(rel_path, path, ctx)) {
                fprintf(ctx->output_stream, "%s:\n", basename);
                write_file_content(path, 1, ctx);
            }
            else {
                fprintf(ctx->output_stream, "%s\n", basename);
            }
        }
    }
    return 0;
}