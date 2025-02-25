#include "Logger.h"
#include <iostream>

// construct, open log
Logger::Logger() {
    log_file.open("/var/log/erss/proxy.log", std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "ERROR: Failed to open log file!\n";
    }
}

// destruct, close log
Logger::~Logger() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

// get the current time（UTC format
std::string Logger::get_current_time() const {
    std::time_t now = std::time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", gmtime(&now));  // formalize the time
    return std::string(buf);
}

// record http request
void Logger::log_request(int id, const std::string& request, const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string log_entry = std::to_string(id) + ": \"" + request + "\" from " + client_ip + " @ " + get_current_time();
    std::cout << log_entry << std::endl;
    log_file << log_entry << std::endl;
}

// record caching status
void Logger::log_cache_status(int id, const std::string& status) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string log_entry = std::to_string(id) + ": " + status;
    std::cout << log_entry << std::endl;
    log_file << log_entry << std::endl;
}

// record http response
void Logger::log_response(int id, const std::string& response) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string log_entry = std::to_string(id) + ": Responding \"" + response + "\"";
    std::cout << log_entry << std::endl;
    log_file << log_entry << std::endl;
}

// record error
void Logger::log_error(int id, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string log_entry = std::to_string(id) + ": ERROR " + message;
    std::cerr << log_entry << std::endl;
    log_file << log_entry << std::endl;
}

// record http tunnel close
void Logger::log_tunnel_closed(int id) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string log_entry = std::to_string(id) + ": Tunnel closed";
    std::cout << log_entry << std::endl;
    log_file << log_entry << std::endl;
}