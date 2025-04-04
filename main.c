#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_SIZE 1024
#define MAX_TYPES 1024
#define MAX_ADDF 1024
#define MAX_RMF 1024

static char *file_types[MAX_TYPES];
static int file_type_count = 0;

static char *addf_dirs[MAX_ADDF];
static int addf_count = 0;

static char *rmf_dirs[MAX_RMF];
static int rmf_count = 0;

static int include_content = 0;
static FILE *output = NULL;

// List of compiled file extensions, add more if needed
const char *compiled_exts[] = {"exe", "bin", "o", "obj", "class", NULL};

int is_compiled_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    ext++;
    for (int i = 0; compiled_exts[i] != NULL; i++) {
        if (strcmp(ext, compiled_exts[i]) == 0)
            return 1;
    }
    return 0;
}

int is_readable_text_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    ext++;
    for (int i = 0; compiled_exts[i] != NULL; i++) {
        if (strcmp(ext, compiled_exts[i]) == 0)
            return 0;
    }
    return 1;
}

void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(output, "  ");
    }
}

void write_file_content_inline(const char *filepath, int depth) {
    if (!is_readable_text_file(filepath)) return;
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        print_indent(depth);
        fprintf(output, "%s", line);
    }
    fprintf(output, "\n");
    fclose(f);
}

void compute_relative_path(const char *base_path, const char *entry_name, char *rel_path, size_t size) {
    if (strcmp(base_path, ".") == 0)
        snprintf(rel_path, size, "%s", entry_name);
    else
        snprintf(rel_path, size, "%s/%s", base_path, entry_name);
}

int is_excluded_path(const char *rel_path) {
    for (int i = 0; i < rmf_count; i++) {
        if (strcmp(rel_path, rmf_dirs[i]) == 0)
            return 1;
    }
    return 0;
}

void traverse_directory(const char *base_path, int depth) {
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    char rel_path[MAX_PATH_SIZE];

    while ((entry = readdir(dir)) != NULL) {
        char path[MAX_PATH_SIZE];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        compute_relative_path(base_path, entry->d_name, rel_path, sizeof(rel_path));

        struct stat st;
        if (stat(path, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) {
            if (rmf_count > 0 && is_excluded_path(rel_path))
                continue;

            print_indent(depth);
            fprintf(output, "%s/\n", entry->d_name);
            traverse_directory(path, depth + 1);
        } else {
            if (strncmp(entry->d_name, "ctf-output", strlen("ctf-output")) == 0)
                continue;

            print_indent(depth);
            if (include_content && is_readable_text_file(entry->d_name)) {
                fprintf(output, "%s:\n", entry->d_name);
                write_file_content_inline(path, depth + 1);
            } else {
                fprintf(output, "%s\n", entry->d_name);
            }
        }
    }
    closedir(dir);
}

int print_path_hierarchy(const char *path) {
    char copy[MAX_PATH_SIZE];
    strncpy(copy, path, MAX_PATH_SIZE);
    copy[MAX_PATH_SIZE - 1] = '\0';

    int depth = 0;
    char *token = strtok(copy, "/");
    while (token != NULL) {
        print_indent(depth);
        fprintf(output, "%s/\n", token);
        depth++;
        token = strtok(NULL, "/");
    }
    return depth;
}

char *get_output_filename(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char *filename = malloc(64);
    if (filename != NULL)
        strftime(filename, 64, "ctf-output-%Y%m%d-%H%M%S", t);
    return filename;
}

void clear_ctf_output_files(void) {
    DIR *dir = opendir(".");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "ctf-output", strlen("ctf-output")) == 0)
            remove(entry->d_name);
    }
    closedir(dir);
}

void parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--clear") == 0) {
            clear_ctf_output_files();
            printf("Cleared ctf-output files.\n");
            exit(0);
        } else if (strcmp(argv[i], "--content") == 0) {
            include_content = 1;
        } else if (strcmp(argv[i], "--addf") == 0 || strcmp(argv[i], "-addf") == 0) {
            i++;
            while (i < argc && argv[i][0] != '-') {
                if (addf_count < MAX_ADDF)
                    addf_dirs[addf_count++] = argv[i];
                else {
                    fprintf(stderr, "Too many addf directories specified. Max allowed is %d\n", MAX_ADDF);
                    exit(1);
                }
                i++;
            }
            i--;
        } else if (strcmp(argv[i], "--rmf") == 0 || strcmp(argv[i], "-rmf") == 0) {
            i++;
            while (i < argc && argv[i][0] != '-') {
                if (rmf_count < MAX_RMF)
                    rmf_dirs[rmf_count++] = argv[i];
                else {
                    fprintf(stderr, "Too many rmf directories specified. Max allowed is %d\n", MAX_RMF);
                    exit(1);
                }
                i++;
            }
            i--;
        } else if (strcmp(argv[i], "--type") == 0) {
            i++;
            while (i < argc && argv[i][0] != '-') {
                if (file_type_count < MAX_TYPES)
                    file_types[file_type_count++] = argv[i];
                else {
                    fprintf(stderr, "Too many file types specified. Max allowed is %d\n", MAX_TYPES);
                    exit(1);
                }
                i++;
            }
            i--;
        }
    }
    if (addf_count == 0)
        addf_dirs[addf_count++] = ".";
}

int main(int argc, char *argv[]) {
    parse_arguments(argc, argv);

    char *filename = get_output_filename();
    if (!filename) {
        fprintf(stderr, "Failed to allocate output filename.\n");
        return 1;
    }
    output = fopen(filename, "w");
    if (!output) {
        perror("fopen");
        free(filename);
        return 1;
    }

    int traversed = 0;
    for (int i = 0; i < addf_count; i++) {
        int skip = 0;
        for (int j = 0; j < rmf_count; j++) {
            if (strcmp(addf_dirs[i], rmf_dirs[j]) == 0) {
                skip = 1;
                break;
            }
        }
        if (skip)
            continue;

        int depth = print_path_hierarchy(addf_dirs[i]);
        traverse_directory(addf_dirs[i], depth);
        traversed = 1;
    }

    fclose(output);
    if (traversed)
        printf("Output written to %s\n", filename);
    else
        printf("No directories traversed.\n");

    free(filename);
    return 0;
}
