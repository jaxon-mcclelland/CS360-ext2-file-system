int do_cat(char* filename)
{
    int fd = open_file(filename, 0);
    int n;
    int total = 0;
    if(fd < 0) {
        return fd;
    }
    do_lseek(fd, 0); 
    char buff[BLKSIZE];
    while(n = myread(fd, buff, BLKSIZE)) {
        buff[n] = '\0';
        printf("%s", buff);
        total += n;
    }
    printf("Printed %d bytes from file\n",total);
    close_file(fd);
}

int do_cp(char* src_path, char* dest_path)
{
    int fd = open_file(src_path, 0);
    if(fd < 0) {
        return fd; // couldn't open file
    }
    int gd = open_file(dest_path, 1);
    if(gd < 0) {
        return gd;
    }
    if(fd == gd) {
        printf("Error - cannot cp %s to %s - same file\n", src_path, dest_path);
        close_file(fd);
        close_file(gd);
        return -1;
    }
    truncate(running->fd[gd]->inodeptr);
    do_lseek(fd, 0); // want to read from the beginning of the file
    char buff[BLKSIZE];
    while(n = myread(fd, buff, BLKSIZE)) {
        mywrite(gd, buff, n);
    }
    close_file(fd);
    close_file(gd);
}
