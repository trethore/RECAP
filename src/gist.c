#include "ctf.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include <jansson.h> 
#include <string.h>
#include <stdlib.h>
#include <stdio.h> 

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        /* out of memory! */
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
    CURL* curl;
    CURLcode res;
    struct curl_slist* headers = NULL;
    char* html_url = NULL;

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen for gist upload");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    if (filesize < 0) {
        perror("ftell error getting file size");
        fclose(file);
        return NULL;
    }
    if (filesize == 0) {
        fprintf(stderr, "Gist upload: Input file '%s' is empty. Skipping upload.\n", filepath);
        fclose(file);
        return strdup("");
    }

    if (filesize > 10 * 1024 * 1024) { // Example: 10MB total limit
        fprintf(stderr, "Gist upload: File '%s' is too large (%ld bytes > 10MB). Skipping upload.\n", filepath, filesize);
        fclose(file);
        return strdup("");
    }


    char* file_content = malloc(filesize + 1);
    if (!file_content) {
        fclose(file);
        fprintf(stderr, "Gist upload: Memory allocation failed for file content.\n");
        return NULL;
    }

    size_t bytes_read = fread(file_content, 1, filesize, file);
    if (bytes_read != (size_t)filesize) {
        fprintf(stderr, "Gist upload: Error reading file %s (read %zu bytes, expected %ld)\n", filepath, bytes_read, filesize);
        free(file_content);
        fclose(file);
        return NULL;
    }
    file_content[filesize] = '\0';
    fclose(file);

    const char* filename_only = strrchr(filepath, '/');
    filename_only = filename_only ? filename_only + 1 : filepath;

    json_t* root = json_object();
    json_t* files = json_object();
    json_t* file_obj = json_object();
    json_error_t error;


    json_object_set_new(file_obj, "content", json_string(file_content));
    json_object_set_new(files, filename_only, file_obj);
    json_object_set_new(root, "description", json_string("CTF Output"));
    json_object_set_new(root, "public", json_false());
    json_object_set_new(root, "files", files);

    char* json_payload = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    free(file_content);

    if (!json_payload) {
        fprintf(stderr, "Gist upload: Failed to dump JSON payload.\n");
        return NULL;
    }

    curl = curl_easy_init();
    if (!curl) {
        free(json_payload);
        fprintf(stderr, "Gist upload: curl_easy_init() failed.\n");
        return NULL;
    }

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", github_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: ctf-tool-c");

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    if (!chunk.memory) {
        fprintf(stderr, "Gist upload: Memory allocation failed for response chunk.\n");
        free(json_payload);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return NULL;
    }
    chunk.memory[0] = '\0';


    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/gists");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);


    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Gist upload: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        fprintf(stderr, "Gist upload: HTTP response code: %ld\n", http_code);
        if (chunk.size > 0) {
            fprintf(stderr, "Gist Response Body: %.*s\n", (int)chunk.size, chunk.memory);
        }
        free(chunk.memory);
        free(json_payload);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return NULL;
    }


    json_t* response_json = json_loadb(chunk.memory, chunk.size, 0, &error);
    if (!response_json) {
        fprintf(stderr, "Gist upload: Failed to parse JSON response: %s (line %d, col %d)\n", error.text, error.line, error.column);
    }
    else {
        json_t* html_url_json = json_object_get(response_json, "html_url");
        if (json_is_string(html_url_json)) {
            html_url = strdup(json_string_value(html_url_json)); // Duplicate the URL string
        }
        else {
            fprintf(stderr, "Gist upload: 'html_url' not found or not a string in response.\n");
        }
        json_decref(response_json);
    }


    free(chunk.memory);
    free(json_payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return html_url;
}
