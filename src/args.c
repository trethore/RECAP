// parse args
#include "ctf.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

char* gist_api_key = NULL;

void clear_ctf_output_files(void) {

    DIR* dir = opendir(".");
    if (!dir) {
        perror("opendir for clearing");
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "ctf-output", strlen("ctf-output")) == 0) {

            if (remove(entry->d_name) != 0) {
                perror("remove ctf-output file");
            }
        }
    }
    closedir(dir);
}

void load_gitignore(void) {
    FILE* git_ignore_file = fopen(".gitignore", "r");
    if (!git_ignore_file) {

        return;
    }

    int gitignore_idx = 0;

    char line[MAX_PATH_SIZE];
    while (fgets(line, sizeof(line), git_ignore_file)) {

        line[strcspn(line, "\r\n")] = 0;

        char* trimmed_line = line;
        while (isspace((unsigned char)*trimmed_line)) trimmed_line++;

        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#') {
            continue;
        }

        if (exclude_count < MAX_PATTERNS && gitignore_idx < MAX_GITIGNORE_ENTRIES) {
            strncpy(gitignore_entries[gitignore_idx], trimmed_line, MAX_PATH_SIZE - 1);
            gitignore_entries[gitignore_idx][MAX_PATH_SIZE - 1] = '\0';
            exclude_patterns[exclude_count++] = gitignore_entries[gitignore_idx];
            gitignore_idx++;
        }
        else {
            if (exclude_count >= MAX_PATTERNS) {
                fprintf(stderr, "Warning: Maximum number of exclude patterns (%d) reached while reading .gitignore.\n", MAX_PATTERNS);
            }
            break;
        }
    }
    fclose(git_ignore_file);
}

void parse_arguments(int argc, char* argv[]) {
    include_count = 0;
    exclude_count = 0;
    content_specifier_count = 0;
    content_flag = 0;
    git_flag = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--clear") == 0) {
            clear_ctf_output_files();
            printf("Cleared ctf-output files.\n");
            exit(0);
        }
        else if (strcmp(argv[i], "--content") == 0 || strcmp(argv[i], "-c") == 0) {
            content_flag = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                i++;
                while (i < argc && argv[i][0] != '-') {
                    if (content_specifier_count < MAX_CONTENT_SPECIFIERS)
                        content_specifiers[content_specifier_count++] = argv[i];
                    else {
                        fprintf(stderr, "Too many content specifiers. Max allowed is %d\n", MAX_CONTENT_SPECIFIERS);
                        exit(1);
                    }
                    i++;
                }
                i--;
            }
        }
        else if (strcmp(argv[i], "--include") == 0 || strcmp(argv[i], "-i") == 0) {
            if (++i < argc) {
                while (i < argc && argv[i][0] != '-') {
                    if (include_count < MAX_PATTERNS)
                        include_patterns[include_count++] = argv[i];
                    else {
                        fprintf(stderr, "Too many include patterns specified. Max allowed is %d\n", MAX_PATTERNS);
                        exit(1);
                    }
                    i++;
                }
                i--;
            }
            else {
                fprintf(stderr, "Error: --include/-i requires at least one argument.\n");
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--exclude") == 0 || strcmp(argv[i], "-e") == 0) {
            if (++i < argc) {
                while (i < argc && argv[i][0] != '-') {
                    if (exclude_count < MAX_PATTERNS)
                        exclude_patterns[exclude_count++] = argv[i];
                    else {
                        fprintf(stderr, "Too many exclude patterns specified. Max allowed is %d\n", MAX_PATTERNS);
                        exit(1);
                    }
                    i++;
                }
                i--;
            }
            else {
                fprintf(stderr, "Error: --exclude/-e requires at least one argument.\n");
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--git") == 0 || strcmp(argv[i], "-g") == 0) {
            git_flag = 1;
        }
        else if (strcmp(argv[i], "--dir") == 0 || strcmp(argv[i], "-d") == 0) {
            if (++i < argc) {
                strncpy(output_dir, argv[i], MAX_PATH_SIZE - 1);
                output_dir[MAX_PATH_SIZE - 1] = '\0';
                normalize_path(output_dir);
            }
            else {
                fprintf(stderr, "Error: --dir requires an argument.\n");
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--name") == 0 || strcmp(argv[i], "-n") == 0) {
            if (++i < argc) {
                strncpy(output_name, argv[i], MAX_PATH_SIZE - 1);
                output_name[MAX_PATH_SIZE - 1] = '\0';
            }
            else {
                fprintf(stderr, "Error: --name requires an argument.\n");
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--paste") == 0 || strcmp(argv[i], "-p") == 0) {
            if (++i < argc) {
                gist_api_key = argv[i];
            }
            else {
                fprintf(stderr, "Error: --paste/-p requires an API key argument.\n");
                exit(1);
            }
        }
        else {
            fprintf(stderr, "Unknown option or missing argument: %s\n", argv[i]);
            exit(1);
        }
    }

    if (include_count == 0)
        include_patterns[include_count++] = ".";
}
