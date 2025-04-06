#include "ctf.h"
#include <curl/curl.h>
#include <sys/stat.h> // Include for stat

// Use new variable names from ctf.h
char *include_patterns[MAX_PATTERNS];
int include_count = 0;

char *exclude_patterns[MAX_PATTERNS];
int exclude_count = 0;
char gitignore_entries[MAX_GITIGNORE_ENTRIES][MAX_PATH_SIZE];

char *content_specifiers[MAX_CONTENT_SPECIFIERS];
int content_specifier_count = 0;
int content_flag = 0;

int git_flag = 0;
FILE *output = NULL;

char output_dir[MAX_PATH_SIZE] = ".";
char output_name[MAX_PATH_SIZE] = "";
// removed duplicate gist_api_key definition

// Keep compiled_exts for now, maybe used by is_text_file implicitly
const char *compiled_exts[] = {"exe", "bin", "o", "obj", "class", "a", "so", "dll", "lib", NULL};

int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);
    parse_arguments(argc, argv); // Parses args and sets flags/arrays

    // Default include is now handled in parse_arguments
    // if (include_count == 0) {
    //     include_patterns[include_count++] = ".";
    // }

    // Load gitignore after parsing CLI args if flag is set
    if (git_flag) {
        load_gitignore();
    }

    char *filename = get_output_filename();
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
        strncpy(current_path_normalized, include_patterns[i], MAX_PATH_SIZE -1);
        current_path_normalized[MAX_PATH_SIZE-1] = '\0';
        normalize_path(current_path_normalized);

        char rel_path[MAX_PATH_SIZE];
        get_relative_path(current_path_normalized, rel_path, sizeof(rel_path));

        // Check exclusion using the relative path
        if (is_excluded(rel_path)) {
             // Provide context if the root is excluded
             if (strcmp(rel_path, ".") == 0) {
                  fprintf(stderr, "Warning: Root include path '.' is excluded, no files will be processed unless other includes are specified.\n");
             }
            continue;
        }

        struct stat st;
        if (stat(current_path_normalized, &st) != 0) {
            // If stat fails, it might be an invalid path or permissions issue.
            // We already normalized, so it's unlikely a format issue.
            perror("stat include path");
            fprintf(stderr, "Warning: Could not stat include path: %s. Skipping.\n", include_patterns[i]);
            continue;
        }

        // Print hierarchy for the base path/file before traversing/handling
        int depth = print_path_hierarchy(current_path_normalized);

        if (S_ISDIR(st.st_mode)) {
            // Pass the normalized path to traverse
            traverse_directory(current_path_normalized, depth);
            traversed_something = 1;
        } else if (S_ISREG(st.st_mode)) {
            // Directly handle the file included
            const char *filename_part = strrchr(current_path_normalized, '/');
            filename_part = filename_part ? filename_part + 1 : current_path_normalized;

            // Depth is correctly calculated by print_path_hierarchy
            print_indent(depth);
            if (should_show_content(filename_part, current_path_normalized)) {
                 fprintf(output, "%s:\n", filename_part);
                 write_file_content_inline(current_path_normalized, depth + 1);
             } else {
                 fprintf(output, "%s\n", filename_part);
             }
            traversed_something = 1;
        }
        // Else: Ignore other file types specified in --include
    }

    fclose(output);

    if (traversed_something) {
        if (gist_api_key) {
            printf("Uploading to Gist...\n");
            char *gist_url = upload_to_gist(filename, gist_api_key);
            if (gist_url && strlen(gist_url) > 0) { // Check if URL is not NULL and not empty
                printf("Output uploaded to: %s\n", gist_url);
                free(gist_url);
                remove(filename); // Remove local file on successful upload
            } else {
                if (gist_url) free(gist_url); // Free empty string if returned (e.g., empty file skipped)
                fprintf(stderr, "Failed to upload to Gist or get URL. Output saved locally to %s\n", filename);
                // Keep local file if upload failed or was skipped
            }
        } else {
            printf("Output written to %s\n", filename);
        }
    } else {
        fprintf(stderr, "No files or directories processed (maybe all were excluded or includes were invalid?). Output file not generated.\n");
        remove(filename); // Remove the empty output file
    }

    free(filename);
    curl_global_cleanup();
    return 0;
}
