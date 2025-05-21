#include "log.h"


static FILE *log_file = NULL;
static log_level_t current_level = LOG_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

int log_init(const char *filename) {
    if (log_file != NULL) {
        fclose(log_file);
    }

    log_file = fopen(filename, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return -1;
    }

    return 0;
}

void log_set_level(log_level_t level) {
    current_level = level;
}

static void get_timestamp(char *buffer, size_t size) {
    struct timeval tv;
    struct tm *tm;
    
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
    snprintf(buffer + strlen(buffer), size - strlen(buffer), ".%06ld", tv.tv_usec);
}

void log_message(log_level_t level, const char *format, ...) {
    if (level < current_level) {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    if (log_file != NULL) {
        fprintf(log_file, "[%s] [%s] ", timestamp, level_strings[level]);
        
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    if (level >= LOG_ERROR) {
        fprintf(stderr, "[%s] [%s] ", timestamp, level_strings[level]);
        
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        
        fprintf(stderr, "\n");
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_access(const char *client_ip, const char *method, const char *uri, 
                int status, long response_size) {
    pthread_mutex_lock(&log_mutex);

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    if (log_file != NULL) {
        fprintf(log_file, "%s - - [%s] \"%s %s\" %d %ld\n",
                client_ip, timestamp, method, uri, status, response_size);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_cleanup(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
    pthread_mutex_destroy(&log_mutex);
} 