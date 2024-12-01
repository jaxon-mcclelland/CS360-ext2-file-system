int do_chmod(char* pathname, char* perm) 
{
    if(pathname[0] == 0 || perm[0] == 0) {
        puts("chmod mode filename");
        return -1;
    }
    MINODE* mip = path2inode(pathname);
    if(mip == NULL) {
        printf("could not open %s\n", pathname);
        return -1;
    }
    int v = 0;
    char* cp = perm;
    while(*cp) {
        v = 8*v + *cp - '0';
        ++cp;
    }
    v = v & 0x01FF; // keep only last 9 bits
    printf("mode=%s v=%d\n", perm, v);
    mip->INODE.i_mode &= 0xFE00; // clear last 9 bits
    mip->INODE.i_mode |= v;
    mip->modified = 1;
    iput(mip);
    return 0;
}

