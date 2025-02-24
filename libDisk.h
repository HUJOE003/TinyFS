#ifndef LIBDISK_H
#define LIBDISK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCKSIZE 256
#define DEFAULT_DISK_SIZE 10240
#define MAX_DISKS 10

#define DISK_ERR -1
#define DISK_INVALID_ARG -2
#define DISK_INVALID_NUM -3

typedef struct Disk {
    FILE *fp;
    int size;
    int inUse;
} Disk;

/**
 * Opens a virtual disk file.
 * 
 * @param filename Name of the disk file.
 * @param nBytes   Size of the disk (if 0, attempts to open an existing disk).
 * 
 * @return Index of the opened disk on success, or an error code on failure.
 */
int openDisk(char *filename, int nBytes);

/**
 * Closes a virtual disk.
 * 
 * @param disk Disk index to close.
 * 
 * @return 0 on success, or an error code on failure.
 */
int closeDisk(int disk);

/**
 * Reads a block from the virtual disk.
 * 
 * @param disk Disk index.
 * @param bNum Block number to read.
 * @param block Buffer to store the read data.
 * 
 * @return 0 on success, or an error code on failure.
 */
int readBlock(int disk, int bNum, void *block);

/**
 * Writes a block to the virtual disk.
 * 
 * @param disk Disk index.
 * @param bNum Block number to write.
 * @param block Data to write.
 * 
 * @return 0 on success, or an error code on failure.
 */
int writeBlock(int disk, int bNum, void *block);

#endif // LIBDISK_H
