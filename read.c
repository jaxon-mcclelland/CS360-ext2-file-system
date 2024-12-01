extern int dev;

int read_file(int fd, int nbytes) {
    if(fd >= NFD || running->fd[fd] == NULL) {
        printf("Error - %d is not a valid fd\n", fd);
        return -1;
    } else if(running->fd[fd]->mode != 0 || running->fd[fd]->mode != 2) {
        printf("Error -%d is open in an incompatible mode\n", fd);
        return -1;
    }
    if(nbytes < 0) {
        puts("Error - must input a positive number of bytes to read");
        return -1;
    }
    char read_buff[nbytes];
    int res =  myread(fd, read_buff, nbytes);
    puts(read_buff);
    return res;
}

int myread(int fd, char buf[], int nbytes) {
    int curr_block = running->fd[fd]->offset / BLKSIZE;
    int start_byte = running->fd[fd]->offset % BLKSIZE;
    INODE* ip = &(running->fd[fd]->inodeptr->INODE);

    int file_remaining_bytes = ip->i_size - running->fd[fd]->offset;
    // if nbytes exceeds file size, just read the rest of the file
    if(nbytes > file_remaining_bytes) {
        nbytes = file_remaining_bytes;
    }

    int count = 0;    

    char block_buffer[BLKSIZE]; // buffer to read into from disk
    int indbuff[256]; // hold block numbers of data blocks (for indirect blocks) or block numbers of doubly indirect block numbers
    int double_indbuff[256];
    char* cp = buf;

    while(nbytes > 0)
    {
        int bytes_to_read = BLKSIZE - start_byte;
        if(nbytes < bytes_to_read) 
        {
            bytes_to_read = nbytes;
        }

        // read block_buffer
        if(curr_block < 12)
        { 
            get_block(dev, ip->i_block[curr_block], block_buffer);
        } 
        else if(curr_block < 268) 
        {
            get_block(dev, ip->i_block[12], indbuff);
            get_block(dev, indbuff[curr_block - 12], block_buffer);
        }
        else 
        {
            get_block(dev, ip->i_block[13], indbuff); 
            get_block(dev, indbuff[(curr_block - 12 - 256) >> 8], double_indbuff);  // (curr_block - 12 - 256) / 256 => double indirect block #
            get_block(dev, double_indbuff[(curr_block-12-256) % 256], block_buffer); // (curr_block - 12 - 256) % 256 => which entry in double indirect block (aka where we want to read data from)
        }
        strncpy(cp, block_buffer + start_byte, bytes_to_read); 
        nbytes -= bytes_to_read;
        start_byte = 0;
        running->fd[fd]->offset += bytes_to_read;
        count += bytes_to_read;
        cp += bytes_to_read;
        ++curr_block;
    }

    return count;
}