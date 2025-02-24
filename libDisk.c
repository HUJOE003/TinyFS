#include "libDisk.h"

static Disk disks[MAX_DISKS] = {{NULL, 0, 0}};

int openDisk(char *filename, int nBytes){
    int diskSize = 0;

    if (nBytes != 0){
        if (nBytes < BLOCKSIZE){
            return DISK_INVALID_ARG; // disk size too small for one block
        }
        // round down to largest multiple of BLOCKSIZE <= to nBytes
        diskSize = (nBytes / BLOCKSIZE) * BLOCKSIZE;
    }

    int diskIndex = -1;
    int i;
    //find next available disk
    for (i = 0; i < MAX_DISKS; i++){
        if(!disks[i].inUse){
            diskIndex = i;
            break;
        }
    }

    if (diskIndex == -1){
        return DISK_ERR; //no available disk slots
    }

    FILE *fp = NULL;
    //if nBytes is 0, open an existing disk, don't overwrite content
    if (nBytes == 0){
        fp = fopen(filename, "r+b");
        if (!fp) return DISK_ERR;

        if(fseek(fp, 0, SEEK_END) != 0){ //move file ptr to end of file
            fclose(fp);
            return DISK_ERR;
        }

        diskSize = ftell(fp); //get current file ptr position, gets total size of file
        rewind(fp); //move file ptr back to beginning of file
    } else {
        //otherwise overwrite disk's content
        fp = fopen(filename, "w+b");
        if (!fp) return DISK_ERR;

        char zeros[BLOCKSIZE];
        memset(zeros, 0, BLOCKSIZE); //set zeros to 256bytes of 0's
        int numBlocks = diskSize / BLOCKSIZE;
        int i;
        for(i = 0; i < numBlocks; i++){
            if (fwrite(zeros, 1, BLOCKSIZE, fp) != BLOCKSIZE){ //check to see if all 256 0's were written
                fclose(fp);
                return DISK_ERR;
            }
        }
        fflush(fp); //forces flush buffer, make sure all data in the buffer is actually written to the file
    }

    disks[diskIndex].fp = fp;
    disks[diskIndex].size = diskSize;
    disks[diskIndex].inUse = 1;

    return diskIndex;
}

int closeDisk(int disk){
    if (disk < 0 || disk >= MAX_DISKS || !disks[disk].inUse) return DISK_INVALID_NUM;

    fclose(disks[disk].fp);
    disks[disk].inUse = 0;
    return 0;
}

int readBlock(int disk, int bNum, void *block){
    if(disk < 0 || disk >= MAX_DISKS || !disks[disk].inUse) return DISK_INVALID_NUM;

    int offset = bNum * BLOCKSIZE; //translate bNum into logical block number
    if (offset < 0 || offset + BLOCKSIZE > disks[disk].size) return DISK_INVALID_ARG;

    FILE *fp = disks[disk].fp;
    if (fseek(fp, offset, SEEK_SET) != 0) return DISK_ERR; //move fp to offset position

    size_t numBytesRead = fread(block, 1, BLOCKSIZE, fp); //read 256B into block starting from offset
    if (numBytesRead != BLOCKSIZE) return DISK_ERR;
    
    return 0;
}

int writeBlock(int disk, int bNum, void *block){
    if(disk < 0 || disk >= MAX_DISKS || !disks[disk].inUse) return DISK_INVALID_NUM;

    int offset = bNum * BLOCKSIZE; //translate bNum into logical block number
    if (offset < 0 || offset + BLOCKSIZE > disks[disk].size) return DISK_INVALID_ARG;

    FILE *fp = disks[disk].fp;
    if (fseek(fp, offset, SEEK_SET) != 0) return DISK_ERR; //move fp to offset position

    size_t numBytesWritten = fwrite(block, 1, BLOCKSIZE, fp);
    if (numBytesWritten != BLOCKSIZE) return DISK_ERR;

    fflush(fp); //ensure bytes are actually written
    return 0;

}