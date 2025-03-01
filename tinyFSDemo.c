/*
 * tinyFSDemo.c
 *
 * Demonstrates basic TinyFS operations and edge case tests.
 *
 * Compile with the provided Makefile.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libTinyFS.h"
#include "libDisk.h"
#include "TinyFS_errno.h"

int main() {
    int result;
    fileDescriptor fd;
    char diskName[] = "tinyFSDisk";
    // Specify disk size in bytes. (Ensure this is a multiple of BLOCKSIZE.)
    int diskSize = 1024 * 10;  

    printf("=== TinyFS Demo ===\n");

    /* Basic Operations Demo */
    printf("\n[Basic Demo] Creating a new file system on disk '%s' with size %d bytes...\n",
           diskName, diskSize);
    result = tfs_mkfs(diskName, diskSize);
    if (result != TFS_SUCCESS) {
        printf("Error creating file system: %d\n", result);
        return 1;
    }

    printf("\nMounting the file system...\n");
    result = tfs_mount(diskName);
    if (result != TFS_SUCCESS) {
        printf("Error mounting file system: %d\n", result);
        return 1;
    }

    printf("\nOpening file 'testfile'...\n");
    fd = tfs_openFile("testfile");
    if (fd < 0) {
        printf("Error opening file: %d\n", fd);
        return 1;
    }

    char data[] = "Hello, TinyFS!";
    printf("\nWriting to file 'testfile': \"%s\"\n", data);
    result = tfs_writeFile(fd, data, strlen(data));
    if (result != TFS_SUCCESS) {
        printf("Error writing file: %d\n", result);
        return 1;
    }

    printf("\nReading file info for 'testfile':\n");
    result = tfs_readFileInfo(fd);
    if (result != TFS_SUCCESS) {
        printf("Error reading file info: %d\n", result);
        return 1;
    }

    int offset = 7;
    char newByte = 'X';
    printf("\nWriting a single byte '%c' at offset %d in 'testfile'...\n", newByte, offset);
    result = tfs_writeByte(fd, offset, (unsigned int)newByte);
    if (result != TFS_SUCCESS) {
        printf("Error writing byte: %d\n", result);
        return 1;
    }

    printf("\nReading file 'testfile' byte-by-byte:\n");
    result = tfs_seek(fd, 0);
    if (result != TFS_SUCCESS) {
        printf("Error seeking in file: %d\n", result);
        return 1;
    }
    int i, fileSize = strlen(data);
    for (i = 0; i < fileSize; i++) {
        char ch;
        result = tfs_readByte(fd, &ch);
        if (result != TFS_SUCCESS) {
            printf("Error reading byte at position %d: %d\n", i, result);
            break;
        }
        printf("%c", ch);
    }
    printf("\n");

    printf("\nRenaming file 'testfile' to 'newname'...\n");
    result = tfs_rename(fd, "newname");
    if (result != TFS_SUCCESS) {
        printf("Error renaming file: %d\n", result);
    }

    printf("\nDirectory Listing:\n");
    result = tfs_readdir();
    if (result != TFS_SUCCESS) {
        printf("Error listing directory: %d\n", result);
    }

    printf("\nSetting file 'newname' to read-only...\n");
    result = tfs_makeRO("newname");
    if (result != TFS_SUCCESS) {
        printf("Error setting file to read-only: %d\n", result);
    }

    printf("\nAttempting to write to read-only file 'newname' (this should fail)...\n");
    result = tfs_writeFile(fd, "Another text", strlen("Another text"));
    if (result == TFS_SUCCESS) {
        printf("Unexpectedly succeeded in writing to a read-only file.\n");
    } else {
        printf("Correctly failed to write to a read-only file.\n");
    }

    printf("\nChanging file 'newname' to read-write...\n");
    result = tfs_makeRW("newname");
    if (result != TFS_SUCCESS) {
        printf("Error setting file to read-write: %d\n", result);
    }

    printf("\nWriting new content to 'newname'...\n");
    result = tfs_writeFile(fd, "New Content", strlen("New Content"));
    if (result != TFS_SUCCESS) {
        printf("Error writing new content: %d\n", result);
    }

    printf("\nUnmounting the file system...\n");
    result = tfs_unmount();
    if (result != TFS_SUCCESS) {
        printf("Error unmounting file system: %d\n", result);
        return 1;
    }

    printf("\nTinyFS basic demo completed successfully.\n");

    /* =========== EDGE CASE TESTS =========== */
    printf("\n=== EDGE CASE TESTS ===\n");

    /* Edge Case 1: Create FS with a size not a multiple of BLOCKSIZE */
    printf("\n[Edge Case 1] Creating FS with size not a multiple of BLOCKSIZE...\n");
    result = tfs_mkfs("edge_nonmultiple.bin", BLOCKSIZE + 1);
    if (result == TFS_SUCCESS)
        printf("Unexpectedly succeeded in creating FS with non-multiple BLOCKSIZE size.\n");
    else
        printf("Correctly failed to create FS with non-multiple BLOCKSIZE size. Error: %d\n", result);

    /* Edge Case 2: Create FS with size zero */
    printf("\n[Edge Case 2] Creating FS with zero size...\n");
    result = tfs_mkfs("edge_zero.bin", 0);
    if (result == TFS_SUCCESS)
        printf("Unexpectedly succeeded in creating FS with zero size.\n");
    else
        printf("Correctly failed to create FS with zero size. Error: %d\n", result);

    /* Edge Case 3: Open a file with a name longer than 8 characters */
    printf("\n[Edge Case 3] Opening file with a name longer than 8 characters...\n");
    result = tfs_openFile("TooLongFileName");
    if (result >= 0)
        printf("Unexpectedly succeeded in opening a file with a too long name.\n");
    else
        printf("Correctly failed to open file with a too long name. Error: %d\n", result);

    /* 
       For Edge Cases 4, 5, and 6 we need a mounted filesystem.
       Remount the default filesystem before continuing.
    */
    printf("\n[Edge Cases Setup] Remounting filesystem '%s' for further tests...\n", diskName);
    result = tfs_mount(diskName);
    if (result != TFS_SUCCESS) {
        printf("Error mounting FS for edge cases: %d\n", result);
        return 1;
    }

    /* Edge Case 4: Writing to a closed file */
    printf("\n[Edge Case 4] Writing to a closed file...\n");
    fd = tfs_openFile("tempfile");
    if (fd < 0) {
        printf("Error opening 'tempfile': %d\n", fd);
    } else {
        result = tfs_closeFile(fd);
        if (result != TFS_SUCCESS) {
            printf("Error closing 'tempfile': %d\n", result);
        }
        result = tfs_writeFile(fd, "Data", strlen("Data"));
        if (result == TFS_SUCCESS)
            printf("Unexpectedly succeeded in writing to a closed file.\n");
        else
            printf("Correctly failed to write to a closed file. Error: %d\n", result);
    }

    /* Edge Case 5: Seeking beyond the end of a file */
    printf("\n[Edge Case 5] Seeking beyond the end of a file...\n");
    fd = tfs_openFile("seekTest");
    if (fd < 0) {
        printf("Error opening 'seekTest': %d\n", fd);
    } else {
        result = tfs_writeFile(fd, "Short", strlen("Short"));
        if (result != TFS_SUCCESS) {
            printf("Error writing to 'seekTest': %d\n", result);
        } else {
            result = tfs_seek(fd, 1000);  // far beyond file size
            if (result == TFS_SUCCESS)
                printf("Unexpectedly succeeded in seeking beyond file end.\n");
            else
                printf("Correctly failed to seek beyond file end. Error: %d\n", result);
        }
    }

    /* Edge Case 6: Writing a byte outside the file's range */
    printf("\n[Edge Case 6] Writing a byte at an invalid offset...\n");
    fd = tfs_openFile("byteTest");
    if (fd < 0) {
        printf("Error opening 'byteTest': %d\n", fd);
    } else {
        result = tfs_writeFile(fd, "12345", 5);
        if (result != TFS_SUCCESS) {
            printf("Error writing to 'byteTest': %d\n", result);
        } else {
            // Attempt to write a byte at offset equal to file size (invalid)
            result = tfs_writeByte(fd, 5, 'Z');
            if (result == TFS_SUCCESS)
                printf("Unexpectedly succeeded in writing a byte out-of-range.\n");
            else
                printf("Correctly failed to write a byte out-of-range. Error: %d\n", result);
        }
    }

    /* Edge Case 7: Mounting a non-existent filesystem */
    printf("\n[Edge Case 7] Mounting a non-existent filesystem...\n");
    result = tfs_mount("nonexistent.bin");
    if (result == TFS_SUCCESS)
        printf("Unexpectedly succeeded in mounting a non-existent filesystem.\n");
    else
        printf("Correctly failed to mount a non-existent filesystem. Error: %d\n", result);

    /* Edge Case 8: Unmounting when no filesystem is mounted */
printf("\n[Edge Case 8] Unmounting when no filesystem is mounted...\n");
// First, unmount the filesystem (this should succeed)
result = tfs_unmount();
if (result != TFS_SUCCESS) {
    printf("Error unmounting FS for edge case 8: %d\n", result);
} else {
    // Now, try to unmount again (with no FS mounted)
    result = tfs_unmount();
    if (result == TFS_SUCCESS)
        printf("Unexpectedly succeeded in unmounting when nothing is mounted.\n");
    else
        printf("Correctly failed to unmount when nothing is mounted. Error: %d\n", result);
}


    return 0;
}
