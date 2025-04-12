#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const char* RECAP_VERSION = "1.0.0";

int main(int argc, char* argv[]) {
    recap_context ctx = { 0 };
    ctx.version = RECAP_VERSION;
    ctx.output_stream = NULL;

    int result = 0;
    char* allocated_output_filename = NULL;

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Error: Failed to initialize libcurl.\n");
        return 1;
    }

    parse_arguments(argc, argv, &ctx);


    if (ctx.output.output_name[0] == '\0' && ctx.output.output_dir[0] == '\0') {
        ctx.output.use_stdout = 1;
        ctx.output_stream = stdout;
        ctx.output.relative_output_path[0] = '\0';
    }
    else {
        ctx.output.use_stdout = 0;
        if (generate_output_filename(&ctx.output) != 0) {
            result = 1;
            goto cleanup;
        }


        ctx.output_stream = fopen(ctx.output.calculated_output_path, "w");
        if (!ctx.output_stream) {
            perror("fopen output file");
            fprintf(stderr, "Error: Could not open output file: %s\n", ctx.output.calculated_output_path);
            result = 1;
            goto cleanup;
        }

        allocated_output_filename = ctx.output.calculated_output_path;
    }



    int processed_something = 0;
    for (int i = 0; i < ctx.includes.include_count; i++) {
        start_traversal(ctx.includes.include_patterns[i], &ctx);
    }

    if (ctx.output_stream) {
        fflush(ctx.output_stream);
    }

    if (!ctx.output.use_stdout && ctx.output_stream) {
        long output_size = ftell(ctx.output_stream);
        if (output_size > 0) {
            processed_something = 1;
        }
        else if (output_size == 0) {
            processed_something = 0;
            fprintf(stderr, "No content written to output file (all includes might have been excluded or empty).\n");
        }
        else {
            perror("ftell output file");
            processed_something = 0;
        }
    }
    else if (ctx.output.use_stdout) {
        if (ctx.includes.include_count > 0) processed_something = 1;
    }


    if (!ctx.output.use_stdout && ctx.output_stream) {
        fclose(ctx.output_stream);
        ctx.output_stream = NULL;
    }

    if (processed_something) {
        if (ctx.gist_api_key != NULL) {
            if (!ctx.gist_api_key || ctx.gist_api_key[0] == '\0') {
                fprintf(stderr, "Error: Gist upload requested (--paste), but no API key provided via argument or GITHUB_API_KEY environment variable.\n");
                fprintf(stderr, "Output saved locally to %s\n", allocated_output_filename);
                result = 1;
            }
            else if (ctx.output.use_stdout) {
                fprintf(stderr, "Warning: Cannot upload to Gist when outputting to stdout. Output was printed above.\n");
            }
            else {
                printf("Uploading to Gist...\n");
                char* gist_url = upload_to_gist(allocated_output_filename, ctx.gist_api_key);

                if (gist_url) {
                    printf("Output uploaded to: %s\n", gist_url);
                    free(gist_url);
                    if (remove(allocated_output_filename) != 0) {
                        perror("remove local output file after Gist upload");
                        fprintf(stderr, "Warning: Failed to remove local file %s after Gist upload.\n", allocated_output_filename);
                    }
                    allocated_output_filename = NULL;
                }
                else {
                    fprintf(stderr, "Failed to upload to Gist. Output saved locally to %s\n", allocated_output_filename);
                    result = 1;
                }
            }
        }
        else {
            if (!ctx.output.use_stdout) {
                printf("Output written to %s\n", allocated_output_filename);
            }
        }
    }
    else {
        if (!ctx.output.use_stdout && allocated_output_filename) {
            fprintf(stderr, "No files processed or written. Removing empty output file: %s\n", allocated_output_filename);
            remove(allocated_output_filename);
            allocated_output_filename = NULL;
        }
        else if (ctx.output.use_stdout) {
            fprintf(stderr, "No files processed or written to stdout.\n");
        }
    }


cleanup:
    if (!ctx.output.use_stdout && ctx.output_stream) {
        fclose(ctx.output_stream);
        ctx.output_stream = NULL;
    }
    free_content_specifiers(&ctx.content);
    curl_global_cleanup();
    return result;
}