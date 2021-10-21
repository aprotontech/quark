#ifndef LOGS_H
#define LOGS_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_LOG_LEVEL LOG_INFO
#define LOG_MAX_LENGTH 128

#define LOG_NONE        0 // No log output
#define LOG_ERROR       1 // Critical errors, software module can not recover on its own
#define LOG_WARN        2 // Error conditions from which recovery measures have been taken
#define LOG_INFO        3 // Information messages which describe normal flow of events
#define LOG_DEBUG       4 // Extra information which is not necessary for normal use (values, pointers, sizes, etc).
#define LOG_VERBOSE     5 // Bigger chunks of debugging information, or frequent messages which can potentially flood the output.

/*
 * param level 0=NONE、1=ERROR、2=WARN、3=INFO、4=DEBUG、5=VERBOSE
 * @return 0 when success
 */
int logs_set_level(int level);

/*
 * enable=0 will disable color, it is enable by default.
 */
void log_set_color(int enable);

void log_write_v(int level, const char *filename, int line, const char *fmt, ...) __attribute__ ((format (printf, 4, 5)));

void log_write_vlist(int level, const char *filename, int line, const char *fmt, va_list args);

#define LOG_FORMAT(format)  "%s: " format
#define LOGE(tag, format, ... )  log_write_v(LOG_ERROR,   __FILE__, __LINE__, LOG_FORMAT(format), tag, ##__VA_ARGS__);
#define LOGW(tag, format, ... )  log_write_v(LOG_WARN,    __FILE__, __LINE__, LOG_FORMAT(format), tag, ##__VA_ARGS__);
#define LOGI(tag, format, ... )  log_write_v(LOG_INFO,    __FILE__, __LINE__, LOG_FORMAT(format), tag, ##__VA_ARGS__);
#define LOGD(tag, format, ... )  log_write_v(LOG_DEBUG,   __FILE__, __LINE__, LOG_FORMAT(format), tag, ##__VA_ARGS__);
#define LOGV(tag, format, ... )  log_write_v(LOG_VERBOSE, __FILE__, __LINE__, LOG_FORMAT(format), tag, ##__VA_ARGS__);

#ifdef __cplusplus
}
#endif

#endif /* LOGS_H */
