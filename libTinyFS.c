/*
 * libTinyFS.c
 *
 * Modified TinyFS implementation.
 *
 * Modifications done to existing functions:
 *   - tfs_openFile
 *   - tfs_writeFile
 *   - tfs_readByte
 *   - tfs_deleteFile
 *
 * New functions added:
 *   - Timestamps:
 *       - tfs_readFileInfo
 *   - Read-only and writeByte support:
 *       - tfs_makeRO
 *       - tfs_makeRW
 *       - tfs_writeByte
 *   - Directory listing and file renaming:
 *       - tfs_rename
 *       - tfs_readdir
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libDisk.h"
#include "libTinyFS.h"
#include "TinyFS_errno.h"
#include <time.h>

/* 
   Conditionally define error codes if not defined in TinyFS_errno.h.
*/
#ifndef TFS_ERR_MKFS
#define TFS_ERR_MKFS -2
#endif

#ifndef TFS_ERR_READINFO
#define TFS_ERR_READINFO -1
#endif

#ifndef TFS_ERR_MAKE_RO
#define TFS_ERR_MAKE_RO -7
#endif

#ifndef TFS_ERR_MAKE_RW
#define TFS_ERR_MAKE_RW -8
#endif

#ifndef TFS_ERR_RENAME
#define TFS_ERR_RENAME -9
#endif

#ifndef TFS_ERR_READDIR
#define TFS_ERR_READDIR -10
#endif

#define MAX_OPEN_FILES 20
#define MAX_INODES 1024

//-------------------------------------------------------------
/*                   Core Features                           */
//-------------------------------------------------------------

typedef struct {
    char name[9];       // 8-character filename (null-terminated)
    int inodeIndex;     // The inode block number
    int firstDataBlock; // The file's first data block (from inode)
    int r, g, b;        // Persistent color (stored in the inode as well)
} InodeColor;

static InodeColor inodeColors[MAX_INODES];
static int inodeCount = 0;


// Open file table entry
typedef struct OpenFile {
    int inodeBlock;    // block number where the inode block is stored
    int filePointer;   // Current file pointer (in bytes).
    int used;          // 1 if used, 0 otherwise.
} OpenFile;

static OpenFile openFileTable[MAX_OPEN_FILES];

static int mountedDisk = -1;
static int totalBlocks = 0;
static int isMounted = -1;  // will be set to 1 when mounted

unsigned int get_seed() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_nsec);  // Use nanoseconds for more randomness
}

// Helper functions to convert int to bytes (big-endian)
static void intToBytes(int value, char *dest) {
    dest[0] = (value >> 24) & 0xFF;
    dest[1] = (value >> 16) & 0xFF;
    dest[2] = (value >> 8) & 0xFF;
    dest[3] = value & 0xFF;
}

// Convert 4 bytes (big-endian) to an int
static int bytesToInt(const char *src) {
    return ((unsigned char)src[0] << 24) |
           ((unsigned char)src[1] << 16) |
           ((unsigned char)src[2] << 8)  |
           ((unsigned char)src[3]);
}

static void clearOpenFileTable() {
    int i;
    for(i = 0; i < MAX_OPEN_FILES; i++){
        openFileTable[i].used = 0;
    }
}

// Returns location of next free block and updates the superblock's free block pointer 
static int getFreeBlock(){
    char superBlock[BLOCKSIZE];
    char freeBlock[BLOCKSIZE];

    if (readBlock(mountedDisk, 0, superBlock) < 0) return -1; // read superblock
    
    int nextFreeBlockLocation = bytesToInt(superBlock+4); // location of next free block
    if (nextFreeBlockLocation == 0) return -1; // no free blocks available

    if(readBlock(mountedDisk, nextFreeBlockLocation, freeBlock) < 0) return -1; // read the free block

    int freeBlockLocation = nextFreeBlockLocation; // store current free block location
    nextFreeBlockLocation = bytesToInt(freeBlock+4); // get next free block from free block's pointer
    intToBytes(nextFreeBlockLocation, superBlock+4); // update superblock

    if (writeBlock(mountedDisk, 0, superBlock) < 0) return -1; // write back superblock

    return freeBlockLocation;
}

// Marks the block at blockNum as free and sets the superblock's free block pointer to it
static int addFreeBlock(int blockNum){
    char superBlock[BLOCKSIZE];
    char freeBlock[BLOCKSIZE];

    if(readBlock(mountedDisk, 0, superBlock) < 0) return -1;

    int currentNextFreeLocation = bytesToInt(superBlock+4);
    memset(freeBlock, 0, BLOCKSIZE);
    freeBlock[0] = 4; // free block type
    freeBlock[1] = 0x44;
    intToBytes(currentNextFreeLocation, freeBlock+4);

    if (writeBlock(mountedDisk, blockNum, freeBlock) < 0 ) return -1;
    
    intToBytes(blockNum, superBlock+4);
    if (writeBlock(mountedDisk, 0, superBlock) < 0) return -1;

    return 0; 
}

static int getFreeBlockCount(){
    if (mountedDisk < 0) return -1;

    int freeBlockCount = 0;
    char block[BLOCKSIZE];
    int i;
    for(i = 0; i < totalBlocks; i++){
        if(readBlock(mountedDisk, i, block) < 0) return -1;
        if(block[0] == 4) freeBlockCount++;
    }
    return freeBlockCount;
}

// Generate random RGB colors
void generateRandomColor(int *r, int *g, int *b) {
    srand(get_seed());
    *r = rand() % 256;
    *g = rand() % 256;
    *b = rand() % 256;
}

// Add a mapping entry if one for this inode does not already exist.
void addMapping(int inodeBlock, char *name, int firstDataBlock, int r, int g, int b) {
    // Check if already exists.
    int i;
    for (i = 0; i < inodeCount; i++) {
        if (inodeColors[i].inodeIndex == inodeBlock) {
            return; // already exists, do nothing.
        }
    }
    if (inodeCount < MAX_INODES) {
        strncpy(inodeColors[inodeCount].name, name, 8);
        inodeColors[inodeCount].name[8] = '\0';
        inodeColors[inodeCount].inodeIndex = inodeBlock;
        inodeColors[inodeCount].firstDataBlock = firstDataBlock;
        inodeColors[inodeCount].r = r;
        inodeColors[inodeCount].g = g;
        inodeColors[inodeCount].b = b;
        inodeCount++;
    }
}

// Remove a mapping entry by inode block number.
void removeMapping(int inodeBlock) {
    int i;
    for (i = 0; i < inodeCount; i++) {
        if (inodeColors[i].inodeIndex == inodeBlock) {
            int j;
            for (j = i; j < inodeCount - 1; j++) {
                inodeColors[j] = inodeColors[j + 1];
            }
            inodeCount--;
            return;
        }
    }
}

// Given a data block number, determine the owning file by checking each mapping's chain.
InodeColor *getOwnerForDataBlock(int dataBlock) {
    if (dataBlock == 0) return NULL;
    int i;
    char tempBlock[BLOCKSIZE];
    for (i = 0; i < inodeCount; i++) {
        int current = inodeColors[i].firstDataBlock;
        while (current != 0) {
            if (current == dataBlock) return &inodeColors[i];
            if (readBlock(mountedDisk, current, tempBlock) < 0) break;
            current = bytesToInt(tempBlock + 4);
        }
    }
    return NULL;
}

/* tfs_mkfs:
   - Checks that nBytes is > 0 and a multiple of BLOCKSIZE.
   - Initializes the superblock and free blocks.
   Returns TFS_SUCCESS on success or TFS_ERR_MKFS on failure.
*/
int tfs_mkfs(char *filename, int nBytes){
    if(nBytes <= 0 || nBytes % BLOCKSIZE != 0)
         return TFS_ERR_MKFS;

    int disk = openDisk(filename, nBytes);
    if (disk < 0) return TFS_ERR_MKFS;

    mountedDisk = disk;
    totalBlocks = nBytes / BLOCKSIZE;

    char superBlock[BLOCKSIZE];
    memset(superBlock, 0, BLOCKSIZE);
    superBlock[0] = 1;       // superblock type
    superBlock[1] = 0x44;    // magic number
    
    int firstFreeBlockLocation = 1;
    intToBytes(firstFreeBlockLocation, superBlock+4);
    intToBytes(totalBlocks, superBlock+8);

    if (writeBlock(mountedDisk, 0, superBlock) < 0) return TFS_ERR_MKFS;

    char freeBlock[BLOCKSIZE];
    int i;
    for(i = 1; i < totalBlocks; i++){
        memset(freeBlock, 0, BLOCKSIZE);
        freeBlock[0] = 4;
        freeBlock[1] = 0x44;
        int nextFreeBlockLocation = (i == totalBlocks - 1) ? 0 : i + 1;
        intToBytes(nextFreeBlockLocation, freeBlock+4);
        if (writeBlock(mountedDisk, i, freeBlock) < 0) return TFS_ERR_MKFS;
    }

    clearOpenFileTable();
    return TFS_SUCCESS;
}

/* tfs_mount:
   - Mounts an existing filesystem.
   - Returns TFS_SUCCESS if successful, TFS_ERR_MOUNT otherwise.
*/
int tfs_mount(char *diskname){
    if (isMounted >= 0) return TFS_ERR_MOUNT;
    
    int disk = openDisk(diskname, 0);
    if (disk < 0) return TFS_ERR_MOUNT;
    mountedDisk = disk;

    char superBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, 0, superBlock) < 0) return TFS_ERR_MOUNT;
    if(superBlock[0] != 1 || superBlock[1] != 0x44) return TFS_ERR_MOUNT;

    totalBlocks = bytesToInt(superBlock+8);
    clearOpenFileTable();
    isMounted = 1;
    return TFS_SUCCESS;
}

/* tfs_unmount:
   - Unmounts the filesystem.
   - Returns TFS_SUCCESS on success or TFS_ERR_UNMOUNT if no filesystem is mounted.
*/
int tfs_unmount(void){
    if (mountedDisk < 0) return TFS_ERR_UNMOUNT;
    if (closeDisk(mountedDisk) < 0) return TFS_ERR_UNMOUNT;

    mountedDisk = -1;
    isMounted = -1;
    clearOpenFileTable();
    return TFS_SUCCESS;
}

/* tfs_openFile:
   - Opens a file. If the file does not exist, a new inode is created.
   - Initializes timestamps and sets file mode to read-write.
   - Returns a file descriptor on success or TFS_ERR_OPEN if an error occurs.
   - Fails if the filename is longer than 8 characters.
*/
fileDescriptor tfs_openFile(char *name) {
    if (mountedDisk < 0) return TFS_ERR_OPEN;
    if (strlen(name) > 8) return TFS_ERR_OPEN;

    int nameLength = strlen(name);
    char inodeName[9];
    memset(inodeName, 0, 9);
    strncpy(inodeName, name, nameLength);

    char block[BLOCKSIZE];
    int inodeBlockLocation = -1;
    int i;
    // Search disk for an inode with matching name.
    for (i = 0; i < totalBlocks; i++){
        if (readBlock(mountedDisk, i, block) < 0) continue;
        if (block[0] == 2 && block[1] == 0x44) {
            if (strncmp(block + 4, inodeName, 8) == 0) {
                inodeBlockLocation = i;
                break;
            }
        }
    }
    if (inodeBlockLocation < 0) {
        // File doesn't exist, create a new inode.
        inodeBlockLocation = getFreeBlock();
        if (inodeBlockLocation < 0) return TFS_ERR_OPEN;

        memset(block, 0, BLOCKSIZE);
        block[0] = 2;        // inode block type
        block[1] = 0x44;     // magic number
        memcpy(block + 4, inodeName, 8);
        intToBytes(0, block + 12); // file size = 0
        intToBytes(0, block + 16); // no data block yet

        // Set timestamps (creation, modification, access)
        int timestamp = (int)time(NULL);
        intToBytes(timestamp, block + 20);
        intToBytes(timestamp, block + 24);
        intToBytes(timestamp, block + 28);
        block[32] = 0;       // read-write flag

        // Generate a random color and store it in bytes 33-35.
        int r, g, b;
        generateRandomColor(&r, &g, &b);
        block[33] = (char)r;
        block[34] = (char)g;
        block[35] = (char)b;

        if (writeBlock(mountedDisk, inodeBlockLocation, block) < 0)
            return TFS_ERR_OPEN;

        // Add one mapping entry.
        addMapping(inodeBlockLocation, inodeName, 0, r, g, b);
    }

    // Insert into open file table.
    int fd = -1;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!openFileTable[i].used) {
            fd = i;
            openFileTable[i].used = 1;
            openFileTable[i].inodeBlock = inodeBlockLocation;
            openFileTable[i].filePointer = 0;
            break;
        }
    }
    if (fd < 0) return TFS_ERR_OPEN;
    return fd;
}

int tfs_closeFile(fileDescriptor FD){
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used) return TFS_ERR_CLOSE;

    openFileTable[FD].used = 0;
    openFileTable[FD].inodeBlock = -1;
    openFileTable[FD].filePointer = -1;
    return TFS_SUCCESS;
}

void freeAllocatedBlocks(int allocatedBlocks[], int length){
    int i;
    for(i = 0; i < length; i++){
        addFreeBlock(allocatedBlocks[i]);
    }
}

/* tfs_writeFile:
   - Writes a buffer to a file.
   - Checks the read-only flag and updates the modification timestamp.
   - Returns TFS_SUCCESS on success or TFS_ERR_WRITE on failure.
*/
int tfs_writeFile(fileDescriptor FD, char *buffer, int size) {
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used)
         return TFS_ERR_WRITE;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
         return TFS_ERR_WRITE;

    if (inodeBlock[32] == 1) return TFS_ERR_WRITE;  // read-only

    // Free old data blocks.
    int dataBlockLocation = bytesToInt(inodeBlock + 16);
    char dataBlock[BLOCKSIZE];
    while (dataBlockLocation != 0) {
         if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0)
             break;
         int next = bytesToInt(dataBlock + 4);
         addFreeBlock(dataBlockLocation);
         dataBlockLocation = next;
    }

    if (size == 0) {
        intToBytes(0, inodeBlock + 12);
        intToBytes(0, inodeBlock + 16);
        intToBytes((int)time(NULL), inodeBlock + 24);
        writeBlock(mountedDisk, inodeBlockLocation, inodeBlock);
        openFileTable[FD].filePointer = 0;
        int i;
        for (i = 0; i < inodeCount; i++) {
            if (inodeColors[i].inodeIndex == inodeBlockLocation) {
                inodeColors[i].firstDataBlock = 0;
                break;
            }
        }
        return TFS_SUCCESS;
    }

    int bytesPerBlock = BLOCKSIZE - 8;
    int blocksNeeded = (size + bytesPerBlock - 1) / bytesPerBlock;
    int availableFreeBlocks = getFreeBlockCount();
    if (blocksNeeded > availableFreeBlocks) return TFS_ERR_WRITE;

    int firstDataBlockLocation = 0;
    int prevBlock = 0;
    int allocatedBlocks[blocksNeeded];
    int allocatedCount = 0;
    int i;
    for (i = 0; i < blocksNeeded; i++) {
        int currentBlock = getFreeBlock();
        if (currentBlock < 0) {
            freeAllocatedBlocks(allocatedBlocks, allocatedCount);
            return TFS_ERR_WRITE;
        }
        allocatedBlocks[allocatedCount++] = currentBlock;

        memset(dataBlock, 0, BLOCKSIZE);
        dataBlock[0] = 3;      // data block type
        dataBlock[1] = 0x44;   // magic number
        intToBytes(0, dataBlock + 4); // next pointer initially 0

        int bufferPos = i * bytesPerBlock;
        int numBytesToWrite = (size - bufferPos < bytesPerBlock) ? (size - bufferPos) : bytesPerBlock;
        memcpy(dataBlock + 8, buffer + bufferPos, numBytesToWrite);

        if (writeBlock(mountedDisk, currentBlock, dataBlock) < 0) {
            freeAllocatedBlocks(allocatedBlocks, allocatedCount);
            return TFS_ERR_WRITE;
        }

        if (firstDataBlockLocation == 0)
            firstDataBlockLocation = currentBlock;

        if (prevBlock != 0) {
            if (readBlock(mountedDisk, prevBlock, dataBlock) < 0)
                return TFS_ERR_WRITE;
            intToBytes(currentBlock, dataBlock + 4);
            if (writeBlock(mountedDisk, prevBlock, dataBlock) < 0)
                return TFS_ERR_WRITE;
        }
        prevBlock = currentBlock;
    }

    intToBytes(size, inodeBlock + 12);
    intToBytes(firstDataBlockLocation, inodeBlock + 16);
    intToBytes((int)time(NULL), inodeBlock + 24);
    if (writeBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
         return TFS_ERR_WRITE;

    openFileTable[FD].filePointer = 0;

    for (i = 0; i < inodeCount; i++) {
        if (inodeColors[i].inodeIndex == inodeBlockLocation) {
            inodeColors[i].firstDataBlock = firstDataBlockLocation;
            break;
        }
    }

    return TFS_SUCCESS;
}

void removeInodeColorByIndex(int inodeIndex) {
    int i;
    for (i = 0; i < inodeCount; i++) {
        if (inodeColors[i].inodeIndex == inodeIndex) {
            int j;
            for (j = i; j < inodeCount - 1; j++) {
                inodeColors[j] = inodeColors[j + 1];
            }
            inodeCount--;
            return;
        }
    }
}
/* tfs_deleteFile:
   - Deletes a file (failing if it is read-only).
*/
int tfs_deleteFile(fileDescriptor FD) {
    if (FD < 0 || FD >= MAX_OPEN_FILES) return TFS_ERR_DELETE;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
        return TFS_ERR_DELETE;

    if (inodeBlock[32] == 1) return TFS_ERR_DELETE;

    removeInodeColorByIndex(inodeBlockLocation);

    int dataBlockLocation = bytesToInt(inodeBlock+16);
    char dataBlock[BLOCKSIZE];
    while (dataBlockLocation != 0){
        if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) break;
        int next = bytesToInt(dataBlock+4);
        addFreeBlock(dataBlockLocation);
        dataBlockLocation = next;
    }
    addFreeBlock(inodeBlockLocation);

    openFileTable[FD].used = 0;
    openFileTable[FD].inodeBlock = -1;
    openFileTable[FD].filePointer = -1;
    return TFS_SUCCESS;
}

/* tfs_readByte:
   - Reads a single byte from a file and updates the access timestamp.
*/
int tfs_readByte(fileDescriptor FD, char *buffer) {
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used) return TFS_ERR_READ;
    
    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_READ;
    
    int fileSize = bytesToInt(inodeBlock+12);
    int fpPosition = openFileTable[FD].filePointer;
    if (fpPosition >= fileSize) return TFS_ERR_READ;

    int bytesPerBlock = BLOCKSIZE - 8;
    int blockIndex = fpPosition / bytesPerBlock;
    int offsetWithinBlock = fpPosition % bytesPerBlock;
    int dataBlockLocation = bytesToInt(inodeBlock+16);
    char dataBlock[BLOCKSIZE];
    int i;
    for(i = 0; i < blockIndex; i++){
        if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) return TFS_ERR_READ;
        dataBlockLocation = bytesToInt(dataBlock+4);
    }
    if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) return TFS_ERR_READ;

    *buffer = dataBlock[8 + offsetWithinBlock];
    openFileTable[FD].filePointer++;

    intToBytes((int)time(NULL), inodeBlock+28);
    writeBlock(mountedDisk, inodeBlockLocation, inodeBlock);
    
    return TFS_SUCCESS;
}

int tfs_seek(fileDescriptor FD, int offset){
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used) return TFS_ERR_SEEK;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_READ;
    
    int fileSize = bytesToInt(inodeBlock+12);
    if (offset < 0 || offset > fileSize) return TFS_ERR_SEEK;

    openFileTable[FD].filePointer = offset;
    return TFS_SUCCESS;
}

//-------------------------------------------------------------
/*                Additional Features                      */
//-------------------------------------------------------------

/* tfs_readFileInfo:
   - Prints file info (name, size, timestamps, read-only status).
*/
int tfs_readFileInfo(fileDescriptor FD) {
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used)
        return TFS_ERR_READINFO;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
        return TFS_ERR_READINFO;

    char filename[9];
    memcpy(filename, inodeBlock+4, 8);
    filename[8] = '\0';
    int fileSize = bytesToInt(inodeBlock+12);
    int creationTime = bytesToInt(inodeBlock+20);
    int modificationTime = bytesToInt(inodeBlock+24);
    int accessTime = bytesToInt(inodeBlock+28);
    int readOnly = inodeBlock[32];

    time_t t_creation = (time_t) creationTime;
    time_t t_mod = (time_t) modificationTime;
    time_t t_access = (time_t) accessTime;

    printf("File Info:\n");
    printf("  Name: %s\n", filename);
    printf("  Size: %d bytes\n", fileSize);
    printf("  Created: %s", ctime(&t_creation));
    printf("  Modified: %s", ctime(&t_mod));
    printf("  Last Accessed: %s", ctime(&t_access));
    printf("  Read-Only: %s\n", readOnly ? "Yes" : "No");
    return TFS_SUCCESS;
}

/* tfs_makeRO:
   - Sets a file's flag to read-only by name.
*/
int tfs_makeRO(char *name) {
    char inodeName[9];
    memset(inodeName, 0, 9);
    strncpy(inodeName, name, 8);

    char block[BLOCKSIZE];
    int inodeBlockLocation = -1;
    int i;
    for(i = 0; i < totalBlocks; i++){
        if (readBlock(mountedDisk, i, block) < 0) continue;
        if (block[0] == 2 && block[1] == 0x44){
            if (strncmp(block+4, inodeName, 8) == 0) {
                inodeBlockLocation = i;
                break;
            }
        }
    }
    if (inodeBlockLocation < 0) return TFS_ERR_MAKE_RO;

    block[32] = 1; // set read-only
    if (writeBlock(mountedDisk, inodeBlockLocation, block) < 0)
        return TFS_ERR_MAKE_RO;
    return TFS_SUCCESS;
}

/* tfs_makeRW:
   - Resets a file's flag to read-write by name.
*/
int tfs_makeRW(char *name) {
    char inodeName[9];
    memset(inodeName, 0, 9);
    strncpy(inodeName, name, 8);

    char block[BLOCKSIZE];
    int inodeBlockLocation = -1;
    int i;
    for(i = 0; i < totalBlocks; i++){
        if (readBlock(mountedDisk, i, block) < 0) continue;
        if (block[0] == 2 && block[1] == 0x44){
            if (strncmp(block+4, inodeName, 8) == 0) {
                inodeBlockLocation = i;
                break;
            }
        }
    }
    if (inodeBlockLocation < 0) return TFS_ERR_MAKE_RW;

    block[32] = 0; // set to read-write
    if (writeBlock(mountedDisk, inodeBlockLocation, block) < 0)
        return TFS_ERR_MAKE_RW;
    return TFS_SUCCESS;
}

/* tfs_writeByte:
   - Writes a single byte at a given offset.
   - Fails if the file is read-only or if the offset is invalid.
*/
int tfs_writeByte(fileDescriptor FD, int offset, unsigned int data) {
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used)
        return TFS_ERR_WRITE;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
        return TFS_ERR_WRITE;

    if (inodeBlock[32] == 1) return TFS_ERR_WRITE;

    int fileSize = bytesToInt(inodeBlock+12);
    if (offset < 0 || offset >= fileSize) return TFS_ERR_WRITE;

    int bytesPerBlock = BLOCKSIZE - 8;
    int blockIndex = offset / bytesPerBlock;
    int offsetWithinBlock = offset % bytesPerBlock;

    int dataBlockLocation = bytesToInt(inodeBlock+16);
    char dataBlock[BLOCKSIZE];
    int i;
    for(i = 0; i < blockIndex; i++){
        if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0)
            return TFS_ERR_WRITE;
        dataBlockLocation = bytesToInt(dataBlock+4);
        if (dataBlockLocation == 0) return TFS_ERR_WRITE;
    }
    if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0)
        return TFS_ERR_WRITE;

    dataBlock[8 + offsetWithinBlock] = (char)data;
    if (writeBlock(mountedDisk, dataBlockLocation, dataBlock) < 0)
        return TFS_ERR_WRITE;

    intToBytes((int)time(NULL), inodeBlock+24);
    if (writeBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
        return TFS_ERR_WRITE;
    return TFS_SUCCESS;
}

/* tfs_rename:
   - Renames an open file by updating its inode's filename field and modification timestamp.
*/
int tfs_rename(fileDescriptor FD, char *newName) {
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used)
        return TFS_ERR_RENAME;

    int nameLength = strlen(newName);
    if (nameLength > 8) nameLength = 8;
    char newNameBuffer[9];
    memset(newNameBuffer, 0, 9);
    strncpy(newNameBuffer, newName, nameLength);

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
        return TFS_ERR_RENAME;

    memset(inodeBlock+4, 0, 8);
    memcpy(inodeBlock+4, newNameBuffer, 8);
    intToBytes((int)time(NULL), inodeBlock+24);
    if (writeBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0)
        return TFS_ERR_RENAME;
    return TFS_SUCCESS;
}

/* tfs_readdir:
   - Scans the disk for inode blocks and prints each file's info.
*/
int tfs_readdir(void) {
    if (mountedDisk < 0) return TFS_ERR_READDIR;

    char block[BLOCKSIZE];
    int i, found = 0;
    printf("Directory Listing:\n");
    for (i = 0; i < totalBlocks; i++){
        if (readBlock(mountedDisk, i, block) < 0)
            continue;
        if (block[0] == 2 && block[1] == 0x44) {
            found = 1;
            char filename[9];
            memcpy(filename, block+4, 8);
            filename[8] = '\0';
            int fileSize = bytesToInt(block+12);
            int readOnly = block[32];
            printf("  Name: %s, Size: %d bytes, Read-Only: %s\n",
                   filename, fileSize, readOnly ? "Yes" : "No");
        }
    }
    if (!found)
        printf("  (No files found)\n");
    return TFS_SUCCESS;
}

void tfs_displayFragments() {
    if (mountedDisk < 0) {
        printf("No filesystem mounted.\n");
        return;
    }

    char block[BLOCKSIZE];
    int i;
    printf("--- File Color Mapping ---\n");
    for (i = 0; i < inodeCount; i++) {
        printf("  \033[1;38;2;%d;%d;%dm%s\033[0m\n", 
               inodeColors[i].r, inodeColors[i].g, inodeColors[i].b, inodeColors[i].name);
    }

    printf("\n--- Disk Fragmentation Map ---\n");
    for (i = 0; i < totalBlocks; i++) {
        if (readBlock(mountedDisk, i, block) < 0) continue;
        if (i == 0) {
            printf("\033[1m[SUPERBLOCK]\033[0m ");
        } else if (block[0] == 2) {  // Inode block
            InodeColor *color = NULL;
            int j;
            for (j = 0; j < inodeCount; j++) {
                if (inodeColors[j].inodeIndex == i) {
                    color = &inodeColors[j];
                    break;
                }
            }
            if (color) {
                printf("\033[3;38;2;%d;%d;%dm[INODE]\033[0m ", 
                       color->r, color->g, color->b);
            } else {
                printf("\033[3m[UNKNOWN INODE]\033[0m ");
            }
        } else if (block[0] == 3) {  // Data block
            InodeColor *owner = getOwnerForDataBlock(i);
            if (owner) {
                printf("\033[1;38;2;%d;%d;%dm[DATA]\033[0m ", 
                       owner->r, owner->g, owner->b);
            } else {
                printf("\033[1;36m[DATA]\033[0m ");
            }
        } else if (block[0] == 4) {
            printf("\033[1;31m[FREE]\033[0m ");
        } else {
            printf("\033[1;33m[UNKNOWN]\033[0m ");
        }
        if ((i + 1) % 10 == 0) printf("\n");
    }
    printf("\n");
}

void tfs_defrag() {
    if (mountedDisk < 0) {
        printf("No filesystem mounted.\n");
        return;
    }

    char block[BLOCKSIZE];
    int *mapping = malloc(totalBlocks * sizeof(int));
    if (!mapping) return;
    int i;
    // Initialize mapping so that mapping[i] = i initially.
    for (i = 0; i < totalBlocks; i++) {
        mapping[i] = i;
    }
    int nextFreeIndex = 1;
    // Move allocated blocks to the beginning (after superblock).
    for (i = 1; i < totalBlocks; i++) {
        if (readBlock(mountedDisk, i, block) < 0) continue;
        if (block[0] != 4) {  // if block is not free
            if (i != nextFreeIndex) {
                // Move block from i to nextFreeIndex.
                writeBlock(mountedDisk, nextFreeIndex, block);
                // Mark block i as free.
                memset(block, 0, BLOCKSIZE);
                block[0] = 4;
                block[1] = 0x44;
                int nextFree = (i == totalBlocks - 1) ? 0 : i + 1;
                intToBytes(nextFree, block+4);
                writeBlock(mountedDisk, i, block);
                // Update mapping: block originally at i now resides at nextFreeIndex.
                mapping[i] = nextFreeIndex;
            }
            nextFreeIndex++;
        }
    }
    // Update chain pointers inside inode and data blocks.
    for (i = 1; i < nextFreeIndex; i++) {
        if (readBlock(mountedDisk, i, block) < 0) continue;
        if (block[0] == 2) { // inode block
            int oldFirstData = bytesToInt(block+16);
            int newFirstData = (oldFirstData == 0) ? 0 : mapping[oldFirstData];
            intToBytes(newFirstData, block+16);
            writeBlock(mountedDisk, i, block);
        } else if (block[0] == 3) { // data block
            int oldNext = bytesToInt(block+4);
            int newNext = (oldNext == 0) ? 0 : mapping[oldNext];
            intToBytes(newNext, block+4);
            writeBlock(mountedDisk, i, block);
        }
    }
    // Update global inodeColors table with new inode and first data block numbers.
    for (i = 0; i < inodeCount; i++) {
        int oldInode = inodeColors[i].inodeIndex;
        inodeColors[i].inodeIndex = mapping[oldInode];
        int oldData = inodeColors[i].firstDataBlock;
        if (oldData != 0)
            inodeColors[i].firstDataBlock = mapping[oldData];
    }
    free(mapping);
    printf("Defragmentation complete.\n");
}