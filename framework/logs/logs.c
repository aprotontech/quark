#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#if defined(__ANDROID__) || (ANDROID)
#include <android/log.h>
#endif

#include "logs.h"

#define LOG_COLOR_BLACK "30"
#define LOG_COLOR_RED "31"
#define LOG_COLOR_GREEN "32"
#define LOG_COLOR_BROWN "33"
#define LOG_COLOR_BLUE "34"
#define LOG_COLOR_PURPLE "35"
#define LOG_COLOR_CYAN "36"
#define LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define LOG_RESET_COLOR "\033[0m"
#define LOG_COLOR_E LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D LOG_COLOR(LOG_COLOR_PURPLE)
#define LOG_COLOR_V LOG_COLOR(LOG_COLOR_CYAN)

#define LOG_PREFIX_BUF_SIZE 128

static int g_level = DEFAULT_LOG_LEVEL;
static int g_enable_color = 0;

static const char *strip_path(const char *full_path);
static const char *str_level(int level);
static const char *COLORS[] = {LOG_COLOR_E, LOG_COLOR_W, LOG_COLOR_I,
                               LOG_COLOR_D, LOG_COLOR_V};
static void print_log(int level, const char *prefix, char *info);

void log_write_v(int level, const char *filename, int line, const char *fmt,
                 ...) {
    va_list args;
    va_start(args, fmt);
    log_write_vlist(level, filename, line, fmt, args);
    va_end(args);
}

void log_write_vlist(int level, const char *filename, int line, const char *fmt,
                     va_list args) {
    if (level > g_level) {
        return;
    }
    if (level <= LOG_NONE || level > LOG_VERBOSE) {
        return;
    }

#if defined(__ANDROID__) || (ANDROID)
    __android_log_vprint(ANDROID_LOG_DEBUG, "quark_jni", fmt, args);
#else
    char buffer[26] = {'\0'};
    int millisec = 0;
    struct tm tm_info;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    millisec = tv.tv_usec / 1000;
    localtime_r(&tv.tv_sec, &tm_info);
    strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", &tm_info);

#if (defined __QUARK_RTTHREAD__) || (defined __QUARK_FREERTOS__)
    char *prefix = malloc(LOG_PREFIX_BUF_SIZE);
    memset(prefix, 0, LOG_PREFIX_BUF_SIZE);
#else
    char prefix[LOG_PREFIX_BUF_SIZE] = {'\0'};
#endif
    snprintf(prefix, LOG_PREFIX_BUF_SIZE, "%s.%03d [%s] %s:%d", buffer,
             millisec, str_level(level), strip_path(filename), line);

    va_list tmpa;
    int required = 0;
    va_copy(tmpa, args);
    required = vsnprintf(NULL, 0, fmt, tmpa);
    va_end(tmpa);

    if (required < LOG_MAX_LENGTH) {
        char info[LOG_MAX_LENGTH] = {'\0'};
        vsnprintf(info, LOG_MAX_LENGTH, fmt, args);
        print_log(level, prefix, info);
    } else {
        char *info = (char *)malloc(required + 1);
        if (!info) {
            printf("malloc fail when logging\n");
#if (defined __QUARK_RTTHREAD__) || (defined __QUARK_FREERTOS__)
            free(prefix);
#endif
            return;
        }
        vsnprintf(info, (required + 1), fmt, args);
        print_log(level, prefix, info);
        free(info);
    }
#if (defined __QUARK_RTTHREAD__) || (defined __QUARK_FREERTOS__)
    free(prefix);
#endif
#endif
}

int logs_set_level(int level) {
    if (level < LOG_NONE || level > LOG_VERBOSE) {
        return -1;
    }
    g_level = level;
    return 0;
}

void log_set_color(int enable) { g_enable_color = enable; }

static const char *strip_path(const char *full_path) {
    if (full_path == NULL || strlen(full_path) < 2) {
        return full_path;
    }
    char *res = strrchr(full_path, '/');
    if (res == NULL) {
        return full_path;
    }
    return res + 1;
}

static const char *str_level(int level) {
    if (level == LOG_ERROR) {
        return "E";
    } else if (level == LOG_WARN) {
        return "W";
    } else if (level == LOG_INFO) {
        return "I";
    } else if (level == LOG_DEBUG) {
        return "D";
    } else if (level == LOG_VERBOSE) {
        return "V";
    }
    return "";
}

static void print_log(int level, const char *prefix, char *info) {
    // trim '\n' at the end
    if (strlen(info) > 0 && info[strlen(info) - 1] == '\n') {
        info[strlen(info) - 1] = '\0';
    }

    if (g_enable_color == 0) {
        printf("%s %s\n", prefix, info);
    } else {
        printf("%s%s %s" LOG_RESET_COLOR "\n", COLORS[level - 1], prefix, info);
    }
}
