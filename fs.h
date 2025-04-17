#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <sys/types.h>

// File‑system management
int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);

// Directory & data operations
int fs_create(char *fname);
int fs_delete(char *fname);

// File descriptor operations
int fs_open(char *fname);
int fs_close(int fildes);

// I/O operations
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);

// Helpers
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

#endif // FS_H