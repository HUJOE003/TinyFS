#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libDisk.h"
#include "libTinyFS.h"
#include "TinyFS_errno.h"

#define MAX_OPEN_FILES 20

static int mountedDisk = -1;
static int totalBlocks = 0;
static int isMounted = -1;

// Open file table entry
typedef struct OpenFile {
    int inodeBlock;    // block number where the inode block is stored
    int filePointer;   // Current file pointer (in bytes).
    int used;          // 1 if used, 0 otherwise.
} OpenFile;

static OpenFile openFileTable[MAX_OPEN_FILES];


//Helper functions to convert int to bytes for storing next-block locations
//Convert an int to 4 bytes (big-endian)
static void intToBytes(int value, char *dest) {
    dest[0] = (value >> 24) & 0xFF;
    dest[1] = (value >> 16) & 0xFF;
    dest[2] = (value >> 8) & 0xFF;
    dest[3] = value & 0xFF;
}

//Convert 4 bytes (big-endian) to an int
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

//returns location of next free block, updates superblock's free block pointer 
static int getFreeBlock(){
    char superBlock[BLOCKSIZE];
    char freeBlock[BLOCKSIZE];

    if (readBlock(mountedDisk, 0, superBlock) < 0) return -1; //read superblock
    
    int nextFreeBlockLocation = bytesToInt(superBlock+4); //use superblock to get loc of next free block
    if (nextFreeBlockLocation == 0) return -1; //no free blocks available

    if(readBlock(mountedDisk, nextFreeBlockLocation, freeBlock) < 0) return -1; //read that free block

    int freeBlockLocation = nextFreeBlockLocation; //store this free block location
    nextFreeBlockLocation = bytesToInt(freeBlock+4); //set the next location to current free block's next
    intToBytes(nextFreeBlockLocation, superBlock+4); //write the new next location to superblock

    if (writeBlock(mountedDisk, 0, superBlock) < 0) return -1; //write superblock

    return freeBlockLocation;
}

//marks block at blockNum to free, sets superblock's free block pointer to it
static int addFreeBlock(int blockNum){
    char superBlock[BLOCKSIZE];
    char freeBlock[BLOCKSIZE];

    if(readBlock(mountedDisk, 0, superBlock) < 0) return -1;

    int currentNextFreeLocation = bytesToInt(superBlock+4); //get current next free loc from superblock
    memset(freeBlock, 0, BLOCKSIZE);
    freeBlock[0] = 4; //free block type
    freeBlock[1] = 0x44;
    intToBytes(currentNextFreeLocation, freeBlock+4); //set new free block's next ptr to current free head

    if (writeBlock(mountedDisk, blockNum, freeBlock) < 0 ) return -1;
    
    intToBytes(blockNum, superBlock+4); //set superblock's next free block ptr to new free block
    if (writeBlock(mountedDisk, 0, superBlock) < 0) return -1; //write back superblock

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

//TODO: ask prof if mkfs should mount it
int tfs_mkfs(char *filename, int nBytes){
    int disk = openDisk(filename, nBytes);
    if (disk < 0) return TFS_ERR_MKFS;

    mountedDisk = disk;
    totalBlocks = nBytes / BLOCKSIZE;

    char superBlock[BLOCKSIZE];
    memset(superBlock, 0, BLOCKSIZE);
    superBlock[0] = 1; //block type
    superBlock[1] = 0x44; //magic num
    
    int firstFreeBlockLocation = 1; //first free block is at block #1
    intToBytes(firstFreeBlockLocation, superBlock+4); //set free block loc to bytes 4-7 of superblock
    intToBytes(totalBlocks, superBlock+8); //set total # of blocks to bytes 8-11 of superblock

    if (writeBlock(mountedDisk, 0, superBlock) < 0) return TFS_ERR_MKFS; //write superblock to disk

    char freeBlock[BLOCKSIZE];
    int i;
    for(i = 1; i < totalBlocks; i++){
        memset(freeBlock, 0, BLOCKSIZE); //zero out block
        freeBlock[0] = 4; //block type
        freeBlock[1] = 0x44; //magic num
        //location of next free block is i + 1 (next number) or 0 if its the last block
        int nextFreeBlockLocation = (i == totalBlocks - 1) ? 0 : i + 1;
        intToBytes(nextFreeBlockLocation, freeBlock+4);
        if (writeBlock(mountedDisk, i, freeBlock) < 0) return TFS_ERR_MKFS; //write new free block to disk
    }

    clearOpenFileTable();

    return TFS_SUCCESS;
}

//TODO: do checking for diskname and fallback to default diskname
int tfs_mount(char *diskname){
    if (isMounted >= 0) return TFS_ERR_MOUNT; //prevent multiple mounts
    
    int disk = openDisk(diskname, 0);
    if (disk < 0) return TFS_ERR_MOUNT;
    mountedDisk = disk;

    char superBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, 0, superBlock) < 0) return TFS_ERR_MOUNT; //read superblock from disk

    if(superBlock[0] != 1 || superBlock[1] != 0x44) return TFS_ERR_MOUNT; //ensure superblock is setup correctly

    //set totalBlocks to what is stored in the superblock
    totalBlocks = bytesToInt(superBlock+8);

    clearOpenFileTable();
    isMounted = 1;

    return TFS_SUCCESS;
}

int tfs_unmount(void){
    if (mountedDisk < 0) return TFS_ERR_UNMOUNT; //no disk to unmount    
    if (closeDisk(mountedDisk) < 0) return TFS_ERR_UNMOUNT; //unmount the disk

    mountedDisk = -1;
    clearOpenFileTable();

    return TFS_SUCCESS;
}

fileDescriptor tfs_openFile(char *name){
    if (mountedDisk < 0) return TFS_ERR_OPEN;

    int nameLength = strlen(name);
    if (nameLength > 8) nameLength = 8;

    char inodeName[9]; //stores the filename within inode block
    memset(inodeName, 0, 9);
    strncpy(inodeName, name, nameLength);

    char block[BLOCKSIZE];
    int inodeBlockLocation = -1;
    int i;
    //search for existing inode with matching file name
    for(i = 0; i < totalBlocks; i++){
        if (readBlock(mountedDisk, i, block) < 0) continue; //read failed
        if (block[0] == 2 && block[1] == 0x44){ //block is an inode block
            if (strncmp(block+4, inodeName, 8) == 0){
                inodeBlockLocation = i;
                break;
            }
        }
    }

    //if not found, create new inode block
    if (inodeBlockLocation < 0){
        inodeBlockLocation = getFreeBlock();
        if (inodeBlockLocation < 0) return TFS_ERR_OPEN; //failed getting next free block

        memset(block, 0, BLOCKSIZE);
        block[0] = 2; //inode block type
        block[1] = 0x44;
        memcpy(block+4, inodeName, 8); //put filename in inode block's bytes 4-11
        intToBytes(0, block+12); //set filesize to 0
        intToBytes(0, block+16); //no data block location yet
        if (writeBlock(mountedDisk, inodeBlockLocation, block) < 0) return TFS_ERR_OPEN; //write new inode block to disk
    }

    //add file to open file table
    int fd = -1;
    for(i = 0; i < MAX_OPEN_FILES; i++){
        if(!openFileTable[i].used){
            fd = i;
            openFileTable[i].used = 1;
            openFileTable[i].inodeBlock = inodeBlockLocation;
            openFileTable[i].filePointer = 0;
            break;
        }
    }

    if (fd < 0) return TFS_ERR_OPEN; //max number of files open
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

int tfs_writeFile(fileDescriptor FD, char *buffer, int size){
    if (FD < 0 || FD > MAX_OPEN_FILES || !openFileTable[FD].used) return TFS_ERR_WRITE;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];
    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_WRITE;

    //free all linked data blocks
    int dataBlockLocation = bytesToInt(inodeBlock+16);
    char dataBlock[BLOCKSIZE];
    while (dataBlockLocation != 0){
        if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) break;
        int next = bytesToInt(dataBlock+4); //get next data block location
        addFreeBlock(dataBlockLocation); //free out current data block
        dataBlockLocation = next;
    }

    // if file size is 0 bytes, just update inode block
    if (size == 0) {
        intToBytes(0, inodeBlock+12); // File size = 0
        intToBytes(0, inodeBlock+16); // No data blocks
        writeBlock(mountedDisk, inodeBlockLocation, inodeBlock);
        openFileTable[FD].filePointer = 0; //set file ptr to 0 (start of file)
        return TFS_SUCCESS;
    }

    //Check if there is enough free space before writing
    int bytesPerBlock = BLOCKSIZE - 8;
    int blocksNeeded = (size + bytesPerBlock - 1) / bytesPerBlock; //ensure correct rounding up for ints
    int availableFreeBlocks = getFreeBlockCount();

    if (blocksNeeded > availableFreeBlocks) return TFS_ERR_WRITE; // Not enough space to write file

    int firstDataBlockLocation = 0;
    int prevBlock = 0;
    int allocatedBlocks[blocksNeeded]; // Track allocated blocks in case of failure
    int allocatedCount = 0;
    
    int i;
    for(i = 0; i < blocksNeeded; i++){
        int currentBlock = getFreeBlock();
        
        if (currentBlock < 0){
            // Free already allocated blocks if we fail mid-write
            freeAllocatedBlocks(allocatedBlocks, allocatedCount);
            return TFS_ERR_WRITE;
        }
        //add current block to allocated list
        allocatedBlocks[allocatedCount++] = currentBlock;

        memset(dataBlock, 0, BLOCKSIZE); //zero out data block before continuing
        dataBlock[0] = 3; //data block type
        dataBlock[1] = 0x44;
        intToBytes(0, dataBlock+4); //initially no next block yet

        //calculate position we are currently at in the buffer (each iteration increments by the # of bytes/block)
        int bufferStartingPosition = i * bytesPerBlock;
        //Ensures that the last block only writes the necessary bytes
        //size - bufferStartingPosition is how many bytes are left to be written
        //if remaining data is less than # bytes/block, write only those bytes
        //otherwise, write the full block (bytesPerBlock)
        int numBytesToWrite = (size - bufferStartingPosition < bytesPerBlock) ? (size - bufferStartingPosition) : bytesPerBlock;
        //copy the buffer at the current position into the data block
        memcpy(dataBlock+8, buffer+bufferStartingPosition, numBytesToWrite);

        //write data block to disk, if fail, free all recently allocated data blocks
        if (writeBlock(mountedDisk, currentBlock, dataBlock) < 0){
            freeAllocatedBlocks(allocatedBlocks, allocatedCount);
            return TFS_ERR_WRITE;
        }

        //store location of first data block, will be needed for the inode block
        if (firstDataBlockLocation == 0) firstDataBlockLocation = currentBlock;

        if (prevBlock != 0){ //if not on first iteration
            if (readBlock(mountedDisk, prevBlock, dataBlock) < 0) return TFS_ERR_WRITE;

            intToBytes(currentBlock, dataBlock+4); //set previous block's next ptr to current block
            
            if (writeBlock(mountedDisk, prevBlock, dataBlock) < 0) {
                freeAllocatedBlocks(allocatedBlocks, allocatedCount);
                return TFS_ERR_WRITE;
            }
        }

        prevBlock = currentBlock;
    }

    intToBytes(size, inodeBlock+12); //store filesize in inode block
    intToBytes(firstDataBlockLocation, inodeBlock+16); //store location for first data block in inode
    if (writeBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_WRITE;

    openFileTable[FD].filePointer = 0; //set file ptr to 0 (start of file)
    return TFS_SUCCESS;
}

int tfs_deleteFile(fileDescriptor FD){
    if (FD < 0 || FD > MAX_OPEN_FILES) return TFS_ERR_DELETE;

    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];

    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_DELETE;

    //free all linked data blocks
    int dataBlockLocation = bytesToInt(inodeBlock+16);
    char dataBlock[BLOCKSIZE];
    while (dataBlockLocation != 0){
        if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) break;
        int next = bytesToInt(dataBlock+4); //get next data block location
        addFreeBlock(dataBlockLocation); //free out current data block
        dataBlockLocation = next;
    }

    //TODO: possibly zero out inode before freeing it?
    //memset(inodeBlock, 0, BLOCKSIZE);
    //if (writeBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_DELETE;
    addFreeBlock(inodeBlockLocation);

    openFileTable[FD].used = 0;
    openFileTable[FD].inodeBlock = -1;
    openFileTable[FD].filePointer = -1;
    return TFS_SUCCESS;
}

int tfs_readByte(fileDescriptor FD, char *buffer){
    if (FD < 0 || FD >= MAX_OPEN_FILES || !openFileTable[FD].used) return TFS_ERR_READ;
    
    int inodeBlockLocation = openFileTable[FD].inodeBlock;
    char inodeBlock[BLOCKSIZE];

    if (readBlock(mountedDisk, inodeBlockLocation, inodeBlock) < 0) return TFS_ERR_READ;
    
    int fileSize = bytesToInt(inodeBlock+12);
    int fpPosition = openFileTable[FD].filePointer;
    //if file ptr is already past the end of file, exit
    if (fpPosition >= fileSize) return TFS_ERR_READ;

    int bytesPerBlock = BLOCKSIZE - 8;
    int blockLocation = fpPosition / bytesPerBlock; //which block the file ptr is on
    int offsetWithinBlock = fpPosition % bytesPerBlock; //where in the block the file ptr is on
    int dataBlockLocation = bytesToInt(inodeBlock+16);
    char dataBlock[BLOCKSIZE];

    //go through list of data blocks until the correct block is found
    int i;
    for(i = 0; i < blockLocation; i++){
        if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) return TFS_ERR_READ;
        dataBlockLocation = bytesToInt(dataBlock+4); //set location to current data block's next ptr
    }

    if (readBlock(mountedDisk, dataBlockLocation, dataBlock) < 0) return TFS_ERR_READ;

    *buffer = dataBlock[8 + offsetWithinBlock];
    openFileTable[FD].filePointer++;
    
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