#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <string>

class Logger {
private:
    std::mutex log_mutex;
    std::ofstream log_file;

    Logger();  // Private constructor for Singleton
    ~Logger(); // Destructor

    // Helper function to format current time
    std::string get_current_time() const;

    // Helper function to log messages
    void log(const std::string& message);

public:
    // Singleton Accessor
    static Logger& get_instance();

    // Logging functions
    void log_request(int id, const std::string& request, const std::string& client_ip);
    void log_cache_status(int id, const std::string& status);
    void log_forward_request(int id, const std::string& request, const std::string& server);
    void log_received_response(int id, const std::string& response, const std::string& server);
    void log_response(int id, const std::string& response);
    void log_error(int id, const std::string& message);
    void log_warning(int id, const std::string& message);
    void log_note(int id, const std::string& message);
    void log_tunnel_closed(int id);
};

#endif