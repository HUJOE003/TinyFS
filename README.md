# CSC-453-Assgn-4

## Todo

### Extra Credit

If we want to do the extra credit (I think we should give it a try doesn't seem too hard), we just focus on:
1. (a) Fragmentation info/defrag (10%)
2. (h) File system consistency checks (10%)

### Demo Program + Video
If we implement EC, edit the demo program make a video which demonstrates the basic functionality of the required functions and the chosen additional functionality. 

### Getting Submission Ready
When we're done we should also go through and comment our code, make it look good, and make sure everything does what it is intended to.

I am not entirely sure if the TinyFS_errno.h file should be capitalized, in the test files it is but in the spec it isn't.

Format the README and fill out survey.

## Implenting the Extra Credit

1. (a) Fragmentation info/defrag (10%)

    A little harder then the prev 3 but I think it would look cool if we did something like on the first page of the project spec (with the color coded disk).

2. (h) File system consistency checks (10%)

    This one has some creative elements to it as its up to us what we want to check. Some I thought of were:
    1. No blocks that are both free and have an inode
    2. Verify that each block is accounted for either by being in a file’s linked chain of data blocks or on the free list
    3. There is a given pdf which talks about more stuff we can do.
