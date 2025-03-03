#include <stdio.h>
#include <stdlib.h>
#include "libTinyFS.h"

#define TEST_DISK "defragTestDisk"
#define DISK_SIZE 20480 // 20KB Disk

int main() {
    // Step 1: Create and mount the filesystem
    if (tfs_mkfs(TEST_DISK, DISK_SIZE) != 0) {
        printf("Failed to create filesystem.\n");
        return 1;
    }
    if (tfs_mount(TEST_DISK) != 0) {
        printf("Failed to mount filesystem.\n");
        return 1;
    }

    // Step 2: Create and write to multiple files
    fileDescriptor fd1 = tfs_openFile("fileA");
    fileDescriptor fd2 = tfs_openFile("fileB");
    fileDescriptor fd3 = tfs_openFile("fileC");
    fileDescriptor fd4 = tfs_openFile("fileD");
    fileDescriptor fd5 = tfs_openFile("fileE");
    fileDescriptor fd6 = tfs_openFile("fileF");
    fileDescriptor fd7 = tfs_openFile("fileG");
    fileDescriptor fd8 = tfs_openFile("fileH");
    fileDescriptor fd9 = tfs_openFile("fileI");


    if (fd1 < 0 || fd2 < 0 || fd3 < 0 || fd4 < 0) {
        printf("Error opening files.\n");
        return 1;
    }

    char *dataA = "Data in file A spanning blocks.";
    char *dataB = "File B content that takes multiple blocks.";
    char *dataC = "File C with smaller content.";
    char *dataD = "File D adding more data.";
    char *dataE = "File D adding more data.";
    char *dataF = "File D adding more data.";
    char *dataG = "File D adding more data.";
    char *dataH = "File D adding more data.";
    char *dataI = "File D adding more data.";

    tfs_writeFile(fd1, dataA, 30);
    tfs_writeFile(fd2, dataB, 50);
    tfs_writeFile(fd3, dataC, 20);
    tfs_writeFile(fd4, dataD, 35);
    tfs_writeFile(fd5, dataE, 35);
    tfs_writeFile(fd6, dataF, 35);
    tfs_writeFile(fd7, dataG, 35);
    tfs_writeFile(fd8, dataH, 35);
    tfs_writeFile(fd9, dataI, 35);

    printf("\n--- Before Deleting ---\n");
    tfs_displayFragments();

    // Step 3: Remove some files to create fragmentation
    tfs_deleteFile(fd2);
    tfs_deleteFile(fd3);
    
    printf("\n--- Before Defragmentation ---\n");
    tfs_displayFragments();

    // Step 4: Run defragmentation
    printf("Running defragmentation...\n");
    tfs_defrag();

    // Step 5: Display fragmentation after defrag
    printf("\n--- After Defragmentation ---\n");
    tfs_displayFragments();

    // Step 6: Unmount filesystem
    tfs_unmount();
    printf("Filesystem unmounted successfully.\n");
    return 0;
}