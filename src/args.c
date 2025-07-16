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

static int add_regex_internal(pcre2_code** re, const char* pattern, uint32_t options) {
    int error_code;
    PCRE2_SIZE error_offset;
    *re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, options, &error_code, &error_offset, NULL);
    if (*re == NULL) {
        PCRE2_UCHAR err_buf[256];
        pcre2_get_error_message(error_code, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error: Could not compile regex '%s': %s at offset %d\n", pattern, (char*)err_buf, (int)error_offset);
        return -1;
    }
    return 0;
}

static int add_regex(regex_ctx* ctx, const char* pattern) {
    if (ctx->count >= MAX_PATTERNS) {
        fprintf(stderr, "Error: Too many regex patterns. Max allowed is %d\n", MAX_PATTERNS);
        return -1;
    }
    if (add_regex_internal(&ctx->compiled[ctx->count], pattern, 0) != 0) {
        return -1;
    }
    ctx->count++;
    return 0;
}

static int add_scoped_strip_rule(recap_context* ctx, const char* path_pattern, const char* strip_pattern) {
    if (ctx->scoped_strip_rule_count >= MAX_SCOPED_STRIP_RULES) {
        fprintf(stderr, "Error: Too many scoped strip rules. Max allowed is %d\n", MAX_SCOPED_STRIP_RULES);
        return -1;
    }

    scoped_strip_rule* rule = &ctx->scoped_strip_rules[ctx->scoped_strip_rule_count];

    if (add_regex_internal(&rule->path_regex, path_pattern, 0) != 0) {
        return -1;
    }

    if (add_regex_internal(&rule->strip_regex, strip_pattern, PCRE2_MULTILINE) != 0) {
        pcre2_code_free(rule->path_regex);
        return -1;
    }

    ctx->scoped_strip_rule_count++;
    return 0;
}

void free_regex_ctx(regex_ctx* ctx) {
    for (int i = 0; i < ctx->count; i++) {
        pcre2_code_free(ctx->compiled[i]);
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
    printf("  -h, --help                         Show this help message and exit.\n");
    printf("  -v, --version                      Show version information (%s) and exit.\n", version);
    printf("  -C, --clear [DIR]                  Clear recap-output files in [DIR] (default: '.') and exit.\n\n");
    printf("Filtering and Content (all filters use PCRE2 REGEX):\n");
    printf("  -i, --include <REGEX>              Include only paths matching REGEX.\n");
    printf("  -e, --exclude <REGEX>              Exclude any path matching REGEX.\n");
    printf("  -I, --include-content <R>          Show content for files matching REGEX <R>.\n");
    printf("  -E, --exclude-content <R>          Exclude content for files matching REGEX <R>.\n");
    printf("  -g, --git [FILE]                   Use .gitignore patterns for exclusions (searches upwards from cwd).\n");
    printf("  -s, --strip <REGEX>                In content blocks, skip all content that matches REGEX.\n");
    printf("  -S, --strip-scope <P_RE> <S_RE>    Apply strip regex <S_RE> to files matching path regex <P_RE>.\n\n");
    printf("Output and Upload:\n");
    printf("  -o, --output <FILE>                Specify the output file name (disables stdout).\n");
    printf("  -O, --output-dir <DIR>             Specify the output directory (disables stdout).\n");
    printf("  -p, --paste [KEY]                  Upload output to Gist (uses GITHUB_API_KEY from env).\n");

    printf("\nExamples:\n");
    printf("  recap src doc -I '\\.(c|h|md)$'\n");
    printf("    Process 'src' and 'doc', showing content for C, header, and markdown files.\n\n");

    printf("  recap -e '^(obj|test)/'\n");
    printf("    Process the current directory, excluding all paths within 'obj/' and 'test/'.\n\n");

    printf("  recap -I '.*'-s '^\\/\\*\\*(.|\\s)*\\*\\/\\s* \n");
    printf("    Strip a JSDoc-style comment header from all files before printing their content.\n\n");

    printf("  recap -g --paste\n");
    printf("    Use .gitignore rules for exclusion and upload the result to a private Gist.\n");
}

void parse_arguments(int argc, char* argv[], recap_context* ctx) {
    ctx->fnmatch_exclude_filters.patterns[ctx->fnmatch_exclude_filters.count++] = ".git/";

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'}, {"version", no_argument, 0, 'v'}, {"clear", optional_argument, 0, 'C'}, {"include", required_argument, 0, 'i'}, {"exclude", required_argument, 0, 'e'}, {"include-content", required_argument, 0, 'I'}, {"exclude-content", required_argument, 0, 'E'}, {"strip", required_argument, 0, 's'}, {"strip-scope", required_argument, 0, 'S'}, {"git", optional_argument, 0, 'g'}, {"paste", optional_argument, 0, 'p'}, {"output", required_argument, 0, 'o'}, {"output-dir", required_argument, 0, 'O'}, {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "hvC::i:e:I:E:s:S:g::p::o:O:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            print_help(ctx->version);
            exit(0);
        case 'v':
            printf("recap version %s\n", ctx->version);
            exit(0);
        case 'C':
            clear_recap_output_files(optarg);
            exit(0);
        case 'i':
            add_regex(&ctx->include_filters, optarg);
            break;
        case 'e':
            add_regex(&ctx->exclude_filters, optarg);
            break;
        case 'I':
            add_regex(&ctx->content_include_filters, optarg);
            add_regex(&ctx->include_filters, optarg);
            break;
        case 'E':
            add_regex(&ctx->content_exclude_filters, optarg);
            break;
        case 's':
            if (add_regex_internal(&ctx->strip_regex, optarg, PCRE2_MULTILINE) == 0) {
                ctx->strip_regex_is_set = 1;
            }
            break;
        case 'S':
            if (optind >= argc) {
                fprintf(stderr, "Error: --strip-scope requires two arguments: a path regex and a strip regex.\n");
                exit(1);
            }
            add_scoped_strip_rule(ctx, optarg, argv[optind]);
            optind++;
            break;
        case 'g':
            load_gitignore(ctx, optarg);
            break;
        case 'p':
            ctx->gist_api_key = optarg ? optarg : getenv("GITHUB_API_KEY");
            if (!ctx->gist_api_key) ctx->gist_api_key = "";
            break;
        case 'o':
            strncpy(ctx->output.output_name, optarg, sizeof(ctx->output.output_name) - 1);
            break;
        case 'O':
            strncpy(ctx->output.output_dir, optarg, sizeof(ctx->output.output_dir) - 1);
            break;
        default:
            exit(1);
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