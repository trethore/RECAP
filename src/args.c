// parse args
#include "ctf.h"
#include <curl/curl.h> // Include curl header

char *pastebin_api_key = NULL; // Define and initialize pastebin_api_key

void clear_ctf_output_files(void) {

    DIR *dir = opendir(".");
    if (!dir) {
        perror("opendir for clearing");
        return;
    }
    struct dirent *entry;
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
    FILE *git_ignore_file = fopen(".gitignore", "r");
    if (!git_ignore_file) {

        return;
    }

    char line[MAX_PATH_SIZE];
    while (fgets(line, sizeof(line), git_ignore_file)) {

        line[strcspn(line, "\r\n")] = 0;


        char *trimmed_line = line;
        while (isspace((unsigned char)*trimmed_line)) trimmed_line++;


        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#') {
            continue;
        }


        if (rmf_count < MAX_RMF) {

            strncpy(gitignore_entries[rmf_count], trimmed_line, MAX_PATH_SIZE - 1);
            gitignore_entries[rmf_count][MAX_PATH_SIZE - 1] = '\0';

            rmf_dirs[rmf_count] = gitignore_entries[rmf_count];
            rmf_count++;
        } else {
            fprintf(stderr, "Warning: Maximum number of ignored paths (%d) reached from .gitignore.\n", MAX_RMF);
            break;
        }
    }
    fclose(git_ignore_file);
}

void parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--clear") == 0) {
            clear_ctf_output_files();
            printf("Cleared ctf-output files.\n");
            exit(0);
        } else if (strcmp(argv[i], "--content") == 0) {
            content_flag = 1;
            i++;

            while (i < argc && argv[i][0] != '-') {
                if (content_type_count < MAX_CONTENT_TYPES)
                    content_types[content_type_count++] = argv[i];
                else {
                    fprintf(stderr, "Too many content types specified. Max allowed is %d\n", MAX_CONTENT_TYPES);
                    exit(1);
                }
                i++;
            }
            i--;
        } else if (strcmp(argv[i], "--addf") == 0) {
            i++;
            while (i < argc && argv[i][0] != '-') {
                if (addf_count < MAX_ADDF)
                    addf_dirs[addf_count++] = argv[i];
                else {
                    fprintf(stderr, "Too many addf directories specified. Max allowed is %d\n", MAX_ADDF);
                    exit(1);
                }
                i++;
            }
            i--;
        } else if (strcmp(argv[i], "--rmf") == 0) {
            i++;
            while (i < argc && argv[i][0] != '-') {
                if (rmf_count < MAX_RMF) {



                    rmf_dirs[rmf_count++] = argv[i];
                } else {
                    fprintf(stderr, "Too many rmf directories specified. Max allowed is %d\n", MAX_RMF);
                    exit(1);
                }
                i++;
            }
            i--;
        } else if (strcmp(argv[i], "--git") == 0) {
             git_flag = 1;
        } else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            strncpy(output_dir, argv[++i], MAX_PATH_SIZE - 1);
            output_dir[MAX_PATH_SIZE - 1] = '\0';
            normalize_path(output_dir);
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            strncpy(output_name, argv[++i], MAX_PATH_SIZE - 1);
             output_name[MAX_PATH_SIZE - 1] = '\0';
        } else if (strcmp(argv[i], "--paste") == 0 && i + 1 < argc) { // Added --paste option
            pastebin_api_key = argv[++i];
        } else {
             fprintf(stderr, "Unknown option or missing argument: %s\n", argv[i]);

             exit(1);
        }
    }

    if (addf_count == 0)
        addf_dirs[addf_count++] = ".";
}
