#ifndef RECAP_H
#define RECAP_H

#include <stdio.h>
#include <regex.h>
#include <sys/stat.h>

#define MAX_PATH_SIZE 4096
#define MAX_PATTERNS 256
#define MAX_GITIGNORE_ENTRIES 1024

typedef struct {
    regex_t compiled[MAX_PATTERNS];
    int count;
} regex_ctx;

typedef struct {
    const char* patterns[MAX_PATTERNS];
    int count;
} fnmatch_ctx;

typedef struct {
    char output_dir[MAX_PATH_SIZE];
    char output_name[MAX_PATH_SIZE];
    char calculated_output_path[MAX_PATH_SIZE];
    char relative_output_path[MAX_PATH_SIZE];
    int use_stdout;
} output_ctx;

typedef struct {
    const char* start_paths[MAX_PATTERNS];
    int start_path_count;
    int items_processed_count;
    char cwd[MAX_PATH_SIZE];

    regex_ctx include_filters;
    regex_ctx exclude_filters;
    regex_ctx content_include_filters;
    regex_ctx content_exclude_filters;

    fnmatch_ctx fnmatch_exclude_filters;
    char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];
    int gitignore_entry_count;

    regex_t strip_regex;
    int strip_regex_is_set;

    output_ctx output;
    const char* gist_api_key;
    const char* version;
    FILE* output_stream;
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

char* upload_to_gist(const char* filepath, const char* github_token);

#endif