#ifndef d7fs_h
#define d7fs_h

#include "device.h"
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTRY_SIZE sizeof(struct Entry)
#define METABLOCK_SIZE 24
#define MAX_FILE_NAME 256
#define BITS_IN_BYTE 8

#define MAX_BLOCKS_PER_INDEX_BLOCK (int)(BLOCK_SIZE/sizeof(int))
#define MAX_BLOCKS_PER_FILE (int)(MAX_BLOCKS_PER_INDEX_BLOCK*MAX_BLOCKS_PER_INDEX_BLOCK)
#define MAX_FILE_SIZE (long)((long)MAX_BLOCKS_PER_INDEX_BLOCK*(long)MAX_BLOCKS_PER_INDEX_BLOCK*(long)BLOCK_SIZE)
#define MAX_DIRECTORY_ENTRIES (int)(BLOCK_SIZE/ENTRY_SIZE)

struct MetaBlock{
	int bitmap_ptr;
	int root_ptr;
	int bitmap_size;
	int disk_size_in_mb;
	int total_blocks;
	int available_blocks;
	char empty_space[BLOCK_SIZE -  METABLOCK_SIZE];
};

struct Entry{ //288
	int index_block;			//4
    char name[MAX_FILE_NAME];	//256
    long size;					//8
    int is_dir;					//4
    long created_at;			//8
    long last_modified;			//8
};

struct Directory{
    struct Entry entries[MAX_DIRECTORY_ENTRIES];
};

void d7fs_load_meta();
void d7fs_load_map();
void d7fs_load_root();
void d7fs_update_meta();
void d7fs_update_map();
void d7fs_update_root();
struct Entry *d7fs_get_entry(const char *name);
struct Entry *d7fs_get_entry_r(const char *name, struct Directory* dir);
struct Entry *d7fs_lookup_entry(const char *name, struct Directory* dir);
struct Directory *d7fs_load_dir(int i_block);
void d7fs_get_file_metadata(struct Entry*, int *size, int *blocks);
int d7fs_get_free_block(char * _bitmap, int bit_map_size);
void d7fs_set_bit(char * _bitmap, int n, int value);
void d7fs_delete_entry(struct Entry* entry);

void * d7fs_init(struct fuse_conn_info *conn);
int d7fs_opendir(const char *path, struct fuse_file_info *fileInfo);
int d7fs_getattr(const char *path, struct stat *statbuf);
int d7fs_releasedir(const char *path, struct fuse_file_info *fileInfo);
int d7fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int d7fs_mknod(const char *path, mode_t mode, dev_t dev);
int d7fs_open(const char *path, struct fuse_file_info *fileInfo);
int d7fs_utime(const char *path, struct utimbuf *ubuf);
int d7fs_flush(const char *path, struct fuse_file_info *fileInfo);
int d7fs_release(const char *path, struct fuse_file_info *fileInfo);
int d7fs_rename(const char *path, const char *newpath);
int d7fs_unlink(const char *path);
int d7fs_mkdir(const char *path, mode_t mode);
int d7fs_rmdir(const char *path);


int d7fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
int d7fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);

int d7fs_statfs(const char *path, struct statvfs *statInfo);
int d7fs_truncate(const char *path, off_t newSize);

#ifdef __cplusplus
}
#endif

#endif //d7fs_h