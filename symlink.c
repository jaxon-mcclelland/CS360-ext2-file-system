// creates a new symlink
void symlink(char* src_file, char* dest_file)
{
    MINODE* src_mip = path2inode(src_file);
    if (src_mip == NULL)
        return;
    if(!S_ISDIR(src_mip->INODE.i_mode) && !S_ISREG(src_mip->INODE.i_mode)) {
        printf("symlink - cannot create link to %s - not a dir or reg file\n",src_file);
        iput(src_mip);
        return;
    }
    iput(src_mip);
    create_file(dest_file);
    MINODE* dest_mip = path2inode(dest_file);
    if(dest_mip== NULL) {
        printf("Error creating symlink - %s could not be created\n", dest_file);
        return;
    }
    INODE* ip = &dest_mip->INODE;
    ip->i_mode &= 0x0FFF; // clear first 4 bits
    ip->i_mode |= 0xA000; // set file type to link
    strcpy(ip->i_block, src_file); // write src_file path to ip->i_block, which has room for 60 chars
    ip->i_size = strlen(src_file);
    ip->i_block[strlen(src_file)] = '\0'; 
    dest_mip->modified = 1;
    iput(dest_mip);    
}

// reads link from src_file and copies link's destination into link_target_path
void read_link(char* src_file, char* link_target_path) 
{
    MINODE* mip = path2inode(src_file);
    if(mip == NULL) {
        printf("Could not read %s - file not found\n", src_file);
        *link_target_path = NULL;
    }
    else if(!S_ISLNK(mip->INODE.i_mode)) {
        printf("Could not read %s - not a valid link\n", src_file);
        *link_target_path = NULL;
    }
    else {
        strcpy(link_target_path, mip->INODE.i_block);
    }
    iput(mip);
}