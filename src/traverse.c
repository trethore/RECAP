#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int path_list_init(path_list* list) {
    list->items = malloc(16 * sizeof(char*));
    if (!list->items) return -1;
    list->count = 0;
    list->capacity = 16;
    return 0;
}

static int path_list_add(path_list* list, const char* path) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        char** new_items = realloc(list->items, new_capacity * sizeof(char*));
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return -1;
    list->count++;
    return 0;
}

static int match_regex_list(const regex_ctx* ctx, const char* str) {
    pcre2_match_data* match_data = pcre2_match_data_create(1, NULL);
    if (!match_data) return 0;

    for (int i = 0; i < ctx->count; i++) {
        int rc = pcre2_match(ctx->compiled[i], (PCRE2_SPTR)str, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL);
        if (rc >= 0) {
            pcre2_match_data_free(match_data);
            return 1;
        }
    }
    pcre2_match_data_free(match_data);
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

static int should_show_content(const char* rel_path, const char* full_path, recap_context* ctx) {
    if (ctx->content_exclude_filters.count > 0 && match_regex_list(&ctx->content_exclude_filters, rel_path)) return 0;
    if (ctx->content_include_filters.count > 0) {
        if (match_regex_list(&ctx->content_include_filters, rel_path)) {
            return is_text_file(full_path);
        }
    }
    return 0;
}

static void write_file_content_block(const char* full_path, const char* rel_path, recap_context* ctx) {
    fprintf(ctx->output_stream, "%s:\n", rel_path);

    FILE* f = fopen(full_path, "rb");
    if (!f) {
        fprintf(ctx->output_stream, "[Error reading file content]\n");
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size > MAX_FILE_CONTENT_SIZE) {
        fprintf(ctx->output_stream, "[File content too large to process (>%dMB)]\n", MAX_FILE_CONTENT_SIZE / (1024 * 1024));
        fclose(f);
        return;
    }

    char* content = malloc(file_size + 1);
    if (!content) {
        fprintf(ctx->output_stream, "[Error: could not allocate memory for file content]\n");
        fclose(f);
        return;
    }

    if (fread(content, 1, file_size, f) != (size_t)file_size) {
        fprintf(ctx->output_stream, "[Error reading file content]\n");
        free(content);
        fclose(f);
        return;
    }
    content[file_size] = '\0';
    fclose(f);

    const char* content_to_print = content;
    pcre2_code* strip_regex_to_use = NULL;

    for (int i = 0; i < ctx->scoped_strip_rule_count; i++) {
        pcre2_match_data* match_data = pcre2_match_data_create(1, NULL);
        if (match_data) {
            if (pcre2_match(ctx->scoped_strip_rules[i].path_regex, (PCRE2_SPTR)rel_path, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL) >= 0) {
                strip_regex_to_use = ctx->scoped_strip_rules[i].strip_regex;
            }
            pcre2_match_data_free(match_data);
        }
    }

    if (!strip_regex_to_use && ctx->strip_regex_is_set) {
        strip_regex_to_use = ctx->strip_regex;
    }

    if (strip_regex_to_use) {
        pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(strip_regex_to_use, NULL);
        if (pcre2_match(strip_regex_to_use, (PCRE2_SPTR)content, file_size, 0, 0, match_data, NULL) >= 0) {
            PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
            content_to_print = content + ovector[1];
        }
        pcre2_match_data_free(match_data);
    }

    const char* p = content_to_print;
    int previous_line_was_blank = 0;
    while (*p) {
        const char* end_of_line = strchr(p, '\n');
        size_t line_len = end_of_line ? (size_t)(end_of_line - p) : strlen(p);
        if (line_len > 0 && p[line_len - 1] == '\r') line_len--;

        if (line_len == 0) {
            if (!previous_line_was_blank) {
                fputc('\n', ctx->output_stream);
                previous_line_was_blank = 1;
            }
        }
        else {
            previous_line_was_blank = 0;
            fprintf(ctx->output_stream, "%.*s\n", (int)line_len, p);
        }

        if (end_of_line) {
            p = end_of_line + 1;
        }
        else {
            break;
        }
    }

    free(content);
}


static void print_output(recap_context* ctx) {
    for (size_t i = 0; i < ctx->matched_files.count; i++) {
        const char* full_path = ctx->matched_files.items[i];
        char rel_path[MAX_PATH_SIZE];
        get_relative_path(full_path, ctx->cwd, rel_path, sizeof(rel_path));

        if (i > 0) {
            fprintf(ctx->output_stream, "---\n");
        }

        if (should_show_content(rel_path, full_path, ctx)) {
            write_file_content_block(full_path, rel_path, ctx);
        }
        else {
            fprintf(ctx->output_stream, "%s\n", rel_path);
        }
    }
}

static void traverse_directory(const char* base_path, const char* rel_path_prefix, recap_context* ctx) {
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

        if (S_ISDIR(st.st_mode)) {
            char dir_rel_path[MAX_PATH_SIZE];
            int dir_len = snprintf(dir_rel_path, sizeof(dir_rel_path), "%s/", rel_path);
            if (dir_len < 0 || (size_t)dir_len >= sizeof(dir_rel_path)) {
                fprintf(stderr, "Warning: path too long, skipping directory: %s\n", rel_path);
                free(namelist[i]);
                continue;
            }
            traverse_directory(full_path, dir_rel_path, ctx);
        }
        else if (S_ISREG(st.st_mode)) {
            path_list_add(&ctx->matched_files, full_path);
        }
        free(namelist[i]);
    }
    free(namelist);
    closedir(dir);
}

int start_traversal(recap_context* ctx) {
    if (path_list_init(&ctx->matched_files) != 0) {
        fprintf(stderr, "Error: Failed to initialize path list.\n");
        return 1;
    }

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

        if (S_ISDIR(st.st_mode)) {
            char dir_rel_path[MAX_PATH_SIZE];
            int dir_len = snprintf(dir_rel_path, sizeof(dir_rel_path), "%s/", rel_path);
            if (dir_len < 0 || (size_t)dir_len >= sizeof(dir_rel_path)) {
                fprintf(stderr, "Warning: path too long, skipping start directory: %s\n", rel_path);
                continue;
            }
            traverse_directory(path, strcmp(rel_path, ".") == 0 ? "" : dir_rel_path, ctx);
        }
        else if (S_ISREG(st.st_mode)) {
            path_list_add(&ctx->matched_files, path);
        }
    }

    print_output(ctx);
    return 0;
}