#ifndef HTTP_H
#define HTTP_H

#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <netinet/tcp.h> 
#include <errno.h>
#include <limits.h>  
#include <time.h>  
#include <stdint.h>
#include <stddef.h>  
#include <sys/types.h> 

#define MAX_HEADERS 32
#define MAX_HEADER_SIZE 1024
#define MAX_URI_SIZE 2048
#define MAX_METHOD_SIZE 16

typedef struct {
    char method[MAX_METHOD_SIZE];
    char uri[MAX_URI_SIZE];
    char version[16];
    char headers[MAX_HEADERS][2][MAX_HEADER_SIZE];
    int header_count;
    int keep_alive;  
} http_request_t;

typedef struct {
    int status_code;
    const char *status_text;
    char headers[MAX_HEADERS][2][MAX_HEADER_SIZE];
    int header_count;
    int keep_alive;
    int is_file;
    int is_cached;
    int file_fd;
    const char *cached_response;
    void *body;
    size_t body_length;
    off_t file_offset;
} http_response_t;

int http_parse_request(const char *buffer, size_t length, http_request_t *request);
void http_create_response(http_response_t *response, int status_code);
void http_add_header(http_response_t *response, const char *name, const char *value);
int http_send_response(int client_fd, http_response_t *response);
int http_serve_file(const char *path, http_response_t *response, const http_request_t *request);
const char *http_get_mime_type(const char *path);
void http_free_response(http_response_t *response);
int http_should_keep_alive(const http_request_t *request);
void http_handle_request(const http_request_t *request, http_response_t *response);

#endif 