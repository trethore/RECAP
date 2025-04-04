// this file add some useful functions for traversing directories and files

#include "ctf.h"

void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(output, "  ");
    }
}

void write_file_content_inline(const char *filepath, int depth) {
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        print_indent(depth);
        fprintf(output, "%s", line);
    }

    fseek(f, -1, SEEK_END);
    if (fgetc(f) != '\n') {
        fprintf(output, "\n");
    }
    fclose(f);
}

void compute_relative_path(const char *base_path, const char *entry_name, char *rel_path, size_t size) {

    if (strcmp(base_path, ".") == 0) {
        snprintf(rel_path, size, "%s", entry_name);
    } else {
        snprintf(rel_path, size, "%s/%s", base_path, entry_name);
    }
    normalize_path(rel_path);
}

int is_excluded_path(const char *rel_path) {
    char normalized_rel_path[MAX_PATH_SIZE];
    strncpy(normalized_rel_path, rel_path, MAX_PATH_SIZE -1);
    normalized_rel_path[MAX_PATH_SIZE - 1] = '\0';
    normalize_path(normalized_rel_path);

    for (int i = 0; i < rmf_count; i++) {
        char normalized_rmf[MAX_PATH_SIZE];
        strncpy(normalized_rmf, rmf_dirs[i], MAX_PATH_SIZE - 1);
        normalized_rmf[MAX_PATH_SIZE - 1] = '\0';
        normalize_path(normalized_rmf);


        size_t rmf_len = strlen(normalized_rmf);
        if (strncmp(normalized_rel_path, normalized_rmf, rmf_len) == 0) {

            if (normalized_rel_path[rmf_len] == '\0' || normalized_rel_path[rmf_len] == '/') {
                return 1;
            }
        }

        if (normalized_rmf[rmf_len - 1] == '/') {
             if (strncmp(normalized_rel_path, normalized_rmf, rmf_len -1) == 0 && normalized_rel_path[rmf_len - 1] == '/') {
                 return 1;
             }
        }
    }
    return 0;
}

void traverse_directory(const char *base_path, int depth) {
    DIR *dir = opendir(base_path);
    if (!dir) {

        struct stat st;
        if (stat(base_path, &st) == 0 && S_ISREG(st.st_mode)) {

        } else {
            perror("opendir");
        }
        return;
    }

    struct dirent *entry;
    char current_rel_path[MAX_PATH_SIZE];

    while ((entry = readdir(dir)) != NULL) {
        char full_path[MAX_PATH_SIZE];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;


        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        normalize_path(full_path);


        compute_relative_path(base_path, entry->d_name, current_rel_path, sizeof(current_rel_path));


        if (is_excluded_path(current_rel_path)) {
            continue;
        }

        char *output_filename_ptr = get_output_filename();
        if (output_filename_ptr) {
            char normalized_output_path[MAX_PATH_SIZE];
            const char *output_basename = strrchr(output_filename_ptr, '/');
            output_basename = output_basename ? output_basename + 1 : output_filename_ptr;


            int required_len = snprintf(NULL, 0, "%s/%s", output_dir, output_basename);

            if (required_len >= 0 && required_len < MAX_PATH_SIZE) {

                snprintf(normalized_output_path, MAX_PATH_SIZE, "%s/%s", output_dir, output_basename);
                normalize_path(normalized_output_path);
                if (strcmp(full_path, normalized_output_path) == 0) {
                    free(output_filename_ptr);
                    continue;
                }
            } else if (required_len >= MAX_PATH_SIZE) {
                 fprintf(stderr, "Warning: Path for output file comparison is too long: %s/%s\n", output_dir, output_basename);


            }


            free(output_filename_ptr);
        }


        struct stat st;
        if (stat(full_path, &st) != 0) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            print_indent(depth);
            fprintf(output, "%s/\n", entry->d_name);
            traverse_directory(full_path, depth + 1);
        } else {

            if (strcmp(entry->d_name, "ctf") == 0 || strcmp(entry->d_name, "ctf.exe") == 0) continue;

            print_indent(depth);
            if (should_show_content(entry->d_name)) {
                fprintf(output, "%s:\n", entry->d_name);
                write_file_content_inline(full_path, depth + 1);
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
    normalize_path(copy);


    if (strcmp(copy, ".") == 0) {
        return 0;
    }


    struct stat st;
    if (stat(path, &st) != 0) {

         return 0;
    }


    char *last_slash = strrchr(copy, '/');
    if (S_ISREG(st.st_mode) && last_slash != NULL) {
        *last_slash = '\0';

        if (strcmp(copy, ".") == 0) return 0;
    } else if (S_ISREG(st.st_mode)) {

        return 0;
    }

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
