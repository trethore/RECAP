#ifndef CTF_H
#define CTF_H

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

extern char *include_patterns[MAX_PATTERNS];
extern int include_count;

extern char *exclude_patterns[MAX_PATTERNS];
extern int exclude_count;
extern char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE]; 

extern char *content_specifiers[MAX_CONTENT_SPECIFIERS];
extern int content_specifier_count;
extern int content_flag;

extern int git_flag;
extern FILE *output;

extern char output_dir[MAX_PATH_SIZE];
extern char output_name[MAX_PATH_SIZE];
extern char *gist_api_key; 

extern const char *compiled_exts[]; 

void parse_arguments(int argc, char *argv[]);
void load_gitignore(void);
void clear_ctf_output_files(void);

void traverse_directory(const char *base_path, int depth);
void print_indent(int depth);
void write_file_content_inline(const char *filepath, int depth);
void get_relative_path(const char *full_path, char *rel_path, size_t size); 
int is_excluded(const char *rel_path); 
int print_path_hierarchy(const char *path);

int is_text_file(const char *full_path); 
void normalize_path(char *path);
int should_show_content(const char *filename, const char *full_path); 
char *get_output_filename(void);
char *upload_to_gist(const char *filepath, const char *api_key);

#endif
