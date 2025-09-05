# FileDirectory
About the project:
This project is a distributed file storage system made using C, socket programming, and process forking.
There are 4 servers in total.
The user only connects to the main server.
The main server uses sockets to talk to the other 3 servers.
Files are shared and stored across all servers.
The user does not know about the backend servers — it feels like talking to one server only.

What it can do:
Upload files from the client to the system.
Download files from the system to the client.
Delete files across multiple servers.
Create tar of all files across servers
Handle multiple clients at the same time using fork().
Use TCP sockets for communication between client and servers.

Technical details:
The client connects to the main server using a TCP socket.
The main server accepts the connection. For each new client request, it calls fork() to create a new child process.
This way, multiple clients can connect at the same time.
The child process handles the request (upload/download).
If a file is uploaded, the main server also forwards the file to the other three servers using socket connections.
When a file is requested, the main server can fetch it from any of the backend servers and return it to the client.

How to run it:
1. Get the code
2. Compile the programs:
gcc server1.c -o server1
(similar for all servers)
gcc client.c -o client
3.Add ip address to server 1 and client file
4.Start the servers (each on a different terminal or machine):
./server1→ main server
./server2→ server 2
./server3→ server 3
./server4→ server 4
Start the client and connect to the main server:
./client

Project files:
client.c → Handles user input and talks to the main server.
servers.c → Main server and backend servers (uses socket programming + fork for concurrency).
README.txt → Documentation.
