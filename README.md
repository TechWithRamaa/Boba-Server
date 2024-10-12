## BoBa Server
* BoBa Server allows users to securely connect to and manage remote servers via the SSH (Secure Shell) protocol. It provides essential functionalities for securely executing commands, transferring files, and managing server instances, ensuring a safe and encrypted communication channel between clients and the server

### Key Features
- **Remote Access**: Execute commands and manage your remote server instances securely over SSH
- **File Transfers**: Transfer files between the local machine and remote server using SCP or SFTP
- **Tunneling & Port Forwarding**: Securely tunnel traffic between systems through encrypted connections
- **Authentication**: Supports password-based or public/private key pair authentication for secure user access

### Tech Stack
- **C++**: Core logic for handling socket programming, client-server interactions, and command execution
- **epoll(), threadpool**: Used for efficiently handling large-number of clients simultaneously
- **Linux**: The server runs on Linux-based systems leveraging its networking and process management features

### Getting Started

To get started with any of the server projects, follow these general steps:

1. **Clone the Repository**
   ```bash
   git clone https://github.com/TechWithRamaa/Boba-Server.git
   cd Boba-SSH-Server
   ```

2. **Build the Project**
    This project uses CMake for building. 
   ```bash
   cd Boba-Server
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Run a Server**
   ```bash
   ./bin/BobaServer
   ```

### Contributions
* Contributions are welcome! If you have an idea for a new feature implementation or improvements to existing projects, feel free to fork the repository and submit a pull request

### License
* This project is licensed under the MIT License - see the LICENSE file for details

### Acknowledgments
* Started this mini project while learning Socket Progamming as part of a course
* Check out the course here: [Advanced C++ with Networking Course](https://register.educosys.com/new-courses)
* Special shout-out to **Educosys**[🔗](https://www.educosys.com/) for meticulously curating this course and skillfully teaching concepts from the ground up
