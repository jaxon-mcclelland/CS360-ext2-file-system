
extern int dev;
extern PROC* running;



int open_file(char* pathname, int mode) 
{
    MINODE* mip = path2inode(pathname);
    if(mip == NULL && mode == 0) {
        printf("Cannot open %s - file not found\n", pathname);
        return -1;
    } else if(mip == NULL) { // file not found but mode is W|RW|APPEND
        int create_status = create_file(pathname);
        if(create_status < 0)
            return -1;
        mip = path2inode(pathname);
    }
    printf("opened file [%d, %d]\n", dev, mip->ino);
    if(!S_ISREG(mip->INODE.i_mode)) {
        printf("Error - cannot open %s - invalid file type\n", pathname);
        iput(mip);
        return -1;
    }
    // verify that the file is not already opened in an incompatible mode
    int oft_idx = 0;
    for(; oft_idx < NFD && running->fd[oft_idx] != NULL; ++oft_idx) {
        if(running->fd[oft_idx]->inodeptr->ino == mip->ino) {
            if(running->fd[oft_idx]->mode != 0 || (mode != 0)) {
                printf("Error - cannot open %s because an incompatible handle to this file is already open\n", pathname);
                return -1;
            }
        }
    }
    OFT* new_fd = malloc(sizeof(OFT));
    new_fd->inodeptr = mip;
    new_fd->mode = mode;
    new_fd->shareCount = 1;

    switch(mode) {
        case 2: // RW and R are the same
        case 0:
            new_fd->offset = 0;
            break;
        case 1:
            truncate(new_fd->inodeptr);
            new_fd->offset = 0;
            break;
        case 3:
            new_fd->offset = mip->INODE.i_size;
            break;
        default:
            printf("open - %d is not a valid mode\n", mode);
            return -1;
    }
    running->fd[oft_idx] = new_fd;
    
    new_fd->inodeptr->INODE.i_atime = time(0L);
    if(mode > 0)
        new_fd->inodeptr->INODE.i_mtime = time(0L);
    
    new_fd->inodeptr->modified = 1;
    iput(new_fd->inodeptr);
    return oft_idx;
}

int close_file(int fd)
{
    if(fd >= NFD || running->fd[fd] == NULL) {
        printf("Error - %d is not a valid file descriptior\n", fd);
        return -1;
    }
    OFT* oftp = running->fd[fd];
    running->fd[fd] = NULL;
    oftp->shareCount--;

    if(oftp->shareCount <= 0)
        iput(oftp->inodeptr); // last user of this file - dispose of inode
    return 0;
}

int do_lseek(int fd, int position) 
{
    if(running->fd[fd] == NULL) {
        printf("Error cannot lseek on %d - invalid fd\n", fd);
        return -1;
    }
    if(position < 0 || position > running->fd[fd]->inodeptr->INODE.i_size) {
        printf("Error - cannot lseek to %d - file overrun\n", position);
        return -1;
    }
    running->fd[fd]->offset = position;
    return position;
}

int pfd()
{
    puts(" fd    mode    offset  INODE");
    puts("----  ------   ------  ------");
    for(int i = 0; i < NFD; ++i)
    {
        if(running->fd[i] != NULL)
        {
            OFT* tst = running->fd[i];
            char* mode = NULL;
            switch(running->fd[i]->mode) {
                case 0:
                    mode = "READ ";
                    break;
                case 1:
                    mode = "WRITE";
                    break;
                case 2:
                    mode = "RW   ";
                    break;
                case 3:
                    mode = "APPEND";
                    break;
            }
            printf(" %d\t%s\t%d\t[%d,%d]\n", i, mode, running->fd[i]->offset, dev, running->fd[i]->inodeptr->ino);
        }
    }
    puts("-------------------------------");
}

// find first available fd and copy fd into it
int do_dup(int fd)
{
    if(running->fd[fd] == NULL) {
        printf("Cannot dup %d - not a valid fd\n", fd);
        return -1;
    }
    for(int i = 0; i < NFD; ++i) {
        if(running->fd[i] == NULL) {
            running->fd[i] = running->fd[fd];
            running->fd[i]->shareCount++;
            return 0;
        }
    }
    puts("Error no open file descriptors");
    return -1;
}

// duplicate fd into gd. close gd if it is currently open.
void do_dup2(int fd, int gd)
{
    if(running->fd[gd] != NULL)
        close_file(gd);
    running->fd[gd] = running->fd[fd];
    running->fd[gd]->shareCount++;
}