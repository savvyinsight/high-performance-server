#include "../include/Logger.h"
#include <iostream>
#include <ctime>

Logger &Logger::instance() {
    static Logger lg;
    return lg;
}

Logger::Logger() {}

Logger::~Logger() {
    if (ofs_.is_open()) ofs_.close();
}

void Logger::init(const std::string &path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (ofs_.is_open()) ofs_.close();
    ofs_.open(path, std::ios::app);
    to_file_ = ofs_.is_open();
}

void Logger::log(Level lvl, const std::string &msg) {
    std::lock_guard<std::mutex> lk(mtx_);
    const char *lvl_s = "INFO";
    switch (lvl) {
        case DEBUG: lvl_s = "DEBUG"; break;
        case INFO: lvl_s = "INFO"; break;
        case WARN: lvl_s = "WARN"; break;
        case ERROR: lvl_s = "ERROR"; break;
    }

    std::time_t t = std::time(nullptr);
    char timebuf[64];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    std::string out = std::string("[") + timebuf + "] [" + lvl_s + "] " + msg + "\n";

    // always echo to stdout as well
    std::fwrite(out.data(), 1, out.size(), stdout);

    if (to_file_) {
        ofs_ << out;
        ofs_.flush();
    }
}
