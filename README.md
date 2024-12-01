This repository contains an EXT2 file system written for CPTS 360. It features the following commands:

cd - change working directory to the provided path
ls - display the contents of the CWD
pwd - display the path of the CWD
mkdir - create a new directory
create - create a new file
rmdir - delete a directory
link - create a link to a file or directory
unlink - deletes the provided link
symlink - creates a symbolic link
chmod - change the permissions of a file/dir
open - opens the provided file
close - closes the provided file 
read - reads a specific number of bytes from the provided file descriptor
write - writes the provided string to the provided file descriptor
cat - writes the contents of a file to the console
cp - copies a source file to a destination path
head - Displays first 10 lines of a file
tail - Displays last 10 lines of a file

show - shows contents of CWD (same functionality as ls)
hits - Shows the hit/miss ratio of the filesystem cache
exit - exits the program

To build this project, you need to install either `e2fsprogs-devel` on fedora, or `e2fslibs-dev` on ubuntu

Then simply run `gcc main.c -o output`
