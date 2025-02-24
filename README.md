# CSC-453-Assgn-4

## Todo

### Additional Features

We have the first 70% of the implementation completed, now we can work on adding the bonus features.

I went through and looked into all of them, factoring which ones would be the easiest and most compatible with each other. I believe that easiest 30% consists of:
1. (b) Directory listing and file renaming (10%)
2. (d) Read-only and writeByte support (10%)
3. (e) Timestamps (10%)

If we want to do the extra credit (I think we should give it a try doesn't seem too hard), we just focus on:
1. (a) Fragmentation info/defrag (10%)
2. (h) File system consistency checks (10%)

> See the bottom of this readme for how I think we should implement these.

### Test Cases
We should create a couple test files which test out every feature of the disk and file system. There are given ones which pass but don't test everything.

#### Running Tests

I have created two commands which makes it easier to run the given test files, these commands clean the output files, create the test executable, and run it. 

 > ⚠️ **Heads up:** These given test files have different outputs depending on whether its being run for the first time or not, to see the different outputs just run the executable after, not the make command.

To run the given diskTest file,
```bash
make diskTestGiven
```
To run the given tfsTest file,
```bash
make tfsTestGiven
```

### Demo Program
We also have to create a demo program and video which demonstrates the basic functionality of the required functions and the chosen additional functionality. 

### Getting Submission Ready
When we're done we should also go through and comment our code, make it look good, and make sure everything does what it is intended to.

I am not entirely sure if the TinyFS_errno.h file should be capitalized, in the test files it is but in the spec it isn't.

Edit the Makefile that compiles all the libraries and makes the executable tinyFSDemo.

Format the README and fill out survey.

## Implenting the Additional Features
Like I said, I think the easiest ones to do are all the 10 pointers.

1. (b) Directory listing and file renaming (10%)

    `tfs_readdir()`: To list all files, we just iterate over all inodes. 
    `tfs_rename()`: Renaming a file is simply updating the file’s name field in its inode.

2. (d) Read-only and writeByte support (10%)

    Keep a small “read-only” flag inside the inode. If set, any attempts to write or delete the file fail with an error.
    `tfs_writeByte(...)` just means indexing into the file data at a given offset. We can reuse `tfs_readByte` and block pointer logic.

3. (e) Timestamps (10%)

    Store three timestamps (creation, modification, and access) in each inode. Update them when you create, write, read, or rename the file.
    `tfs_readFileInfo(FD)` that returns or prints these timestamps.

4. (a) Fragmentation info/defrag (10%)

    A little harder then the prev 3 but I think it would look cool if we did something like on the first page of the project spec (with the color coded disk).

5. (h) File system consistency checks (10%)

    This one has some creative elements to it as its up to us what we want to check. Some I thought of were:
    1. No blocks that are both free and have an inode
    2. Verify that each block is accounted for either by being in a file’s linked chain of data blocks or on the free list
    3. There is a given pdf which talks about more stuff we can do.