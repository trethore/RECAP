#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>

#define GIST_API_URL "https://api.github.com/gists"
#define GIST_USER_AGENT "recap-c-tool/2.0"
#define GIST_MAX_FILESIZE (10 * 1024 * 1024) // 10 MB

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        fprintf(stderr, "Gist upload: not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

static char* create_gist_payload(const char* filename, const char* content) {
    json_t* root, * files_obj, * file_obj;
    root = json_object();
    files_obj = json_object();
    file_obj = json_object();

    if (!root || !files_obj || !file_obj) {
        fprintf(stderr, "Gist upload error: Failed to create JSON objects.\n");
        json_decref(root); json_decref(files_obj); json_decref(file_obj);
        return NULL;
    }

    json_object_set_new(file_obj, "content", json_string(content));
    json_object_set_new(files_obj, filename, file_obj);
    json_object_set_new(root, "description", json_string("Recap output"));
    json_object_set_new(root, "public", json_false());
    json_object_set_new(root, "files", files_obj);

    char* payload = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    return payload;
}

char* upload_to_gist(const char* filepath, const char* github_token) {
    struct stat st;
    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Gist upload error: Input is not a valid file: %s\n", filepath);
        return NULL;
    }
    if (st.st_size <= 0) {
        fprintf(stderr, "Gist upload info: Input file '%s' is empty. Skipping.\n", filepath);
        return NULL;
    }
    if (st.st_size > GIST_MAX_FILESIZE) {
        fprintf(stderr, "Gist upload error: File '%s' is too large.\n", filepath);
        return NULL;
    }

    char* file_content = NULL;
    size_t file_len = 0;
    if (read_file_into_buffer(filepath, GIST_MAX_FILESIZE, &file_content, &file_len) != 0) {
        fprintf(stderr, "Gist upload error: Failed to read file '%s'.\n", filepath);
        return NULL;
    }

    const char* filename_only = strrchr(filepath, '/');
    filename_only = filename_only ? filename_only + 1 : filepath;

    char* json_payload = create_gist_payload(filename_only, file_content);
    free(file_content);
    if (!json_payload) return NULL;

    CURL* curl = NULL;
    struct curl_slist* headers = NULL;
    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
    char* html_url = NULL;

    if (!chunk.memory) goto cleanup;
    chunk.memory[0] = '\0';

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", github_token);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: " GIST_USER_AGENT);

    curl_easy_setopt(curl, CURLOPT_URL, GIST_API_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    if (curl_easy_perform(curl) == CURLE_OK) {
        json_error_t error;
        json_t* response_json = json_loadb(chunk.memory, chunk.size, 0, &error);
        if (response_json) {
            json_t* url_json = json_object_get(response_json, "html_url");
            if (json_is_string(url_json)) {
                html_url = strdup(json_string_value(url_json));
            }
            json_decref(response_json);
        }
    }
    else {
        fprintf(stderr, "Gist upload error: curl_easy_perform() failed.\n");
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        fprintf(stderr, "HTTP response code: %ld. Response: %.*s\n", http_code, (int)chunk.size, chunk.memory);
    }

cleanup:
    free(chunk.memory);
    free(json_payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return html_url;
}
