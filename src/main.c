#include "ctf.h"
#include <curl/curl.h>
#include <sys/stat.h> // Include for stat

char* include_patterns[MAX_PATTERNS];
int include_count = 0;

char* exclude_patterns[MAX_PATTERNS];
int exclude_count = 0;
char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];

char* content_specifiers[MAX_CONTENT_SPECIFIERS];
int content_specifier_count = 0;
int content_flag = 0;

int git_flag = 0;
FILE* output = NULL;

char output_dir[MAX_PATH_SIZE] = ".";
char output_name[MAX_PATH_SIZE] = "";

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);
    parse_arguments(argc, argv);

    if (git_flag) {
        load_gitignore();
    }

    char* filename = get_output_filename();
    if (!filename) {
        fprintf(stderr, "Failed to generate output filename.\n");
        curl_global_cleanup();
        return 1;
    }
    output = fopen(filename, "w");
    if (!output) {
        perror("fopen output file");
        free(filename);
        curl_global_cleanup();
        return 1;
    }

    int traversed_something = 0;
    for (int i = 0; i < include_count; i++) {
        char current_path_normalized[MAX_PATH_SIZE];
        strncpy(current_path_normalized, include_patterns[i], MAX_PATH_SIZE - 1);
        current_path_normalized[MAX_PATH_SIZE - 1] = '\0';
        normalize_path(current_path_normalized);

        char rel_path[MAX_PATH_SIZE];
        get_relative_path(current_path_normalized, rel_path, sizeof(rel_path));

        if (is_excluded(rel_path)) {
            if (strcmp(rel_path, ".") == 0) {
                fprintf(stderr, "Warning: Root include path '.' is excluded, no files will be processed unless other includes are specified.\n");
            }
            continue;
        }

        struct stat st;
        if (stat(current_path_normalized, &st) != 0) {
            perror("stat include path");
            fprintf(stderr, "Warning: Could not stat include path: %s. Skipping.\n", include_patterns[i]);
            continue;
        }

        int depth = print_path_hierarchy(current_path_normalized);

        if (S_ISDIR(st.st_mode)) {
            traverse_directory(current_path_normalized, depth);
            traversed_something = 1;
        }
        else if (S_ISREG(st.st_mode)) {
            const char* filename_part = strrchr(current_path_normalized, '/');
            filename_part = filename_part ? filename_part + 1 : current_path_normalized;

            print_indent(depth);
            if (should_show_content(filename_part, current_path_normalized)) {
                fprintf(output, "%s:\n", filename_part);
                write_file_content_inline(current_path_normalized, depth + 1);
            }
            else {
                fprintf(output, "%s\n", filename_part);
            }
            traversed_something = 1;
        }
    }

    fclose(output);

    if (traversed_something) {
        if (gist_api_key) {
            printf("Uploading to Gist...\n");
            char* gist_url = upload_to_gist(filename, gist_api_key);
            if (gist_url && strlen(gist_url) > 0) {
                printf("Output uploaded to: %s\n", gist_url);
                free(gist_url);
                remove(filename);
            }
            else {
                if (gist_url) free(gist_url);
                fprintf(stderr, "Failed to upload to Gist or get URL. Output saved locally to %s\n", filename);
            }
        }
        else {
            printf("Output written to %s\n", filename);
        }
    }
    else {
        fprintf(stderr, "No files or directories processed (maybe all were excluded or includes were invalid?). Output file not generated.\n");
        remove(filename);
    }

    free(filename);
    curl_global_cleanup();
    return 0;
}
