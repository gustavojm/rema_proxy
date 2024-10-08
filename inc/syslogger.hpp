#pragma once

#include <cstdarg>
#include <cstdlib>
#include <memory>
#include <restbed>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

class SyslogLogger : public restbed::Logger {
  public:
    void stop() {
        return;
    }

    void start(const std::shared_ptr<const restbed::Settings>&) {
        return;
    }

    void log(const Level level, const char *format, ...) {
        setlogmask(LOG_UPTO(LOG_DEBUG));
        openlog("JSON_proxy Restbed", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        int priority = 0;
        switch (level) {
        case FATAL: priority = LOG_CRIT; break;

        case ERROR: priority = LOG_ERR; break;

        case WARNING: priority = LOG_WARNING; break;

        case SECURITY: priority = LOG_ALERT; break;

        case INFO:
        case DEBUG:
        default: priority = LOG_NOTICE;
        }

        va_list arguments;
        va_start(arguments, format);
        vsyslog(priority, format, arguments);
        va_end(arguments);
        closelog();
    }

    void log_if(bool expression, const Level level, const char* format, ...) {
        if (expression) {
            va_list arguments;
            va_start(arguments, format);
            log(level, format, arguments);
            va_end(arguments);
        }
    }
};
