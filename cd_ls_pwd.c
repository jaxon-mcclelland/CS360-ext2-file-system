// cd_ls_pwd.c file
int cd(char* pathname)
{
  MINODE* mip = path2inode(pathname);
  if(mip == NULL) {
    printf("cd - %s is an invalid path\n", pathname);
    return -1;
  }
  if(S_ISLNK(mip->INODE.i_mode)) { // extra credit cd link logic
    char link_path[60];
    read_link(pathname, link_path);
    iput(mip);
    cd(link_path);
    return;
  } 
  else if(!S_ISDIR(mip->INODE.i_mode)) { 
    printf("Cannot cd into %s - not a dir\n", pathname);
    iput(mip);
    return -1;
  }
  iput(running->cwd);
  running->cwd = mip;
  return 1;
}

int ls_file(MINODE *mip, char *name)
{
    // permission character strings
    char* perm1 = "xwrxwrxwr-"; // full permissions
    char* perm2 = "----------"; // no permissions
  
    char ftime[256];
    unsigned int mode = mip->INODE.i_mode;

    // check file type to display first character
    if (S_ISREG(mode))
    {
        printf("%c", '-');
    }
    if (S_ISDIR(mode))
    {
        printf("%c", 'd');
    }
    if (S_ISLNK(mode))
    {
        printf("%c", 'l');
    }

    for (int i = 8; i >= 0; i--)
    {
        if (mode & (1 << i)) // bitwise check if 1 or 0 for permission
            printf("%c", perm1[i]); // print r|w|x printf("%c", t1[i]);
        else
            printf("%c", perm2[i]); // or print -
    }

    printf("%4d ", mip->INODE.i_links_count); // link count
    printf("%4d ", mip->INODE.i_gid);         // gid
    printf("%4d ", mip->INODE.i_uid);         // uid
    printf("%8d ", mip->INODE.i_size);       // file size
    unsigned long mtime = mip->INODE.i_mtime;
    strcpy(ftime, (char*)ctime((time_t*)(&mtime))); // this copies time as clandar time
    ftime[strlen(ftime) - 1] = 0;                // removes the \n created from the line above.
    printf("%s ", ftime);


    printf("%s", name); // finale ll is file/dir/link main name
    if (S_ISLNK(mode))
    {
        printf(" -> %s", (char*)mip->INODE.i_block); // print linked name 
    }

    printf("   [%d %d]", mip->dev, mip->ino);
    printf("\n"); // the new line we need, but it was on ctime, now its at the end.
    return 0;
}
  
int ls_dir(MINODE *pip)
{
  char sbuf[BLKSIZE], name[256];
  DIR  *dp;
  char *cp;
  MINODE* temp_mip;
  
  printf("simple ls_dir()\n");   

  get_block(dev, pip->INODE.i_block[0], sbuf);
  dp = (DIR *)sbuf;
  cp = sbuf;
   
  while (cp < sbuf + BLKSIZE){
    strncpy(name, dp->name, dp->name_len);
    name[dp->name_len] = 0;

    temp_mip = iget(dev, dp->inode);
    ls_file(temp_mip, name);
    iput(temp_mip);

    cp += dp->rec_len;
    dp = cp;
  }

  printf("\n");

}

int ls(char* pathname)
{
    int mode;
    printf("ls %s\n", pathname);

    MINODE* mip = path2inode(pathname);
    if(mip)
    {
        mode = mip->INODE.i_mode;
        if (S_ISDIR(mode))
        {
            ls_dir(mip);
        }
        else
        {
            ls_file(mip, basename(pathname));
        }
        iput(mip);
    }

    return 0;
    
}

int pwd()
{
  MINODE* wd = running->cwd;
  if(wd == root) {
    printf("/");
  } else {
    rpwd(wd);
  }
  puts("");
}



