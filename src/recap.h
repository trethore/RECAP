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
#include "lib/fnmatch.h"
#include <limits.h>

#define MAX_PATH_SIZE 1024
#define MAX_PATTERNS 1024
#define MAX_CONTENT_SPECIFIERS 1024
#define MAX_GITIGNORE_ENTRIES 1024


typedef struct {
    const char* include_patterns[MAX_PATTERNS];
    int include_count;
} include_patterns_ctx;

typedef struct {
    const char* exclude_patterns[MAX_PATTERNS];
    int exclude_count;
    char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];
    int gitignore_entry_count;
} exclude_patterns_ctx;

typedef struct {
    char output_dir[MAX_PATH_SIZE];
    char output_name[MAX_PATH_SIZE];
    char calculated_output_path[MAX_PATH_SIZE];
    char relative_output_path[MAX_PATH_SIZE];
    int use_stdout;
} output_ctx;

typedef struct {
    const char* content_specifiers[MAX_CONTENT_SPECIFIERS];
    int content_specifier_count;
    int content_flag;
} content_ctx;

typedef struct {
    include_patterns_ctx includes;
    exclude_patterns_ctx excludes;
    output_ctx output;
    content_ctx content;
    const char* gist_api_key;
    const char* version;
    FILE* output_stream;
} recap_context;



void parse_arguments(int argc, char* argv[], recap_context* ctx);
void load_gitignore(exclude_patterns_ctx* exclude_ctx, const char* gitignore_filename);
void clear_recap_output_files(const char* target_dir);


#ifdef _WIN32
    #include <stdlib.h>
    #define realpath(N, R) _fullpath((R), (N), PATH_MAX)
#else
    char* realpath(const char* restrict path, char* restrict resolved_path);
#endif




void traverse_directory(const char* base_path, int depth, recap_context* ctx);
void print_indent(int depth, FILE* output);
void write_file_content_inline(const char* filepath, int depth, recap_context* ctx);
void get_relative_path(const char* full_path, char* rel_path, size_t size);
int is_excluded(const char* rel_path, const recap_context* ctx);
int print_path_hierarchy(const char* path, recap_context* ctx);
int start_traversal(const char* initial_path, recap_context* ctx);

int is_text_file(const char* full_path);
void normalize_path(char* path);
int should_show_content(const char* filename, const char* full_path, const content_ctx* content_context);

int generate_output_filename(output_ctx* output_context);
void free_content_specifiers(content_ctx* content_context);

char* upload_to_gist(const char* filepath, const char* github_token);

#endif