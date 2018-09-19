# FUSE D7 File System

D7FS implements the FUSE driver for a custom file system. This was developed for the class Sistemas Operativos II in Unitec SPS, Q3 2018. 

File System Description
-----------------------

The first block of the disk stores the ```MetaBlock``` which contains relevant metadata.

```
struct MetaBlock{
	int bitmap_ptr;
	int root_ptr;
	int bitmap_size;
	int disk_size_in_mb;
	int total_blocks;
	int available_blocks;
};
```

The next block(s) store the bitmap which tells the file system which of the available blocks are free (1 = free : 0 = taken).

The first block after the bitmap stores the root directory, which is comprised of object entries.

```
struct Directory{
    struct Entry entries[MAX_DIRECTORY_ENTRIES];
};

struct Entry{				//total size = 288
    int index_block;			//4
    char name[MAX_FILE_NAME];		//256
    long size;				//8
    int is_dir;				//4
    long created_at;			//8
    long last_modified;			//8
};
```

This marks one of the limitations of this filesystem, it can only have (BLOCK_SIZE/ENTRY_SIZE) entries per directory, including root.

If the entry is a directory its ```index_block``` points to the block which contains its entries. If it is a file ```index_block``` points to a doubly indirect block, which contains pointers to indirect blocks, which contain pointers to actual data blocks.

