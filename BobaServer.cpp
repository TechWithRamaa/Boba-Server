#include <iostream>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <array>

const int MAX_EVENTS = 10;
const int PORT = 8080;
const int BACKLOG = 128;
const int BUFFER_SIZE = 1024;

// Thread pool to manage worker threads
class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    if (this->stop && this->tasks.empty()) return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) worker.join();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

// Utility to set a socket to non-blocking mode
void setNonBlocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

// Function to send HTTP response
void sendHttpResponse(int client_fd, const std::string &content) {
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += content;

    send(client_fd, response.c_str(), response.size(), 0);
}

// Function to execute a shell command and return its output
std::string executeCommand(const std::string &cmd) {
    std::array<char, 128> buffer;
    std::string result;

    // Use '2>&1' to redirect stderr to stdout
    std::string full_command = cmd + " 2>&1"; 
    std::cout << "full_command - " << full_command << std::endl;

    FILE* pipe = popen(full_command.c_str(), "r");
    if (!pipe) {
        return "Failed to execute command\n";
    }

    std::cout << "command executed "  << std::endl;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);

    // Debug logging to check what is captured
    std::cout << "Executed command: " << full_command << std::endl;
    std::cout << "Command output: " << result << std::endl;

    return result;
}

// Function to handle a single client connection
void handleClient(int client_fd) {
    char buffer[BUFFER_SIZE] = {0};
    std::string username, password;
    // Authentication flow
    ssize_t bytes_read;
    std::cout << "Waiting to receive username" << std::endl;

    // Receive username
    while (true) {
        bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_read == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // No data available yet, continue waiting
                continue;
            } else {
                std::cerr << "Failed to receive username" << std::endl;
                close(client_fd);
                return;
            }
        }
        if (bytes_read == 0) {
            std::cerr << "Connection closed" << std::endl;
            close(client_fd);
            return;
        }
        buffer[bytes_read] = '\0';
        username = buffer;
        break; // Username received successfully
    }

    std::cout << "Waiting to receive password" << std::endl;
    // Receive password
    while (true) {    
        bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_read == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; // No data available yet, continue waiting
            } else {
                std::cerr << "Failed to receive password" << std::endl;
                close(client_fd);
                return;
            }
        }
        if (bytes_read == 0) {
            std::cerr << "Connection closed" << std::endl;
            close(client_fd);
            return;
        }
        buffer[bytes_read] = '\0';
        password = buffer;
        break; // Password received successfully
    }

    if (username == "admin" && password == "password") {
        const char *auth_success = "Authentication successful\n";
        send(client_fd, auth_success, strlen(auth_success), 0);
        std::cout << auth_success << std::endl;
    } else {
        const char *auth_failed = "Authentication failed\n";
        send(client_fd, auth_failed, strlen(auth_failed), 0);
        std::cout << auth_failed << std::endl;
        close(client_fd);
        return;
    }

    std::cout << "Waiting for commands from client..." << std::endl;
    // Command execution loop
    while (true) {
        std::string command;
        
        // Receive command from client
        while (true) {
            bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_read == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                } else {
                    std::cerr << "Failed to receive command or connection closed" << std::endl;
                    close(client_fd);
                    return;
                }
            }
            if (bytes_read == 0) {
                std::cerr << "Connection closed" << std::endl;
                close(client_fd);
                return;
            }
            buffer[bytes_read] = '\0';
            command += buffer;
            break; 
        }

        // Check if command is received
        if (command.empty()) {
            continue; // No command received, wait for more data
        }

        std::cout << "Received command - " << command << std::endl;

        if (strcmp(buffer, "DISCONNECT") == 0) {
            std::cout << "Client has requested to disconnect." << std::endl;
            break; // Exit the loop and close the connection
        }  

        // Execute the command and send the result back to the client
        std::cout << "Executing command " << command;
        std::string result = executeCommand(command);
        send(client_fd, result.c_str(), result.size(), 0);
    }

    close(client_fd);
}

int main() {
    // Create thread pool with 4 threads
    ThreadPool pool(4);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) == -1) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return -1;
    }

    std::cout << "Server started and listening on port " << PORT << std::endl;

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "Epoll create failed" << std::endl;
        close(server_fd);
        return -1;
    }

    epoll_event event{}, events[MAX_EVENTS];
    event.data.fd = server_fd;
    event.events = EPOLLIN;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        std::cerr << "Epoll ctl failed" << std::endl;
        close(server_fd);
        close(epoll_fd);
        return -1;
    }

    while (true) {
        int n_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n_fds; i++) {
            if (events[i].data.fd == server_fd) {
                // Accept new client connection
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd == -1) {
                    std::cerr << "Accept failed" << std::endl;
                    continue;
                }
                std::cout << "Server accepted new client" << std::endl;

                // Set client socket to non-blocking
                setNonBlocking(client_fd);

                // Add client socket to epoll
                event.data.fd = client_fd;
                event.events = EPOLLIN | EPOLLET; // Edge-triggered
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    std::cerr << "Epoll ctl add failed" << std::endl;
                    close(client_fd);
                    continue;
                }
            } else {
                // Handle existing client connection
                int client_fd = events[i].data.fd;
                pool.enqueue([client_fd] { handleClient(client_fd); });
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}
