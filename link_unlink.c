// creates a new file dest_file with same inode as src_file
void create_link(char* src_file, char* dest_file) 
{
	MINODE* src_mip = path2inode(src_file);
    if(src_file == NULL)
        return;
    if (!src_mip)
    {
        return;
    }
	if(S_ISDIR(src_mip->INODE.i_mode)) {
		printf("error creating link - %s is a dir\n", src_file);
		return;
	}
	char pathcpy1[256], pathcpy2[256]; // for basename and dirname functions which are destructive.
	strcpy(pathcpy1, dest_file);
	strcpy(pathcpy2, dest_file);

	char* dest_parent = dirname(pathcpy1);
	char* dest_child = basename(pathcpy2);

	MINODE* dest_pip = path2inode(dest_parent);

	if (!S_ISDIR(dest_pip->INODE.i_mode)) 
	{
		printf("ERROR: %s is not a directory\n", dest_parent);
		iput(src_mip);
		iput(dest_pip);
		return -1;
	}
	if (search(dest_pip, dest_child)) 
	{
		printf("ERROR: a directory or file named: %s already exists inside the directory: %s", dest_child, dest_parent);
		iput(src_mip);
		iput(dest_pip);
		return -1;
	}
	if(dest_pip->dev != src_mip->dev) {
		printf("ERROR: %s and %s need to be on the same device\n", src_file, dest_file);
		iput(src_mip);
		iput(dest_pip);
		return -1;
	}

	enter_child(dest_pip, src_mip->ino, dest_child); // write new link entry into its parent
	src_mip->INODE.i_links_count++;
    // mark both files as modified and write them back to disk
	src_mip->modified = 1;
	iput(src_mip);
    dest_pip->modified = 1;
    iput(dest_pip);
	return 0;
}

// destroys the link at pathname (assuming it's a valid link)
void destroy_link(char* pathname) 
{
    MINODE* mip = path2inode(pathname);
    if(mip == NULL)
        return;
    if(S_ISDIR(mip->INODE.i_mode) || !(S_ISREG(mip->INODE.i_mode) || S_ISLNK(mip->INODE.i_mode))) {
        printf("%s cannot be unlinked - invalid file type\n", pathname);
        iput(mip);
        return;
    }
    mip->INODE.i_links_count--;
    mip->modified = 1;
    if(mip->INODE.i_links_count <= 0) {
        if(!S_ISLNK(mip->INODE.i_mode)) // don't call truncate on symlinks.
            truncate(mip);
        idalloc(dev, mip->ino);
    }
    char temp[256];
    char temp2[256];
    strcpy(temp, pathname);
    strcpy(temp2, pathname);
    char* child_name = basename(temp);
    char* parent_name = dirname(temp2);
    iput(mip);
    MINODE* pip = path2inode(parent_name);
    rm_child(pip, child_name); // remove link entry from its parent
    iput(pip);
}

