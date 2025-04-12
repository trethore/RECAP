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


static int add_single_content_specifier(const char* token, content_ctx* ctx) {
    if (ctx->content_specifier_count >= MAX_CONTENT_SPECIFIERS) {
        fprintf(stderr, "Too many content specifiers. Max allowed is %d\n", MAX_CONTENT_SPECIFIERS);
        return -1;
    }
    char* token_dup = strdup(token);
    if (!token_dup) {
        perror("strdup content specifier");
        return -1;
    }
    ctx->content_specifiers[ctx->content_specifier_count++] = token_dup;
    return 0;
}


static int add_content_specifiers(const char* arg, content_ctx* ctx) {
    char* arg_copy = strdup(arg);
    if (!arg_copy) {
        perror("strdup for content specifier parsing");
        return -1;
    }
    char* save_ptr = NULL;
    char* token = strtok_r(arg_copy, ",", &save_ptr);
    int result = 0;
    while (token != NULL) {
        if (add_single_content_specifier(token, ctx) != 0) {
            result = -1;
            break;
        }
        token = strtok_r(NULL, ",", &save_ptr);
    }
    free(arg_copy);
    return result;
}


void free_content_specifiers(content_ctx* content_context) {
    for (int i = 0; i < content_context->content_specifier_count; i++) {
        free((void*)content_context->content_specifiers[i]);
        content_context->content_specifiers[i] = NULL;
    }
    content_context->content_specifier_count = 0;
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

            // Optional: Add a stat check to ensure it's a file before removing
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


void load_gitignore(exclude_patterns_ctx* exclude_ctx, const char* gitignore_filename_arg) {
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
            // printf("Loading exclude patterns from: %s\n", search_path);
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


                if (exclude_ctx->exclude_count < MAX_PATTERNS && exclude_ctx->gitignore_entry_count < MAX_GITIGNORE_ENTRIES) {
                    strncpy(exclude_ctx->gitignore_entries[exclude_ctx->gitignore_entry_count], trimmed_line, MAX_PATH_SIZE - 1);
                    exclude_ctx->gitignore_entries[exclude_ctx->gitignore_entry_count][MAX_PATH_SIZE - 1] = '\0';

                    exclude_ctx->exclude_patterns[exclude_ctx->exclude_count++] =
                        exclude_ctx->gitignore_entries[exclude_ctx->gitignore_entry_count];

                    exclude_ctx->gitignore_entry_count++;
                }
                else {
                    if (exclude_ctx->exclude_count >= MAX_PATTERNS) {
                        fprintf(stderr, "Warning: Maximum number of exclude patterns (%d) reached while reading %s.\n", MAX_PATTERNS, search_path);
                    }
                    else {
                        fprintf(stderr, "Warning: Maximum number of gitignore storage slots (%d) reached while reading %s.\n", MAX_GITIGNORE_ENTRIES, search_path);
                    }
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
    printf("Usage: recap [options] [include-path...]\n");
    printf("Options:\n");
    printf(
        "-h, --help                Show this help message and exit\n"
        "-v, --version             Show version information (%s) and exit\n"
        "-C, --clear [DIR]         Clear recap-output files in the specified optional directory (default: current directory)\n"
        "-c, --content [SPEC,...]  Specify content specifiers to include in the output (comma separated)\n"
        "-i, --include PATH        Include files or directories matching the or multiple specified path(s)\n"
        "-e, --exclude PATTERN     Exclude files or directories matching the specified pattern(s)\n"
        "-g, --git [FILE]          Use .gitignore for exclusions (if FILE is omitted, searches for .gitignore)\n"
        "-p, --paste [KEY]         Upload output to a gist using the specified GitHub API key (or GITHUB_API_KEY env var)\n"
        "-o, --output FILE         Specify the output file name\n"
        "-O, --output-dir DIR      Specify the output directory\n"
        "\n"
        , version);
    printf("\nExamples:\n");
    printf("  recap src                          # Process 'src' directory\n");
    printf("  recap -i src -e test.txt           # Include 'src' and exclude 'test.txt'\n");
    printf("  recap -C                           # Clear all recap-output files in the current directory\n");
    printf("  recap src -c c -o somedir/out.txt  # Process 'src' with content specifier 'c' to 'somedir/out.txt'\n");
}


void parse_arguments(int argc, char* argv[], recap_context* ctx) {
    ctx->includes.include_count = 0;
    ctx->excludes.exclude_count = 0;
    ctx->excludes.gitignore_entry_count = 0;
    ctx->content.content_specifier_count = 0;
    ctx->content.content_flag = 0;
    ctx->gist_api_key = NULL;
    ctx->output.output_name[0] = '\0';
    ctx->output.output_dir[0] = '\0';
    ctx->output.calculated_output_path[0] = '\0';
    ctx->output.relative_output_path[0] = '\0';
    ctx->output.use_stdout = 0;

    static const char* default_excludes[] = { ".git/" };
    int num_default_excludes = sizeof(default_excludes) / sizeof(default_excludes[0]);
    for (int i = 0; i < num_default_excludes && ctx->excludes.exclude_count < MAX_PATTERNS; ++i) {
        ctx->excludes.exclude_patterns[ctx->excludes.exclude_count++] = default_excludes[i];
    }


    static struct option long_options[] = {
        {"help",       no_argument,       0, 'h'},
        {"version",    no_argument,       0, 'v'},
        {"clear",      optional_argument, 0, 'C'},
        {"content",    optional_argument, 0, 'c'},
        {"include",    required_argument, 0, 'i'},
        {"exclude",    required_argument, 0, 'e'},
        {"git",        optional_argument, 0, 'g'},
        {"paste",      optional_argument, 0, 'p'},
        {"output",     required_argument, 0, 'o'},
        {"output-dir", required_argument, 0, 'O'},
        {0, 0, 0, 0}
    };

    int opt;
    const char* short_opts = "hvC::c::i:e:g::p::o:O:";

    while ((opt = getopt_long(argc, argv, short_opts, long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            print_help(ctx->version);
            exit(0);
        case 'v':
            printf("recap version %s\n", ctx->version);
            exit(0);

        case 'C':
        {
            const char* clear_dir = optarg;
            if (!clear_dir && optind < argc && argv[optind][0] != '-') {
                clear_dir = argv[optind++];
            }
            clear_recap_output_files(clear_dir ? clear_dir : ".");
            exit(0);
        }


        case 'c': {
            ctx->content.content_flag = 1;
            int parse_error = 0;

            const char* spec = optarg;
            if (!spec && optind < argc && argv[optind][0] != '-') {
                spec = argv[optind++];
            }

            if (spec) {
                if (add_content_specifiers(spec, &ctx->content) != 0) {
                    parse_error = 1;
                }
            }

            if (parse_error) {
                exit(1);
            }
            break;
        }


        case 'i':
            if (ctx->includes.include_count < MAX_PATTERNS) {
                ctx->includes.include_patterns[ctx->includes.include_count++] = optarg;
            }
            else {
                fprintf(stderr, "Too many include paths specified. Max allowed is %d\n", MAX_PATTERNS);
                exit(1);
            }
            break;

        case 'e':
            if (ctx->excludes.exclude_count < MAX_PATTERNS) {
                ctx->excludes.exclude_patterns[ctx->excludes.exclude_count++] = optarg;
            }
            else {
                fprintf(stderr, "Too many exclude patterns specified. Max allowed is %d\n", MAX_PATTERNS);
                exit(1);
            }
            break;

        case 'g':
        {
            const char* gitignore_file = optarg;
            if (!gitignore_file && optind < argc && argv[optind][0] != '-') {
                gitignore_file = argv[optind++];
            }
            load_gitignore(&ctx->excludes, gitignore_file);
        }
        break;

        case 'p':
            if (optarg) {
                ctx->gist_api_key = optarg;
            }
            else {
                char* env_key = getenv("GITHUB_API_KEY");
                if (env_key && strlen(env_key) > 0) {
                    ctx->gist_api_key = env_key;
                }
                else {
                    ctx->gist_api_key = "";
                    fprintf(stderr, "Warning: --paste option used without API key argument and GITHUB_API_KEY env var not set.\n");
                }
            }
            break;


        case 'o':
            if (ctx->output.output_dir[0] != '\0') {
                fprintf(stderr, "Error: Cannot use both --output (-o) and --output-dir (-O) simultaneously.\n");
                exit(1);
            }
            strncpy(ctx->output.output_name, optarg, MAX_PATH_SIZE - 1);
            ctx->output.output_name[MAX_PATH_SIZE - 1] = '\0';
            break;

        case 'O':
            if (ctx->output.output_name[0] != '\0') {
                fprintf(stderr, "Error: Cannot use both --output (-o) and --output-dir (-O) simultaneously.\n");
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
        if (ctx->includes.include_count < MAX_PATTERNS) {
            ctx->includes.include_patterns[ctx->includes.include_count++] = argv[optind++];
        }
        else {
            fprintf(stderr, "Too many include paths specified (including positional). Max allowed is %d\n", MAX_PATTERNS);
            exit(1);
        }
    }


    if (ctx->includes.include_count == 0) {
        int action_taken = 0;
        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ||
                strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 ||
                strcmp(argv[i], "-C") == 0 || strcmp(argv[i], "--clear") == 0) {
                action_taken = 1;
                break;
            }
        }
        if (!action_taken) {
            fprintf(stderr, "No include path specified. Defaulting to current directory (\".\").\n");
            static char* default_include = ".";
            ctx->includes.include_patterns[ctx->includes.include_count++] = default_include;
        }
    }
}