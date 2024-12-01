int tst_bit(char* buf, int bit)
{
	return buf[bit >> 3] & (1 << (bit % 8));
}

int set_bit(char* buf, int bit)
{
	int mybit, byte;
	byte = bit >> 3;
	mybit = bit % 8;
	if (buf[byte] |= (1 << mybit))
	{
		return 1;
		return 1;
	}
	return 0;
}

int clr_bit(char* buf, int bit)
{
	int mybit, byte;
	byte = bit >> 3;
	mybit = bit % 8;
	if (buf[byte] &= ~(1 << mybit))
	{
		return 1;
	}
	return 0;
}

int ialloc(int dev)  // allocate an inode number from imap block
{
	int  i;
	char buf[BLKSIZE];

	// read inode_bitmap block
	get_block(dev, imap, buf);

	for (i = 0; i < ninodes; i++) {
		if (tst_bit(buf, i) == 0) {
			set_bit(buf, i);
			put_block(dev, imap, buf);

			decFreeInodes(dev);

			printf("ialloc : ino=%d\n", i + 1);
			return i + 1;
		}
	}
	return 0;
}

int balloc(int dev)
{
	int i;
	char buf[BLKSIZE];

	get_block(dev, bmap, buf);

	for (i = 0; i < nblocks; i++)
	{
		if (tst_bit(buf, i) == 0)
		{
			set_bit(buf, i);
			decFreeBlocks(dev);
			put_block(dev, bmap, buf);
			return i + 1;
		}
	}
	return 0;
}

int idalloc(int dev, int ino)  // deallocate an ino number
{
	int i;
	char buf[BLKSIZE];

	// return 0 if ino < 0 OR > ninodes
	if (ino < 0 || ino > ninodes)
	{
		return 0;
	}
	// get inode bitmap block
	get_block(dev, bmap, buf);
	clr_bit(buf, ino - 1);

	// write buf back
	put_block(dev, imap, buf);

	// update free inode count in SUPER and GD
	incFreeInodes(dev);
}

int bdalloc(int dev, int blk) // deallocate a blk number
{
	char buf[BLKSIZE];

	get_block(dev, bmap, buf); // get the block
	clr_bit(buf, blk - 1);     // clear the bits to 0
	put_block(dev, bmap, buf); // write the block back
	incFreeBlocks(dev);        // increment the free block count
	return 0;
}