#ifndef CTF_H
#define CTF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h> // Include for isspace

// --- Constants ---
#define MAX_PATH_SIZE 1024
#define MAX_CONTENT_TYPES 1024
#define MAX_ADDF 1024
#define MAX_RMF 1024

// --- Global Variables (Declarations) ---
extern char *content_types[MAX_CONTENT_TYPES];
extern int content_type_count;

extern char *addf_dirs[MAX_ADDF];
extern int addf_count;

extern char *rmf_dirs[MAX_RMF];
extern int rmf_count;
extern char gitignore_entries[MAX_RMF][MAX_PATH_SIZE]; // Storage for gitignore lines

extern int content_flag;
extern int git_flag;
extern FILE *output;

extern char output_dir[MAX_PATH_SIZE];
extern char output_name[MAX_PATH_SIZE];

extern const char *compiled_exts[];
extern const char *content_exceptions[];

// --- Function Prototypes ---

// args.c
void parse_arguments(int argc, char *argv[]);
void load_gitignore(void);
void clear_ctf_output_files(void);

// traverse.c
void traverse_directory(const char *base_path, int depth);
void print_indent(int depth);
void write_file_content_inline(const char *filepath, int depth);
void compute_relative_path(const char *base_path, const char *entry_name, char *rel_path, size_t size);
int is_excluded_path(const char *rel_path);
int print_path_hierarchy(const char *path);

// utils.c
int is_compiled_file(const char *filename);
int is_text_file(const char *filename);
void normalize_path(char *path);
int should_show_content(const char *filename);
char *get_output_filename(void);

#endif // CTF_H
