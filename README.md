# FAT32 File System Interpreter

## Overview
This project implements a simple FAT32 file system interpreter in C. It allows users to open a FAT32 disk image and interact with it using various commands, such as listing directories, retrieving file statistics, and manipulating files and directories. 

## Features
The following commands are supported:

- **OPEN \<file\>** — Opens a FAT32 file system image for reading and writing.
- **CLOSE** — Closes the currently open file system image.
- **INFO** — Displays key file system information, including bytes per sector, sectors per cluster, and FAT size.
- **LS** — Lists the contents of the current or parent directory.
- **CD \<directory\>** — Changes the current directory. Supports relative and absolute paths.
- **STAT \<file/dir\>** — Displays the attributes and starting cluster number of a file or directory.
- **GET \<file\> [new_name]** — Copies a file from the FAT32 image to the local file system.
- **PUT \<file\> [new_name]** — Copies a file from the local file system into the FAT32 image.
- **READ \<file\> \<position\> \<bytes\> [-ASCII|-DEC]** — Reads a specified number of bytes from a file at a given position, with optional output formatting.
- **DEL \<file\>** — Deletes a file by marking its directory entry as deleted.
- **UNDEL \<file\>** — Recovers a deleted file if its entry has not been overwritten.
- **EXIT/QUIT** — Exits the program.

## Compilation
Compile the program using `gcc`:
```bash
gcc mfs.c -o mfs
