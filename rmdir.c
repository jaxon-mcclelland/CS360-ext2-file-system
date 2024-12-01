int rmdir(char* pathname)
{
	MINODE* mip = path2inode(pathname);
	if (mip)
	{
		if (mip == running->cwd || mip == running->cwd->dev)
		{
			// trying to remove / . or ..
			printf("Error: Cannot remove this directory, permission denied");
			iput(mip);
			return -1;
		}

		if (!S_ISDIR(mip->INODE.i_mode)) // verrify  it's a dir
		{
			printf("ERROR: this is not a directory\n");
			iput(mip);
			return -1;
		}
		if (!is_empty(mip)) // verrify it's empty
		{
			printf("ERROR: Directory is not empty\n");
			iput(mip);
			return -1;
		}
		if (mip->shareCount > 2) // verrify it's not being used
		{
			printf("ERROR: directory is busy, shareCount > 2, shareCount = %d\n", mip->shareCount);
			iput(mip);
			return -1;
		}

		for (int i = 0; i < 12; i++)
		{
			if (mip->INODE.i_block[i] == 0)
				continue;
			bdalloc(mip->dev, mip->INODE.i_block[i]);
		}
		idalloc(mip->dev, mip->ino);
		mip->modified = 1;
		iput(mip);

		char pathcpy1[256], pathcpy2[256]; // for basename and dirname functions which are destructive.
		strcpy(pathcpy1, pathname);
		strcpy(pathcpy2, pathname);

		// library functions to separate /x/y/z into /x/y and /z
		char* parent = dirname(pathcpy1);
		char* child = basename(pathcpy2);

		MINODE* pip = path2inode(parent);

		rm_child(pip, child);

		pip->INODE.i_links_count--;
		pip->INODE.i_atime = time(0L);
		pip->INODE.i_ctime = time(0L);
		pip->modified = 1;

		iput(pip);
	}

	return 0;
}

int is_empty(MINODE* mip)
{
	char buf[BLKSIZE], * cp, temp[256];
	DIR* dp;
	INODE* ip = &mip->INODE;

	if (ip->i_links_count > 2) // make sure there aren't any dirs inside -- links count only looks at dirs, still could have files
	{
		return 0;
	}
	else if (ip->i_links_count == 2) //check files
	{
		for (int i = 0; i < 12; i++) // Search DIR, assume directl blocks only.
		{
			if (ip->i_block[i] == 0)
				break;
			get_block(mip->dev, mip->INODE.i_block[i], buf); // read the blocks
			dp = (DIR*)buf;
			cp = buf;

			while (cp < buf + BLKSIZE) // while not at the end of the block
			{
				strncpy(temp, dp->name, dp->name_len);
				temp[dp->name_len] = 0;
				printf("%8d%8d%8u %s\n", dp->inode, dp->rec_len, dp->name_len, temp); // print the name of the files
				if (strcmp(temp, ".") && strcmp(temp, ".."))                          // if neither match, then this is a file
				{
					return 0; // there is a file, not empty.
				}
				cp += dp->rec_len; // go to next entry in block
				dp = (DIR*)cp;
			}
		}
	}
	return 1; // is empty
}


int rm_child(MINODE* parent, char* name)
{
	DIR* dp, * prevdp;
	char* cp, buf[BLKSIZE], temp[256];
	INODE* ip = &parent->INODE;
	for (int i = 0; i < 12; i++) // loop through all 12 blocks of memory
	{
		if (ip->i_block[i] != 0)
		{
			get_block(parent->dev, ip->i_block[i], buf); // get block from file
			dp = (DIR*)buf;
			cp = buf;

			while (cp < buf + BLKSIZE) // while not at the end of the block
			{
				strncpy(temp, dp->name, dp->name_len); // copy name
				temp[dp->name_len] = 0;                // add name delimiter

				if (!strcmp(temp, name)) // name found, we can remove the child.
				{
					if (prevdp)
					{
						prevdp->rec_len += dp->rec_len; // prior entry consumes allocated length
					}
					if (cp == buf && cp + dp->rec_len == buf + BLKSIZE && i != 0) // first/only record, can't be i_block[0]
					{
						// using option 2 to keep things clean. option 1 just set ino=0 and neam_len = 0
						bdalloc(parent->dev, ip->i_block[i]);
						ip->i_size -= BLKSIZE;

						while (ip->i_block[i + 1] != 0 && i + 1 < 12) // filling hole in the i_blocks since we deallocated this one
						{
							i++;
							get_block(parent->dev, ip->i_block[i], buf);
							put_block(parent->dev, ip->i_block[i - 1], buf);
						}
					}
					else
					{
						// using option 1, making dp invisible.
						dp->name_len = 0;
						dp->inode = 0;
						put_block(parent->dev, ip->i_block[i], buf);
					}
					parent->modified = 1;
					parent->shareCount++;
					iput(parent);
					return 0;
				}
				prevdp = dp;
				cp += dp->rec_len;
				dp = (DIR*)cp;
			}
		}
	}
	printf("ERROR: directory not found\n");
	return -1;
}