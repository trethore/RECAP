#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>


static int add_regex(regex_ctx* ctx, const char* pattern) {
    if (ctx->count >= MAX_PATTERNS) {
        fprintf(stderr, "Error: Too many regex patterns. Max allowed is %d\n", MAX_PATTERNS);
        return -1;
    }

    int flags = REG_EXTENDED | REG_NOSUB;
    int ret = regcomp(&ctx->compiled[ctx->count], pattern, flags);
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
    char full_path[MAX_PATH_SIZE];
    const char* dir_to_open = target_dir;

    if (!target_dir || strlen(target_dir) == 0) {
        dir_to_open = ".";
    }

    if (realpath(dir_to_open, full_path) == NULL) {
        strncpy(full_path, dir_to_open, MAX_PATH_SIZE - 1);
        full_path[MAX_PATH_SIZE - 1] = '\0';
        if (strcmp(full_path, ".") != 0 && strcmp(full_path, "..") != 0) {
            normalize_path(full_path);
        }
    }


    printf("Warning: This will delete every file matching 'recap-output*' in '%s'.\n", full_path);
    printf("Are you sure? (y/N): ");
    int confirmation_char = getchar();
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    if (tolower(confirmation_char) != 'y') {
        printf("Operation cancelled.\n");
        return;
    }


    DIR* dir = opendir(dir_to_open);
    if (!dir) {
        perror("opendir for clearing");
        fprintf(stderr, "Failed to open directory: %s\n", dir_to_open);
        return;
    }

    struct dirent* entry;
    int deleted_count = 0;
    char file_to_remove[MAX_PATH_SIZE];
    const char* pattern = "recap-output";
    size_t pattern_len = strlen(pattern);

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, pattern, pattern_len) == 0) {
            int path_len = snprintf(file_to_remove, sizeof(file_to_remove), "%s/%s", dir_to_open, entry->d_name);
            if (path_len < 0 || (size_t)path_len >= sizeof(file_to_remove)) {
                fprintf(stderr, "Warning: Path too long, skipping file: %s/%s\n", dir_to_open, entry->d_name);
                continue;
            }

            struct stat st;
            if (stat(file_to_remove, &st) == 0) {
                if (!S_ISREG(st.st_mode)) {
                    continue;
                }
            }
            else {
                perror("stat before remove");
                fprintf(stderr, "Warning: Could not stat file, skipping: %s\n", file_to_remove);
                continue;
            }


            if (remove(file_to_remove) == 0) {
                printf("Deleted: %s\n", file_to_remove);
                deleted_count++;
            }
            else {
                perror("remove recap-output file");
                fprintf(stderr, "Failed to delete: %s\n", file_to_remove);
            }
        }
    }
    closedir(dir);
    if (deleted_count > 0) {
        printf("Cleared %d recap-output file(s) from %s.\n", deleted_count, full_path);
    }
    else {
        printf("No recap-output files found to clear in %s.\n", full_path);
    }
}


void load_gitignore(recap_context* ctx, const char* gitignore_filename_arg) {
    char cwd[MAX_PATH_SIZE];
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd in load_gitignore");
        return;
    }

    char search_path[MAX_PATH_SIZE];
    const char* filename_to_find = (gitignore_filename_arg && strlen(gitignore_filename_arg) > 0)
        ? gitignore_filename_arg : ".gitignore";
    int found = 0;

    char* current_dir = strdup(cwd);
    if (!current_dir) {
        perror("strdup in load_gitignore");
        return;
    }

    while (current_dir && strlen(current_dir) > 0 && strcmp(current_dir, "/") != 0) {
        int path_len = snprintf(search_path, sizeof(search_path), "%s/%s", current_dir, filename_to_find);
        if (path_len < 0 || (size_t)path_len >= sizeof(search_path)) {
            fprintf(stderr, "Warning: Path too long while searching for gitignore: %s/%s\n", current_dir, filename_to_find);
            break;
        }

        FILE* git_ignore_file = fopen(search_path, "r");
        if (git_ignore_file) {
            found = 1;
            char line[MAX_PATH_SIZE];
            while (fgets(line, sizeof(line), git_ignore_file)) {
                line[strcspn(line, "\r\n")] = 0;

                char* trimmed_line = line;
                while (isspace((unsigned char)*trimmed_line)) {
                    trimmed_line++;
                }
                if (trimmed_line[0] == '\0' || trimmed_line[0] == '#') {
                    continue;
                }

                char* end = trimmed_line + strlen(trimmed_line) - 1;
                while (end > trimmed_line && isspace((unsigned char)*end)) {
                    end--;
                }
                *(end + 1) = '\0';

                if (ctx->fnmatch_exclude_filters.count < MAX_PATTERNS && ctx->gitignore_entry_count < MAX_GITIGNORE_ENTRIES) {
                    strncpy(ctx->gitignore_entries[ctx->gitignore_entry_count], trimmed_line, MAX_PATH_SIZE - 1);
                    ctx->gitignore_entries[ctx->gitignore_entry_count][MAX_PATH_SIZE - 1] = '\0';
                    ctx->fnmatch_exclude_filters.patterns[ctx->fnmatch_exclude_filters.count++] =
                        ctx->gitignore_entries[ctx->gitignore_entry_count];
                    ctx->gitignore_entry_count++;
                }
                else {
                    fprintf(stderr, "Warning: Maximum number of gitignore/exclude patterns (%d) reached.\n", MAX_PATTERNS);
                    fclose(git_ignore_file);
                    goto cleanup_and_exit;
                }
            }
            fclose(git_ignore_file);
            break;
        }

        char* last_slash = strrchr(current_dir, '/');
        if (last_slash == current_dir) {
            if (strlen(current_dir) > 1) {
                current_dir[1] = '\0';
            }
            else {
                break;
            }
        }
        else if (last_slash) {
            *last_slash = '\0';
        }
        else {
            break;
        }
    }

cleanup_and_exit:
    free(current_dir);
    if (!found && gitignore_filename_arg) {
        fprintf(stderr, "Warning: Specified gitignore file '%s' not found in current or parent directories.\n", filename_to_find);
    }
    else if (!found && !gitignore_filename_arg) {
        printf("Info: No default .gitignore found in current or parent directories.\n");
    }
}


void print_help(const char* version) {
    printf("Usage: recap [options] [path...]\n");
    printf("  `path...` are the starting points for traversal (default: .)\n\n");
    printf("Options:\n");
    printf(
        "  -h, --help                  Show this help message and exit\n"
        "  -v, --version               Show version information (%s) and exit\n"
        "  -C, --clear [DIR]           Clear recap-output files in the specified optional directory (default: .)\n"
        "  -i, --include <REGEX>       Include paths matching the regex (applied after excludes).\n"
        "  -e, --exclude <REGEX>       Exclude paths matching the regex.\n"
        "      --include-content <REGEX> Include content for files matching the regex.\n"
        "      --exclude-content <REGEX> Exclude content for files matching the regex (takes precedence).\n"
        "      --strip-until <REGEX>   For files with content, skip all lines until a line matches the regex.\n"
        "  -g, --git [FILE]            Use .gitignore for exclusions (searches upwards from cwd).\n"
        "  -p, --paste [KEY]           Upload output to a Gist (uses GITHUB_API_KEY env var if no key).\n"
        "  -o, --output FILE           Specify the output file name.\n"
        "  -O, --output-dir DIR        Specify the output directory for a timestamped file.\n"
        "\n", version);
    printf("Examples:\n");
    printf("  recap src                               # Process 'src' directory\n");
    printf("  recap -e '\\.o$' -e '/build/'           # Exclude .o files and build/ directory\n");
    printf("  recap --include-content '\\.c$'         # Show content for all .c files\n");
    printf("  recap --strip-until 'End of license'    # Skip license headers in all included content\n");
}


void parse_arguments(int argc, char* argv[], recap_context* ctx) {

    memset(ctx, 0, sizeof(recap_context));

    ctx->fnmatch_exclude_filters.patterns[ctx->fnmatch_exclude_filters.count++] = ".git/";

    enum {
        INCLUDE_CONTENT_OPT = 256,
        EXCLUDE_CONTENT_OPT,
        STRIP_UNTIL_OPT
    };

    static struct option long_options[] = {
        {"help",            no_argument,       0, 'h'},
        {"version",         no_argument,       0, 'v'},
        {"clear",           optional_argument, 0, 'C'},
        {"include",         required_argument, 0, 'i'},
        {"exclude",         required_argument, 0, 'e'},
        {"include-content", required_argument, 0, INCLUDE_CONTENT_OPT},
        {"exclude-content", required_argument, 0, EXCLUDE_CONTENT_OPT},
        {"strip-until",     required_argument, 0, STRIP_UNTIL_OPT},
        {"git",             optional_argument, 0, 'g'},
        {"paste",           optional_argument, 0, 'p'},
        {"output",          required_argument, 0, 'o'},
        {"output-dir",      required_argument, 0, 'O'},
        {0, 0, 0, 0}
    };

    int opt;
    const char* short_opts = "hvC::i:e:g::p::o:O:";

    while ((opt = getopt_long(argc, argv, short_opts, long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            print_help(ctx->version);
            exit(0);
        case 'v':
            printf("recap version %s\n", ctx->version);
            exit(0);

        case 'C': {
            const char* clear_dir = optarg;
            if (!clear_dir && optind < argc && argv[optind][0] != '-') {
                clear_dir = argv[optind++];
            }
            clear_recap_output_files(clear_dir ? clear_dir : ".");
            exit(0);
        }

        case 'i':
            if (add_regex(&ctx->include_filters, optarg) != 0) exit(1);
            break;
        case 'e':
            if (add_regex(&ctx->exclude_filters, optarg) != 0) exit(1);
            break;
        case INCLUDE_CONTENT_OPT:
            if (add_regex(&ctx->content_include_filters, optarg) != 0) exit(1);
            break;
        case EXCLUDE_CONTENT_OPT:
            if (add_regex(&ctx->content_exclude_filters, optarg) != 0) exit(1);
            break;
        case STRIP_UNTIL_OPT: {
            int ret = regcomp(&ctx->strip_until_regex, optarg, REG_EXTENDED | REG_NOSUB);
            if (ret) {
                char err_buf[256];
                regerror(ret, &ctx->strip_until_regex, err_buf, sizeof(err_buf));
                fprintf(stderr, "Error: Could not compile --strip-until regex '%s': %s\n", optarg, err_buf);
                exit(1);
            }
            ctx->strip_until_regex_is_set = 1;
            break;
        }

        case 'g': {
            const char* gitignore_file = optarg;
            if (!gitignore_file && optind < argc && argv[optind][0] != '-') {
                gitignore_file = argv[optind++];
            }
            load_gitignore(ctx, gitignore_file);
        }
                break;

        case 'p':
            if (optarg) {
                ctx->gist_api_key = optarg;
            }
            else {
                ctx->gist_api_key = getenv("GITHUB_API_KEY");
                if (!ctx->gist_api_key || ctx->gist_api_key[0] == '\0') {
                    fprintf(stderr, "Warning: --paste used without API key and GITHUB_API_KEY is not set.\n");
                    ctx->gist_api_key = "";
                }
            }
            break;

        case 'o':
            if (ctx->output.output_dir[0] != '\0') {
                fprintf(stderr, "Error: Cannot use both --output (-o) and --output-dir (-O).\n");
                exit(1);
            }
            strncpy(ctx->output.output_name, optarg, MAX_PATH_SIZE - 1);
            ctx->output.output_name[MAX_PATH_SIZE - 1] = '\0';
            break;

        case 'O':
            if (ctx->output.output_name[0] != '\0') {
                fprintf(stderr, "Error: Cannot use both --output (-o) and --output-dir (-O).\n");
                exit(1);
            }
            strncpy(ctx->output.output_dir, optarg, MAX_PATH_SIZE - 1);
            ctx->output.output_dir[MAX_PATH_SIZE - 1] = '\0';
            break;

        case '?':
            fprintf(stderr, "Try 'recap --help' for more information.\n");
            exit(1);
        default:
            abort();
        }
    }

    while (optind < argc) {
        if (ctx->start_path_count < MAX_PATTERNS) {
            ctx->start_paths[ctx->start_path_count++] = argv[optind++];
        }
        else {
            fprintf(stderr, "Error: Too many start paths specified. Max is %d\n", MAX_PATTERNS);
            exit(1);
        }
    }

    if (ctx->start_path_count == 0) {
        int action_taken = 0;
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ||
                strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 ||
                strncmp(argv[i], "-C", 2) == 0 || strcmp(argv[i], "--clear") == 0) {
                action_taken = 1;
                break;
            }
        }
        if (!action_taken) {
            static char* default_path = ".";
            ctx->start_paths[ctx->start_path_count++] = default_path;
        }
    }
}