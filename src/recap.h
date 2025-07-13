#ifndef RECAP_H
#define RECAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fnmatch.h>
#include <limits.h>
#include <regex.h>

#define MAX_PATH_SIZE 1024
#define MAX_PATTERNS 1024
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

    regex_ctx include_filters;
    regex_ctx exclude_filters;
    regex_ctx content_include_filters;
    regex_ctx content_exclude_filters;

    fnmatch_ctx fnmatch_exclude_filters;
    char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];
    int gitignore_entry_count;

    regex_t strip_until_regex;
    int strip_until_regex_is_set;

    output_ctx output;
    const char* gist_api_key;
    const char* version;
    FILE* output_stream;
} recap_context;


void parse_arguments(int argc, char* argv[], recap_context* ctx);
void load_gitignore(recap_context* ctx, const char* gitignore_filename);
void clear_recap_output_files(const char* target_dir);
void free_regex_ctx(regex_ctx* ctx);

void traverse_directory(const char* base_path, int depth, recap_context* ctx);
void print_indent(int depth, FILE* output);
void write_file_content_inline(const char* filepath, int depth, recap_context* ctx);
void get_relative_path(const char* full_path, char* rel_path, size_t size);
int start_traversal(const char* initial_path, recap_context* ctx);

int is_text_file(const char* full_path);
void normalize_path(char* path);
int should_show_content(const char* rel_path, const char* full_path, const recap_context* ctx);
int generate_output_filename(output_ctx* output_context);
char* upload_to_gist(const char* filepath, const char* github_token);

char* realpath(const char* restrict path, char* restrict resolved_path);

#endif