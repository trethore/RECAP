// src/pastebin.c
#include "ctf.h"
#include <curl/curl.h>
#include <sys/stat.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        /* out of memory! */
        fprintf(stderr, "not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

char *upload_to_gist(const char *filepath, const char *github_token) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char *url = NULL;

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen for gist upload");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char *file_content = malloc(filesize + 1);
    if (!file_content) {
        fclose(file);
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    fread(file_content, 1, filesize, file);
    file_content[filesize] = '\0';
    fclose(file);

    const char *filename_only = strrchr(filepath, '/');
    filename_only = filename_only ? filename_only + 1 : filepath;

    const char *json_format =
        "{ \"description\": \"CTF Output\", \"public\": false, \"files\": { \"%s\": { \"content\": \"%s\" } } }";
    int json_len = snprintf(NULL, 0, json_format, filename_only, file_content);
    char *json_payload = malloc(json_len + 1);
    snprintf(json_payload, json_len + 1, json_format, filename_only, file_content);
    free(file_content);

    curl = curl_easy_init();
    if (!curl) {
        free(json_payload);
        return NULL;
    }

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", github_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: CTF-Uploader");

    struct MemoryStruct {
        char *memory;
        size_t size;
    } chunk = {malloc(1), 0};

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/gists");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);
    free(json_payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        return NULL;
    }

    const char *url_marker = "\"html_url\":\"";
    char *start = strstr(chunk.memory, url_marker);
    if (start) {
        start += strlen(url_marker);
        char *end = strchr(start, '"');
        if (end) {
            size_t len = end - start;
            url = malloc(len + 1);
            strncpy(url, start, len);
            url[len] = '\0';
        }
    }

    free(chunk.memory);
    return url;
}
