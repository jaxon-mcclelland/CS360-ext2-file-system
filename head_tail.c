extern int dev;
extern PROC* running;

int myHead(char* pathname)
{
	MINODE* temp;
	char pathcpy1[256], pathcpy2[256]; // for basename and dirname functions which are destructive.
	strcpy(pathcpy1, pathname);
	strcpy(pathcpy2, pathname);

	// library functions to separate /x/y/z into /x/y and /z
	char* parent = dirname(pathcpy1);
	char* fileName = basename(pathcpy2);

	MINODE* pip = path2inode(parent);
	if (pip)
	{

		if (!S_ISDIR(pip->INODE.i_mode)) // verrfies parent is a dir
		{
			printf("ERROR: %s is not a directory\n", parent);
			iput(pip);
			return -1;
		}

		if (!search(pip, fileName)) // verrifes the child name doesn't already exist
		{
			printf("ERROR: a file named: %s does not exist in directory: %s", fileName, parent);
			iput(pip);
			return -1;
		}

		iput(pip);

		int fd = open_file(fileName, 0); // open for read. 
		if (fd < 0)
		{
			return -1; // open failed.
		}


		do_lseek(fd, 0); // start read from head of file. 
		int n = 0;
		int lineCount = 0;

		char buff[BLKSIZE];
		n = myread(fd, buff, BLKSIZE);
		while (lineCount < 10 && n) // until we hit 10 lines or EOF. 
		{
			buff[n] = '\0';
			char* cp = buff;
			while (*cp != '\0' && lineCount < 10) // make sure we do not hit line count 10 in the middle of a read block.
			{
				while (*cp != '\n' && *cp != '\0') // print character by character until new line is hit
				{
					printf("%c", *cp);
					cp++;
				}
				if (*cp == '\n')
				{
					printf("%c", *cp); // print the new line character
					lineCount++;
					cp++;
				}
			}
			n = myread(fd, buff, BLKSIZE);
		}
		close_file(fd);
	}
	return 0;
}

int myTail(char* pathname)
{
	MINODE* temp;
	char pathcpy1[256], pathcpy2[256]; // for basename and dirname functions which are destructive.
	strcpy(pathcpy1, pathname);
	strcpy(pathcpy2, pathname);

	// library functions to separate /x/y/z into /x/y and /z
	char* parent = dirname(pathcpy1);
	char* fileName = basename(pathcpy2);

	MINODE* pip = path2inode(parent);
	if (pip)
	{
		if (!S_ISDIR(pip->INODE.i_mode)) // verrfies parent is a dir
		{
			printf("ERROR: %s is not a directory\n", parent);
			iput(pip);
			return -1;
		}

		if (!search(pip, fileName)) // verrifes the child name doesn't already exist
		{
			printf("ERROR: a file named: %s does not exist in directory: %s", fileName, parent);
			iput(pip);
			return -1;
		}
		iput(pip);

		int fd = open_file(fileName, 0); // open for read. 
		if (fd < 0)
		{
			return -1; // open failed.
		}

		char buff[BLKSIZE];
		int longFile = 0;
		int fileSize = running->fd[fd]->inodeptr->INODE.i_size;
		if (fileSize > BLKSIZE) // we will read whole blocks, lets get to the last block in the file.
		{
			do_lseek(fd, fileSize - BLKSIZE);
			longFile = 1;
		}

		int lineCount = 0;
		char* ltp[BLKSIZE];
		int longTail = 0;
		int n = myread(fd, buff, BLKSIZE); // read the last block of the file.
		int loops = 1;
		while (lineCount < 11 && n)
		{
			loops++;
			buff[n] = '\0';
			char* cp = &buff[n] - 1; // set cp to last character of file
			while (cp > buff - 1) // make sure cp does not go past buff adress.
			{
				while (*cp != '\n') // read character by character looking for new lines
				{
					cp--; // we are moving backwards down buff.
				}
				lineCount++;
				if (lineCount == 11 && longTail == 0) // found all 10 in this blksize limit, we can print.
				{
					printf("%s", cp + 1);
					close_file(fd);
					return 1;
				}
				else if (lineCount == 11 && longTail == 1) // all 10 found, but some stored in ltp.
				{
					printf("%s", cp + 1); // prints first lines.
					char* hp = ltp;
					char* tp = ltp;
					while (*tp)
					{
						tp++;
					}
					while (tp > hp) // print each character array stored in ltp in reverse order stored.
					{
						tp--;
						printf("%s", tp);
					}
					close_file(fd);
					return 1;
				}
				cp--;
			}
			if (lineCount < 11) // we broke the loop befopre finding all 10. save this text to be used later.
			{
				if (longFile == 0) // this was all the lines there were.
				{
					printf("%s", buff);
					close_file(fd);
					return 1;
				}
				else
				{
					longTail = 1;
					char* rp = ltp;
					while (*rp) // find next avialable space in lines to print array.
					{
						rp++;
					}
					*rp = buff; // store all the lines before looking for more.

					if (fileSize > loops * BLKSIZE) // seek back to start of file from end.
					{
						do_lseek(fd, (fileSize - (loops * BLKSIZE)));
						n = myread(fd, buff, BLKSIZE);
					}
					else
					{
						do_lseek(fd, 0); // remaining lines go to start of file.
						n = myread(fd, buff, BLKSIZE);
					}
				}
			}
		}
		close_file(fd);
		return -1;
	}
}