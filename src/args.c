#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>

static int add_regex(regex_ctx* ctx, const char* pattern) {
    if (ctx->count >= MAX_PATTERNS) {
        fprintf(stderr, "Error: Too many regex patterns. Max allowed is %d\n", MAX_PATTERNS);
        return -1;
    }
    int ret = regcomp(&ctx->compiled[ctx->count], pattern, REG_EXTENDED | REG_NOSUB);
    if (ret) {
        char err_buf[256];
        regerror(ret, &ctx->compiled[ctx->count], err_buf, sizeof(err_buf));
        fprintf(stderr, "Error: Could not compile regex '%s': %s\n", pattern, err_buf);
        return -1;
    }
    ctx->count++;
    return 0;
}

void free_regex_ctx(regex_ctx* ctx) {
    for (int i = 0; i < ctx->count; i++) {
        regfree(&ctx->compiled[i]);
    }
    ctx->count = 0;
}

void clear_recap_output_files(const char* target_dir) {
    const char* dir_to_open = (target_dir && *target_dir) ? target_dir : ".";
    printf("Warning: This will delete every file matching 'recap-output*' in '%s'.\n", dir_to_open);
    printf("Are you sure? (y/N): ");
    int confirmation = getchar();
    if (tolower(confirmation) != 'y') {
        printf("Operation cancelled.\n");
        return;
    }
    DIR* dir = opendir(dir_to_open);
    if (!dir) {
        perror("opendir for clearing");
        return;
    }
    struct dirent* entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "recap-output", 12) == 0) {
            char path[MAX_PATH_SIZE];
            int len = snprintf(path, sizeof(path), "%s/%s", dir_to_open, entry->d_name);
            if (len < 0 || (size_t)len >= sizeof(path)) {
                fprintf(stderr, "Warning: path too long, skipping: %s/%s\n", dir_to_open, entry->d_name);
                continue;
            }
            if (remove(path) == 0) {
                printf("Deleted: %s\n", path);
                count++;
            }
            else {
                perror("remove recap-output file");
            }
        }
    }
    closedir(dir);
    printf("Cleared %d file(s).\n", count);
}

void load_gitignore(recap_context* ctx, const char* gitignore_filename_arg) {
    char path[MAX_PATH_SIZE];
    strncpy(path, ctx->cwd, sizeof(path));
    const char* filename = (gitignore_filename_arg && *gitignore_filename_arg) ? gitignore_filename_arg : ".gitignore";

    while (1) {
        char gitignore_path[MAX_PATH_SIZE];
        int len = snprintf(gitignore_path, sizeof(gitignore_path), "%s/%s", path, filename);
        if (len < 0 || (size_t)len >= sizeof(gitignore_path)) {
            fprintf(stderr, "Warning: path too long, stopping gitignore search.\n");
            break;
        }

        FILE* file = fopen(gitignore_path, "r");
        if (file) {
            char line[MAX_PATH_SIZE];
            while (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\r\n")] = 0;
                char* trimmed = line;
                while (isspace((unsigned char)*trimmed)) trimmed++;
                if (*trimmed == '\0' || *trimmed == '#') continue;
                if (ctx->fnmatch_exclude_filters.count < MAX_PATTERNS) {
                    strncpy(ctx->gitignore_entries[ctx->gitignore_entry_count], trimmed, MAX_PATH_SIZE - 1);
                    ctx->gitignore_entries[ctx->gitignore_entry_count][MAX_PATH_SIZE - 1] = '\0';
                    ctx->fnmatch_exclude_filters.patterns[ctx->fnmatch_exclude_filters.count++] = ctx->gitignore_entries[ctx->gitignore_entry_count++];
                }
            }
            fclose(file);
            return;
        }
        char* sep = strrchr(path, '/');
        if (sep == path || !sep) break;
        *sep = '\0';
    }
}

void print_help(const char* version) {
    printf("Usage: recap [options] [path...]\n");
    printf("  `path...` are the starting points for traversal (default: .).\n\n");
    printf("General Options:\n");
    printf("  -h, --help                  Show this help message and exit.\n");
    printf("  -v, --version               Show version information (%s) and exit.\n", version);
    printf("  -C, --clear [DIR]           Clear recap-output files in [DIR] (default: '.') and exit.\n\n");
    printf("Filtering and Content (all filters use extended REGEX):\n");
    printf("  -i, --include <REGEX>       Include only paths matching REGEX.\n");
    printf("  -e, --exclude <REGEX>       Exclude any path matching REGEX.\n");
    printf("  -I, --include-content <R>   Show content for files matching REGEX <R>.\n");
    printf("  -E, --exclude-content <R>   Exclude content for files matching REGEX <R>.\n");
    printf("  -g, --git [FILE]            Use .gitignore patterns for exclusions (searches upwards from cwd).\n");
    printf("  -s, --strip <REGEX>         In content blocks, skip lines until a line matches REGEX.\n\n");
    printf("Output and Upload:\n");
    printf("  -o, --output <FILE>         Specify the output file name (disables stdout).\n");
    printf("  -O, --output-dir <DIR>      Specify the output directory (disables stdout).\n");
    printf("  -p, --paste [KEY]           Upload output to Gist (uses GITHUB_API_KEY from env).\n");
}

void parse_arguments(int argc, char* argv[], recap_context* ctx) {
    ctx->fnmatch_exclude_filters.patterns[ctx->fnmatch_exclude_filters.count++] = ".git/";

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'}, {"version", no_argument, 0, 'v'},
        {"clear", optional_argument, 0, 'C'}, {"include", required_argument, 0, 'i'},
        {"exclude", required_argument, 0, 'e'}, {"include-content", required_argument, 0, 'I'},
        {"exclude-content", required_argument, 0, 'E'}, {"strip", required_argument, 0, 's'},
        {"git", optional_argument, 0, 'g'}, {"paste", optional_argument, 0, 'p'},
        {"output", required_argument, 0, 'o'}, {"output-dir", required_argument, 0, 'O'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvC::i:e:I:E:s:g::p::o:O:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h': print_help(ctx->version); exit(0);
        case 'v': printf("recap version %s\n", ctx->version); exit(0);
        case 'C': clear_recap_output_files(optarg); exit(0);
        case 'i': add_regex(&ctx->include_filters, optarg); break;
        case 'e': add_regex(&ctx->exclude_filters, optarg); break;
        case 'I': add_regex(&ctx->content_include_filters, optarg); add_regex(&ctx->include_filters, optarg); break;
        case 'E': add_regex(&ctx->content_exclude_filters, optarg); break;
        case 's':
            if (regcomp(&ctx->strip_regex, optarg, REG_EXTENDED | REG_NOSUB) == 0) {
                ctx->strip_regex_is_set = 1;
            }
            else {
                fprintf(stderr, "Error: Could not compile --strip regex '%s'\n", optarg);
            }
            break;
        case 'g': load_gitignore(ctx, optarg); break;
        case 'p': ctx->gist_api_key = optarg ? optarg : getenv("GITHUB_API_KEY"); if (!ctx->gist_api_key) ctx->gist_api_key = ""; break;
        case 'o': strncpy(ctx->output.output_name, optarg, sizeof(ctx->output.output_name) - 1); break;
        case 'O': strncpy(ctx->output.output_dir, optarg, sizeof(ctx->output.output_dir) - 1); break;
        default: exit(1);
        }
    }

    while (optind < argc) {
        if (ctx->start_path_count < MAX_PATTERNS) {
            ctx->start_paths[ctx->start_path_count++] = argv[optind++];
        }
        else {
            fprintf(stderr, "Error: Too many start paths specified.\n");
            exit(1);
        }
    }

    if (ctx->start_path_count == 0) {
        ctx->start_paths[ctx->start_path_count++] = ".";
    }
}