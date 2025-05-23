#include "http.h"


static const struct {
    int code;
    const char *text;
} status_messages[] = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {505, "HTTP Version Not Supported"},
    {0, NULL}
};

static const struct {
    const char *ext;
    const char *type;
} mime_types[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain"},
    {".pdf", "application/pdf"},
    {NULL, "application/octet-stream"}
};

#define CACHE_SIZE 10000    
#define CACHE_TIMEOUT 3600      

static char header_buffer[8192];

typedef struct {
    char path[PATH_MAX];
    char *response;
    size_t response_len;
    time_t timestamp;
    char vary_key[256];  
} cache_entry_t;

static cache_entry_t response_cache[CACHE_SIZE];
static int cache_index = 0;

static void generate_vary_key(const char *path, const http_request_t *request, char *key, size_t key_size) {
    if (!request) {
        strncpy(key, path, key_size - 1);
        key[key_size - 1] = '\0';
        return;
    }
    
    snprintf(key, key_size, "%s:", path);
    
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "User-Agent") == 0) {
            size_t current_len = strlen(key);
            snprintf(key + current_len, key_size - current_len, "UA:%s:", request->headers[i][1]);
            break;
        }
    }
    
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "Accept-Encoding") == 0) {
            size_t current_len = strlen(key);
            snprintf(key + current_len, key_size - current_len, "AE:%s", request->headers[i][1]);
            break;
        }
    }
}

static cache_entry_t *find_cached_response(const char *path, const http_request_t *request) {
    char vary_key[256];
    generate_vary_key(path, request, vary_key, sizeof(vary_key));
    
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (response_cache[i].path[0] != '\0' && 
            strcmp(response_cache[i].path, path) == 0 &&
            strcmp(response_cache[i].vary_key, vary_key) == 0 &&
            time(NULL) - response_cache[i].timestamp < CACHE_TIMEOUT) {
            LOG_DEBUG("Cache hit for %s with vary key %s", path, vary_key);
            return &response_cache[i];
        }
    }
    LOG_DEBUG("Cache miss for %s with vary key %s", path, vary_key);
    return NULL;
}

static void cache_response(const char *path, const char *response, size_t response_len, const http_request_t *request) {
    cache_entry_t *entry = &response_cache[cache_index];
    
    if (entry->response) {
        free(entry->response);
        entry->response = NULL;
    }
    
    strncpy(entry->path, path, PATH_MAX - 1);
    entry->path[PATH_MAX - 1] = '\0';
    
    generate_vary_key(path, request, entry->vary_key, sizeof(entry->vary_key));
    
    entry->response = malloc(response_len);
    if (entry->response) {
        memcpy(entry->response, response, response_len);
        entry->response_len = response_len;
        entry->timestamp = time(NULL);
        LOG_DEBUG("Cached response for %s with vary key %s", path, entry->vary_key);
    } else {
        LOG_ERROR("Failed to allocate memory for cached response");
    }
    
    cache_index = (cache_index + 1) % CACHE_SIZE;
}

int http_parse_request(const char *buffer, size_t length, http_request_t *request) {
    char *line_start = (char *)buffer;
    char *line_end;
    
    request->keep_alive = 0;
    
    line_end = strstr(line_start, "\r\n");
    if (!line_end) return -1;
    
    char method[16], uri[2048], version[16];
    if (sscanf(line_start, "%15s %2047s %15s", method, uri, version) != 3) {
        return -1;
    }
    
    strncpy(request->method, method, sizeof(request->method) - 1);
    strncpy(request->uri, uri, sizeof(request->uri) - 1);
    strncpy(request->version, version, sizeof(request->version) - 1);
    
    line_start = line_end + 2;
    request->header_count = 0;
    
    while (line_start < (char *)buffer + length && request->header_count < MAX_HEADERS) {
        line_end = strstr(line_start, "\r\n");
        if (!line_end) break;
        
        if (line_end == line_start) break;
        
        char *colon = strchr(line_start, ':');
        if (colon) {
            *colon = '\0';
            char *value = colon + 1;
            while (*value == ' ') value++;
            
            strncpy(request->headers[request->header_count][0], line_start, MAX_HEADER_SIZE - 1);
            strncpy(request->headers[request->header_count][1], value, MAX_HEADER_SIZE - 1);
            
            if (strcasecmp(line_start, "Connection") == 0) {
                LOG_DEBUG("Found Connection header: %s", value);
            }
            
            request->header_count++;
        }
        
        line_start = line_end + 2;
    }
    
    request->keep_alive = (strcmp(request->version, "HTTP/1.1") == 0);
    
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "Connection") == 0) {
            if (strcasecmp(request->headers[i][1], "close") == 0) {
                request->keep_alive = 0;
                LOG_DEBUG("Connection: close header found, disabling keep-alive");
            } else if (strcasecmp(request->headers[i][1], "keep-alive") == 0) {
                request->keep_alive = 1;
                LOG_DEBUG("Connection: keep-alive header found, enabling keep-alive");
            }
            break;
        }
    }
    
    LOG_DEBUG("Request parsed: %s %s %s, keep-alive=%d", 
              request->method, request->uri, request->version, request->keep_alive);
    
    return 0;
}

void http_create_response(http_response_t *response, int status_code) {
    memset(response, 0, sizeof(http_response_t));
    response->status_code = status_code;
    response->keep_alive = 0;  
    
    for (int i = 0; status_messages[i].code != 0; i++) {
        if (status_messages[i].code == status_code) {
            response->status_text = status_messages[i].text;
            break;
        }
    }
    
    http_add_header(response, "Server", "NxLite");
}

void http_add_header(http_response_t *response, const char *name, const char *value) {
    if (response->header_count < MAX_HEADERS) {
        strncpy(response->headers[response->header_count][0], name, MAX_HEADER_SIZE - 1);
        strncpy(response->headers[response->header_count][1], value, MAX_HEADER_SIZE - 1);
        response->header_count++;
    }
}

const char *http_get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return mime_types[0].type;
    
    for (int i = 0; mime_types[i].ext != NULL; i++) {
        if (strcasecmp(ext, mime_types[i].ext) == 0) {
            return mime_types[i].type;
        }
    }
    
    return mime_types[0].type;
}

int http_serve_file(const char *path, http_response_t *response, const http_request_t *request) {
    char full_path[PATH_MAX];
    
    strncpy(full_path, path, PATH_MAX - 1);
    full_path[PATH_MAX - 1] = '\0';
    
    size_t path_len = strlen(full_path);
    if (full_path[path_len - 1] == '/') {
        strncat(full_path, "index.html", PATH_MAX - path_len - 1);
    }
    
    LOG_DEBUG("Serving file: %s", full_path);
    
    cache_entry_t *cache = find_cached_response(full_path, request);
    if (cache) {
        LOG_DEBUG("Using cached response for %s", full_path);
        response->is_cached = 1;
        response->cached_response = cache->response;
        response->body_length = cache->response_len;
        return 0;
    }
    
    int file_fd = open(full_path, O_RDONLY | O_NONBLOCK);
    if (file_fd == -1) {
        LOG_WARN("Failed to open file %s: %s", full_path, strerror(errno));
        return -1;
    }
    
    struct stat st;
    if (fstat(file_fd, &st) == -1) {
        LOG_ERROR("Failed to stat file %s: %s", full_path, strerror(errno));
        close(file_fd);
        return -1;
    }
    
    if (!S_ISREG(st.st_mode)) {
        LOG_WARN("Not a regular file: %s", full_path);
        close(file_fd);
        return -1;
    }
    
    response->body_length = st.st_size;
    response->file_fd = file_fd;
    response->is_file = 1;
    
    const char *mime_type = http_get_mime_type(full_path);
    http_add_header(response, "Content-Type", mime_type);
    
    char content_length[32];
    snprintf(content_length, sizeof(content_length), "%ld", (long)st.st_size);
    http_add_header(response, "Content-Length", content_length);
    
    char last_modified[64];
    struct tm *tm_info = gmtime(&st.st_mtime);
    strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    http_add_header(response, "Last-Modified", last_modified);
    
    char etag[64];
    snprintf(etag, sizeof(etag), "\"%lx-%lx-%lx\"", 
             (unsigned long)st.st_ino, 
             (unsigned long)st.st_size, 
             (unsigned long)st.st_mtime);
    http_add_header(response, "ETag", etag);
    
    http_add_header(response, "Vary", "Accept-Encoding, User-Agent");
    
    const char *ext = strrchr(full_path, '.');
    if (ext) {
        if (strcasecmp(ext, ".css") == 0 || strcasecmp(ext, ".js") == 0) {
            http_add_header(response, "Cache-Control", "public, max-age=86400, must-revalidate");
        } 
        else if (strcasecmp(ext, ".png") == 0 || 
                 strcasecmp(ext, ".jpg") == 0 || 
                 strcasecmp(ext, ".jpeg") == 0 || 
                 strcasecmp(ext, ".gif") == 0 || 
                 strcasecmp(ext, ".ico") == 0) {
            http_add_header(response, "Cache-Control", "public, max-age=604800, immutable");
        }
        else if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) {
            http_add_header(response, "Cache-Control", "public, max-age=300, must-revalidate");
        }
        else if (strcasecmp(ext, ".pdf") == 0 || 
                 strcasecmp(ext, ".doc") == 0 || 
                 strcasecmp(ext, ".docx") == 0) {
            http_add_header(response, "Cache-Control", "public, max-age=86400");
        }
        else {
            http_add_header(response, "Cache-Control", "public, max-age=3600");
        }
        
        if (st.st_size < 1024 * 1024) {
            char *file_content = malloc(st.st_size);
            if (file_content) {
                ssize_t bytes_read = pread(file_fd, file_content, st.st_size, 0);
                if (bytes_read == st.st_size) {
                    char header[4096];
                    int header_len = 0;
                    
                    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                          "HTTP/1.1 200 OK\r\n");
                    
                    for (int i = 0; i < response->header_count; i++) {
                        header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                             "%s: %s\r\n", 
                                             response->headers[i][0], 
                                             response->headers[i][1]);
                    }
                    
                    header_len += snprintf(header + header_len, sizeof(header) - header_len,
                                          "Connection: keep-alive\r\n");
                    
                    header_len += snprintf(header + header_len, sizeof(header) - header_len, "\r\n");
                    
                    char *complete_response = malloc(header_len + st.st_size);
                    if (complete_response) {
                        memcpy(complete_response, header, header_len);
                        memcpy(complete_response + header_len, file_content, st.st_size);
                        cache_response(full_path, complete_response, header_len + st.st_size, request);
                        free(complete_response);
                    }
                }
                free(file_content);
            }
        }
    } else {
        http_add_header(response, "Cache-Control", "no-cache, no-store, must-revalidate");
    }
    
    return 0;
}

int http_should_keep_alive(const http_request_t *request) {
    if (strcmp(request->version, "HTTP/1.1") == 0) {
        for (int i = 0; i < request->header_count; i++) {
            if (strcasecmp(request->headers[i][0], "Connection") == 0) {
                if (strcasecmp(request->headers[i][1], "close") == 0) {
                    LOG_DEBUG("HTTP/1.1 request with Connection: close, disabling keep-alive");
                    return 0;
                }
                break;
            }
        }
        LOG_DEBUG("HTTP/1.1 request without Connection: close, enabling keep-alive");
        return 1;
    }
    
    if (strcmp(request->version, "HTTP/1.0") == 0) {
        for (int i = 0; i < request->header_count; i++) {
            if (strcasecmp(request->headers[i][0], "Connection") == 0) {
                if (strcasecmp(request->headers[i][1], "keep-alive") == 0) {
                    LOG_DEBUG("HTTP/1.0 request with Connection: keep-alive, enabling keep-alive");
                    return 1;
                }
                break;
            }
        }
        LOG_DEBUG("HTTP/1.0 request without Connection: keep-alive, disabling keep-alive");
        return 0;
    }
    
    LOG_DEBUG("Unknown HTTP version, disabling keep-alive");
    return 0;
}

int http_send_response(int client_fd, http_response_t *response) {
    if (response->is_cached && response->cached_response) {
        ssize_t total_sent = 0;
        size_t remaining = response->body_length;
        const char *ptr = response->cached_response;
        
        while (remaining > 0) {
            ssize_t sent = send(client_fd, ptr + total_sent, remaining, MSG_NOSIGNAL);
            if (sent <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return 0;  
                } else if (errno == EPIPE || errno == ECONNRESET) {
                    LOG_DEBUG("Client disconnected during send: %s", strerror(errno));
                    return -1;
                }
                LOG_ERROR("Failed to send cached response: %s", strerror(errno));
                return -1;
            }
            
            total_sent += sent;
            remaining -= sent;
        }
        
        return 1;  
    }
    
    int header_len = 0;
    
    header_len += snprintf(header_buffer + header_len, sizeof(header_buffer) - header_len,
                          "HTTP/1.1 %d %s\r\n", 
                          response->status_code, 
                          response->status_text ? response->status_text : "Unknown");
    
    for (int i = 0; i < response->header_count; i++) {
        header_len += snprintf(header_buffer + header_len, sizeof(header_buffer) - header_len,
                              "%s: %s\r\n", 
                              response->headers[i][0], 
                              response->headers[i][1]);
    }
    
    if (response->keep_alive) {
        header_len += snprintf(header_buffer + header_len, sizeof(header_buffer) - header_len,
                              "Connection: keep-alive\r\n");
    } else {
        header_len += snprintf(header_buffer + header_len, sizeof(header_buffer) - header_len,
                              "Connection: close\r\n");
    }
    
    header_len += snprintf(header_buffer + header_len, sizeof(header_buffer) - header_len, "\r\n");
    
    if (response->is_file && response->file_fd >= 0) {
        ssize_t sent = send(client_fd, header_buffer, header_len, MSG_MORE | MSG_NOSIGNAL);
        if (sent != header_len) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            } else if (errno == EPIPE || errno == ECONNRESET) {
                LOG_DEBUG("Client disconnected during header send: %s", strerror(errno));
                return -1;
            }
            LOG_ERROR("Failed to send headers: %s", strerror(errno));
            return -1;
        }
        
        off_t offset = response->file_offset; 
        size_t total_sent = 0;
        size_t remaining = response->body_length - offset;
        
        const size_t CHUNK_SIZE = 1024 * 1024;
        
        while (remaining > 0) {
            size_t to_send = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
            ssize_t sent = sendfile(client_fd, response->file_fd, &offset, to_send);
            
            if (sent <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    response->file_offset = offset;
                    return 0;  
                } else if (errno == EPIPE || errno == ECONNRESET) {
                    LOG_DEBUG("Client disconnected during file send: %s", strerror(errno));
                    return -1;
                }
                LOG_ERROR("Failed to send file: %s", strerror(errno));
                return -1;
            }
            
            total_sent += sent;
            remaining -= sent;
        }
        
        int off = 0;
        setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
        
        return 1;  
    }
    
    if (response->body && response->body_length > 0) {
        ssize_t sent = send(client_fd, header_buffer, header_len, MSG_MORE | MSG_NOSIGNAL);
        if (sent != header_len) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  
            } else if (errno == EPIPE || errno == ECONNRESET) {
                LOG_DEBUG("Client disconnected during header send: %s", strerror(errno));
                return -1;
            }
            LOG_ERROR("Failed to send headers: %s", strerror(errno));
            return -1;
        }
        
        sent = send(client_fd, response->body, response->body_length, MSG_NOSIGNAL);
        if (sent != (ssize_t)response->body_length) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  
            } else if (errno == EPIPE || errno == ECONNRESET) {
                LOG_DEBUG("Client disconnected during body send: %s", strerror(errno));
                return -1;
            }
            LOG_ERROR("Failed to send body: %s", strerror(errno));
            return -1;
        }
        
        return 1;  
    }
    
    ssize_t sent = send(client_fd, header_buffer, header_len, MSG_NOSIGNAL);
    if (sent != header_len) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  
        } else if (errno == EPIPE || errno == ECONNRESET) {
            LOG_DEBUG("Client disconnected during header send: %s", strerror(errno));
            return -1;
        }
        LOG_ERROR("Failed to send headers: %s", strerror(errno));
        return -1;
    }
    
    return 1;  
}

void http_free_response(http_response_t *response) {
    if (response->is_file && response->file_fd != -1) {
        close(response->file_fd);
    }
}

void http_handle_request(const http_request_t *request, http_response_t *response) {
    http_create_response(response, 200);

    int is_head = 0;
    if (strcmp(request->method, "GET") == 0) {
        is_head = 0;
    } else if (strcmp(request->method, "HEAD") == 0) {
        is_head = 1;
    } else {
        response->status_code = 501;
        response->status_text = "Not Implemented";
        response->keep_alive = 0;  
        return;
    }

    config_t *config = config_get_instance();

    char file_path[PATH_MAX];
    const char *request_path = strcmp(request->uri, "/") == 0 ? "/index.html" : request->uri;

    size_t root_len = strlen(config->root_dir);
    size_t path_len = strlen(request_path);

    if (root_len + path_len >= sizeof(file_path)) {
        LOG_ERROR("Path too long: %s%s", config->root_dir, request_path);
        response->status_code = 414;  
        response->status_text = "Request-URI Too Long";
        response->keep_alive = 0;  
        return;
    }

    int written = snprintf(file_path, sizeof(file_path), "%s%s", config->root_dir, request_path);
    if (written < 0 || (size_t)written >= sizeof(file_path)) {
        LOG_ERROR("Path truncation occurred: %s%s", config->root_dir, request_path);
        response->status_code = 414;  
        response->status_text = "Request-URI Too Long";
        response->keep_alive = 0;  
        return;
    }
    
    cache_entry_t *cache = find_cached_response(file_path, request);
    if (cache) {
        LOG_DEBUG("Using cached response for %s", file_path);
        response->is_cached = 1;
        response->cached_response = cache->response;
        response->body_length = cache->response_len;
        response->keep_alive = http_should_keep_alive(request);
        
        if (is_head) {
            response->body_length = 0;
        }
        
        return;
    }

    struct stat st;
    if (stat(file_path, &st) == -1) {
        LOG_WARN("File not found: %s", file_path);
        response->status_code = 404;
        response->status_text = "Not Found";
        response->keep_alive = 0;
        return;
    }

    const char* if_none_match = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "If-None-Match") == 0) {
            if_none_match = request->headers[i][1];
            break;
        }
    }

    const char* if_modified_since = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "If-Modified-Since") == 0) {
            if_modified_since = request->headers[i][1];
            break;
        }
    }

    char etag[64];
    snprintf(etag, sizeof(etag), "\"%lx-%lx-%lx\"", 
             (unsigned long)st.st_ino, 
             (unsigned long)st.st_size, 
             (unsigned long)st.st_mtime);

    if (if_none_match) {
        LOG_DEBUG("Checking ETag: client sent '%s', server has '%s'", if_none_match, etag);
        
        char if_none_match_copy[1024];
        strncpy(if_none_match_copy, if_none_match, sizeof(if_none_match_copy) - 1);
        if_none_match_copy[sizeof(if_none_match_copy) - 1] = '\0';
        
        char *token = strtok(if_none_match_copy, ",");
        int matched = 0;
        
        while (token && !matched) {
            while (isspace(*token)) token++;
            
            if (strcmp(token, "*") == 0) {
                matched = 1;
                break;
            }
            
            char clean_token[256] = {0};
            const char *start = token;
            if (*start == 'W' && *(start+1) == '/') {
                start += 2;
            }
            
            if (*start == '"') {
                start++;
                const char *end = strrchr(start, '"');
                if (end) {
                    size_t len = end - start;
                    strncpy(clean_token, start, len);
                    clean_token[len] = '\0';
                } else {
                    strncpy(clean_token, start, sizeof(clean_token) - 1);
                }
            } else {
                strncpy(clean_token, start, sizeof(clean_token) - 1);
            }
            
            char *p = clean_token + strlen(clean_token) - 1;
            while (p >= clean_token && isspace(*p)) {
                *p = '\0';
                p--;
            }
            
            char our_etag[64] = {0};
            const char *etag_start = etag;
            if (*etag_start == '"') {
                etag_start++;
                const char *etag_end = strrchr(etag_start, '"');
                if (etag_end) {
                    size_t len = etag_end - etag_start;
                    strncpy(our_etag, etag_start, len);
                    our_etag[len] = '\0';
                }
            } else {
                strncpy(our_etag, etag_start, sizeof(our_etag) - 1);
            }
            
            LOG_DEBUG("Comparing cleaned ETags: client '%s' vs server '%s'", clean_token, our_etag);
            
            if (strcmp(clean_token, our_etag) == 0) {
                matched = 1;
                break;
            }
            
            token = strtok(NULL, ",");
        }
        
        if (matched) {
            LOG_DEBUG("ETag match found, returning 304 Not Modified");
            response->status_code = 304;
            response->status_text = "Not Modified";
            
            http_add_header(response, "ETag", etag);
            
            const char *ext = strrchr(file_path, '.');
            if (ext) {
                if (strcasecmp(ext, ".css") == 0 || strcasecmp(ext, ".js") == 0) {
                    http_add_header(response, "Cache-Control", "public, max-age=86400, must-revalidate");
                } 
                else if (strcasecmp(ext, ".png") == 0 || 
                         strcasecmp(ext, ".jpg") == 0 || 
                         strcasecmp(ext, ".jpeg") == 0 || 
                         strcasecmp(ext, ".gif") == 0 || 
                         strcasecmp(ext, ".ico") == 0) {
                    http_add_header(response, "Cache-Control", "public, max-age=604800, immutable");
                }
                else if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) {
                    http_add_header(response, "Cache-Control", "public, max-age=300, must-revalidate");
                }
                else {
                    http_add_header(response, "Cache-Control", "public, max-age=3600");
                }
            }
            
            http_add_header(response, "Vary", "Accept-Encoding, User-Agent");
            
            response->keep_alive = http_should_keep_alive(request);
            return;
        }
    }

    if (if_modified_since) {
        LOG_DEBUG("Checking If-Modified-Since: %s", if_modified_since);
        
        struct tm tm_since;
        memset(&tm_since, 0, sizeof(struct tm));
        int parsed = 0;
        
        if (!parsed && strptime(if_modified_since, "%a, %d %b %Y %H:%M:%S GMT", &tm_since) != NULL) {
            parsed = 1;
        }
        
        if (!parsed && strptime(if_modified_since, "%A, %d-%b-%y %H:%M:%S GMT", &tm_since) != NULL) {
            parsed = 1;
        }
        
        if (!parsed && strptime(if_modified_since, "%a %b %d %H:%M:%S %Y", &tm_since) != NULL) {
            parsed = 1;
        }
        
        if (parsed) {
            tm_since.tm_isdst = -1;
            time_t since_time = mktime(&tm_since);
            
            if (since_time != -1) {
                since_time += timezone;
                
                struct tm *tm_file = gmtime(&st.st_mtime);
                char file_time_str[64];
                strftime(file_time_str, sizeof(file_time_str), "%a, %d %b %Y %H:%M:%S GMT", tm_file);
                
                LOG_DEBUG("Comparing times: file time %s (%ld) vs if-modified-since %s (%ld)", 
                          file_time_str, (long)st.st_mtime, if_modified_since, (long)since_time);
                
                if (difftime(st.st_mtime, since_time) <= 0) {
                    LOG_DEBUG("File not modified since %s, returning 304", if_modified_since);
                    response->status_code = 304;
                    response->status_text = "Not Modified";
                    
                    http_add_header(response, "ETag", etag);
                    
                    char last_modified[64];
                    strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S GMT", tm_file);
                    http_add_header(response, "Last-Modified", last_modified);
                    http_add_header(response, "Vary", "Accept-Encoding, User-Agent");
                    
                    response->keep_alive = http_should_keep_alive(request);
                    return;
                } else {
                    LOG_DEBUG("File was modified since %s, returning full response", if_modified_since);
                }
            } else {
                LOG_WARN("Failed to convert parsed time to time_t");
            }
        } else {
            LOG_WARN("Failed to parse If-Modified-Since date: %s", if_modified_since);
        }
    }

    if (http_serve_file(file_path, response, request) != 0) {
        response->status_code = 404;
        response->status_text = "Not Found";
        response->keep_alive = 0;  
        return;
    }

    response->keep_alive = http_should_keep_alive(request);
    
    if (response->keep_alive) {
        char timeout_str[32];
        snprintf(timeout_str, sizeof(timeout_str), "timeout=%d", config->keep_alive_timeout);
        http_add_header(response, "Keep-Alive", timeout_str);
        LOG_DEBUG("Keep-alive enabled for request: %s %s", request->method, request->uri);
    } else {
        LOG_DEBUG("Keep-alive disabled for request: %s %s", request->method, request->uri);
    }

    if (is_head) {
        response->is_file = 0;
        response->is_cached = 0;
        response->body_length = 0;
    }
} 