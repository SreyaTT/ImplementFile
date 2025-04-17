#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "disk.h"
#include "fs.h"

#define MAX_FILES       64
#define MAX_FD          32
#define MAX_FILENAME    16
#define FAT_EOF         -1
#define FAT_FREE        -2
#define DATA_BLOCKS     4096
#define BLOCK_SIZE      4096
#define META_BLOCKS     2     // blocks 0 = FAT, 1 = directory
#define MAX_FILE_SIZE   (DATA_BLOCKS * BLOCK_SIZE)

// --- On‑disk directory entry ---
typedef struct {
    char     filename[MAX_FILENAME];
    int      size;         // in bytes
    int      first_block;  // head of FAT chain
    int      used;         // 1 if in use
} DirEntry;

// --- In‑RAM file descriptor ---
typedef struct {
    int      offset;       // current seek offset
    int      dir_index;    // which directory entry
    int      used;         // 1 if open
} FileDesc;

// --- In‑RAM caches ---
static DirEntry  directory[MAX_FILES];
static int       FAT[DATA_BLOCKS];
static FileDesc  fd_table[MAX_FD];
static int       fs_mounted = 0;

// --- Helpers ---
static int save_metadata() {
    if (block_write(0, (char*)FAT) < 0)    return -1;
    if (block_write(1, (char*)directory) < 0) return -1;
    return 0;
}
static int load_metadata() {
    if (block_read(0, (char*)FAT) < 0)    return -1;
    if (block_read(1, (char*)directory) < 0) return -1;
    return 0;
}
static int alloc_block() {
    for (int i = 0; i < DATA_BLOCKS; i++)
        if (FAT[i] == FAT_FREE)
            return i;
    return -1;
}

// --- 1) make_fs ---
int make_fs(char *disk_name) {
    if (make_disk(disk_name) < 0) return -1;
    if (open_disk(disk_name) < 0) return -1;

    // init FAT
    for (int i = 0; i < DATA_BLOCKS; i++)
        FAT[i] = FAT_FREE;
    // init directory
    for (int i = 0; i < MAX_FILES; i++)
        directory[i].used = 0;

    if (save_metadata() < 0) return -1;
    return close_disk();
}

// --- 2) mount_fs ---
int mount_fs(char *disk_name) {
    if (open_disk(disk_name) < 0)   return -1;
    if (load_metadata() < 0)       return -1;
    // reset FDs
    for (int i = 0; i < MAX_FD; i++)
        fd_table[i].used = 0;
    fs_mounted = 1;
    return 0;
}

// --- 3) umount_fs ---
int umount_fs(char *disk_name) {
    if (!fs_mounted) return -1;
    // close any open FDs
    for (int i = 0; i < MAX_FD; i++)
        fd_table[i].used = 0;
    if (save_metadata() < 0) return -1;
    if (close_disk() < 0)    return -1;
    fs_mounted = 0;
    return 0;
}

// --- 4) fs_create ---
int fs_create(char *fname) {
    if (!fs_mounted || strlen(fname) >= MAX_FILENAME) 
        return -1;
    int slot = -1;
    // look for existing or free slot
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && strcmp(directory[i].filename, fname) == 0)
            return -1; // exists
        if (!directory[i].used && slot < 0)
            slot = i;
    }
    if (slot < 0) return -1;
    directory[slot].used = 1;
    strncpy(directory[slot].filename, fname, MAX_FILENAME);
    directory[slot].size        = 0;
    directory[slot].first_block = FAT_EOF;
    return 0;
}

// --- 5) fs_delete ---
int fs_delete(char *fname) {
    if (!fs_mounted) return -1;
    int di = -1;
    for (int i = 0; i < MAX_FILES; i++)
        if (directory[i].used && strcmp(directory[i].filename, fname) == 0)
            di = i;
    if (di < 0) return -1;
    // ensure no open FDs
    for (int i = 0; i < MAX_FD; i++)
        if (fd_table[i].used && fd_table[i].dir_index == di)
            return -1;
    // free FAT chain
    int b = directory[di].first_block;
    while (b != FAT_EOF) {
        int next = FAT[b];
        FAT[b] = FAT_FREE;
        b = next;
    }
    directory[di].used = 0;
    return 0;
}

// --- 6) fs_open ---
int fs_open(char *fname) {
    if (!fs_mounted) return -1;
    int di = -1;
    for (int i = 0; i < MAX_FILES; i++)
        if (directory[i].used && strcmp(directory[i].filename, fname) == 0)
            di = i;
    if (di < 0) return -1;
    // find free FD
    int fd = -1;
    for (int i = 0; i < MAX_FD; i++)
        if (!fd_table[i].used) { fd = i; break; }
    if (fd < 0) return -1;
    fd_table[fd].used      = 1;
    fd_table[fd].dir_index = di;
    fd_table[fd].offset    = 0;
    return fd;
}

// --- 7) fs_close ---
int fs_close(int fildes) {
    if (!fs_mounted || fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used)
        return -1;
    fd_table[fildes].used = 0;
    return 0;
}

// --- 8) fs_read ---
int fs_read(int fildes, void *buf, size_t nbyte) {
    if (!fs_mounted || fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used)
        return -1;
    DirEntry *de = &directory[ fd_table[fildes].dir_index ];
    size_t to_read = nbyte;
    if ((size_t)fd_table[fildes].offset + to_read > (size_t)de->size)
        to_read = de->size - fd_table[fildes].offset;
    if (to_read == 0) return 0;

    size_t total = 0;
    int    off   = fd_table[fildes].offset;
    int    block_idx = de->first_block;
    char   block_buf[BLOCK_SIZE];

    // skip to starting block
    while (off >= BLOCK_SIZE && block_idx != FAT_EOF) {
        off -= BLOCK_SIZE;
        block_idx = FAT[block_idx];
    }
    while (total < to_read && block_idx != FAT_EOF) {
        block_read(block_idx + META_BLOCKS, block_buf);
        size_t chunk = BLOCK_SIZE - off;
        if (chunk > to_read - total) chunk = to_read - total;
        memcpy((char*)buf + total, block_buf + off, chunk);
        total += chunk;
        off = 0;
        block_idx = FAT[block_idx];
    }
    fd_table[fildes].offset += total;
    return total;
}

// --- 9) fs_write ---
int fs_write(int fildes, void *buf, size_t nbyte) {
    if (!fs_mounted || fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used)
        return -1;
    DirEntry *de = &directory[ fd_table[fildes].dir_index ];
    if ((size_t)de->size + nbyte > MAX_FILE_SIZE)
        nbyte = MAX_FILE_SIZE - de->size;

    size_t total = 0;
    int    off   = fd_table[fildes].offset;
    int    block_idx = de->first_block;
    char   block_buf[BLOCK_SIZE];

    // if empty file, allocate first block
    if (block_idx == FAT_EOF) {
        int nb = alloc_block();
        if (nb < 0) return 0;
        de->first_block = nb;
        FAT[nb] = FAT_EOF;
        block_idx = nb;
    }
    // skip to correct block
    while (off >= BLOCK_SIZE) {
        if (FAT[block_idx] == FAT_EOF) {
            int nb = alloc_block();
            if (nb < 0) break;
            FAT[block_idx] = nb;
            FAT[nb] = FAT_EOF;
        }
        off -= BLOCK_SIZE;
        block_idx = FAT[block_idx];
    }
    // write loop
    while (total < nbyte) {
        block_read(block_idx + META_BLOCKS, block_buf);
        size_t chunk = BLOCK_SIZE - off;
        if (chunk > nbyte - total) chunk = nbyte - total;
        memcpy(block_buf + off, (char*)buf + total, chunk);
        block_write(block_idx + META_BLOCKS, block_buf);
        total += chunk;
        off = 0;
        // Allocate next if needed
        if (total < nbyte) {
            if (FAT[block_idx] == FAT_EOF) {
                int nb = alloc_block();
                if (nb < 0) break;
                FAT[block_idx] = nb;
                FAT[nb] = FAT_EOF;
            }
            block_idx = FAT[block_idx];
        }
    }
    fd_table[fildes].offset += total;
    if (fd_table[fildes].offset > de->size)
        de->size = fd_table[fildes].offset;
    return total;
}

// --- 10) fs_get_filesize ---
int fs_get_filesize(int fildes) {
    if (!fs_mounted || fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used)
        return -1;
    return directory[ fd_table[fildes].dir_index ].size;
}

// --- 11) fs_lseek ---
int fs_lseek(int fildes, off_t offset) {
    if (!fs_mounted || fildes < 0 || fildes >= MAX_FD ||
        !fd_table[fildes].used || offset < 0 ||
        offset > directory[ fd_table[fildes].dir_index ].size)
        return -1;
    fd_table[fildes].offset = offset;
    return 0;
}

// --- 12) fs_truncate ---
int fs_truncate(int fildes, off_t length) {
    if (!fs_mounted || fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used)
        return -1;
    DirEntry *de = &directory[ fd_table[fildes].dir_index ];
    if (length > de->size) return -1;
    int keep_blocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (length == 0) keep_blocks = 0;

    // free chain beyond keep_blocks
    int b = de->first_block;
    int prev = -1;
    for (int i = 0; i < keep_blocks && b != FAT_EOF; i++) {
        prev = b;
        b = FAT[b];
    }
    // detach
    if (prev >= 0) FAT[prev] = FAT_EOF;
    else        de->first_block = FAT_EOF;

    // free
    while (b != FAT_EOF && b != FAT_FREE) {
        int next = FAT[b];
        FAT[b] = FAT_FREE;
        b = next;
    }
    de->size = length;
    if (fd_table[fildes].offset > length)
        fd_table[fildes].offset = length;
    return 0;
}