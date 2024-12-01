// allocates new block - zeros it out on disk - returns zeroed out blk number
int create_and_zero_block(int target_dev)
{
    int blk = balloc(target_dev);
    char target_buff[BLKSIZE];
    get_block(target_dev, blk, target_buff);
    memset(target_buff, 0, BLKSIZE);
    put_block(target_dev, blk, target_buff);
    return blk;
}

int write_file(int fd, char* data)
{
    if(running->fd[fd]->mode < 1)
    {
        printf("Error - cannot write to %d - fd not open in compatible mode\n", fd);
        return -1;
    }
    int nbytes = strlen(data);
    char buff[nbytes];
    strcpy(buff, data);
    return mywrite(fd, buff, nbytes);

}
int mywrite(int fd, char buf[], int nbytes)
{
    int curr_block = running->fd[fd]->offset / BLKSIZE; 
    int start_byte = running->fd[fd]->offset % BLKSIZE;
    int target_dev = running->fd[fd]->inodeptr->dev;
    INODE* ip = &(running->fd[fd]->inodeptr->INODE);

    int count = 0; // amnt of bytes we have written
    
    char block_buffer[BLKSIZE]; // data from block we are writing to. Most of the time this will be all zeros and we just overwrite it with our data
    int indbuff[256];
    int double_indbuff[256];
    char* cp = buf;

    int write_block = -1; // block number we are writing to
    
    while(nbytes > 0)
    {

        if(curr_block < 12)
        { 
            if(ip->i_block[curr_block] == 0)
            {
                ip->i_block[curr_block] = create_and_zero_block(target_dev);                
            }
            write_block = ip->i_block[curr_block];
        } 
        else if(curr_block < 268)
        {
            if(ip->i_block[12] == 0)
            {
                ip->i_block[12] = create_and_zero_block(target_dev);
            }
            get_block(target_dev, ip->i_block[12], indbuff);
            if(indbuff[curr_block - 12] == 0)
            {
                indbuff[curr_block - 12] = create_and_zero_block(target_dev);
                put_block(target_dev, ip->i_block[12], indbuff);
            }
            write_block = indbuff[curr_block - 12];
        } else {
            if(ip->i_block[13] == 0)
            {
                ip->i_block[13] = create_and_zero_block(target_dev);
            }
            get_block(target_dev, ip->i_block[13], indbuff);
            
            
            int lbk = curr_block - 12 - 256;
            if(indbuff[lbk >> 8] == 0)
            {
                indbuff[lbk >> 8] = create_and_zero_block(target_dev);
                put_block(target_dev, ip->i_block[13], indbuff);
            }
            get_block(target_dev, indbuff[lbk >> 8], double_indbuff);
            
            if(double_indbuff[lbk % 256] == 0)  // (curr_block - 12 - 256) / 256 => double indirect block #
            {
                double_indbuff[lbk % 256] = create_and_zero_block(target_dev); 
                put_block(target_dev, indbuff[lbk >> 8], double_indbuff);
            }
            write_block = double_indbuff[lbk % 256]; // (curr_block - 12 - 256) % 256 => which entry in double indirect block (aka where we want to wrute data to)
        }


        int bytes_to_write = BLKSIZE - start_byte;
        if(nbytes < bytes_to_write) // we want to fetch the data in the data, overwrite nbytes of data, and put that back
        {                           // otherwise we just write BLKSIZE bytes from our input buffer
            bytes_to_write = nbytes;
        } 
        get_block(target_dev, write_block, block_buffer);
        strncpy(block_buffer + start_byte, cp, bytes_to_write);
        put_block(target_dev, write_block, block_buffer);

        cp += bytes_to_write;
        nbytes -= bytes_to_write;
        start_byte = 0; // now we have written up to start of the next block - in which case we can write from block start of next block. Or, we have not written to the block end, in which case nbytes is 0 and we will stop writing
        running->fd[fd]->offset += bytes_to_write;
        if(ip->i_size < running->fd[fd]->offset) { // since write can increase the file size by writing past the offset, we need to reflect that
            ip->i_size = running->fd[fd]->offset;
        }
        count += bytes_to_write;
        ++curr_block;
    }
    running->fd[fd]->inodeptr->modified = 1; // mark for iput
    return count;
}