#ifndef LOGGER_H
#define LOGGER_H

#include <string_view>


struct Logger{
    void info(std::string_view sv);
    void error(std::string_view sv);
    
private:
    std::string getCurrentTime();
};
static Logger log;
#endif