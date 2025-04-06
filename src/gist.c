#include "ctf.h"
#include <curl/curl.h>
#include <sys/stat.h> // For file size check
#include <jansson.h> // Use jansson included by user
#include <string.h>
#include <stdlib.h>
#include <stdio.h> // Added for fprintf, perror


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
        fprintf(stderr, "Gist upload: not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Renamed api_key parameter to github_token for clarity internally
char *upload_to_gist(const char *filepath, const char *github_token) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char *html_url = NULL; // Store the final HTML URL

    // --- Read File Content --- 
    FILE *file = fopen(filepath, "rb"); // Use binary read 'rb'
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
         return strdup(""); // Return empty string indicates skipped/empty file
     }
     // Add a size limit? GitHub might reject huge gists. ~1MB per file is a rough limit.
     // Let's set a higher limit for the total payload for now.
     if (filesize > 10 * 1024 * 1024) { // Example: 10MB total limit
          fprintf(stderr, "Gist upload: File '%s' is too large (%ld bytes > 10MB). Skipping upload.\n", filepath, filesize);
          fclose(file);
          return strdup(""); // Indicate skipped due to size
     }


    char *file_content = malloc(filesize + 1);
    if (!file_content) {
        fclose(file);
        fprintf(stderr, "Gist upload: Memory allocation failed for file content.\n");
        return NULL;
    }

    size_t bytes_read = fread(file_content, 1, filesize, file);
     if (bytes_read != (size_t)filesize) {
         // Check ferror or feof? 
         fprintf(stderr, "Gist upload: Error reading file %s (read %zu bytes, expected %ld)\n", filepath, bytes_read, filesize);
         free(file_content);
         fclose(file);
         return NULL;
     }
    file_content[filesize] = '\0'; // Null-terminate
    fclose(file);

    // --- Prepare JSON using Jansson --- 
    const char *filename_only = strrchr(filepath, '/');
    filename_only = filename_only ? filename_only + 1 : filepath;

    json_t *root = json_object();
    json_t *files = json_object();
    json_t *file_obj = json_object();
    json_error_t error; // For JSON parsing errors

    // Check if content is valid UTF-8? Jansson might handle invalid sequences,
    // but the GitHub API might reject non-UTF8 content in JSON strings.
    // For simplicity, assume valid text or let the API handle errors.
    json_object_set_new(file_obj, "content", json_string(file_content)); // Jansson handles JSON string escaping
    json_object_set_new(files, filename_only, file_obj);
    json_object_set_new(root, "description", json_string("CTF Output"));
    json_object_set_new(root, "public", json_false()); // Create a secret gist
    json_object_set_new(root, "files", files);

    char *json_payload = json_dumps(root, JSON_COMPACT);
    json_decref(root); // Clean up jansson object
    free(file_content); // Free raw content now that it's in the JSON string

    if (!json_payload) {
        fprintf(stderr, "Gist upload: Failed to dump JSON payload.\n");
        return NULL;
    }

    // --- Setup cURL --- 
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
    headers = curl_slist_append(headers, "User-Agent: ctf-tool-c"); // Use a specific user agent

    // --- Prepare Response Buffer --- 
    struct MemoryStruct chunk;
    chunk.memory = malloc(1); // Will be grown by realloc in callback
    chunk.size = 0;           // No data yet
     if (!chunk.memory) {
         fprintf(stderr, "Gist upload: Memory allocation failed for response chunk.\n");
         free(json_payload);
         curl_slist_free_all(headers);
         curl_easy_cleanup(curl);
         return NULL;
     }
    chunk.memory[0] = '\0';


    // --- Set cURL Options --- 
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/gists");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Uncomment for debugging curl
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // Fail on HTTP >= 400


    // --- Perform Request --- 
    res = curl_easy_perform(curl);

    // --- Check for Errors --- 
    if (res != CURLE_OK) {
        fprintf(stderr, "Gist upload: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         long http_code = 0;
         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
         fprintf(stderr, "Gist upload: HTTP response code: %ld\n", http_code);
         // Print response body only if there's an error and content exists
         if (chunk.size > 0) {
             fprintf(stderr, "Gist Response Body: %.*s\n", (int)chunk.size, chunk.memory);
         }
        free(chunk.memory);
        free(json_payload);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return NULL;
    }

    // --- Process Successful Response --- 
    // Parse the response JSON to get the html_url
    json_t *response_json = json_loadb(chunk.memory, chunk.size, 0, &error);
     if (!response_json) {
         fprintf(stderr, "Gist upload: Failed to parse JSON response: %s (line %d, col %d)\n", error.text, error.line, error.column);
         // html_url remains NULL
     } else {
        json_t *html_url_json = json_object_get(response_json, "html_url");
        if (json_is_string(html_url_json)) {
            html_url = strdup(json_string_value(html_url_json)); // Duplicate the URL string
        } else {
             fprintf(stderr, "Gist upload: 'html_url' not found or not a string in response.\n");
             // Optional: print full response for debugging
             // fprintf(stderr, "Full response: %s\n", chunk.memory);
             // html_url remains NULL
        }
        json_decref(response_json); // Clean up response JSON object
     }


    // --- Cleanup --- 
    free(chunk.memory);
    free(json_payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // Return the duplicated html_url string (or NULL if not found/error)
    // If strdup returns NULL, it indicates memory allocation failure.
    return html_url;
}
