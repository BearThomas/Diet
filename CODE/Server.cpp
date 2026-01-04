// Server.cpp - Custom HTTP Server
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    #pragma comment(lib, "Ws2_32.lib")
    #pragma comment(lib, "Mswsock.lib")
    
    #define close closesocket
    #define read recv
    #define write send
    #define socklen_t int
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <errno.h>
#endif

using namespace std;

// Configuration
const int PORT = 8080;
const int BUFFER_SIZE = 8192;
const int MAX_CONNECTIONS = 10;
const string SERVER_NAME = "MyHttpServer/1.0";

// MIME types
map<string, string> mime_types = {
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".js", "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain; charset=utf-8"},
    {".svg", "image/svg+xml"}
};

// Function declarations
bool init_network();
void cleanup_network();
int create_server_socket();
void handle_client(int client_socket);
void handle_request(int client_socket, const string& request);
string get_mime_type(const string& filename);
string read_file(const string& filename);
string url_decode(const string& encoded);
void send_response(int client_socket, int status_code, 
                   const string& content_type, const string& body);
string generate_error_page(int status_code, const string& message);
void log_message(const string& message);

// Main function
int main() {
    cout << "==================================" << endl;
    cout << "   Custom HTTP Server v1.0       " << endl;
    cout << "==================================" << endl;
    
    // Initialize network
    if (!init_network()) {
        cerr << "Network initialization failed" << endl;
        return 1;
    }
    
    // Create server socket
    int server_socket = create_server_socket();
    if (server_socket < 0) {
        cleanup_network();
        return 1;
    }
    
    // Server info
    log_message("Server started on port " + to_string(PORT));
    log_message("Open: http://localhost:" + to_string(PORT));
    log_message("Press Ctrl+C to stop");
    cout << endl;
    
    // Main server loop
    try {
        while (true) {
            // Wait for client connection
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            log_message("Waiting for connection...");
            
            int client_socket = accept(server_socket, 
                                      (struct sockaddr*)&client_addr,
                                      &client_len);
            
            if (client_socket < 0) {
                log_message("Accept failed");
                continue;
            }
            
            // Get client IP
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            log_message("Client connected: " + string(client_ip));
            
            // Handle client
            handle_client(client_socket);
        }
    }
    catch (const exception& e) {
        log_message("Error: " + string(e.what()));
    }
    catch (...) {
        log_message("Unknown error");
    }
    
    // Cleanup
    log_message("Shutting down...");
    close(server_socket);
    cleanup_network();
    
    log_message("Server stopped");
    return 0;
}

// Network initialization
bool init_network() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log_message("WSAStartup failed: " + to_string(result));
        return false;
    }
#endif
    return true;
}

// Network cleanup
void cleanup_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Logging
void log_message(const string& message) {
    time_t now = time(nullptr);
    char time_buf[80];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    cout << "[" << time_buf << "] " << message << endl;
}

// Create server socket
int create_server_socket() {
    // Create socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        log_message("Socket creation failed");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
              (char*)&opt, sizeof(opt));
    
    // Bind address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_message("Bind failed on port " + to_string(PORT));
        close(server_socket);
        return -1;
    }
    
    // Listen
    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        log_message("Listen failed");
        close(server_socket);
        return -1;
    }
    
    return server_socket;
}

// Handle client connection
void handle_client(int client_socket) {
    // Set receive timeout
#ifdef _WIN32
    int timeout = 5000;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, 
              (char*)&timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, 
              &timeout, sizeof(timeout));
#endif
    
    // Read request
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        string request(buffer);
        
        // Log request line
        size_t line_end = request.find("\r\n");
        if (line_end != string::npos) {
            log_message("Request: " + request.substr(0, line_end));
        }
        
        // Handle request
        handle_request(client_socket, request);
    }
    else if (bytes_received == 0) {
        log_message("Client disconnected");
    }
    else {
        log_message("Receive error");
    }
    
    close(client_socket);
}

// Parse request line
bool parse_request_line(const string& line, 
                       string& method, 
                       string& path) {
    size_t space1 = line.find(' ');
    if (space1 == string::npos) return false;
    
    size_t space2 = line.find(' ', space1 + 1);
    if (space2 == string::npos) return false;
    
    method = line.substr(0, space1);
    path = line.substr(space1 + 1, space2 - space1 - 1);
    
    return true;
}

// URL decode
string url_decode(const string& encoded) {
    string decoded;
    
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            string hex_str = encoded.substr(i + 1, 2);
            int hex_value;
            stringstream ss;
            ss << hex << hex_str;
            ss >> hex_value;
            decoded += static_cast<char>(hex_value);
            i += 2;
        } 
        else if (encoded[i] == '+') {
            decoded += ' ';
        }
        else {
            decoded += encoded[i];
        }
    }
    
    return decoded;
}

// Check path safety
bool is_safe_path(const string& path) {
    if (path.find("..") != string::npos) return false;
    if (path.find("//") != string::npos) return false;
    if (path.find('\\') != string::npos) return false;
    return true;
}

// Get MIME type
string get_mime_type(const string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != string::npos) {
        string ext = filename.substr(dot_pos);
        string ext_lower = ext;
        transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        
        auto it = mime_types.find(ext_lower);
        if (it != mime_types.end()) {
            return it->second;
        }
    }
    
    return "application/octet-stream";
}

// Read file
string read_file(const string& filename) {
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) return "";
    
    streamsize size = file.tellg();
    if (size <= 0) return "";
    
    file.seekg(0, ios::beg);
    string content(size, '\0');
    
    if (!file.read(&content[0], size)) {
        return "";
    }
    
    return content;
}

// Get HTTP date
string get_http_date() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
    return string(buf);
}

// Build response headers
string build_response_headers(int status_code, 
                             const string& status_text,
                             const string& content_type,
                             size_t content_length) {
    stringstream headers;
    
    headers << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    headers << "Server: " << SERVER_NAME << "\r\n";
    headers << "Date: " << get_http_date() << "\r\n";
    headers << "Content-Type: " << content_type << "\r\n";
    headers << "Content-Length: " << content_length << "\r\n";
    headers << "Connection: close\r\n";
    headers << "\r\n";
    
    return headers.str();
}

// Send response
void send_response(int client_socket, 
                  int status_code,
                  const string& content_type,
                  const string& body) {
    
    map<int, string> status_texts = {
        {200, "OK"},
        {400, "Bad Request"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {500, "Internal Server Error"}
    };
    
    string status_text = "Unknown";
    if (status_texts.find(status_code) != status_texts.end()) {
        status_text = status_texts[status_code];
    }
    
    string headers = build_response_headers(status_code, status_text, 
                                           content_type, body.length());
    
    send(client_socket, headers.c_str(), headers.length(), 0);
    
    if (!body.empty()) {
        send(client_socket, body.c_str(), body.length(), 0);
    }
}

// Generate error page
string generate_error_page(int status_code, const string& message) {
    stringstream html;
    
    html << "<!DOCTYPE html>"
         << "<html>"
         << "<head>"
         << "<title>" << status_code << " " << message << "</title>"
         << "<style>"
         << "body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }"
         << "h1 { color: #333; }"
         << "p { color: #666; }"
         << ".container { max-width: 500px; margin: 0 auto; }"
         << "</style>"
         << "</head>"
         << "<body>"
         << "<div class='container'>"
         << "<h1>" << status_code << " - " << message << "</h1>"
         << "<p>Custom HTTP Server</p>"
         << "</div>"
         << "</body>"
         << "</html>";
    
    return html.str();
}

// Handle HTTP request
void handle_request(int client_socket, const string& request) {
    // Find end of headers
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == string::npos) {
        string error_page = generate_error_page(400, "Bad Request");
        send_response(client_socket, 400, "text/html", error_page);
        return;
    }
    
    string request_headers = request.substr(0, header_end);
    
    // Parse first line
    size_t line_end = request_headers.find("\r\n");
    if (line_end == string::npos) {
        string error_page = generate_error_page(400, "Bad Request");
        send_response(client_socket, 400, "text/html", error_page);
        return;
    }
    
    string request_line = request_headers.substr(0, line_end);
    string method, path;
    
    if (!parse_request_line(request_line, method, path)) {
        string error_page = generate_error_page(400, "Bad Request");
        send_response(client_socket, 400, "text/html", error_page);
        return;
    }
    
    // Check method
    if (method != "GET" && method != "HEAD") {
        string error_page = generate_error_page(405, "Method Not Allowed");
        send_response(client_socket, 405, "text/html", error_page);
        return;
    }
    
    // Check path safety
    if (!is_safe_path(path)) {
        string error_page = generate_error_page(403, "Forbidden");
        send_response(client_socket, 403, "text/html", error_page);
        return;
    }
    
    // Normalize path
    string filename = path;
    if (filename.empty() || filename == "/") {
        filename = "index.html";
    } else if (filename[0] == '/') {
        filename = filename.substr(1);
    }
    
    // URL decode
    filename = url_decode(filename);
    
    // Read file
    string content = read_file(filename);
    
    if (content.empty()) {
        string error_page = generate_error_page(404, "Not Found");
        send_response(client_socket, 404, "text/html", error_page);
        return;
    }
    
    // Get MIME type and send response
    string mime_type = get_mime_type(filename);
    send_response(client_socket, 200, mime_type, content);
    
    log_message("Served: " + filename + " (" + to_string(content.length()) + " bytes)");
}