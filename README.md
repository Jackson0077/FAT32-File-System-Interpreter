FAT32 File System Interpreter

Overview

This program is a FAT32 file system interpreter written in C. It allows users to interact with a FAT32-formatted disk image through a custom shell interface (msf>). The interpreter supports a variety of commands for file and directory manipulation.

Features

File system operations:

OPEN <filename>: Opens a FAT32 disk image.

CLOSE: Closes the currently opened disk image.

INFO: Displays file system information.

File operations:

GET <filename>: Retrieves a file from the disk image.

PUT <filename> [newname]: Adds a file to the disk image, optionally renaming it.

READ <filename> <position> <num_bytes> [-ASCII | -DEC]: Reads bytes from a file.

DEL <filename>: Marks a file as deleted.

UNDEL <filename>: Recovers a deleted file.

Directory operations:

CD <path>: Changes the current directory (supports relative and absolute paths).

LS [..]: Lists the contents of the current or parent directory.

Meta commands:

EXIT or QUIT: Exits the program.

Compilation

Compile the program using:

gcc -o mfs mfs.c

Usage

Run the compiled program:

./mfs

Once running, use the available commands to interact with the file system.

Example session:

msf> OPEN disk.img
msf> INFO
msf> LS
msf> CD Documents
msf> GET file.txt
msf> PUT newfile.txt
msf> EXIT

Implementation

The program parses user commands with strtok and processes them case-insensitively.

FAT32 metadata is read from the disk image using direct file operations (fseek, fread, fwrite).

Directory entries are represented using a DirectoryEntry struct.

Cluster addressing and traversal are implemented via LABToOffset and NextLB functions.

Error Handling

The program includes basic error checking, with error messages for:

Invalid commands or missing arguments.

Attempting operations without an open disk image.

File not found errors.

Limitations

Assumes a valid FAT32 disk image is used.

Does not support long file names (uses the 8.3 filename format).

Limited error handling for complex directory structures.

Acknowledgments

This project was developed as part of an educational exercise in file system design and C programming.

Let me know if you'd like me to adjust the structure or tone!
