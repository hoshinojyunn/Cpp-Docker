#include "logger.h"
#include <format>
#include <chrono>
#include <iostream>
std::string Logger::getCurrentTime(){
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    std::tm timeInfo = *std::localtime(&currentTime);

    std::ostringstream oss;
    oss << std::put_time(&timeInfo, "[%Y-%m-%d %H:%M:%S]");
    
    return oss.str();
}


void Logger::info(std::string_view sv){
    
    std::string msg = std::format("{}->{}", getCurrentTime(), sv);
    std::cout << msg << std::endl;
}

void Logger::error(std::string_view sv){
    std::string msg = std::format("{}error occur->{}", getCurrentTime(), sv);
    std::cerr << msg << std::endl;
}
