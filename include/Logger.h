// Logger.h
// Simple thread-safe logger with levels and file output.
#pragma once

#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };

    static Logger &instance();
    void init(const std::string &path);

    void log(Level lvl, const std::string &msg);

    // convenience
    void debug(const std::string &msg) { log(DEBUG, msg); }
    void info(const std::string &msg) { log(INFO, msg); }
    void warn(const std::string &msg) { log(WARN, msg); }
    void error(const std::string &msg) { log(ERROR, msg); }

private:
    Logger();
    ~Logger();
    std::mutex mtx_;
    std::ofstream ofs_;
    bool to_file_{false};
};
