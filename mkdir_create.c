extern int  dev;
extern SUPER* sp;
extern GD* gp;
extern int ninodes;


int incFreeInodes(int dev)
{
	char buf[BLKSIZE];

	// inc free inodes count in SUPER and GD
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_inodes_count++;
	put_block(dev, 1, buf);

	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_inodes_count++;
	put_block(dev, 2, buf);
}

int incFreeBlocks(int dev)
{
	char buf[BLKSIZE];

	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_blocks_count++;
	put_block(dev, 1, buf);

	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_blocks_count++;
	put_block(dev, 2, buf);
}

int decFreeInodes(int dev)
{
	char buf[BLKSIZE];

	// dec free inodes count by 1 in SUPER and GD
	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_inodes_count--;
	put_block(dev, 1, buf);

	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_inodes_count--;
	put_block(dev, 2, buf);
}

int decFreeBlocks(int dev)
{
	char buf[BLKSIZE];

	get_block(dev, 1, buf);
	sp = (SUPER*)buf;
	sp->s_free_blocks_count--;
	put_block(dev, 1, buf);

	get_block(dev, 2, buf);
	gp = (GD*)buf;
	gp->bg_free_blocks_count--;
	put_block(dev, 2, buf);
}



int make_dir(char* pathname)
{
	MINODE* temp;
	char pathcpy1[256], pathcpy2[256]; // for basename and dirname functions which are destructive.
	strcpy(pathcpy1, pathname);
	strcpy(pathcpy2, pathname);

	// library functions to separate /x/y/z into /x/y and /z
	char* parent = dirname(pathcpy1); 
	char* child = basename(pathcpy2);

	MINODE* pip = path2inode(parent);
	if (pip)
	{

		if (!S_ISDIR(pip->INODE.i_mode)) // verrfies parent is a dir
		{
			printf("ERROR: %s is not a directory\n", parent);
			iput(pip);
			return -1;
		}

		if (search(pip, child)) // verrifes the child name doesn't already exist
		{
			printf("ERROR: a directory or file named: %s already exists inside the directory: %s", child, parent);
			iput(pip);
			return -1;
		}

		// now we know where to make the directory, so we are ready to build it
		mymkdir(pip, child);
		pip->INODE.i_links_count++;    // increment link count
		pip->INODE.i_atime = time(0L); // update time last modified
		pip->modified = 1;             // mark as changed
		iput(pip);                     //writes the changed MINODE to the block
	}
	return 0;

}


int mymkdir(MINODE* pip, char* name)
{
	char* buf[BLKSIZE], * cp;
	DIR* dp;

	int ino = ialloc(dev);
	int bno = balloc(dev);
	printf("ino: %d, bno: %d", ino, bno);


	MINODE* mip = iget(dev, ino);
	INODE* ip = &mip->INODE;

	ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
	ip->i_uid = running->uid;	// Owner uid 
	ip->i_gid = running->gid;	// Group Id
	ip->i_size = BLKSIZE;		// Size in bytes 
	ip->i_links_count = 2;	        // Links count=2 because of . and ..
	ip->i_atime = time(0L);
	ip->i_ctime = time(0L);
	ip->i_mtime = time(0L);  // set to current time
	ip->i_blocks = 2;                	// LINUX: Blocks count in 512-byte chunks 
	ip->i_block[0] = bno;             // new DIR has one data block   

	for (int i = 1; i < 15; i++) // clears all the block memeory
	{
		ip->i_block[i] = 0;
	}

	mip->modified = 1;            // mark minode MODIFIED
	iput(mip);                    // write INODE to disk

	get_block(dev, bno, buf); // get the newly allocated block
	dp = (DIR*)buf;
	cp = buf;

	printf("Create . and .. in %s\n", name);
	// making . entry
	dp->inode = ino;   // child ino
	dp->rec_len = 12;  // 4 * [(8 + name_len + 3) / 4], in the case of name ., its always 12. 
	dp->name_len = 1;  // len of name
	dp->name[0] = '.'; // name

	cp += dp->rec_len; // advancing to end of '.' entry
	dp = (DIR*)cp;

	//making .. entry
	dp->inode = pip->ino;       // setting to parent ino
	dp->rec_len = BLKSIZE - 12; // size is rest of block
	dp->name_len = 2;           // size of the name
	dp->name[0] = '.';
	dp->name[1] = '.';

	put_block(dev, bno, buf); // write the block

	enter_child(pip, ino, name); // add the name to the block

	return 0;
}

int enter_child(MINODE* pip, int myino, char* myname)
{
	char buf[BLKSIZE], * cp;
	int bno;
	INODE* ip;
	DIR* dp;

	int neededLen = 4 * ((8 + strlen(myname) + 3) / 4);
	ip = &pip->INODE; // get the inode
	
	for (int i = 0; i < 12; i++) // assume no indirect blocks
	{

		if (ip->i_block[i] == 0)
		{
			break;
		}

		bno = ip->i_block[i];
		get_block(pip->dev, ip->i_block[i], buf); // get the block
		dp = (DIR*)buf;
		cp = buf;

		int ideal_len = 4 * ((8 + dp->name_len + 3) / 4); // ideal len of the name

		while (cp + dp->rec_len < buf + BLKSIZE && dp->rec_len < neededLen + ideal_len) // find first available space.
		{
			printf("%s\n", dp->name);
			cp += dp->rec_len;
			dp = (DIR*)cp;
			ideal_len = 4 * ((8 + dp->name_len + 3) / 4);
		}

		int remainder = dp->rec_len - ideal_len;          // remaining space

		// make sure we found a large enough space and not just the end of the block.
		if (remainder >= neededLen)
		{                            // space available for new netry
			dp->rec_len = ideal_len; //trim current entry to ideal len
			cp += dp->rec_len;       // advance to end
			dp = (DIR*)cp;          // point to new open entry space

			dp->inode = myino;             // add the inode
			strcpy(dp->name, myname);      // add the name
			dp->name_len = strlen(myname); // len of name
			dp->rec_len = remainder;       // size of the record

			put_block(dev, bno, buf); // save block
			return 0;
		}
		// trouble if there is not enough space
		else
		{
			ip->i_size = BLKSIZE; // size is new block
			bno = balloc(dev);    // allocate new block
			ip->i_block[i] = bno; // add the block to the list
			pip->modified = 1;       // ino is changed so make dirty

			get_block(dev, bno, buf); // get the blcok from memory
			dp = (DIR*)buf;
			cp = buf;

			dp->name_len = strlen(myname); // add name len
			strcpy(dp->name, myname);      // name
			dp->inode = myino;             // inode
			dp->rec_len = BLKSIZE;         // only entry so full size

			put_block(dev, bno, buf); //save
			return 1;
		}
	}	
}

int create_file(char* pathname)
{
	MINODE* temp;
	char pathcpy1[256], pathcpy2[256]; // for basename and dirname functions which are destructive.
	strcpy(pathcpy1, pathname);
	strcpy(pathcpy2, pathname);

	// library functions to separate /x/y/z into /x/y and /z
	char* parent = dirname(pathcpy1);
	char* child = basename(pathcpy2);

	MINODE* pip = path2inode(parent);

	if (pip)
	{
		if (!S_ISDIR(pip->INODE.i_mode)) // verrfies parent is a dir
		{
			printf("ERROR: %s is not a directory\n", parent);
			iput(pip);
			return -1;
		}

		if (search(pip, child)) // verrifes the child name doesn't already exist
		{
			printf("ERROR: a directory or file named: %s already exists inside the directory: %s", child, parent);
			iput(pip);
			return -1;
		}

		// now we know where to make the directory, so we are ready to build it
		my_create(pip, child);
		pip->INODE.i_atime = time(0L); // update time last modified
		pip->modified = 1;             // mark as changed
		iput(pip);                     //writes the changed MINODE to the block
	}
	return 0;
}

int my_create(MINODE* pip, char* name)
{
	char* buf[BLKSIZE], * cp;
	DIR* dp;

	int ino = ialloc(dev);

	MINODE* mip = iget(dev, ino);
	INODE* ip = &mip->INODE;

	ip->i_mode = 0x81A4;		// OR 0100644: file type and permissions
	ip->i_uid = running->uid;	// Owner uid 
	ip->i_gid = running->gid;	// Group Id
	ip->i_size = 0;		// Size in bytes 
	ip->i_links_count = 1;	        // Links count=1 since it is a file.
	ip->i_atime = time(0L);
	ip->i_ctime = time(0L);
	ip->i_mtime = time(0L);  // set to current time
	ip->i_blocks = 0;                	// LINUX: Blocks count in 512-byte chunks 
	ip->i_block[0] = 0;             // new file is given no blocks

	for (int i = 1; i < 15; i++) // clears all the block memeory
	{
		ip->i_block[i] = 0;
	}

	mip->modified = 1;            // mark minode MODIFIED
	iput(mip);                    // write INODE to disk

	enter_child(pip, ino, name); // add the name to the block

	return 0;
}

