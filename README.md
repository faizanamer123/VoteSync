# 🗳️ VoteSync

**VoteSync** is a lightweight distributed voting application built in C++. It leverages a client-server architecture to demonstrate key concepts in distributed computing such as asynchronous communication, concurrency, and fault tolerance using **Boost.Asio**.

---

## 📌 Features

- ✅ **Simple Client-Server Design**  
  Central server collects votes from multiple TCP-connected clients.

- ⚡ **Asynchronous Networking**  
  Uses Boost.Asio to handle multiple concurrent client connections efficiently.

- 🔐 **Thread-Safe Vote Tallying**  
  Ensures consistent updates using C++11 threads and mutexes.

- 🔧 **Modular & Extensible**  
  Easily extendable for encrypted communication, consensus protocols, and more.

---

## 🚀 Getting Started

### Prerequisites

- C++11 or later
- [Boost.Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
- [CMake](https://cmake.org/)
- A C++ compiler (GCC, Clang, MSVC)

> You can use [vcpkg](https://github.com/microsoft/vcpkg) to install dependencies:
```bash
./vcpkg install boost-asio
Building the Project
bash
Copy
Edit
mkdir build
cd build
cmake ..
cmake --build .
📦 Project Structure
bash
Copy
Edit
VoteSync/
├── client.cpp        # Client application
├── server.cpp        # Server application
├── CMakeLists.txt    # CMake configuration
└── README.md         # Project documentation
💡 Usage
Run the Server
bash
Copy
Edit
./server
The server starts listening on port 54000.

Run the Client(s)
bash
Copy
Edit
./client
You will be prompted to enter a vote. Each vote is sent to the server over TCP, which acknowledges the vote and updates the tally.

⚙️ Architecture Overview
Server:

Accepts multiple client connections.

Reads votes, updates a shared vote count.

Uses threads and mutexes for concurrency.

Client:

Connects to server via TCP.

Sends vote input and receives confirmation.

🛠️ Future Enhancements
🔒 SSL/TLS encryption for secure vote transmission

🧠 Distributed consensus (e.g., Raft) for multiple server support

📊 Web or GUI-based dashboard for vote visualization

🧪 Unit tests and stress testing for larger-scale deployments
