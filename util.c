/*********** globals in main.c ***********/
extern PROC   proc[NPROC];
extern PROC   *running;

extern MINODE minode[NMINODE];   // minodes
extern MINODE *freeList;         // free minodes list
extern MINODE *cacheList;        // cacheCount minodes list

extern MINODE *root;

extern OFT    oft[NOFT];

extern char gline[256];   // global line hold token strings of pathname
extern char *name[64];    // token string pointers
extern int  n;            // number of token strings                    

extern int ninodes, nblocks;
extern int bmap, imap, inodes_start, iblk;  // bitmap, inodes block numbers

extern int  fd, dev;
//extern char cmd[16], pathname[128], parameter[128];
extern int  requests, hits;
extern int inode_size, INODEsize, inodes_per_block, ifactor; // for improved mailmains algorithm across ubuntu editions.

/**************** util.c file **************/

int get_block(int dev, int blk, char buf[ ])
{
  lseek(dev, blk*BLKSIZE, SEEK_SET);
  int n = read(fd, buf, BLKSIZE);
  return n;
}

int put_block(int dev, int blk, char buf[ ])
{
  lseek(dev, blk*BLKSIZE, SEEK_SET);
  int n = write(fd, buf, BLKSIZE);
  return n; 
}       

int tokenize(char *pathname)
{
    int size_path = strlen(pathname); 
    char* pathname_copy = malloc((size_path + 1) * sizeof(char));
    strncpy(pathname_copy, pathname, size_path);
    pathname_copy[size_path] = '\0';
    char* s = strtok(pathname_copy, "/");
    int idx = 0;
    while(s != NULL) 
    {
      name[idx++] = s;
      s = strtok(NULL, "/");
    }
    return idx;
}

void get_inode(INODE* ip, int ino) 
{
   char buff[BLKSIZE];
   int blk = (ino - 1) / inodes_per_block + inodes_start;
   int offset = (ino - 1) % inodes_per_block;

   get_block(dev, blk, buff); // this is the block or the "street" containing the house we want to visit
   *ip = *((INODE*)buff + offset*ifactor); // by combining with the offset we obtain the "house" we are to visit
}

MINODE *iget(int dev, int ino) // return minode pointer of (dev, ino)
{
  MINODE* pCache = cacheList;
  int currentSmallest = INT8_MAX; // used if not found in cache and freelist is empty
  ++requests;

  // search cacheList for matching inode
  while(pCache != NULL) 
  {
    if(pCache->dev == dev && pCache->ino == ino) 
    {
      pCache->shareCount++;
      pCache->cacheCount++;
      ++hits;
      return pCache; // minode was present in cache
    }
    if(pCache->cacheCount < currentSmallest && pCache->shareCount == 0) {
      currentSmallest = pCache->cacheCount;
    }
    pCache = pCache->next;
  }
  // if freelist isn't empty then fetch an inode from there and insert it into the cachelist
  if(freeList != NULL) 
  {
   MINODE* mip = freeList;
   freeList = freeList->next;
   mip->cacheCount = mip->shareCount = 1;
   mip->modified = 0;
   mip->dev = dev;
   mip->ino = ino;
   mip->next = NULL;

   get_inode(&mip->INODE, ino);
   
   if(cacheList == NULL) 
   {
      cacheList = mip;
      return mip;
   }

   pCache = cacheList;
   while(pCache->next != NULL) { pCache = pCache->next; }
   pCache->next = mip;
   return mip;
  } else // no free inodes - remove inode with lowest cacheCount from cacheList and insert new inode
  {
   pCache = cacheList;
   while(pCache->cacheCount != currentSmallest) { pCache = pCache->next; } // will cause a segfault if every minode in the cacheList has a shareCount > 0 (Dr.Wang said that this is ok during lecture)
   pCache->cacheCount = 1;
   pCache->shareCount = 1;
   pCache->modified = 0;

   get_inode(&pCache->INODE, ino);
   return pCache;
   
  }
}
int iput(MINODE *mip)  // release a mip
{
  if(mip == 0) 
  {
    return -1;
  }
  mip->shareCount--;

  if(mip->shareCount > 0 || !(mip->modified)) // no need to do extra work if there are still users for this minode or it has not been modified
  {
    return 0;
  }
  // no more users of this minode or it has been modified
   char buff[BLKSIZE];
   int adjusted_ino = mip->ino - 1;
   int blk = adjusted_ino / inodes_per_block + inodes_start;
   int offset = adjusted_ino % inodes_per_block;

   INODE* ip;

   get_block(mip->dev, blk, buff); 
   ip = ((INODE*)buff + offset*ifactor); 
   *ip = mip->INODE; // replace INODE contents from disk with mip INODE contents

   put_block(mip->dev, blk, buff);
   remove_from_cachelist(mip);
} 

// look for a matching entry in mip with dp->name == name 
int search(MINODE *mip, char *name)
{
  if(mip == NULL || name == NULL) 
  {
    return 0;
  }
  int target_name_len = strlen(name);
  INODE ip = mip->INODE;
  DIR *dp;
  char sbuf[BLKSIZE];
  char temp[256];
  get_block(mip->dev, ip.i_block[0], sbuf);
  char* cp = sbuf;
  dp = (DIR*)sbuf;
  puts("i_number rec_len name_len\tname");
  while(cp < sbuf + BLKSIZE) 
  {
     printf("%d\t%d\t%d\t\t%s\n", dp->inode, dp->rec_len, dp->name_len, dp->name);
     if(dp->name_len == target_name_len && (strncmp(name, dp->name, target_name_len) == 0)) {
        printf("found %s: ino = %d\n", name, dp->inode);
        return dp->inode;
     }
     if(dp->rec_len == 0) {
        return 0;
     }
     cp += dp->rec_len;
     dp = (DIR*)cp;
  }
  return 0;
}

// print out a directory
int show_dir(MINODE* mip) 
{
   char sbuf[BLKSIZE], temp[256];
   DIR *dp;
   char *cp;
   INODE ip = mip->INODE;
   get_block(mip->dev, ip.i_block[0], sbuf);

   dp = (DIR *)sbuf;
   cp = sbuf;
   puts("i_number rec_len name_len name");
   while(cp < sbuf + BLKSIZE)
   {
       strncpy(temp, dp->name, dp->name_len);
       temp[dp->name_len] = 0;
      
       printf("%4d\t%4d\t%4d\t%s\n", dp->inode, dp->rec_len, dp->name_len, temp);
       cp += dp->rec_len;
       if(dp->rec_len == 0) {
          return 0;
       }
       dp = (DIR *)cp;
   }
   puts("*************************************************");
}

// convert a pathname string to a minode
MINODE *path2inode(char *filename) 
{
   if(filename == NULL || !strcmp(filename, ".")) {
      running->cwd->shareCount++;
      return running->cwd;
   } else if(!strcmp(filename, "..")) {
      int current_inode = 0;
      int parent_inode = findino(running->cwd, &current_inode);
      return iget(running->cwd->dev, parent_inode);
   }

   MINODE* mip;
   if(filename[0] != '/') {
     mip = running->cwd;
   } else {
     mip = root;
   }
   
   int num_tokens = tokenize(filename);
   mip->shareCount++;
   int ino;
   for (int i = 0; i < num_tokens; ++i)
   {
       if ((mip->INODE.i_mode & 0xF000) != 0x4000)
           return 0; // inode is not a dir
       ino = search(mip, name[i]);
       if (ino == 0) {
           printf("Couldn't find %s\n", name[i]);
           iput(mip);
           return 0;
       }

      iput(mip);
      mip = iget(dev, ino);
   }
   return mip;
}   

// searches for myino in pip, once found returns ino of found entry and copies the name into myname
int findmyname(MINODE *pip, int myino, char myname[ ]) 
{
  INODE ip = pip->INODE;
  DIR *dp;
  char sbuf[BLKSIZE];
  get_block(pip->dev, ip.i_block[0], sbuf);
  char* cp = sbuf;
  dp = (DIR*)sbuf;
  while(cp < sbuf + BLKSIZE) 
  {
     if(dp->inode == myino) 
     {
        strncpy(myname, dp->name, dp->name_len);
        myname[dp->name_len] = '\0';
        return dp->inode;
     }
     if(dp->rec_len == 0) {
        return 0;
     }
     cp += dp->rec_len;
     dp = (DIR*)cp;
  }
  return 0;
}
 
// passes mip's inode into myino and returns parents inode
int findino(MINODE *mip, int *myino) 
{
  show_dir(mip);
  INODE ip = mip->INODE;
  DIR *dp;
  char sbuf[BLKSIZE];
  get_block(mip->dev, ip.i_block[0], sbuf);
  char* cp = sbuf;
  dp = (DIR*)sbuf;
  while(cp < sbuf + BLKSIZE) 
  {
     if(!strcmp(dp->name, ".")) {
         *myino = dp->inode;
     } else if(!strcmp(dp->name, "..")) {
         return dp->inode;
     }
     if(dp->rec_len == 0) {
        return -1;
     }
     cp += dp->rec_len;
     dp = (DIR*)cp;
  }
  return 0;
}

// print out all the blocks of ip. Leftover code from previous lab
void output_inode_blocks(INODE* ip) 
{
   puts("------- DIRECT BLOCKS -------");
   int blk_number = 0;
   while(blk_number < 12 && ip->i_block[blk_number] != 0) 
   {
      printf("%d ", ip->i_block[blk_number]);
      ++blk_number;
   }
   if(ip->i_block[12] != 0) 
   {
      puts("\n------- INDIRECT BLOCKS -------");
      int indbuff[BLKSIZE];
      get_block(dev,ip->i_block[12],indbuff);

      for(int i = 0; i < 256; ++i) {
         if(indbuff[i] != 0) {
            printf("%d ", indbuff[i]);
         }
      }
      puts("");
   }
   if(ip->i_block[13] != 0) 
   {
      puts("\n------- DOUBLE INDIRECT BLOCKS -------");
      int indbuff[BLKSIZE];
      int double_indbuff[BLKSIZE];
      get_block(dev,ip->i_block[13],indbuff);
      for(int i = 0; i < 256; ++i) {
         if(indbuff[i] != 0) {
            get_block(dev, indbuff[i], double_indbuff);
            for(int j = 0; j < 256; ++j) {
               if(double_indbuff[j] != 0) {
                  printf("%d ", double_indbuff[j]);
               }
            }
         }
      }
      puts("");
   }
}

// remove mip from cachelist
void remove_from_cachelist(MINODE* mip) 
{
   if(cacheList == NULL)
      return;
   if(cacheList == mip) {
      cacheList = cacheList->next;
      return;
   }

   MINODE* pPrev = cacheList;
   MINODE* pCache = cacheList->next;
   MINODE* pNext = NULL;
   while(pCache != NULL) {
      pNext = pCache->next;
      if(pCache->id == mip->id) {
         pPrev = pNext;
      }
      pPrev = pCache;
      pCache = pCache->next;
   }
   if(pCache == NULL) {
      return;
   }
   pCache->next = NULL;
   insert_into_freelist(pCache); 

}

void insert_into_freelist(MINODE* mip) 
{
   if(freeList == NULL) {
      freeList = mip;
      mip->cacheCount = 0;
      mip->shareCount = 0;
      mip->modified = 0;
      return;
   }
   MINODE* pFree = freeList;
   while(pFree->next != NULL) { pFree = pFree->next; }
   pFree->next = mip;
}
// recursive function which outputs cwd starting from passed in minode
void rpwd(MINODE* mip) 
{
   int currIno = 0;
   int parentIno = findino(mip, &currIno);
   MINODE* pip = iget(mip->dev, parentIno);
   char buff[256];
   findmyname(pip, currIno, buff);
   if(currIno != parentIno) {
      rpwd(pip);
      iput(pip);
      printf("/%s", buff);
   }
}

// deallocates data blocks of mip
void truncate(MINODE* mip) 
{
   INODE* ip = &mip->INODE;
   int blk_number = 0;
   // deallocate direct blocks
   while(blk_number < 12 && ip->i_block[blk_number] != 0) 
   {
      bdalloc(dev, ip->i_block[blk_number]);
      ++blk_number;
   }
   // deallocate indirect blocks
   if(ip->i_block[12] != 0) 
   {
      int indbuff[256];
      get_block(dev,ip->i_block[12],indbuff);

      for(int i = 0; i < 256; ++i) {
         if(indbuff[i] != 0) {
            bdalloc(dev, indbuff[i]);
         }
      }
   }
   // deallocate doubly indirect blocks
   if(ip->i_block[13] != 0) 
   {
      int indbuff[256];
      int double_indbuff[256*256];
      get_block(dev,ip->i_block[13],indbuff);
      for(int i = 0; i < 256; ++i) {
         if(indbuff[i] != 0) {
            get_block(dev, indbuff[i], double_indbuff);
            for(int j = 0; j < 256; ++j) {
               if(double_indbuff[j] != 0) {
                  bdalloc(dev, double_indbuff[j]);
               }
            }
         }
      }
   }
   ip->i_size = 0;
}

/*
************************************************
*  Cache Sorting and printing functions        *
************************************************
*/
MINODE* merge(MINODE* pLeft,MINODE* pRight) {
    if (pLeft == NULL)
        return pRight;
    if (pRight == NULL)
        return pLeft;

   MINODE* result = NULL;

    if (pLeft->cacheCount <= pRight->cacheCount) {
        result = pLeft;
        result->next = merge(pLeft->next, pRight);
    }
    else {
        result = pRight;
        result->next = merge(pLeft, pRight->next);
    }

    return result;
}

void split_cache(MINODE* pCache, MINODE** pLeft, MINODE** pRight) {
    MINODE* pSlow = pCache;
    MINODE* pFast = pCache->next;

    while (pFast != NULL) 
    {
        pFast = pFast->next;
        if (pFast != NULL) 
        {
            pSlow = pSlow->next;
            pFast = pFast->next;
        }
    }

    *pLeft = pCache;
    *pRight = pSlow->next;
    pSlow->next = NULL; // break the sublist in half
}

void mergeSort(MINODE** pHead) {
   MINODE* pCache = *pHead;
   MINODE* pLeft;
   MINODE* pRight;

    if (pCache == NULL || pCache->next == NULL) // no need to sort if cache has less than 2 mips
        return;

    split_cache(pCache, &pLeft, &pRight);

    mergeSort(&pLeft);
    mergeSort(&pRight);

    *pHead = merge(pLeft, pRight);
}

void sort_cache() 
{
   mergeSort(&cacheList);
}

void printCache() {
  if(cacheList == NULL) {
    printf("[]\n");
    return;
  }
  
  sort_cache();
  MINODE* pCache = cacheList;
  while(pCache != NULL) {
    printf("%dc[%d %d]%ds -> ", pCache->cacheCount, pCache->dev, pCache->ino, pCache->shareCount);
    pCache = pCache->next;
  }
  printf("NULL\n");
}

