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

char *upload_to_pastebin(const char *filepath, const char *api_key) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1); 
    chunk.size = 0;          

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen for pastebin upload");
        free(chunk.memory);
        return NULL;
    }

    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("stat for pastebin upload");
        fclose(file);
        free(chunk.memory);
        return NULL;
    }
    long file_size = st.st_size;

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        fprintf(stderr, "Failed to allocate memory for file content\n");
        fclose(file);
        free(chunk.memory);
        return NULL;
    }
    if (fread(file_content, 1, file_size, file) != (size_t)file_size) {
        fprintf(stderr, "Failed to read entire file content\n");
        fclose(file);
        free(file_content);
        free(chunk.memory);
        return NULL;
    }
    file_content[file_size] = '\0';
    fclose(file);

    curl = curl_easy_init();
    if(curl) {
        const char *filename_only = strrchr(filepath, '/');
        if (!filename_only) {
            filename_only = strrchr(filepath, '\\'); 
        }
        filename_only = filename_only ? filename_only + 1 : filepath;

        char *encoded_content = curl_easy_escape(curl, file_content, 0);
        if (!encoded_content) {
            fprintf(stderr, "Failed to URL-encode file content\n");
            free(file_content);
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return NULL;
        }
        free(file_content); 

        const char *post_format = "api_dev_key=%s&api_option=paste&api_paste_code=%s&api_paste_private=0&api_paste_name=%s&api_paste_format=text";

        int post_data_len = snprintf(NULL, 0, post_format,
                                     api_key, encoded_content, filename_only);

        char *post_data = malloc(post_data_len + 1);
        if (!post_data) {
             fprintf(stderr, "Failed to allocate memory for post data\n");
             curl_free(encoded_content);
             free(chunk.memory);
             curl_easy_cleanup(curl);
             return NULL;
        }

        snprintf(post_data, post_data_len + 1, post_format,
                 api_key, encoded_content, filename_only);

        curl_free(encoded_content);

        curl_easy_setopt(curl, CURLOPT_URL, "https://pastebin.com/api/api_post.php");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            if(chunk.memory) free(chunk.memory);
            chunk.memory = NULL;
        } else {
            const char *expected_prefix = "https://pastebin.com/";
            if (chunk.memory && strncmp(chunk.memory, expected_prefix, strlen(expected_prefix)) == 0) {
            } else {
                fprintf(stderr, "Pastebin API Error or Unexpected Response: %s\n", chunk.memory ? chunk.memory : "(No response body)");
                if(chunk.memory) free(chunk.memory); 
                chunk.memory = NULL; 
            }
        }
        curl_easy_cleanup(curl);
    }

    return chunk.memory;
}
