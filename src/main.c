#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

const char* RECAP_VERSION = "2.0.1";

static void pcre2_code_ptr_cleanup(void* data) {
    pcre2_code** code = data;
    if (code && *code) {
        pcre2_code_free(*code);
        *code = NULL;
    }
}

static int setup_output_stream(recap_context* ctx) {
    int is_output_specified = (ctx->output.output_name[0] != '\0' || ctx->output.output_dir[0] != '\0');

    if (!is_output_specified && !ctx->copy_to_clipboard) {
        ctx->output.use_stdout = 1;
        ctx->output_stream = stdout;
    }
    else {
        if (ctx->copy_to_clipboard && !is_output_specified) {
            ctx->output.is_temp_file = 1;
        }
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

    int clipboard_success = 0;
    if (ctx->copy_to_clipboard && !ctx->output.use_stdout) {
        if (copy_file_content_to_clipboard(ctx->output.calculated_output_path) == 0) {
            clipboard_success = 1;
        }
        else {
            fprintf(stderr, "Error: Failed to copy output to clipboard.\n");
        }
    }

    char* gist_url = NULL;
    if (ctx->gist_api_key != NULL) {
        if (ctx->gist_api_key[0] == '\0') {
            fprintf(stderr, "Error: Gist upload requested, but no API key found.\n");
        }
        else if (ctx->output.use_stdout) {
            fprintf(stderr, "Warning: Cannot upload to Gist when outputting to stdout.\n");
        }
        else {
            printf("Uploading to Gist...\n");
            gist_url = upload_to_gist(ctx->output.calculated_output_path, ctx->gist_api_key);
        }
    }

    if (clipboard_success) {
        printf("Output copied to clipboard.\n");
    }

    if (gist_url) {
        printf("Output uploaded to: %s\n", gist_url);
        free(gist_url);
        remove(ctx->output.calculated_output_path);
    }
    else if (ctx->output.is_temp_file) {
        remove(ctx->output.calculated_output_path);
    }
    else if (!ctx->output.use_stdout) {
        if (ctx->gist_api_key != NULL) {
            fprintf(stderr, "Failed to upload to Gist. Output saved locally to %s\n", ctx->output.calculated_output_path);
        }
        else {
            if (!ctx->copy_to_clipboard) {
                printf("Output written to %s\n", ctx->output.calculated_output_path);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    recap_context ctx = {0};
    int result = 0;
    int curl_initialized = 0;

    memlst_init(&ctx.cleanup);
    if (!memlst_add(&ctx.cleanup, (dtor_fn)free_regex_ctx, &ctx.include_filters) ||
        !memlst_add(&ctx.cleanup, (dtor_fn)free_regex_ctx, &ctx.exclude_filters) ||
        !memlst_add(&ctx.cleanup, (dtor_fn)free_regex_ctx, &ctx.content_include_filters) ||
        !memlst_add(&ctx.cleanup, (dtor_fn)free_regex_ctx, &ctx.content_exclude_filters) ||
        !memlst_add(&ctx.cleanup, pcre2_code_ptr_cleanup, &ctx.strip_regex) ||
        !memlst_add(&ctx.cleanup, (dtor_fn)path_list_free, &ctx.matched_files)) {
        fprintf(stderr, "Error: Failed to register cleanup handlers.\n");
        result = 1;
        goto cleanup;
    }

    ctx.version = RECAP_VERSION;

    if (!getcwd(ctx.cwd, sizeof(ctx.cwd))) {
        perror("Failed to get current working directory");
        return 1;
    }
    normalize_path(ctx.cwd);

    parse_arguments(argc, argv, &ctx);

    if (ctx.gist_api_key && ctx.gist_api_key[0] != '\0') {
        if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
            fprintf(stderr, "Error: Failed to initialize libcurl.\n");
            result = 1;
            goto cleanup;
        }
        curl_initialized = 1;
    }

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
    memlst_destroy(&ctx.cleanup);
    if (curl_initialized) {
        curl_global_cleanup();
    }
    return result;
}
