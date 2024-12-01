#include "type.h"

/********** globals **************/
PROC   proc[NPROC];
PROC   *running;

MINODE minode[NMINODE];   // in memory INODES
MINODE *freeList;         // free minodes list
MINODE *cacheList;        // cached minodes list

MINODE *root;             // root minode pointer

OFT    oft[NOFT];         // for level-2 only
SUPER* sp;                // superblock
GD* gp;

char gline[256];          // global line hold token strings of pathname
char *name[64];           // token string pointers
int  n;                   // number of token strings                    

int ninodes, nblocks;     // ninodes, nblocks from SUPER block
int bmap, imap, inodes_start, iblk;  // bitmap, inodes block numbers

int inode_size, INODEsize, inodes_per_block, ifactor; // for improved mailmains algorithm across ubuntu editions.

int  fd, dev;
char cmd[16], pathname[128], parameter[128];
int  requests, hits;

// start up files
#include "util.c"
#include "alloc_dealloc.c"
#include "mkdir_create.c"
#include "rmdir.c"
#include "symlink.c"
#include "link_unlink.c"
#include "chmod_stat.c"
#include "open_close.c"
#include "read.c"
#include "write.c"
#include "cat_cp.c"
#include "head_tail.c"
#include "cd_ls_pwd.c"



int init()
{
  int i, j;
  // initialize minodes into a freeList
  for (i=0; i<NMINODE; i++){
    MINODE *mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->id = i;
    mip->next = &minode[i+1];
  }
  minode[NMINODE-1].next = 0;
  freeList = &minode[0];       // free minodes list

  cacheList = 0;               // cacheList = 0

  for (i=0; i<NOFT; i++)
    oft[i].shareCount = 0;     // all oft are FREE
 
  for (i=0; i<NPROC; i++){     // initialize procs
     PROC *p = &proc[i];    
     p->uid = p->gid = i;      // uid=0 for SUPER user
     p->pid = i+1;             // pid = 1,2,..., NPROC-1

     for (j=0; j<NFD; j++)
       p->fd[j] = 0;           // open file descritors are 0
  }
  
  running = &proc[0];          // P1 is running
  requests = hits = 0;         // for hit_ratio of minodes cache
}

char *disk = "disk2";

int main(int argc, char *argv[ ]) 
{
  char line[128];
  char buf[BLKSIZE];

  init();
  
  fd = dev = open(disk, O_RDWR);
  printf("dev = %d\n", dev);  // YOU should check dev value: exit if < 0
  if(dev < 0) {
    puts("Unable to open disk");
    exit(1);
  }
  // get super block of dev
  get_block(dev, 1, buf);
  sp = (SUPER *)buf; 
  if(sp->s_magic != 0xEF53) {
     printf("%s is not a valid extfs\n", dev);
     exit(1);
  } else {
    printf("check superblock magic: ok\n");
  }
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;
  inode_size = sp->s_inode_size;
  INODEsize = sizeof(INODE);
  inodes_per_block = BLKSIZE / inode_size;
  ifactor = inode_size / INODEsize;
  printf("ninodes=%d  nblocks=%d\n", ninodes, nblocks);

  get_block(dev, 2, buf);
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  iblk = inodes_start = gp->bg_inode_table;

  printf("bmap=%d  imap=%d  iblk=%d\n", bmap, imap, iblk);
                   
  printf("set P1's CWD to root\n");
  root = iget(dev, 2);
  running->cwd = iget(dev, 2);
  printf("Root shareCount=%d\n", root->shareCount);
  while(1){
     printf("P%d running\n", running->pid);
     bzero(pathname, 128);
     bzero(parameter, 128);
      
     printf("enter command [cd|ls|pwd|mkdir|create|rmdir|link|unlink|symlink|chmod|open|close|read|write|cat|cp|head|tail] [exit|show|hits|pfd] : ");
     fgets(line, 128, stdin);
     line[strlen(line)-1] = 0;    // kill \n at end

     if (line[0]==0)
        continue;
      
     sscanf(line, "%s %s %64c", cmd, pathname, parameter);
     printf("pathname=%s parameter=%s\n", pathname, parameter);
      
     if (strncmp(cmd, "ls",2)==0)
	      ls(pathname);
     if (strcmp(cmd, "cd")==0)
	      cd(pathname);
     if (strncmp(cmd, "pwd", 3)==0)
        pwd();
     if (strncmp(cmd, "mkdir", 5) == 0)
         make_dir(pathname);
     if (strncmp(cmd, "create", 6) == 0)
         create_file(pathname);
     if (strncmp(cmd, "rmdir", 5) == 0)
         rmdir(pathname);
     if(strncmp(cmd, "link", 4) == 0)
          create_link(pathname, parameter);
     if(strncmp(cmd, "unlink", 6) ==0) 
          destroy_link(pathname);
     if(strncmp(cmd, "symlink", 7) == 0)
          symlink(pathname, parameter);
     if(strcmp(cmd, "chmod") == 0)
          do_chmod(pathname, parameter);
     if(strcmp(cmd, "open") == 0) {
          open_file(pathname, atoi(parameter));
     }
     if(strcmp(cmd, "close") == 0)
          close_file(atoi(pathname));
     if(strcmp(cmd, "pfd") == 0)
          pfd();
     if(strcmp(cmd, "read") == 0) {
          int target_fd = atoi(pathname);
          int nbytes = atoi(parameter);
          if(target_fd < 0) {
            printf("Error - %d is not a valid fd\n", target_fd);
          } else if(nbytes < 1) {
            printf("Error - %d is not a valid number of bytes\n", nbytes);
          } else {
            read_file(target_fd, nbytes);
          }
     }
     if(strcmp(cmd, "write") == 0) {
          int target_fd = atoi(pathname);
          if(target_fd < 0) {
            printf("Error - %d is not a valid fd to write to\n", target_fd);
          } else {
            write_file(target_fd, parameter);
          }
     }
     if(strcmp(cmd, "cat") == 0) {
       do_cat(pathname);
     }
     if(strcmp(cmd, "cp") == 0) {
        do_cp(pathname, parameter);
     }
     if (strcmp(cmd, "head") == 0) {
         myHead(pathname);
     }
     if (strcmp(cmd, "tail") == 0) {
         myTail(pathname);
     }
     if (strncmp(cmd, "show", 4)==0)
	     show_dir(running->cwd);
     if (strncmp(cmd, "hits", 4)==0)
	     hit_ratio();
     if (strncmp(cmd, "exit", 4)==0)
	     quit();
  }
}


int hit_ratio()
{
  printCache();
  double ratio = (double)hits / requests;
  printf("Requests: %d Hits: %d Ratio: %0.2lf\n", requests, hits,(ratio * 100));
}

int quit()
{
   MINODE *mip = cacheList;
   while(mip != NULL){
     if (mip->shareCount){
        mip->shareCount = 1;
        iput(mip);    // write INODE back if modified
     }
     mip = mip->next;
   }
   exit(0);
}







