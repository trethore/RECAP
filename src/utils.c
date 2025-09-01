#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

int is_text_file(const char* full_path) {
    FILE* file = fopen(full_path, "rb");
    if (!file) return 0;
    unsigned char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);
    if (bytes_read == 0) return 1;
    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\0') return 0;
    }
    return 1;
}

int path_list_init(path_list* list) {
    list->items = malloc(16 * sizeof(char*));
    if (!list->items) return -1;
    list->count = 0;
    list->capacity = 16;
    return 0;
}

int path_list_add(path_list* list, const char* path) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        char** new_items = realloc(list->items, new_capacity * sizeof(char*));
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return -1;
    list->count++;
    return 0;
}

void path_list_free(path_list* list) {
    if (list) {
        for (size_t i = 0; i < list->count; i++) free(list->items[i]);
        free(list->items);
        list->items = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

void normalize_path(char* path) {
    if (!path || path[0] == '\0') return;
    char* p = path;
    char* q = path;
    size_t len = strlen(path);
    if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';
    char** components = malloc(len * sizeof(char*));
    if (!components) return;
    int i = 0;
    if (*p == '/') {
        q++;
        p++;
    }
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            components[i++] = q;
            q = p + 1;
        }
        p++;
    }
    components[i++] = q;
    int top = 0;
    for (int j = 0; j < i; j++) {
        if (strcmp(components[j], ".") == 0) continue;
        if (strcmp(components[j], "..") == 0) {
            if (top > 0) top--;
        }
        else {
            components[top++] = components[j];
        }
    }
    p = path;
    if (path[0] == '/') *p++ = '/';
    for (int j = 0; j < top; j++) {
        size_t component_len = strlen(components[j]);
        memmove(p, components[j], component_len);
        p += component_len;
        if (j < top - 1) *p++ = '/';
    }
    *p = '\0';
    if (path[0] == '\0') strcpy(path, ".");
    free(components);
}

void get_relative_path(const char* full_path, const char* cwd, char* rel_path_out, size_t size) {
    char normalized_full_path[MAX_PATH_SIZE];
    strncpy(normalized_full_path, full_path, sizeof(normalized_full_path) - 1);
    normalized_full_path[sizeof(normalized_full_path) - 1] = '\0';
    normalize_path(normalized_full_path);

    if (normalized_full_path[0] != '/') {
        snprintf(rel_path_out, size, "%s", normalized_full_path);
        return;
    }

    size_t cwd_len = strlen(cwd);
    if (strncmp(normalized_full_path, cwd, cwd_len) == 0) {
        const char* p = normalized_full_path + cwd_len;
        if (*p == '/') p++;
        snprintf(rel_path_out, size, "%s", *p ? p : ".");
    }
    else {
        snprintf(rel_path_out, size, "%s", normalized_full_path);
    }
}

int generate_output_filename(output_ctx* ctx) {
    char combined_path[MAX_PATH_SIZE];
    const char* dir = (strlen(ctx->output_dir) > 0) ? ctx->output_dir : ".";
    int len;

    if (strlen(ctx->output_name) > 0) {
        len = snprintf(combined_path, sizeof(combined_path), "%s/%s", dir, ctx->output_name);
    }
    else {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "recap-output-%Y%m%d-%H%M%S.txt", t);
        len = snprintf(combined_path, sizeof(combined_path), "%s/%s", dir, timestamp);
    }

    if (len < 0 || (size_t)len >= sizeof(combined_path)) {
        fprintf(stderr, "Error: Constructed output path is too long.\n");
        return -1;
    }

    normalize_path(combined_path);
    strncpy(ctx->calculated_output_path, combined_path, MAX_PATH_SIZE - 1);
    ctx->calculated_output_path[MAX_PATH_SIZE - 1] = '\0';

    char cwd[MAX_PATH_SIZE];
    if (getcwd(cwd, sizeof(cwd))) {
        normalize_path(cwd);
        get_relative_path(ctx->calculated_output_path, cwd, ctx->relative_output_path, sizeof(ctx->relative_output_path));
    }
    else {
        strncpy(ctx->relative_output_path, ctx->calculated_output_path, sizeof(ctx->relative_output_path));
    }
    return 0;
}

int copy_file_content_to_clipboard(const char* filepath) {
    char command[MAX_PATH_SIZE + 64];
    int result = -1;

#if defined(__APPLE__)
    snprintf(command, sizeof(command), "cat \"%s\" | pbcopy", filepath);
    result = system(command);
#elif defined(_WIN32)
    snprintf(command, sizeof(command), "clip < \"%s\"", filepath);
    result = system(command);
#elif defined(__linux__)
    if (getenv("WAYLAND_DISPLAY")) {
        snprintf(command, sizeof(command), "cat \"%s\" | wl-copy", filepath);
        result = system(command);
        if (result != 0) {
            fprintf(stderr, "Warning: 'wl-copy' command failed. Is 'wl-clipboard' installed?\n");
        }
    }
    else {
        snprintf(command, sizeof(command), "cat \"%s\" | xclip -selection clipboard", filepath);
        result = system(command);
        if (result != 0) {
            fprintf(stderr, "Warning: 'xclip' command failed. Is 'xclip' installed?\n");
        }
    }
#else
    fprintf(stderr, "Error: Clipboard functionality is not supported on this operating system.\n");
    return -1;
#endif

    if (result != 0) return -1;

    return 0;
}

int read_file_into_buffer(const char* path, size_t max_bytes, char** out_buf, size_t* out_len) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return -1;
    if ((size_t)st.st_size > max_bytes) return -2;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    size_t sz = (size_t)st.st_size;
    char* buf = sz ? malloc(sz + 1) : strdup("");
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (sz && fread(buf, 1, sz, f) != sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    if (sz) buf[sz] = '\0';
    *out_buf = buf;
    if (out_len) *out_len = sz;
    return 0;
}
