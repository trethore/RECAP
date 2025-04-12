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

#define MAX_PATH_SIZE 1024
#define MAX_PATTERNS 1024
#define MAX_CONTENT_SPECIFIERS 1024
#define MAX_GITIGNORE_ENTRIES 1024

typedef struct {
    char* include_patterns[MAX_PATTERNS];
    int include_count;
} include_patterns_ctx;

typedef struct {
    char* exclude_patterns[MAX_PATTERNS];
    int exclude_count;
    char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];
} exclude_patterns_ctx;

typedef struct {
    char output_dir[MAX_PATH_SIZE];
    char output_name[MAX_PATH_SIZE];
} output_ctx;

typedef struct {
    char* content_specifiers[MAX_CONTENT_SPECIFIERS];
    int content_specifier_count;
    int content_flag;
} content_ctx;


void parse_arguments(int argc, char* argv[], include_patterns_ctx* include_ctx, exclude_patterns_ctx* exclude_ctx, output_ctx* output_context, content_ctx* content_context, char** gist_api_key);
void load_gitignore(exclude_patterns_ctx* exclude_ctx, const char* gitignore_filename);
void clear_recap_output_files(const char* target_dir);

void traverse_directory(const char* base_path, int depth, FILE* output, exclude_patterns_ctx* exclude_ctx, output_ctx* output_context, content_ctx* content_context);
void print_indent(int depth, FILE* output);
void write_file_content_inline(const char* filepath, int depth, FILE* output, content_ctx* content_context);
void get_relative_path(const char* full_path, char* rel_path, size_t size);
int is_excluded(FILE* output, const char* rel_path, exclude_patterns_ctx* exclude_ctx, output_ctx* output_context);
int print_path_hierarchy(const char* path, FILE* output);

int is_text_file(const char* full_path);
void normalize_path(char* path);
int should_show_content(const char* filename, const char* full_path, content_ctx* content_context);
char* get_output_filename(FILE* output, output_ctx* output_context);
char* upload_to_gist(const char* filepath, const char* api_key);

#endif
