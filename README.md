# SocketTalk: A LAN-Based Chatroom and File Sharing System

## Overview

SocketTalk is a real-time chat and file-sharing system built using C and Unix sockets. It enables communication between multiple clients connected to a single server within a Local Area Network (LAN). The project demonstrates the use of TCP sockets, multithreading, `select()` for handling multiple connections, and custom message/file protocols.

---

## Features Implemented

### 1. **Multi-Client Server with `select()`**

* Server can handle up to 10 clients simultaneously.
* Uses `select()` system call to monitor multiple sockets.
* Maintains a list of connected clients and their custom names.

### 2. **Client Features**

* Text-based chat interface via Terminal.
* Supports interactive command input:

  * `/name <your_name>` to set a custom display name
  * `/msg <target_name> <message>` to send a private message
  * `/sendfile <target_name> <file_path>` to send files
  * `/exit` to leave the chat

### 3. **Private Messaging**

* Server routes private messages only to the intended recipient using `/msg <name> <message>`.
* Messages appear prefixed with the sender’s name.

### 4. **File Sharing**

* Clients can send files using `/sendfile`.
* Server reads the header (`/sendfile <target> <filename> <size>`) and routes the file data.
* Receiver stores files in a `received_files/` folder.
* Auto-renaming and path management implemented to prevent file overwrite.
* Received files are saved with sender name prefixed: `sender-recipient-filename.ext`

### 5. **Admin Commands (Server Side)**

* `/kick <client_name>`: Disconnect a specific client
* `/shutdown`: Gracefully disconnect all clients and stop the server
* Server can also broadcast messages to all clients by typing in the console

### 6. **File Transfer**

* Files are transferred in binary mode to preserve content.
* Progress tracking shown during transfer.

---

## How It Works

### Server:

* Accepts new clients
* Maintains an array of `Client` structs
* Routes messages based on command prefixes
* Sends file data by reading chunks from the sender and writing to the receiver socket

### Client:

* On start, connects to the server (default `127.0.0.1:8080`)
* A separate thread listens for incoming messages/files
* Uses commands to interact and send messages/files
* Stores received files in `received_files/`

---

## Folder Structure

```
SocketTalk/
├── server.c              # Server code
├── client.c              # Client code
├── received_files/       # Folder where received files are saved
└── README.md             # This file
```

---

## Example Usage

### Starting Server:

```bash
gcc -o server server.c
./server
```

### Starting Clients:

```bash
gcc -o client client.c
./client
```

### On Client Terminal:

```bash
/name Prabhat
/msg Supreme Hello!
/sendfile Supreme cat.jpg
/exit
```

---

## Limitations

* No encryption or authentication implemented
* Designed for use within a LAN only
* Manual input parsing; no GUI

---

## Conclusion

SocketTalk is a fully functional CLI-based LAN chatroom with private messaging and file sharing. It demonstrates the use of TCP sockets, `select()`-based server multiplexing, thread-based receiving on clients, and binary file transfers in a low-level language like C.

This project provides a strong foundation for building more advanced chat or collaboration tools, including encryption, GUI, group chats, and remote connectivity in future iterations.