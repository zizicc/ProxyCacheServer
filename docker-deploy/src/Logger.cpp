#include "Logger.h"
#include <iostream>
#include <ctime>
#include <sstream>

// Singleton instance getter
Logger& Logger::get_instance() {
    static Logger instance; // Single instance
    return instance;
}

// Private constructor: Opens log file
Logger::Logger() {
    log_file.open("/var/log/erss/proxy.log", std::ios::trunc);
    
    // Fallback to local log if system log fails
    if (!log_file.is_open()) {
        std::cerr << "ERROR: Failed to open /var/log/erss/proxy.log! Logging to ./proxy.log instead.\n";
        log_file.open("proxy.log", std::ios::trunc);
    }

    if (!log_file.is_open()) {
        std::cerr << "ERROR: Failed to open ANY log file! Logging is disabled.\n";
    }
}

// Destructor: Closes log file
Logger::~Logger() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

// Get current time in UTC format
std::string Logger::get_current_time() const {
    std::time_t now = std::time(nullptr);
    char buf[100];
    //strftime(buf, sizeof(buf), "%a %d %b %Y %H:%M:%S", gmtime(&now)); // Ensure GMT formatting
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", gmtime(&now));
    return std::string(buf);
}

// Thread-safe logging function
void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);

    // Output to terminal
    std::cout << message << std::endl;

    if (log_file.is_open()) {
        log_file << message << std::endl;
    } else {
        std::cerr << "ERROR: Log file is not open. Cannot write log entry: " << message << std::endl;
    }
}

// Log request
void Logger::log_request(int id, const std::string& request, const std::string& client_ip) {
    log(std::to_string(id) + ": \"" + request + "\" from " + client_ip + " @ " + get_current_time());
}

// Log cache status
void Logger::log_cache_status(int id, const std::string& status) {
    log(std::to_string(id) + ": " + status);
    std::cout << "logged! cache!" << std::endl;
}

// Log request forwarding to origin server
void Logger::log_forward_request(int id, const std::string& request, const std::string& server) {
    std::istringstream request_stream(request);
    std::string first_line;
    if (std::getline(request_stream, first_line)) {
        log(std::to_string(id) + ": Requesting \"" + first_line + "\" from " + server);
    } else {
        log_error(id, "Malformed request string in log_forward_request");
    }
}

// Log response received from origin server
void Logger::log_received_response(int id, const std::string& response, const std::string& server) {
    log(std::to_string(id) + ": Received \"" + response + "\" from " + server);
}

// Log final response sent to client
void Logger::log_response(int id, const std::string& response) {
    log(std::to_string(id) + ": Responding \"" + response + "\"");
}

// Log errors
void Logger::log_error(int id, const std::string& message) {
    std::ostringstream log_entry;
    log_entry << id << ": ERROR " << message;

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_file.is_open()) {
            log_file << log_entry.str() << std::endl;
        } else {
            std::cerr << log_entry.str() << std::endl;
        }
    }
}

// Log warnings
void Logger::log_warning(int id, const std::string& message) {
    log(std::to_string(id) + ": WARNING " + message);
}

// Log notes
void Logger::log_note(int id, const std::string& message) {
    log(std::to_string(id) + ": NOTE " + message);
}

// Log tunnel closure
void Logger::log_tunnel_closed(int id) {
    log(std::to_string(id) + ": Tunnel closed");
}