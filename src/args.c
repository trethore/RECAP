// parse args
#include "recap.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h> 
#include <limits.h>

void clear_recap_output_files(const char* target_dir) {
    char full_path[MAX_PATH_SIZE];
    if (!target_dir || strlen(target_dir) == 0) {
        target_dir = ".";
    }

    if (realpath(target_dir, full_path) == NULL) {
        strncpy(full_path, target_dir, MAX_PATH_SIZE - 1);
        full_path[MAX_PATH_SIZE - 1] = '\0';
    }


    printf("Warning: This will delete every file matching 'recap-output*' in '%s'.\n", full_path);
    printf("Are you sure? (y/N): ");
    char confirmation;
    if (scanf(" %c", &confirmation) != 1 || tolower(confirmation) != 'y') {
        printf("Operation cancelled.\n");
        return;
    }

    DIR* dir = opendir(target_dir);
    if (!dir) {
        perror("opendir for clearing");
        fprintf(stderr, "Failed to open directory: %s\n", target_dir);
        return;
    }

    struct dirent* entry;
    int deleted_count = 0;
    char file_to_remove[MAX_PATH_SIZE];

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "recap-output", strlen("recap-output")) == 0) {
            snprintf(file_to_remove, sizeof(file_to_remove), "%s/%s", target_dir, entry->d_name);
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

void load_gitignore(exclude_patterns_ctx* exclude_ctx) {
    FILE* git_ignore_file = fopen(".gitignore", "r");
    if (!git_ignore_file) {
        return;
    }
    int gitignore_idx = 0;
    char line[MAX_PATH_SIZE];
    while (fgets(line, sizeof(line), git_ignore_file)) {
        line[strcspn(line, "\r\n")] = 0;
        char* trimmed_line = line;
        while (isspace((unsigned char)*trimmed_line))
            trimmed_line++;
        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#') {
            continue;
        }
        // f this
        if (exclude_ctx->exclude_count < MAX_PATTERNS && gitignore_idx < MAX_GITIGNORE_ENTRIES) {
            strncpy(exclude_ctx->gitignore_entries[gitignore_idx], trimmed_line, MAX_PATH_SIZE - 1);
            exclude_ctx->gitignore_entries[gitignore_idx][MAX_PATH_SIZE - 1] = '\0';
            exclude_ctx->exclude_patterns[exclude_ctx->exclude_count++] = exclude_ctx->gitignore_entries[gitignore_idx];
            gitignore_idx++;
        }
        else {
            if (exclude_ctx->exclude_count >= MAX_PATTERNS) {
                fprintf(stderr, "Warning: Maximum number of exclude patterns (%d) reached while reading .gitignore.\n", MAX_PATTERNS);
            }
            break;
        }
    }
    fclose(git_ignore_file);
}

void print_help() {
    printf("Usage: recap [options]\n");
    printf("Options:\n");
    printf("  --help, -h            Show this help message and exit\n");
    printf("  --clear [DIR]         Remove previous recap-output files (optionally from DIR)\n");
    printf("  --content, -c [exts]  Include content of files with given extensions (comma separated)\n");
    printf("  --include, -i PATH    Include specific file or directory (repeatable)\n");
    printf("  --exclude, -e PATH    Exclude specific file or directory (repeatable)\n");
    printf("  --git, -g             Use .gitignore for exclusions\n");
    printf("  --paste, -p API_KEY   Upload output as GitHub Gist\n");
    printf("\nExample:\n");
    printf("  ./recap -i src -e build -c c h\n");
}


void parse_arguments(int argc, char* argv[],
    include_patterns_ctx* include_ctx,
    exclude_patterns_ctx* exclude_ctx,
    output_ctx* output_context,
    content_ctx* content_context,
    char** gist_api_key) {
    include_ctx->include_count = 0;
    exclude_ctx->exclude_count = 0;
    content_context->content_specifier_count = 0;
    content_context->content_flag = 0;
    *gist_api_key = NULL;

    static struct option long_options[] = {
        {"help",       no_argument,       0, 'h'},
        {"clear",      optional_argument, 0, 'C'}, // Changed to optional_argument
        {"content",    required_argument, 0, 'c'},
        {"include",    required_argument, 0, 'i'},
        {"exclude",    required_argument, 0, 'e'},
        {"git",        no_argument,       0, 'g'},
        {"paste",      required_argument, 0, 'p'},
        {"out",        required_argument, 0, 'o'},
        {"output-dir", required_argument, 0, 'O'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hC::c:i:e:gp:o:O:", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'h':
            print_help();
            exit(0);
            break;
        case 'C':
            clear_recap_output_files(optarg);
            exit(0);
            break;
        case 'c': {
            content_context->content_flag = 1;
            char* arg_copy = strdup(optarg);
            if (!arg_copy) {
                fprintf(stderr, "Memory allocation error\n");
                exit(1);
            }
            char* token = strtok(arg_copy, ",");
            while (token != NULL) {
                if (content_context->content_specifier_count < MAX_CONTENT_SPECIFIERS) {
                    content_context->content_specifiers[content_context->content_specifier_count++] = token;
                }
                else {
                    fprintf(stderr, "Too many content specifiers. Max allowed is %d\n", MAX_CONTENT_SPECIFIERS);
                    free(arg_copy);
                    exit(1);
                }
                token = strtok(NULL, ",");
            }
            break;
        }
        case 'i':
            if (include_ctx->include_count < MAX_PATTERNS)
                include_ctx->include_patterns[include_ctx->include_count++] = optarg;
            else {
                fprintf(stderr, "Too many include patterns specified. Max allowed is %d\n", MAX_PATTERNS);
                exit(1);
            }
            break;
        case 'e':
            if (exclude_ctx->exclude_count < MAX_PATTERNS)
                exclude_ctx->exclude_patterns[exclude_ctx->exclude_count++] = optarg;
            else {
                fprintf(stderr, "Too many exclude patterns specified. Max allowed is %d\n", MAX_PATTERNS);
                exit(1);
            }
            break;
        case 'g':
            load_gitignore(exclude_ctx);
            break;
        case 'p':
            *gist_api_key = optarg;
            break;
        case '?':
        default:
            print_help();
            exit(1);
        }
    }

    if (include_ctx->include_count == 0)
        include_ctx->include_patterns[include_ctx->include_count++] = ".";
}