#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <string>

class Logger {
private:
    std::mutex log_mutex;
    std::ofstream log_file;

public:
    Logger();
    ~Logger();
    void log_request(int id, const std::string& request, const std::string& client_ip);
    void log_cache_status(int id, const std::string& status);
    void log_response(int id, const std::string& response);
    void log_error(int id, const std::string& message);
    void log_tunnel_closed(int id);
    std::string get_current_time() const;
};

#endif