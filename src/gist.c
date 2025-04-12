#define _POSIX_C_SOURCE 200809L
#include "recap.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include <jansson.h>

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    if (mem->size + realsize < mem->size) {
        fprintf(stderr, "Gist upload: Memory size overflow detected in callback.\n");
        return 0;
    }

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

char* upload_to_gist(const char* filepath, const char* github_token) {
    if (!github_token || github_token[0] == '\0') {
        fprintf(stderr, "Gist upload error: GitHub API token is missing or empty.\n");
        return NULL;
    }

    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("stat file for gist upload");
        fprintf(stderr, "Gist upload error: Cannot stat input file: %s\n", filepath);
        return NULL;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Gist upload error: Input path is not a regular file: %s\n", filepath);
        return NULL;
    }

    long filesize = st.st_size;

    if (filesize <= 0) {
        fprintf(stderr, "Gist upload info: Input file '%s' is empty or invalid size (%ld). Skipping upload.\n", filepath, filesize);
        return NULL;
    }

    long max_filesize = 10 * 1024 * 1024; // 10 MB limit
    if (filesize > max_filesize) {
        fprintf(stderr, "Gist upload error: File '%s' is too large (%ld bytes > %ld bytes limit). Skipping upload.\n", filepath, filesize, max_filesize);
        return NULL;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen for gist upload");
        fprintf(stderr, "Gist upload error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    char* file_content = malloc(filesize + 1);
    if (!file_content) {
        fclose(file);
        fprintf(stderr, "Gist upload error: Memory allocation failed for file content (%ld bytes).\n", filesize);
        return NULL;
    }

    size_t bytes_read = fread(file_content, 1, filesize, file);
    if (bytes_read != (size_t)filesize || ferror(file)) {
        fprintf(stderr, "Gist upload error: Error reading file %s (read %zu bytes, expected %ld, ferror: %d)\n",
            filepath, bytes_read, filesize, ferror(file));
        free(file_content);
        fclose(file);
        return NULL;
    }
    file_content[filesize] = '\0';
    fclose(file);

    const char* filename_only = strrchr(filepath, '/');
    filename_only = filename_only ? filename_only + 1 : filepath;

    json_t* root = json_object();
    json_t* files_obj = json_object();
    json_t* file_obj = json_object();
    json_error_t error;

    if (!root || !files_obj || !file_obj) {
        fprintf(stderr, "Gist upload error: Failed to create JSON objects.\n");
        free(file_content);
        json_decref(root);
        json_decref(files_obj);
        json_decref(file_obj);
        return NULL;
    }

    if (json_object_set_new(file_obj, "content", json_string(file_content)) != 0 ||
        json_object_set_new(files_obj, filename_only, file_obj) != 0 ||
        json_object_set_new(root, "description", json_string("Recap output")) != 0 ||
        json_object_set_new(root, "public", json_false()) != 0 ||
        json_object_set_new(root, "files", files_obj) != 0) {
        fprintf(stderr, "Gist upload error: Failed to populate JSON object.\n");
        free(file_content);
        json_decref(root);
        return NULL;
    }

    free(file_content);

    char* json_payload = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    if (!json_payload) {
        fprintf(stderr, "Gist upload error: Failed to dump JSON payload.\n");
        return NULL;
    }

    CURL* curl = curl_easy_init();
    struct curl_slist* headers = NULL;
    struct MemoryStruct chunk = { .memory = NULL, .size = 0 }; // Initialize
    char* html_url = NULL;
    CURLcode res = CURLE_OK;

    if (!curl) {
        fprintf(stderr, "Gist upload error: curl_easy_init() failed.\n");
        free(json_payload);
        return NULL;
    }

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        fprintf(stderr, "Gist upload error: Memory allocation failed for response chunk.\n");
        free(json_payload);
        curl_easy_cleanup(curl);
        return NULL;
    }
    chunk.memory[0] = '\0';
    chunk.size = 0;


    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", github_token);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: recap-c-tool/1.0"); // More specific user agent

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/gists");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);


    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Gist upload error: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        fprintf(stderr, "Gist upload error: HTTP response code: %ld\n", http_code);
        if (chunk.size > 0) {
            fprintf(stderr, "Gist Response Body: %.*s\n", (int)chunk.size, chunk.memory);
        }
    }
    else {
        json_t* response_json = json_loadb(chunk.memory, chunk.size, 0, &error);
        if (!response_json) {
            fprintf(stderr, "Gist upload warning: Failed to parse JSON response: %s (line %d, col %d)\n", error.text, error.line, error.column);
            fprintf(stderr, "Gist Response Body: %.*s\n", (int)chunk.size, chunk.memory);
        }
        else {
            json_t* url_json = json_object_get(response_json, "html_url");
            if (json_is_string(url_json)) {
                const char* url_str = json_string_value(url_json);
                size_t url_len = strlen(url_str);
                html_url = malloc(url_len + 1);
                if (html_url) {
                    strcpy(html_url, url_str);
                }
                else {
                    perror("malloc for Gist URL");
                }

            }
            else {
                fprintf(stderr, "Gist upload warning: 'html_url' not found or not a string in response.\n");
                char* resp_str = json_dumps(response_json, JSON_INDENT(2));
                if (resp_str) {
                    fprintf(stderr, "Gist Full Response:\n%s\n", resp_str);
                    free(resp_str);
                }
            }
            json_decref(response_json);
        }
    }


    free(chunk.memory);
    free(json_payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return html_url;
}