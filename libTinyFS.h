#ifndef LIBTINYFS_H
#define LIBTINYFS_H

#include "tinyFS.h"

int tfs_mkfs(char *filename, int nBytes);
int tfs_mount(char *diskname);
int tfs_unmount(void);
fileDescriptor tfs_openFile(char *name);
int tfs_closeFile(fileDescriptor FD);
int tfs_writeFile(fileDescriptor FD, char *buffer, int size);
int tfs_deleteFile(fileDescriptor FD);
int tfs_readByte(fileDescriptor FD, char *buffer);
int tfs_seek(fileDescriptor FD, int offset);

/*
Block Structures:

Superblock (block 0):
– Bytes 0: block type (1)
– Byte 1: magic number (0x44)
– Bytes 4–7: pointer to the first free block
- Bytes 8-11: total number of blocks on disk

Inode block:
– Byte 0: type (2)
– Byte 1: magic (0x44)
– Bytes 4–11: file name (up to 8 characters)
– Bytes 12–15: file size (stored as 4 bytes)
– Bytes 16-19: pointer to the first data block (0 if none)

Data (file extent) block:
– Byte 0: type (3)
– Byte 1: magic (0x44)
- Bytes 4-7: pointer to the next file extent block (int)
- Bytes 8-...: file data

Free block (type 4):
– Byte 0: type (4)
– Byte 1: magic (0x44)
- Bytes 4-7: pointer to next free block

*/

#endif
