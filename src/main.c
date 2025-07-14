#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

const char* RECAP_VERSION = "2.0.0";

static void path_list_free(path_list* list) {
    if (list) {
        for (size_t i = 0; i < list->count; i++) {
            free(list->items[i]);
        }
        free(list->items);
        list->items = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

static int setup_output_stream(recap_context* ctx) {
    if (ctx->output.output_name[0] == '\0' && ctx->output.output_dir[0] == '\0') {
        ctx->output.use_stdout = 1;
        ctx->output_stream = stdout;
    }
    else {
        ctx->output.use_stdout = 0;
        if (generate_output_filename(&ctx->output) != 0) {
            return 1;
        }
        ctx->output_stream = fopen(ctx->output.calculated_output_path, "w");
        if (!ctx->output_stream) {
            perror("fopen output file");
            fprintf(stderr, "Error: Could not open output file: %s\n", ctx->output.calculated_output_path);
            return 1;
        }
    }
    return 0;
}

static void handle_post_processing(recap_context* ctx) {
    if (ctx->matched_files.count == 0) {
        if (!ctx->output.use_stdout) {
            fprintf(stderr, "Info: No files matched criteria. Removing empty output file: %s\n", ctx->output.calculated_output_path);
            remove(ctx->output.calculated_output_path);
        }
        else {
            fprintf(stderr, "Info: No files matched the specified criteria.\n");
        }
        return;
    }

    if (ctx->gist_api_key != NULL) {
        if (ctx->gist_api_key[0] == '\0') {
            fprintf(stderr, "Error: Gist upload requested, but no API key found.\n");
            if (!ctx->output.use_stdout) {
                fprintf(stderr, "Output saved locally to %s\n", ctx->output.calculated_output_path);
            }
        }
        else if (ctx->output.use_stdout) {
            fprintf(stderr, "Warning: Cannot upload to Gist when outputting to stdout.\n");
        }
        else {
            printf("Uploading to Gist...\n");
            char* gist_url = upload_to_gist(ctx->output.calculated_output_path, ctx->gist_api_key);
            if (gist_url) {
                printf("Output uploaded to: %s\n", gist_url);
                free(gist_url);
                remove(ctx->output.calculated_output_path);
            }
            else {
                fprintf(stderr, "Failed to upload to Gist. Output saved locally to %s\n", ctx->output.calculated_output_path);
            }
        }
    }
    else if (!ctx->output.use_stdout) {
        printf("Output written to %s\n", ctx->output.calculated_output_path);
    }
}

int main(int argc, char* argv[]) {
    recap_context ctx = { 0 };
    int result = 0;

    ctx.version = RECAP_VERSION;

    if (!getcwd(ctx.cwd, sizeof(ctx.cwd))) {
        perror("Failed to get current working directory");
        return 1;
    }
    normalize_path(ctx.cwd);

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Error: Failed to initialize libcurl.\n");
        return 1;
    }

    parse_arguments(argc, argv, &ctx);

    if (setup_output_stream(&ctx) != 0) {
        result = 1;
        goto cleanup;
    }

    start_traversal(&ctx);

    if (ctx.output_stream && ctx.output_stream != stdout) {
        fclose(ctx.output_stream);
        ctx.output_stream = NULL;
    }

    handle_post_processing(&ctx);

cleanup:
    if (ctx.output_stream && ctx.output_stream != stdout) {
        fclose(ctx.output_stream);
    }
    path_list_free(&ctx.matched_files);
    free_regex_ctx(&ctx.include_filters);
    free_regex_ctx(&ctx.exclude_filters);
    free_regex_ctx(&ctx.content_include_filters);
    free_regex_ctx(&ctx.content_exclude_filters);
    if (ctx.strip_regex_is_set) {
        pcre2_code_free(ctx.strip_regex);
    }
    for (int i = 0; i < ctx.scoped_strip_rule_count; i++) {
        pcre2_code_free(ctx.scoped_strip_rules[i].path_regex);
        pcre2_code_free(ctx.scoped_strip_rules[i].strip_regex);
    }
    curl_global_cleanup();
    return result;
}