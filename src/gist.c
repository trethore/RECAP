#include "ctf.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include <jansson.h> // JSON library for proper escaping

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

    // Use JSON library to escape file content
    json_t *root = json_object();
    json_t *files = json_object();
    json_t *file_obj = json_object();

    json_object_set_new(file_obj, "content", json_string(file_content));
    json_object_set_new(files, filename_only, file_obj);
    json_object_set_new(root, "description", json_string("CTF Output"));
    json_object_set_new(root, "public", json_false());
    json_object_set_new(root, "files", files);

    char *json_payload = json_dumps(root, JSON_COMPACT);
    json_decref(root);
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
            char *base_url = malloc(len + 1);
            if (!base_url) { /* handle malloc failure */ }

            strncpy(base_url, start, len);
            base_url[len] = '\0';

            size_t raw_len = len + strlen("/raw");
            url = malloc(raw_len + 1);
            if (!url) { 
                printf(stderr, "Memory allocation failed for URL.\n");
            }else {
                snprintf(url, raw_len + 1, "%s/raw", base_url);                
            }
            free(base_url);
        }
    }

    free(chunk.memory);

    return url;
}
