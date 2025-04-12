#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <sys/stat.h>

FILE* output = NULL;
char* gist_api_key = NULL;
const char* RECAP_VERSION = "1.0.0";

int main(int argc, char* argv[]) {
    include_patterns_ctx include_ctx = { 0 };
    exclude_patterns_ctx exclude_ctx = { 0 };
    output_ctx output_context = { 0 };
    content_ctx content_context = { 0 };
    curl_global_init(CURL_GLOBAL_ALL);

    parse_arguments(argc, argv, &include_ctx, &exclude_ctx, &output_context, &content_context, &gist_api_key, RECAP_VERSION);

    char* filename = NULL;
    int use_stdout = 0;
    if (output_context.output_name[0] == '\0' && output_context.output_dir[0] == '\0') {
        // Neither --output nor --output-dir specified: use stdout
        output = stdout;
        use_stdout = 1;
    }
    else {
        filename = get_output_filename(output, &output_context);
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
    }

    int traversed_something = 0;
    for (int i = 0; i < include_ctx.include_count; i++) {
        char current_path_normalized[MAX_PATH_SIZE];
        strncpy(current_path_normalized, include_ctx.include_patterns[i], MAX_PATH_SIZE - 1);
        current_path_normalized[MAX_PATH_SIZE - 1] = '\0';
        normalize_path(current_path_normalized);

        char rel_path[MAX_PATH_SIZE];
        get_relative_path(current_path_normalized, rel_path, sizeof(rel_path));

        if (is_excluded(output, rel_path, &exclude_ctx, &output_context)) {
            if (strcmp(rel_path, ".") == 0) {
                fprintf(stderr, "Warning: Root include path '.' is excluded, no files will be processed unless other includes are specified.\n");
            }
            continue;
        }

        struct stat st;
        if (stat(current_path_normalized, &st) != 0) {
            perror("stat include path");
            fprintf(stderr, "Warning: Could not stat include path: %s. Skipping.\n", include_ctx.include_patterns[i]);
            continue;
        }

        int depth = print_path_hierarchy(current_path_normalized, output);

        if (S_ISDIR(st.st_mode)) {
            traverse_directory(current_path_normalized, depth, output, &exclude_ctx, &output_context, &content_context);
            traversed_something = 1;
        }
        else if (S_ISREG(st.st_mode)) {
            const char* filename_part = strrchr(current_path_normalized, '/');
            filename_part = filename_part ? filename_part + 1 : current_path_normalized;

            print_indent(depth, output);
            if (should_show_content(filename_part, current_path_normalized, &content_context)) {
                fprintf(output, "%s:\n", filename_part);
                write_file_content_inline(current_path_normalized, depth + 1, output, &content_context);
            }
            else {
                fprintf(output, "%s\n", filename_part);
            }
            traversed_something = 1;
        }
    }

    if (!use_stdout && output) {
        fclose(output);
    }

    if (traversed_something) {
        if (gist_api_key) {
            printf("Uploading to Gist...\n");
            char* gist_url = upload_to_gist(filename, gist_api_key);
            if (gist_url && strlen(gist_url) > 0) {
                printf("Output uploaded to: %s\n", gist_url);
                free(gist_url);
                if (!use_stdout && filename) {
                    remove(filename);
                }
            }
            else {
                if (gist_url) free(gist_url);
                fprintf(stderr, "Failed to upload to Gist or get URL. Output saved locally to %s\n", filename);
            }
        }
        else {
            if (!use_stdout) {
                printf("Output written to %s\n", filename);
            }
        }
    }
    else {
        fprintf(stderr, "No files or directories processed (maybe all were excluded or includes were invalid?). Output file not generated.\n");
        remove(filename);
    }
    for (int i = 0; i < content_context.content_specifier_count; i++) {
        free(content_context.content_specifiers[i]);
    }
    if (!use_stdout && filename) {
        free(filename);
    }
    curl_global_cleanup();
    return 0;
}
