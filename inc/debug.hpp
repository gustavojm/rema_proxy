#ifndef DEBUG_H_
#define DEBUG_H_

#include <cstdarg>
#include <iostream>
#include <string_view>
#include <experimental/source_location>

#define DEBUG_ENABLED  1

enum debugLevels {
    Debug,
    Info,
    InfoLocal,
    Warn,
    Error,
};

extern enum debugLevels debugLevel;

static inline std::string levelText(enum debugLevels level) {
    const char *ret;
    switch (level) {
    case Debug: ret = "Debug"; break;
    case Info:
    case InfoLocal: ret = "Info"; break;
    case Warn: ret = "Warn"; break;
    case Error: ret = "Error"; break;
    default: ret = ""; break;
    }
    return ret;
}

template<typename... Args> void TraceLoc(std::string level, const std::experimental::source_location &location, const char *format, ...) {
    std::cout << "[" << level << "]" << location.file_name() << ":" << location.line() << " - ";
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    std::cout << std::endl; 
}

/**
 * controls how much debug output is produced. Higher values produce more
 * output. See the use in <tt>lDebug()</tt>.
 */
extern enum debugLevels debugLevel;

#define lDebug(level, format, ...)                                                                                  \
    do {                                                                                                            \
        if (DEBUG_ENABLED && (debugLevel <= level)) {                                                               \
            TraceLoc(levelText(level), std::experimental::source_location::current(), format, ##__VA_ARGS__);       \
        }                                                                                                           \
    } while (0)

#endif
