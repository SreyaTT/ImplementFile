# SimpleFS – A Basic File System Implementation in C

This project is a lightweight file system simulation in C, built on top of a virtual disk interface. It demonstrates basic file system operations like creating, mounting, reading, writing, copying, and deleting files.

## 📁 Features

- Create a virtual disk
- Mount and unmount the file system
- Create files (`fs_create`)
- Open and close file descriptors (`fs_open`, `fs_close`)
- Read and write file data (`fs_read`, `fs_write`)
- Delete files (`fs_delete`)
- Block-level read/write with `block_read` and `block_write`

## 🛠️ Tech Stack

- **Language**: C
- **I/O**: POSIX system calls (`open`, `read`, `write`, `lseek`, `close`)
- **File System Components**: Virtual disk, file descriptors, memory block simulation

## 🚀 Demo

Here's a sample demo from `main.c`:
```c
fs_create("hello.txt");
int fd = fs_open("hello.txt");
const char *msg = "Hello, SimpleFS!";
fs_write(fd, (void*)msg, strlen(msg));
fs_close(fd);
