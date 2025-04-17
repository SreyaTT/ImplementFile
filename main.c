#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "disk.h"

int main() {
    char buf[BLOCK_SIZE + 1];

    // 1) Create & mount filesystem
    if (make_fs("disk.fs") < 0)    { perror("mkfs");   return 1; }
    if (mount_fs("disk.fs") < 0)   { perror("mount");  return 1; }

    // 2) Create and write "hello.txt"
    if (fs_create("hello.txt") < 0) perror("create");
    int fd = fs_open("hello.txt");
    const char *msg = "Hello, SimpleFS!";
    fs_write(fd, (void*)msg, strlen(msg));
    fs_close(fd);

    // 3) Read it back
    fd = fs_open("hello.txt");
    int n = fs_read(fd, buf, strlen(msg));
    buf[n] = '\0';
    printf("Read: %s\n", buf);
    fs_close(fd);

    // 4) Copy to "copy.txt"
    if (fs_create("copy.txt") < 0) perror("create copy");
    int src = fs_open("hello.txt");
    int dst = fs_open("copy.txt");
    while ((n = fs_read(src, buf, BLOCK_SIZE)) > 0)
        fs_write(dst, buf, n);
    fs_close(src);
    fs_close(dst);

    // 5) Delete original
    if (fs_delete("hello.txt") < 0) perror("delete");

    // 6) Unmount
    if (umount_fs("disk.fs") < 0)   { perror("umount"); return 1; }

    printf("Demo complete.\n");
    return 0;
}