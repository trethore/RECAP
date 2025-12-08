#ifndef RECAP_H
#define RECAP_H

#include <stdio.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <sys/stat.h>
#include <stddef.h>

#include "lib/memlst.h"

#define MAX_PATH_SIZE 4096
#define MAX_PATTERNS 256
#define MAX_SCOPED_STRIP_RULES 32
#define MAX_GITIGNORE_ENTRIES 1024
#define MAX_FILE_CONTENT_SIZE (10 * 1024 * 1024) // 10MB

typedef struct {
    char* full_path;
    char* rel_path;
} path_entry;

typedef struct {
    path_entry* items;
    size_t count;
    size_t capacity;
} path_list;

typedef struct {
    pcre2_code* compiled[MAX_PATTERNS];
    pcre2_match_data* match_data[MAX_PATTERNS];
    int count;
    memlst_t destructors;
} regex_ctx;

typedef struct {
    const char* patterns[MAX_PATTERNS];
    int count;
} fnmatch_ctx;

typedef struct {
    pcre2_code* path_regex;
    pcre2_code* strip_regex;
    pcre2_match_data* path_match_data;
    pcre2_match_data* strip_match_data;
} scoped_strip_rule;

typedef struct {
    char output_dir[MAX_PATH_SIZE];
    char output_name[MAX_PATH_SIZE];
    char calculated_output_path[MAX_PATH_SIZE];
    char relative_output_path[MAX_PATH_SIZE];
    int use_stdout;
    int is_temp_file;
} output_ctx;

typedef struct {
    const char* start_paths[MAX_PATTERNS];
    int start_path_count;
    char cwd[MAX_PATH_SIZE];

    regex_ctx include_filters;
    regex_ctx exclude_filters;
    regex_ctx content_include_filters;
    regex_ctx content_exclude_filters;

    fnmatch_ctx fnmatch_exclude_filters;
    char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];
    int gitignore_entry_count;

    pcre2_code* strip_regex;
    pcre2_match_data* strip_match_data;

    scoped_strip_rule scoped_strip_rules[MAX_SCOPED_STRIP_RULES];
    int scoped_strip_rule_count;

    output_ctx output;
    path_list matched_files;

    memlst_t cleanup;

    const char* gist_api_key;
    const char* version;
    FILE* output_stream;
    int copy_to_clipboard;
    int compact_output;

} recap_context;

void parse_arguments(int argc, char* argv[], recap_context* ctx);
void load_gitignore(recap_context* ctx, const char* gitignore_filename);
void clear_recap_output_files(const char* target_dir);
void free_regex_ctx(regex_ctx* ctx);

int start_traversal(recap_context* ctx);

int is_text_file(const char* full_path);
void normalize_path(char* path);
int generate_output_filename(output_ctx* output_context);
void get_relative_path(const char* full_path, const char* cwd, char* rel_path_out, size_t size);

int path_list_init(path_list* list);
int path_list_add(path_list* list, const char* full_path, const char* rel_path);
void path_list_free(path_list* list);
void path_list_sort(path_list* list);

int read_file_into_buffer(const char* path, size_t max_bytes, char** out_buf, size_t* out_len);

char* upload_to_gist(const char* filepath, const char* github_token);

int program_exists(const char* name);

int copy_file_content_to_clipboard(const char* filepath);

char* apply_compact_transformations(const char* content, const char* filename);

#endif
