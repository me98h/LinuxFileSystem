/*********************************************************************
 *  Projet "Systèmes d'Exploitation"
 *  Licence Informatique - Université de Cergy
 * 
 *  Auteur: Maaloul Amira 40%, El Mouaouine Mehdi 40%, El Baraka Amine 20%
 *  Testeurs: El Baraka Amine
 *********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fs.h"
#include "fs_util.h"
#include "disk.h"

//---GLOBAL VARIABLE(S)---
//STRUCT(S)
Inode inode[MAX_INODE];
SuperBlock superBlock;
Dentry curDir;
//BOOLEAN(S)
int hasRemovedBefore = 0;
//INT(S)
int curDirBlock;
int currentDirectoryInode;
//CHAR(S)
char inodeMap[MAX_INODE / 8];
char blockMap[MAX_BLOCK / 8];

/**
 * This function is used to mount an existing file as a virtual disk 
 *  if it already exists or create it and format it with the file system 
 *
 * @param name Name of the disk to mount
 *
 * @return: int
 */
int fs_mount(char *name)
{
    //---VARIALBE(S)---
    //  int(S)
    int i = 0;
    int index = 0;
    int inode_index = 0;
    int numInodeBlock = (sizeof(Inode) * MAX_INODE) / BLOCK_SIZE;

    // load superblock, inodeMap, blockMap and inodes into the memory
    if (disk_mount(name) == 1)
    {
        disk_read(0, (char *)&superBlock);
        disk_read(1, inodeMap);
        disk_read(2, blockMap);

        for (i = 0; i < numInodeBlock; i++)
        {
            index = i + 3;
            disk_read(index, (char *)(inode + inode_index));
            inode_index += (BLOCK_SIZE / sizeof(Inode));
        }

        // root directory
        curDirBlock = inode[0].directBlock[0];

        disk_read(curDirBlock, (char *)&curDir);
    }
    else
    {
        // Init file system superblock, inodeMap and blockMap
        superBlock.freeBlockCount = MAX_BLOCK - (1 + 1 + 1 + numInodeBlock);
        superBlock.freeInodeCount = MAX_INODE;

        //Init inodeMap
        for (i = 0; i < MAX_INODE / 8; i++)
        {
            set_bit(inodeMap, i, 0);
        }

        //Init blockMap
        for (i = 0; i < MAX_BLOCK / 8; i++)
        {
            if (i < (1 + 1 + 1 + numInodeBlock))
            {
                set_bit(blockMap, i, 1);
            }
            else
            {
                set_bit(blockMap, i, 0);
            }
        }

        //Init root dir
        int rootInode = get_free_inode();
        currentDirectoryInode = rootInode;

        curDirBlock = get_free_block();

        inode[rootInode].type = directory;
        inode[rootInode].owner = 0;
        inode[rootInode].group = 0;

        inode[rootInode].size = 1;
        inode[rootInode].blockCount = 1;
        inode[rootInode].directBlock[0] = curDirBlock;

        curDir.numEntry = 1;
        strncpy(curDir.dentry[0].name, ".", 1);
        curDir.dentry[0].name[1] = '\0';
        curDir.dentry[0].inode = rootInode;
        currentDirectoryInode = rootInode;

        disk_write(curDirBlock, (char *)&curDir);
    }

    return 0;
}

/**
 * This function is used to unmount a disk. 
 *
 * @param name - the name of the disk to unmount
 *
 * @return: int
 */
int fs_umount(char *name)
{
    int numInodeBlock = (sizeof(Inode) * MAX_INODE) / BLOCK_SIZE;
    int i, index, inode_index = 0;

    disk_write(0, (char *)&superBlock);
    disk_write(1, inodeMap);
    disk_write(2, blockMap);

    for (i = 0; i < numInodeBlock; i++)
    {
        index = i + 3;
        disk_write(index, (char *)(inode + inode_index));
        inode_index += (BLOCK_SIZE / sizeof(Inode));
    }
    // current directory
    disk_write(curDirBlock, (char *)&curDir);

    disk_umount(name);

    return 0;
}

/**
 * This function is used to search for the inode of a file given its name.
 * 
 * @param name The name of the file that will be searched for. 
 *
 * @return Returns the inode of the file and -1 if the file doesn't exist.
 */
int search_cur_dir(char *name)
{
    int i;

    for (i = 0; i < MAX_DIR_ENTRY; i++)
    {
        if (str_equal(name, curDir.dentry[i].name))
        {
            return curDir.dentry[i].inode;
        }
    }

    return -1; // return inode. If not exist, return -1
}

/**
 * This function creates a file with a given name and size
 *
 * @param name The file name that will be created
 * @param size  The size of the file to create
 *
 * @return: int
 */
int file_create(char *name, int size)
{
    //---VARIABLE(S)---
    //  boolean(s)
    int foundSpot = 0;
    //  integer(s)
    int i;
    int inodeNum;
    int numBlock;

    if (size >= SMALL_FILE)
    {
        printf("File create error: Do not support files larger than %d bytes yet.\n", SMALL_FILE);

        return -1;
    }

    inodeNum = search_cur_dir(name);

    if (inodeNum >= 0)
    {
        printf("File create error:  %s already exists.\n", name);

        return -1;
    }
    printf("%lu\n", BLOCK_SIZE / sizeof(DirectoryEntry));

    if (curDir.numEntry + 1 > (BLOCK_SIZE / sizeof(DirectoryEntry)))
    {
        printf("File create error: directory is full!\n");

        return -1;
    }

    //  Sets initialized number of blocks
    numBlock = size / BLOCK_SIZE;

    //  Adds to the size depending on the file size
    if (size % BLOCK_SIZE > 0)
    {
        numBlock++;
    }

    //  Ensures that there is enough space to add file
    if (numBlock > superBlock.freeBlockCount)
    {
        printf("File create error: not enough blocks\n");

        return -1;
    }

    //  Ensures that there are enough iNodes to add file
    if (superBlock.freeInodeCount < 1)
    {
        printf("File create error: not enough inodes.\n");

        return -1;
    }

    //  Sets random chars for a string based on the input size
    char *tmp = (char *)malloc(sizeof(int) * size + 1);
    rand_string(tmp, size);
    printf("Random String: %s\n", tmp);

    // Get inode and fill it
    inodeNum = get_free_inode();

    if (inodeNum < 0)
    {
        printf("File create error: not enough inodes.\n");
        free(tmp);

        return -1;
    }

    //  Sets type, owner, group
    inode[inodeNum].type = file; //Is set to 0 for file
    inode[inodeNum].owner = 1;
    inode[inodeNum].group = 2;

    //  Sets time for create & access
    gettimeofday(&(inode[inodeNum].created), NULL);
    gettimeofday(&(inode[inodeNum].lastAccess), NULL);

    //  Sets the size and blockCount
    inode[inodeNum].size = size;
    inode[inodeNum].blockCount = numBlock;

    if (hasRemovedBefore == 1)
    {
        //  Search for all entries for first empty spot
        for (i = 0; i < MAX_DIR_ENTRY; i++)
        {
            if ((curDir.dentry[i].inode == 0) && strcmp(curDir.dentry[i].name, "") == 0)
            {
                // Add a new file into the current directory entry
                strncpy(curDir.dentry[i].name, name, strlen(name));
                curDir.dentry[i].name[strlen(name)] = '\0';
                curDir.dentry[i].inode = inodeNum;
                curDir.numEntry++;

                // Get data blocks
                for (i = 0; i < numBlock; i++)
                {
                    int block = get_free_block();

                    if (block == -1)
                    {
                        printf("File create error: get_free_block has failed.\n");
                        free(tmp);

                        return -1;
                    }
                    inode[inodeNum].directBlock[i] = block;

                    disk_write(block, tmp + (i * BLOCK_SIZE));
                }

                printf("File created: %s, inode %d, size %d\n", name, inodeNum, size);
                free(tmp);

                return 0;
            }
        }
    }
    else
    {
        // Add a new file into the current directory entry
        strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
        curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
        curDir.dentry[curDir.numEntry].inode = inodeNum;
        curDir.numEntry++;

        // Get data blocks
        for (i = 0; i < numBlock; i++)
        {
            int block = get_free_block();

            if (block == -1)
            {
                printf("File create error: get_free_block has failed.\n");
                free(tmp);

                return -1;
            }
            inode[inodeNum].directBlock[i] = block;

            disk_write(block, tmp + (i * BLOCK_SIZE));
        }

        printf("File created: %s, inode %d, size %d\n", name, inodeNum, size);
        free(tmp);
    }

    return 0;
}

/**
 * This function is used to read a file. 
 *
 * @param name The file name to read from
 * @param offset The point where read starts
 * @param size The number of bytes to read
 *
 * @return: int
 */
int file_read(char *name, int offset, int size)
{
    int i = 0;
    int inodeNum = 0;
    int blockNum = 0;
    //  char *(s)
    char fileContents[MAX_BLOCK + MAX_BLOCK];
    char tempContentsHolder[MAX_BLOCK + MAX_BLOCK];
    //  other
    size_t len;

    //ERROR CHECKING
    if (offset < 0 || size < 0)
    {
        if (offset < 0 && size < 0)
        {
            printf("File read error: Can not have an offset and size less than 0\n");
        }
        else if (offset < 0)
        {
            printf("File read error: Can not have an offset less than 0\n");
        }
        else if (size < 0)
        {
            printf("File read error: Can not have a size less than 0\n");
        }

        //Clear the buffer / array to avoid segfault
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

        return 0;
    }

    //  Gets the inode of the file
    inodeNum = search_cur_dir(name);

    if (inodeNum == -1) //IF: ERROR CHECKING - inodeNum is -1 it doesn't exist
    {
        printf("File read error: file does not exist.\n");

        //Clear the buffer / array to avoid segfault
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

        return 0;
    }
    else //ELSE: inodeNum is > -1 therefore it exist
    {
        //  Get number of blocks to iterate through
        blockNum = inode[inodeNum].blockCount;

        //  FOR LOOP: loop through blocks used to get contents
        for (i = 0; i < blockNum; i++)
        {
            //  Get the directBlock number
            int block = inode[inodeNum].directBlock[i];

            //  Pull contents from the disk at that block
            disk_read(block, fileContents);

            strcat(tempContentsHolder, fileContents);
        }

        //  Get the size of the file contents
        len = strlen(tempContentsHolder);

        if (offset > len) //IF: Error - offset greater than size of file
        {
            printf("File read error: the offset is greater than the size of the file.\n");

            //Clear the buffer / array to avoid segfault
            memset(fileContents, 0, sizeof(fileContents));
            memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

            return 0;
        }
        else //ELSE: Printing out the contents of the string
        {
            printf("%.*s\n", size, (tempContentsHolder + offset));
        }

        //Clear the buffer / array to avoid segfault
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));
    }

    gettimeofday(&(inode[inodeNum].lastAccess), NULL);

    return 0;
}

/**
 * This function is used to write the content of buf into the file starting from an offset.
 *
 * @param name The file name to write to
 * @param offset The point where it will start to write
 * @param size The size of the buffer to write
 * @param buf The string buffer that will be written
 *
 * @return: int
 */
int file_write(char *name, int offset, int size, char *buf)
{
    int i = 0;
    int inodeNum = 0;
    int blockNum = 0;

    char temp[MAX_BLOCK + MAX_BLOCK];
    char temp2[MAX_BLOCK + MAX_BLOCK];
    char temp3[MAX_BLOCK + MAX_BLOCK];
    char fileContents[MAX_BLOCK + MAX_BLOCK];
    char tempContentsHolder[MAX_BLOCK + MAX_BLOCK];

    size_t len;

    //ERROR CHECKING: that the parameters are valid
    if (offset < 0 || size < 0)
    {
        if (offset < 0 && size < 0)
        {
            printf("File write error: Can not have an offset and size less than 0\n");
        }
        else if (offset < 0)
        {
            printf("File write error: Can not have an offset less than 0\n");
        }
        else
        {
            printf("File write error: Can not have a size less than 0\n");
        }

        //  Free buffer
        memset(temp, 0, sizeof(temp));
        memset(temp2, 0, sizeof(temp2));
        memset(temp3, 0, sizeof(temp3));
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

        return 0;
    }

    //  ERROR CHECKING: that the size matches the char * buf size
    if (size != strlen(buf))
    {
        printf("File write error: The size you entered doesn't match the length of the buffer string you entered\n");

        //Error so freeing buffer
        memset(temp, 0, sizeof(temp));
        memset(temp2, 0, sizeof(temp2));
        memset(temp3, 0, sizeof(temp3));
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

        return 0;
    }

    //  Gets the inode of the file
    inodeNum = search_cur_dir(name);

    if (inodeNum == -1) //IF: inodeNum is -1 it doesn't exist
    {
        printf("File write error: file does not exist.\n");

        //Error so freeing buffer
        memset(temp, 0, sizeof(temp));
        memset(temp2, 0, sizeof(temp2));
        memset(temp3, 0, sizeof(temp3));
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

        return -1;
    }
    else //ELSE: inodeNum is > -1 therefore it exist
    {
        //  Get number of blocks to iterate through
        blockNum = inode[inodeNum].blockCount;

        //  FOR LOOP: loop through blocks used to get contents
        for (i = 0; i < blockNum; i++)
        {
            //  Get the directBlock number
            int block = inode[inodeNum].directBlock[i];

            //  Pull contents helded in disk at that block
            disk_read(block, fileContents);
            //memcpy( fileContents, disk[block], BLOCK_SIZE );

            strcat(tempContentsHolder, fileContents);
        }

        //  Get the size of the file contents
        len = strlen(tempContentsHolder);

        if (offset > len) //IF: ERROR CHECKING - offset greater than size of file
        {
            printf("The offset is greater than the size of the file contents\n");

            //Error so freeing buffer
            memset(temp, 0, sizeof(temp));
            memset(temp2, 0, sizeof(temp2));
            memset(temp3, 0, sizeof(temp3));
            memset(fileContents, 0, sizeof(fileContents));
            memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

            return 0;
        }
        else //ELSE: Printing out the contents of the string
        {
            //  Will grab the string of the string prior to the start of the offset
            sprintf(temp3, "%.*s", offset, (tempContentsHolder));
            //  Will grab the string that is passed in to be appended
            sprintf(temp, "%s", buf);
            //  Concat the two strings together
            strcat(temp3, temp);

            //  Will grab the rest of the string starting from the offset + size
            sprintf(temp2, "%s", (tempContentsHolder + offset + size));
            //  Concat the two strings together
            strcat(temp3, temp2);
            //  Will print the new string with the passed in string added
            printf("%s\n", temp3);

            if (inode[inodeNum].size == strlen(temp3)) //IF: Size was not changed enter
            {
                int k = 0;
                for (k = 0; k < blockNum; k++)
                {
                    int block = inode[inodeNum].directBlock[k];

                    disk_write(block, temp3 + (k * BLOCK_SIZE));
                }
            }
            else //ELSE: Size was changed
            {
                if (strlen(temp3) >= SMALL_FILE)
                {
                    printf("File write error: Do not support files larger than %d bytes yet.\n", SMALL_FILE);

                    //Error so freeing buffer
                    memset(temp, 0, sizeof(temp));
                    memset(temp2, 0, sizeof(temp2));
                    memset(temp3, 0, sizeof(temp3));
                    memset(fileContents, 0, sizeof(fileContents));
                    memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

                    return -1;
                }

                //  Sets initialized number of blocks
                int newBlockNum = (size + strlen(tempContentsHolder)) / BLOCK_SIZE;
                //  Adds to the size depending on the file size
                if ((size + strlen(tempContentsHolder)) % BLOCK_SIZE > 0)
                {
                    newBlockNum++;
                }

                if (newBlockNum == blockNum) //IF: block number didn't change from old block numbers
                {
                    //Assign the new size of concatenated string to file
                    inode[inodeNum].size = strlen(temp3);

                    int k = 0;
                    for (k = 0; k < blockNum; k++)
                    {
                        int block = inode[inodeNum].directBlock[k];

                        disk_write(block, temp3 + (k * BLOCK_SIZE));
                    }
                }
                else //ELSE: else block number increased from orginal block numbers
                {
                    int count = 0;
                    //Get the difference to see if we have enough free blocks
                    int blockDifference = newBlockNum - blockNum;

                    //ERROR CHECKING: Ensures that there is enough space
                    if (blockDifference > superBlock.freeBlockCount)
                    {
                        printf("File write error: File create failed: not enough space\n");

                        //Error so freeing buffer
                        memset(temp, 0, sizeof(temp));
                        memset(temp2, 0, sizeof(temp2));
                        memset(temp3, 0, sizeof(temp3));
                        memset(fileContents, 0, sizeof(fileContents));
                        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

                        return -1;
                    }

                    //Assign the new size of concatenated string to file
                    inode[inodeNum].size = strlen(temp3);

                    //Assign the number of new blocks used to the inode
                    inode[inodeNum].blockCount = newBlockNum;

                    int k = 0;
                    for (k = 0; k < newBlockNum; k++)
                    {
                        if (count != blockDifference) //IF: Replace data in old blocks
                        {
                            int block = inode[inodeNum].directBlock[k];

                            disk_write(block, temp3 + (k * BLOCK_SIZE));

                            count++;
                        }
                        else //ELSE: Get new data blocks and place the rest there
                        {
                            int block = get_free_block();

                            //ERROR CHECKING: Ensures that the block was created
                            if (block == -1)
                            {
                                printf("File write error: get_free_block has failed.\n");

                                //Error so freeing buffer
                                memset(temp, 0, sizeof(temp));
                                memset(temp2, 0, sizeof(temp2));
                                memset(temp3, 0, sizeof(temp3));
                                memset(fileContents, 0, sizeof(fileContents));
                                memset(tempContentsHolder, 0, sizeof(tempContentsHolder));

                                return -1;
                            }

                            //Assign direct block to inode
                            inode[inodeNum].directBlock[k] = block;
                            //Write the rest of the data to block
                            disk_write(block, temp3 + (k * BLOCK_SIZE));
                        }
                    }
                }
            }
        }

        //Update the last access time for file
        gettimeofday(&(inode[inodeNum].lastAccess), NULL);

        //Clear the buffer / array to avoid segfault
        memset(temp, 0, sizeof(temp));
        memset(temp2, 0, sizeof(temp2));
        memset(temp3, 0, sizeof(temp3));
        memset(fileContents, 0, sizeof(fileContents));
        memset(tempContentsHolder, 0, sizeof(tempContentsHolder));
    }

    return 0;
}

/**
 * This function is used to remove a file given its name.
 *
 * @param name The name of the file that will be removed
 *
 * @return: int
 */
int file_remove(char *name)
{
    int inodeNum = 0;
    int i = 0;
    int counter = 0;

    // Check if file exists
    inodeNum = search_cur_dir(name);

    //  ERROR CHECK: check to see if file exists
    if (inodeNum < 0)
    {
        printf("File remove failed:  %s does not exist.\n", name);

        return -1;
    }

    if (inode[inodeNum].type == file) //IF: type is file
    {
        for (i = 0; i < MAX_DIR_ENTRY; i++)
        {
            if (curDir.dentry[i].inode == inodeNum)
            {
                //  Set global variable 'hasRemovedBefore' to 1 (true)
                hasRemovedBefore = 1;

                //  Making entry name blank
                strncpy(curDir.dentry[i].name, "", strlen(""));
                curDir.dentry[i].name[strlen("")] = '\0';

                //Change entry inode to 0
                curDir.dentry[i].inode = 0;

                //  Set inodemap spot to 0 and increment freeInodeCount
                set_bit(inodeMap, inodeNum, 0);
                superBlock.freeInodeCount++;

                //  Decrement the number of entries present
                curDir.numEntry--;

                //  Get the number of blocks used and cycle through
                int numBlock = inode[inodeNum].blockCount;
                for (i = 0; i < numBlock; i++)
                {
                    set_bit(blockMap, inode[inodeNum].directBlock[i], 0);
                }

                //  Increment the free block count
                superBlock.freeBlockCount = numBlock + superBlock.freeBlockCount;

                //  Set access time of directory, though this doesn't matter
                gettimeofday(&(inode[inodeNum].lastAccess), NULL);

                break;
            }
        }
    }
    else //ELSE: ERROR CHECK - type is directory
    {
        printf("File remove error: This is a directory, cannot delete it\n");

        return -1;
    }

    return 0;
}

/**
 * This function is used to create a new directory.
 * 
 * @param name The name of the directory
 *
 * @return: int
 */
int dir_make(char *name)
{
    int i;
    int parentDirInode;
    int currentDirInode;
    int oldCurDirBlock;

    //  Returns -1 or existing inode number
    int inodeNum = search_cur_dir(name);

    //  ERROR CHECK: Ensures that directory doesn't already exist
    if (inodeNum >= 0)
    {
        printf("Directory create failed: %s exists.\n", name);

        return -1;
    }

    //  ERROR CHECK: Ensures that there are enough directory entries
    if (curDir.numEntry + 1 > (BLOCK_SIZE / sizeof(DirectoryEntry)))
    {
        printf("Directory create failed: directory is full!\n");

        return -1;
    }

    //  ERROR CHECK: Ensures that there are enough iNodes to add file
    if (superBlock.freeInodeCount < 1)
    {
        printf("Directory create failed: not enough inodes\n");

        return -1;
    }

    //  Get new inode for directory
    int directoryInode = get_free_inode();
    //  ERROR CHECK: Ensures one free inode
    if (directoryInode < 0)
    {
        printf("Directory create error: not enough inodes.\n");

        return -1;
    }

    oldCurDirBlock = curDirBlock;

    //  Gets a free block
    curDirBlock = get_free_block();
    //  ERROR CHECK: Ensures one free block out of the 10
    if (curDirBlock == -1)
    {
        printf("Directory create error: get_free_block failed\n");

        return -1;
    }

    //*********************************
    //***STEP 1: CREATE NEW DIRECTORY & WRITE
    //  Sets type, owner, group
    inode[directoryInode].type = directory; //Is set to 1 for directory
    inode[directoryInode].owner = 1;
    inode[directoryInode].group = 2;

    gettimeofday(&(inode[directoryInode].created), NULL);
    gettimeofday(&(inode[directoryInode].lastAccess), NULL);

    //  Set size, blockCount and directBlock
    inode[directoryInode].size = 1;
    inode[directoryInode].blockCount = 1;
    inode[directoryInode].directBlock[0] = curDirBlock;

    if (hasRemovedBefore == 1) //IF: there hasn't been a remove it
    {
        //  Search for all entries for first empty spot
        for (i = 0; i < MAX_DIR_ENTRY; i++)
        {
            if ((curDir.dentry[i].inode == 0) && strcmp(curDir.dentry[i].name, "") == 0)
            {
                //  Set the parents directory inode
                parentDirInode = currentDirectoryInode;

                //  Set the name of the directory
                strncpy(curDir.dentry[i].name, name, strlen(name));
                curDir.dentry[i].name[strlen(name)] = '\0';

                //  Set the inode of the directory
                curDir.dentry[i].inode = directoryInode;
                currentDirInode = directoryInode;

                //  Increment the numEntries
                curDir.numEntry++;

                break;
            }
        }
    }
    else //ELSE: there hasn't been a remove
    {
        //  Set the parents directory inode
        parentDirInode = currentDirectoryInode;

        //  Set the name of the directory
        strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
        curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';

        //  Set the inode of the directory
        curDir.dentry[curDir.numEntry].inode = directoryInode;
        currentDirInode = directoryInode;

        //  Increment the numEntries
        curDir.numEntry++;
    }

    //  Write all of this new information to disk
    disk_write(oldCurDirBlock, (char *)&curDir);

    //*********************************
    //***STEP 2: INITIALIZE THE NEW DIRECTORY & WRITE
    //  Resets curDir so it can be reused
    bzero(&curDir, sizeof(curDir));

    //  Set numEntry to zero
    curDir.numEntry = 0;

    //  Set up '.' for new directory
    strncpy(curDir.dentry[curDir.numEntry].name, ".", 1);
    curDir.dentry[curDir.numEntry].name[1] = '\0';
    //  Set the inode of the directory
    curDir.dentry[curDir.numEntry].inode = currentDirInode;
    curDir.numEntry++;

    //  Set up '..' for parent directory
    strncpy(curDir.dentry[curDir.numEntry].name, "..", 2);
    curDir.dentry[curDir.numEntry].name[2] = '\0';
    //  Set the inode of the directory
    curDir.dentry[curDir.numEntry].inode = parentDirInode;
    curDir.numEntry++;

    //  Write information for this new directory to disk
    disk_write(inode[directoryInode].directBlock[0], (char *)&curDir);

    //*********************************
    //***STEP 3: REREAD THE OLD
    //  Read from disk to reload old directory
    disk_read(oldCurDirBlock, (char *)&curDir);
    curDirBlock = oldCurDirBlock;

    //  Print the recently created directory information
    printf("Directory created: %s, inode %d, size %d\n", name, directoryInode, inode[directoryInode].size);

    return 0;
}

/**
 * This function is used to remove a directory.
 * 
 * @param name The name of the directory to remove
 *
 * @return: int
 */
int dir_remove(char *name)
{

    int i;
    int j;

    //  ERROR CHECKING: Making sure you don't remove parent or current directory
    if (strcmp(name, ".") == 0)
    {
        printf("Directory remove error: Can't remove the directory you are in.\n");

        return -1;
    }
    else if (strcmp(name, "..") == 0)
    {
        printf("Directory remove error: Can't remove parent directory because it contains files.\n");

        return -1;
    }

    int directoryInodeNum = search_cur_dir(name);

    //  ERROR CHECKING: Making sure the directory exists
    if (directoryInodeNum < 0)
    {
        printf("Directory remove error: directory does not exist.\n");

        return -1;
    }

    if (inode[directoryInodeNum].type == directory)
    {
        //Check to see if there are any files in the directory
        //CD into directory to check and see if any files are present
        dir_change(name);

        //  ERROR CHECKING
        if (curDir.numEntry > 2) //IF: number of entries is less than 2
        {
            printf("Directory remove error: The directory has files in it. Cannot remove it.\n");

            dir_change("..");

            return -1;
        }

        //  Made it out the loop, we can remove directory; Enter into parent directory
        dir_change("..");

        hasRemovedBefore = 1;

        //  Will loop through entir number of entries to find directory
        for (j = 0; j < MAX_DIR_ENTRY; j++)
        {
            if (curDir.dentry[j].inode == directoryInodeNum)
            {
                //  Set name to blank string
                strncpy(curDir.dentry[j].name, "", strlen(""));
                curDir.dentry[j].name[strlen("")] = '\0';

                //  Change inode to 0
                curDir.dentry[j].inode = 0;

                //  Change inodeMap to 0 and increase inode count
                set_bit(inodeMap, directoryInodeNum, 0);
                superBlock.freeInodeCount++;

                //  Change blockMap to 0 and increase block count
                set_bit(blockMap, inode[directoryInodeNum].directBlock[j], 0);
                superBlock.freeBlockCount++;

                //  Decrement the number of numEntries for directory
                curDir.numEntry--;
            }
        }
    }
    else //ELSE: It's a file you must use rm to remove that
    {
        printf("Directory remove error: You are trying to remove a file. Please check the name of the directory and try again.\n");

        return -1;
    }

    return 0;
}

/**
 * This function is used to change the current directory.
 * 
 * @param name The name of the directory to change to
 *
 * @return: int
 */
int dir_change(char *name)
{
    if (strcmp(name, ".") == 0) //IF: user types '.'
    {
        printf("Change directory error: Currently in this directory\n");

        return 0;
    }
    else if (strcmp(name, "..") == 0) //ELSE IF: user types '..'
    {
        //*********************************
        //  STEP 1: GET INODE & curDirBlock OF CURRENT & PARENT DIRECTORY
        int changeFromDirectoryInode = search_cur_dir(".");
        int changeFromDirectoryCurDirBlock = inode[changeFromDirectoryInode].directBlock[0];
        int changeToParentDirectoryInode = search_cur_dir("..");
        int changeToParentDirectoryCurDirBlock = inode[changeToParentDirectoryInode].directBlock[0];

        //  ERROR CHECKING: making sure that they aren't in root directory trying to use '..'
        if (changeFromDirectoryInode == 0)
        {
            printf("Change directory error: Currently in this directory.\n");

            return 0;
        }

        //*********************************
        //  STEP 2: WRITE NEW FILES / INFORMATION TO DISK
        disk_write(changeFromDirectoryCurDirBlock, (char *)&curDir);

        //*********************************
        //***STEP 3: READ IN PARENT DIRECTORY STUFF
        //  Set global variables that holds current directories inode & block
        currentDirectoryInode = changeToParentDirectoryInode;
        curDirBlock = changeToParentDirectoryCurDirBlock;

        gettimeofday(&(inode[currentDirectoryInode].lastAccess), NULL);

        //  Read / open the directory that you are entering information
        disk_read(changeToParentDirectoryCurDirBlock, (char *)&curDir);
    }
    else //ELSE: Use the name passed in to find directory's inode number
    {
        //  Gets directories Inode
        int changeToDirectoryInode = search_cur_dir(name);

        //  ERROR CHECK: making sure directory exist
        if (changeToDirectoryInode < 0)
        {
            printf("Change directory error: file does not exist.\n");

            return -1;
        }

        // Sets global variable that holds inode of current directory
        currentDirectoryInode = changeToDirectoryInode;

        //Checks to see if it is of type directory
        if (inode[changeToDirectoryInode].type == directory) //IF: type directory, enter
        {
            //  Get this directories inode to changeToDirectoryCurDirBlock
            int changeToDirectoryCurDirBlock = inode[changeToDirectoryInode].directBlock[0];

            //  Get this directories parent inode to changeFromDirectoryInode

            //  Get this directories parent curDirBlock
            int changeFromDirectoryCurDirBlock = inode[search_cur_dir(".")].directBlock[0];

            //*********************************
            //***STEP 1: Write new files / information to disk
            //  Write the files / data into disk for this directory
            disk_write(changeFromDirectoryCurDirBlock, (char *)&curDir);

            //*********************************
            //***STEP 2: READ NEW DIRECTORY INFORMATION FROM DISK
            //  Set global variable that holds current directory's block
            curDirBlock = changeToDirectoryCurDirBlock;

            gettimeofday(&(inode[changeToDirectoryInode].lastAccess), NULL);
            //  Read / open the directory that you are entering information
            disk_read(changeToDirectoryCurDirBlock, (char *)&curDir);
        }
        else //ELSE: File return
        {
            printf("Change directory error: Type is file, not directory\n");

            return -1;
        }
    }

    return 0;
}

/**
 * This function is used to list the content of the current directory.
 * 
 */
int ls()
{
    //---VARIABLE(S)---
    //  integer(s)
    int i;
    
    //  Loop through the # of MAX_DIR_ENTRY
    for( i = 0; i < MAX_DIR_ENTRY; i++ )
    {
        //  Assign the inode to n
        int n = curDir.dentry[i].inode;
        
        //IF: Enter the loop if size doesn't equal one / name isn't blank
        if(( inode[n].size != 1 ) || ( strcmp( curDir.dentry[i].name, "" ) != 0  ))
        {
            if( inode[n].type == file ) //IF: type is file
            {
                printf( "type: file, " );
            }
            else //ELSE: type is directory
            {
                printf( "type: dir, " );
            }
            
            printf( "name \"%s\", inode %d, size %d byte\n", curDir.dentry[i].name, curDir.dentry[i].inode, inode[n].size );
        }
    }
    
    return 0;
}



/**
 * This function is used to execute a given command 
 * 
 * @param comm The name of the command
 * @param arg1 The first argument
 * @param arg2  The second argument
 * @param arg3 The third argument
 * @param arg4 The fourth argument
 * @param numArg The number of arguments
 *
 * @return: int
 */
int execute_command(char *comm, char *arg1, char *arg2, char *arg3, char *arg4, int numArg)
{
    if (str_equal(comm, "create"))
    {
        if (numArg < 2)
        {
            printf("Error: create <filename> <size>\n");

            return -1;
        }

        return file_create(arg1, atoi(arg2)); // (filename, size)
    }
    else if (str_equal(comm, "write"))
    {
        if (numArg < 4)
        {
            printf("Error: write <filename> <offset> <size> <buf>\n");

            return -1;
        }

        return file_write(arg1, atoi(arg2), atoi(arg3), arg4); // file_write(filename, offset, size, buf);
    }
    else if (str_equal(comm, "read"))
    {
        if (numArg < 3)
        {
            printf("Error: read <filename> <offset> <size>\n");

            return -1;
        }

        return file_read(arg1, atoi(arg2), atoi(arg3)); // file_read(filename, offset, size);
    }
    else if (str_equal(comm, "rm"))
    {
        if (numArg < 1)
        {
            printf("Error: rm <filename>\n");

            return -1;
        }

        return file_remove(arg1); //(filename)
    }
    else if (str_equal(comm, "mkdir"))
    {
        if (numArg < 1)
        {
            printf("Error: mkdir <dirname>\n");

            return -1;
        }

        return dir_make(arg1); // (dirname)
    }
    else if (str_equal(comm, "rmdir"))
    {
        if (numArg < 1)
        {
            printf("Error: rmdir <dirname>\n");

            return -1;
        }

        return dir_remove(arg1); // (dirname)
    } 
    else if (str_equal(comm, "cd")) {
        if (numArg < 1)
        {
            printf("Error: cd <dirname>\n");

            return -1;
        }

        return dir_change(arg1); // (dirname)
    }
    else if(str_equal( comm, "ls" ))
    {
        return ls();
    }
    else if (str_equal(comm, "help"))
    {
        printf("Commands available:\n");
        printf("   create <filename> <size>\n");
        printf("   write <filename> <offset> <size> <buf>\n");
        printf("   read <filename> <offset> <size>\n");
        printf("   rm <filename>\n");
        printf("   mkdir <dirname>\n");
        printf("   rmdir <dirname>\n");
        printf("   cd <dirname>\n");
        printf("   ls\n");
    }
    else
    {
        fprintf(stderr, "%s: command not found. Type help to show available commands.\n", comm);
        return -1;
    }

    return 0;
}
