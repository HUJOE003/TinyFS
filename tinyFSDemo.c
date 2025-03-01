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
#include <unistd.h> 

#include "libTinyFS.h"
#include "libDisk.h"
#include "TinyFS_errno.h"

#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"


void printHeader() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf(CYAN "==========================================================================================\n" RESET);
    printf(CYAN" _    _       _                            _ __  __       _                      \n");
    printf(CYAN"| |  | |     (_)                          | |  \\/  |     | |                     \n");
    printf(CYAN"| |__| |_   _ _  ___   ___  __ _ _ __   __| | \\  / | __ _| |_ ___  ___ _ __  ___ \n");
    printf(CYAN"|  __  | | | | |/ _ \\ / _ \\/ _` | '_ \\ / _` | |\\/| |/ _` | __/ _ \\/ _ \\ '_ \\/ __|\n");
    printf(CYAN"| |  | | |_| | | (_) |  __/ (_| | | | | (_| | |  | | (_| | ||  __/  __/ | | \\__ \\\n");
    printf(CYAN"|_|  |_|\\__,_| |\\___/ \\___|\\__,_|_| |_|\\__,_|_|  |_|\\__,_|\\__\\___|\\___|_| |_|___/\n");
    printf(CYAN"            _/ |\n");
    printf(CYAN"           |__/\n");
    printf("\n");
    printf(RED " TinyFS Demo\n" RESET);
    printf(CYAN "==========================================================================================\n" RESET);
    printf("Timestamp: %02d:%02d:%02d\n\n", t->tm_hour, t->tm_min, t->tm_sec);
    sleep(1);
}

// Helper function to print operation status
void printStatus(const char *operation, int result) {
    if(result == TFS_SUCCESS)
        printf(GREEN "[SUCCESS] " RESET "%s\n", operation);
    else
        printf(RED "[ERROR %d] " RESET "%s\n", result, operation);
    sleep(1);
}

int main() {
    int result;
    fileDescriptor fd;
    char diskName[] = "tinyFSDisk";
    // Specify disk size in bytes. (Ensure this is a multiple of BLOCKSIZE.)
    int diskSize = 1024 * 10;

    printHeader();
    printf(BLUE "Starting TinyFS operations demo...\n" RESET);
    sleep(1);

    /* Basic Operations Demo */
    printf(YELLOW "\n[Basic Demo] Creating a new file system on disk '%s' with size %d bytes...\n" RESET,
           diskName, diskSize);
    result = tfs_mkfs(diskName, diskSize);
    printStatus("Creating file system", result);
    if (result != TFS_SUCCESS) return 1;

    printf(YELLOW "\nMounting the file system...\n" RESET);
    result = tfs_mount(diskName);
    printStatus("Mounting file system", result);
    if (result != TFS_SUCCESS) return 1;

    printf(YELLOW "\nOpening file 'testfile'...\n" RESET);
    fd = tfs_openFile("testfile");
    if (fd < 0) {
        printStatus("Opening file 'testfile'", fd);
        return 1;
    } else {
        printStatus("Opening file 'testfile'", TFS_SUCCESS);
    }

    char data[] = "Hello, TinyFS!";
    printf(YELLOW "\nWriting to file 'testfile': \"%s\"\n" RESET, data);
    result = tfs_writeFile(fd, data, strlen(data));
    printStatus("Writing data to 'testfile'", result);
    if (result != TFS_SUCCESS) return 1;

    printf(YELLOW "\nReading file info for 'testfile':\n" RESET);
    result = tfs_readFileInfo(fd);
    printStatus("Reading file info", result);
    if (result != TFS_SUCCESS) return 1;

    int offset = 7;
    char newByte = 'X';
    printf(YELLOW "\nOverwriting a single byte '%c' at offset %d in 'testfile'...\n" RESET, newByte, offset);
    result = tfs_writeByte(fd, offset, (unsigned int)newByte);
    printStatus("Overwriting byte", result);
    if (result != TFS_SUCCESS) return 1;

    printf(YELLOW "\nReading file 'testfile' byte-by-byte:\n" RESET);
    result = tfs_seek(fd, 0);
    if (result != TFS_SUCCESS) {
        printStatus("Seeking to beginning", result);
        return 1;
    }
    int i, fileSize = strlen(data);
    for (i = 0; i < fileSize; i++) {
        char ch;
        result = tfs_readByte(fd, &ch);
        if (result != TFS_SUCCESS) {
            printf(RED "Error reading byte at position %d: %d\n" RESET, i, result);
            break;
        }
        printf("%c", ch);
        fflush(stdout);
        usleep(150000);  // short delay for a cool typewriter effect
    }
    printf("\n");

    printf(YELLOW "\nRenaming file 'testfile' to 'newname'...\n" RESET);
    result = tfs_rename(fd, "newname");
    printStatus("Renaming file", result);

    printf(YELLOW "\nDirectory Listing:\n" RESET);
    result = tfs_readdir();
    printStatus("Directory Listing", result);

    printf(YELLOW "\nSetting file 'newname' to read-only...\n" RESET);
    result = tfs_makeRO("newname");
    printStatus("Setting read-only", result);

    printf(YELLOW "\nAttempting to write to read-only file 'newname' (should fail)...\n" RESET);
    result = tfs_writeFile(fd, "Another text", strlen("Another text"));
    if (result == TFS_SUCCESS)
        printf(RED "Unexpectedly succeeded in writing to a read-only file.\n" RESET);
    else
        printf(GREEN "Correctly failed to write to a read-only file.\n" RESET);
    sleep(1);

    printf(YELLOW "\nChanging file 'newname' to read-write...\n" RESET);
    result = tfs_makeRW("newname");
    printStatus("Changing to read-write", result);

    printf(YELLOW "\nWriting new content to 'newname'...\n" RESET);
    result = tfs_writeFile(fd, "New Content", strlen("New Content"));
    printStatus("Writing new content", result);

    printf(YELLOW "\nUnmounting the file system...\n" RESET);
    result = tfs_unmount();
    printStatus("Unmounting file system", result);
    if (result != TFS_SUCCESS) return 1;

    printf(MAGENTA "\nTinyFS basic demo completed successfully!\n" RESET);
    sleep(2);

    /* =========== EDGE CASE TESTS =========== */
    printf(CYAN "\n============================================\n" RESET);
    printf(CYAN "            EDGE CASE TESTS\n" RESET);
    printf(CYAN "============================================\n\n" RESET);
    sleep(1);

    /* Edge Case 1: Create FS with a size not a multiple of BLOCKSIZE */
    printf(YELLOW "[Edge Case 1] Creating FS with size not a multiple of BLOCKSIZE...\n" RESET);
    result = tfs_mkfs("edge_nonmultiple.bin", BLOCKSIZE + 1);
    if (result == TFS_SUCCESS)
        printf(RED "Unexpectedly succeeded in creating FS with non-multiple BLOCKSIZE size.\n" RESET);
    else
        printf(GREEN "Correctly failed to create FS with non-multiple BLOCKSIZE size. Error: %d\n" RESET, result);
    sleep(1);

    /* Edge Case 2: Create FS with size zero */
    printf(YELLOW "\n[Edge Case 2] Creating FS with zero size...\n" RESET);
    result = tfs_mkfs("edge_zero.bin", 0);
    if (result == TFS_SUCCESS)
        printf(RED "Unexpectedly succeeded in creating FS with zero size.\n" RESET);
    else
        printf(GREEN "Correctly failed to create FS with zero size. Error: %d\n" RESET, result);
    sleep(1);

    /* Edge Case 3: Open a file with a name longer than 8 characters */
    printf(YELLOW "\n[Edge Case 3] Opening file with a name longer than 8 characters...\n" RESET);
    result = tfs_openFile("TooLongFileName");
    if (result >= 0)
        printf(RED "Unexpectedly succeeded in opening a file with a too long name.\n" RESET);
    else
        printf(GREEN "Correctly failed to open file with a too long name. Error: %d\n" RESET, result);
    sleep(1);

    /* For Edge Cases 4, 5, and 6 we need a mounted filesystem.
       Remount the default filesystem before continuing.
    */
    printf(YELLOW "\n[Edge Cases Setup] Remounting filesystem '%s' for further tests...\n" RESET, diskName);
    result = tfs_mount(diskName);
    printStatus("Remounting filesystem", result);
    if (result != TFS_SUCCESS) return 1;

    /* Edge Case 4: Writing to a closed file */
    printf(YELLOW "\n[Edge Case 4] Writing to a closed file...\n" RESET);
    fd = tfs_openFile("tempfile");
    if (fd < 0) {
        printf(RED "Error opening 'tempfile': %d\n" RESET, fd);
    } else {
        result = tfs_closeFile(fd);
        printStatus("Closing 'tempfile'", result);
        result = tfs_writeFile(fd, "Data", strlen("Data"));
        if (result == TFS_SUCCESS)
            printf(RED "Unexpectedly succeeded in writing to a closed file.\n" RESET);
        else
            printf(GREEN "Correctly failed to write to a closed file. Error: %d\n" RESET, result);
    }
    sleep(1);

    /* Edge Case 5: Seeking beyond the end of a file */
    printf(YELLOW "\n[Edge Case 5] Seeking beyond the end of a file...\n" RESET);
    fd = tfs_openFile("seekTest");
    if (fd < 0) {
        printf(RED "Error opening 'seekTest': %d\n" RESET, fd);
    } else {
        result = tfs_writeFile(fd, "Short", strlen("Short"));
        if (result != TFS_SUCCESS) {
            printf(RED "Error writing to 'seekTest': %d\n" RESET, result);
        } else {
            result = tfs_seek(fd, 1000);  // far beyond file size
            if (result == TFS_SUCCESS)
                printf(RED "Unexpectedly succeeded in seeking beyond file end.\n" RESET);
            else
                printf(GREEN "Correctly failed to seek beyond file end. Error: %d\n" RESET, result);
        }
    }
    sleep(1);

    /* Edge Case 6: Writing a byte outside the file's range */
    printf(YELLOW "\n[Edge Case 6] Writing a byte at an invalid offset...\n" RESET);
    fd = tfs_openFile("byteTest");
    if (fd < 0) {
        printf(RED "Error opening 'byteTest': %d\n" RESET, fd);
    } else {
        result = tfs_writeFile(fd, "12345", 5);
        if (result != TFS_SUCCESS) {
            printf(RED "Error writing to 'byteTest': %d\n" RESET, result);
        } else {
            // Attempt to write a byte at offset equal to file size (invalid)
            result = tfs_writeByte(fd, 5, 'Z');
            if (result == TFS_SUCCESS)
                printf(RED "Unexpectedly succeeded in writing a byte out-of-range.\n" RESET);
            else
                printf(GREEN "Correctly failed to write a byte out-of-range. Error: %d\n" RESET, result);
        }
    }
    sleep(1);

    /* Edge Case 7: Mounting a non-existent filesystem */
    printf(YELLOW "\n[Edge Case 7] Mounting a non-existent filesystem...\n" RESET);
    result = tfs_mount("nonexistent.bin");
    if (result == TFS_SUCCESS)
        printf(RED "Unexpectedly succeeded in mounting a non-existent filesystem.\n" RESET);
    else
        printf(GREEN "Correctly failed to mount a non-existent filesystem. Error: %d\n" RESET, result);
    sleep(1);

    /* Edge Case 8: Unmounting when no filesystem is mounted */
    printf(YELLOW "\n[Edge Case 8] Unmounting when no filesystem is mounted...\n" RESET);
    result = tfs_unmount();
    if (result != TFS_SUCCESS) {
        printf(RED "Error unmounting FS for edge case 8: %d\n" RESET, result);
    } else {
        result = tfs_unmount();
        if (result == TFS_SUCCESS)
            printf(RED "Unexpectedly succeeded in unmounting when nothing is mounted.\n" RESET);
        else
            printf(GREEN "Correctly failed to unmount when nothing is mounted. Error: %d\n" RESET, result);
    }
    sleep(1);

    printf(MAGENTA "\nTinyFS demo (including edge cases) completed. Goodbye!\n" RESET);
    return 0;
}
