#include "d7fs.h"
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

char * bitmap;
struct Directory root;
struct Directory current_dir;
int current_dir_block;
struct MetaBlock metadata;
int root_dir_loaded;

void * d7fs_init(struct fuse_conn_info *conn){
    printf("Loading file system bitmap ...\n");
    d7fs_load_meta();
    d7fs_load_map();
    d7fs_load_root();
    return NULL;
};

void d7fs_load_meta(){
    printf("%s\n", __FUNCTION__);
    device_read_block((unsigned char*)&metadata, 0);
    printf("Loading metadata ...\n");
    printf("bitmap size in blocks - %d\n", metadata.bitmap_size);
    printf("bitmap ptr - %d\n", metadata.bitmap_ptr);
    printf("root ptr - %d\n", metadata.root_ptr);
    printf("total blocks - %d\n", metadata.total_blocks);
    printf("max blocks per index blok - %d\n", MAX_BLOCKS_PER_INDEX_BLOCK);
    printf("max blocks per file - %d\n", MAX_BLOCKS_PER_FILE);
    printf("max file size - %ld\n", MAX_FILE_SIZE);
};

void d7fs_update_meta(){
    printf("%s\n", __FUNCTION__);
    device_write_block((unsigned char*)&metadata, 0);
}

void d7fs_load_map(){
    printf("%s\n", __FUNCTION__);
    bitmap = (char*)malloc(metadata.bitmap_size*BLOCK_SIZE);
    for(int i = 0; i < metadata.bitmap_size; i++)
        device_read_block(&((unsigned char*)bitmap)[i*BLOCK_SIZE], i + metadata.bitmap_ptr);
};

void d7fs_update_map(){
    printf("%s\n", __FUNCTION__);
    for(int i = 0; i < metadata.bitmap_size; i++)
        device_write_block(&((unsigned char*)bitmap)[i*BLOCK_SIZE], i + metadata.bitmap_ptr);
}

void d7fs_load_root(){
    printf("%s\n", __FUNCTION__);
    unsigned char * char_root = (unsigned char*)malloc(BLOCK_SIZE);
    device_read_block(char_root, metadata.root_ptr);
    memcpy(&root, char_root, sizeof(root));
    root_dir_loaded = 1;
    current_dir_block = -1;
    free(char_root);
}

void d7fs_update_root(){
    printf("%s\n", __FUNCTION__);
    unsigned char *char_root=(unsigned char*)malloc(sizeof(root));
    memcpy(char_root, &root, sizeof(root));
    device_write_block(char_root, metadata.root_ptr);
    free(char_root);
}

int d7fs_opendir(const char *path, struct fuse_file_info *fileInfo){
    //printf("%s: %s\n", __FUNCTION__, path);
    
    int path_len = strlen(path);
    if (root_dir_loaded)
        return 0;

    if ((path_len != 1) && path[0] != '/')
        return -EPERM;
    
    d7fs_load_root();

    return 0;    
}

int d7fs_getattr(const char *path, struct stat *statbuf){
    //printf("d7fs_getattr(path=%s)\n", path);
    int path_len = strlen(path);
    if(path_len == 1 && path[0] == '/'){
        statbuf->st_mode=S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_nlink = 1;
        statbuf->st_ino = 0;
        statbuf->st_size = BLOCK_SIZE;
        statbuf->st_blksize = BLOCK_SIZE;
        statbuf->st_blocks = 1;
    }
    else{
        struct Entry* entry = d7fs_get_entry(path);
        if(entry==NULL)
            return -ENOENT;
        if(entry->is_dir){
            statbuf->st_mode=S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
            statbuf->st_uid = getuid();
            statbuf->st_gid = getgid();
            statbuf->st_nlink = 1;
            statbuf->st_ino = 0;
            statbuf->st_size = BLOCK_SIZE;
            statbuf->st_blksize = BLOCK_SIZE;
            statbuf->st_blocks = 1;
        }
        else{
            int size, blocks;
            d7fs_get_file_metadata(entry, &size, &blocks);
            statbuf->st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;
            statbuf->st_nlink = 1;
            statbuf->st_ino = 0;//dentry - root_dir
            statbuf->st_uid = getuid();
            statbuf->st_gid = getgid();
            statbuf->st_size = size; 
            statbuf->st_blksize = BLOCK_SIZE;
            statbuf->st_blocks = blocks;
        }
    }
    return 0;
}

struct Entry * d7fs_get_entry(const char *name){
    //printf("d7fs_get_entry(path=%s)\n", name);
    struct Directory * dir = &root;
    return d7fs_get_entry_r(&name[1], dir);
}

struct Entry * d7fs_get_entry_r(const char *name, struct Directory* dir){
    //printf("d7fs_get_entry_r(path=%s)\n", name);
    int i, is_current_dir = 1;

    for(i = 0; i < MAX_FILE_NAME && name[i] != 0; i++){
        if(name[i] == '/'){
            is_current_dir = 0;
            break;
        }
    }
    if(is_current_dir)
        return d7fs_lookup_entry(name, dir);
    char dirname[MAX_FILE_NAME];
    memset(dirname, 0, MAX_FILE_NAME);
    memcpy(dirname, name, i);
    struct Entry * direntry = d7fs_lookup_entry(dirname, dir);
    struct Directory * new_dir = d7fs_load_dir(direntry->index_block);
    return d7fs_get_entry_r(&name[i+1], new_dir);
}

struct Entry * d7fs_lookup_entry(const char *name, struct Directory* dir){
    //printf("d7fs_lookup_entry(path=%s)\n", name);
    for(int i = 0; i < MAX_DIRECTORY_ENTRIES; i++){
        if(dir->entries[i].index_block != 0){
            if(strcmp(name, dir->entries[i].name) == 0)
                return &(dir->entries[i]);
        }
    }
    printf("Entry %s was not found in directory\n", name);
    return NULL;
}

struct Entry * d7fs_get_dir_entry(const char * name, struct Directory* dir){
    printf("d7fs_get_dir_entry(path=%s)\n", name);
    int i;
    int path_len = strlen(name);
    int is_last_dir = 1;

    for(i = 0; i < path_len - 1; i++){
        if(name[i] == '/'){
            is_last_dir = 0;
            break;
        }
    }

    if(is_last_dir){
        char dirname[MAX_FILE_NAME];
        memset(dirname, 0, MAX_FILE_NAME);
        memcpy(dirname, name, path_len);
        return d7fs_lookup_entry(dirname, dir);
    }
    char dirname[MAX_FILE_NAME];
    memset(dirname, 0, MAX_FILE_NAME);
    memcpy(dirname, name, i);
    struct Entry * direntry = d7fs_lookup_entry(dirname, dir);
    struct Directory * new_dir = d7fs_load_dir(direntry->index_block);
    return d7fs_get_entry_r(&name[i+1], new_dir);
}

struct Directory * d7fs_load_dir(int i_block){
    if(current_dir_block == i_block)
        return &current_dir;
    printf("d7fs_load_dir(block=%d)\n", i_block);
    unsigned char * char_dir = (unsigned char*)malloc(BLOCK_SIZE);
    device_read_block(char_dir, i_block);
    memset(&current_dir, 0, sizeof(current_dir));
    memcpy(&current_dir, char_dir, sizeof(current_dir));
    free(char_dir);
    current_dir_block = i_block;
    return &current_dir;
}

void d7fs_get_file_metadata(struct Entry* entry, int *size, int *blocks){
    printf("d7fs_get_metadata(file=%s)\n", entry->name);

    *blocks = 0;
    *size = 0;

    if(entry->is_dir){
        *blocks = 1;
        return;
    }
    
    int index_block_l1[BLOCK_SIZE/sizeof(int)];
    int index_block_l2[BLOCK_SIZE/sizeof(int)];

    device_read_block((unsigned char*) index_block_l1, entry->index_block);

    int last_data_block = -1;

    for(int i = 0; index_block_l1[i] != 0 && i < MAX_BLOCKS_PER_INDEX_BLOCK; i++){
        device_read_block((unsigned char*) index_block_l2, index_block_l1[i]);
        for(int j = 0; index_block_l2[j] != 0 && i < MAX_BLOCKS_PER_INDEX_BLOCK; j++){
            (*blocks)++;
            last_data_block = index_block_l2[j];
        }
    }

    for(int i = 0; i < (*blocks) - 1; i++)
        *size = (*size) + BLOCK_SIZE;

    if(blocks != 0){
        char * data = (char*)malloc(BLOCK_SIZE);
        device_read_block((unsigned char*)data, last_data_block);
        int length = BLOCK_SIZE - 1;
        for(; length > 0; length--){
            if(data[length] != 0){
                length++;
                break;
            }
        }
        *size = (*size) + length;
        free(data);
    }
}


int d7fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    //printf("d7fs_readdir(path=%s)\n", path); //needs implementation of directories
    struct Directory * dir = NULL;
    int len = strlen(path);
    if(len == 1 && path[0] == '/'){
        dir = &root;
    }
    else{
        char dirname[MAX_FILE_NAME];
        memset(dirname, 0, MAX_FILE_NAME);
        memcpy(dirname, &path[1], len - 1);
        struct Entry * direntry = d7fs_get_dir_entry(dirname, &root);
        if(direntry == NULL){
            printf("ERROR FATAL\n");
            return -ENOSPC;
        }
        dir = d7fs_load_dir(direntry->index_block);
    }
    if(dir == NULL){
        printf("Error in readdir. Directory not found\n");
        return -ENOENT;
    }
    for(int i = 0; i<MAX_DIRECTORY_ENTRIES; i++){
        if(dir->entries[i].index_block!=0){
            //printf("Reading dir %s\n", dir->entries[i].name);
            if(filler(buffer, dir->entries[i].name, NULL, 0)!=0)
                return -ENOMEM;
        }
    }
    return 0;
}

int d7fs_mknod(const char *path, mode_t mode, dev_t dev){
    printf("d7fs_mknod(path=%s)\n", path);
    if(S_ISREG(mode)){
        struct Entry* entry = d7fs_get_entry(path);
        if(entry!=NULL){
            printf("Ya existe el entry\n");
            return -EEXIST;
        }

        struct Directory * dir = NULL;
        int dir_block;
        int len;
        for(len = strlen(path) - 1; len >= 1 && path[len] != '/'; len--);
        if(len == 0){
            dir = &root;
            dir_block = metadata.root_ptr;
        }
        else{
            char dirname[MAX_FILE_NAME];
            memset(dirname, 0, MAX_FILE_NAME);
            memcpy(dirname, &path[1], len-1);
            struct Entry * direntry = d7fs_get_dir_entry(dirname, &root);
            if(direntry == NULL){
                printf("ERROR FATAL\n");
                return -ENOSPC;
            }
            dir = d7fs_load_dir(direntry->index_block);
            dir_block = direntry->index_block;
        }

        int j = 0;
        while(j<MAX_DIRECTORY_ENTRIES){    
            if(dir->entries[j].index_block==0)
                break;
            j++;
        }

        int free_block = d7fs_get_free_block(bitmap, metadata.bitmap_size);

        if(j>=MAX_DIRECTORY_ENTRIES || free_block==-1){
            printf("No more available space\n");
            return -ENOSPC;
        }

        if(strlen(&path[len+1])>=MAX_FILE_NAME)
            return -ENAMETOOLONG;


        struct timeval now;
        gettimeofday(&now, NULL);

        long created_at = now.tv_sec * 1000;

        strcpy(dir->entries[j].name, &path[len+1]);
        dir->entries[j].is_dir = 0;
        dir->entries[j].size = 0;
        dir->entries[j].created_at = created_at;
        dir->entries[j].last_modified = created_at;
        dir->entries[j].index_block = free_block;//assign new block
        d7fs_set_bit(bitmap, free_block, 0);
        d7fs_update_map();

        int index_block[MAX_BLOCKS_PER_INDEX_BLOCK];

        for(int i = 0; i < MAX_BLOCKS_PER_INDEX_BLOCK; i++)
            index_block[i] = 0;

        device_write_block((unsigned char*)index_block, dir->entries[j].index_block);

        unsigned char *char_dir = (unsigned char*)malloc(sizeof(*dir));
        memcpy(char_dir, dir, sizeof(*dir));
        device_write_block(char_dir, dir_block);
        free(char_dir);

        return 0;
    }
    return -EPERM;
}

int d7fs_get_free_block(char * _bitmap, int _bitmap_size){
    int i;
    for(i = 0; i < _bitmap_size*BLOCK_SIZE && _bitmap[i] == 0; i++);
    int j, k;
    for(j = 0, k = BITS_IN_BYTE; !(((_bitmap[i])>>(j))&1) && j < BITS_IN_BYTE; j++, k++);
    return !(((_bitmap[i])>>(j))&1) ? -1 : BITS_IN_BYTE*i + j;
}

void d7fs_set_bit(char * _bitmap, int n, int value){
    if(value)
        _bitmap[n/BITS_IN_BYTE] = _bitmap[n/BITS_IN_BYTE] | (1<<(n%BITS_IN_BYTE));
    else
        _bitmap[n/BITS_IN_BYTE] = _bitmap[n/BITS_IN_BYTE] & ~(1<<(n%BITS_IN_BYTE));
}

int d7fs_open(const char *path, struct fuse_file_info *fileInfo){
    printf("open(path=%s)\n", path);

    struct Entry * de = d7fs_get_entry(&path[0]);

    if(de == NULL)
        return -ENOENT;

    printf("Opening %s ......\n", de->name);

    fileInfo->fh = (uint64_t)de;
    return 0;
}

int d7fs_utime(const char *path, struct utimbuf *ubuf) {
    printf("utime(path=%s)\n", path);
    //return -EPERM;
    return 0;
}

int d7fs_flush(const char *path, struct fuse_file_info *fileInfo){
    printf("flush(path=%s)\n", path);
    return 0;
}

int d7fs_release(const char *path, struct fuse_file_info *fileInfo){
    //printf("release(path=%s)\n", path);
    fileInfo->fh = 0;
    return 0;
}

int d7fs_releasedir(const char *path, struct fuse_file_info *fileInfo) {
    //int path_len = strlen(path);
    //printf("%s: %s\n", __FUNCTION__, path);
    return 0;
}

int d7fs_rename(const char *path, const char *newpath){
    printf("rename(path=%s, newPath=%s)\n", path, newpath);
    struct Directory * dir = NULL;
    int dir_block;
    int len;
    for(len = strlen(path) - 1; len >= 1 && path[len] != '/'; len--);
    if(len == 0){
        dir = &root;
        dir_block = metadata.root_ptr;
    }
    else{
        char dirname[MAX_FILE_NAME];
        memset(dirname, 0, MAX_FILE_NAME);
        memcpy(dirname, &path[1], len-1);
        struct Entry * direntry = d7fs_get_dir_entry(dirname, &root);
        if(direntry == NULL){
            printf("ERROR FATAL\n");
            return -ENOENT;
        }
        dir = d7fs_load_dir(direntry->index_block);
        dir_block = direntry->index_block;
    }
    struct Entry* entry = d7fs_lookup_entry(&path[len+1], dir);
    if(entry==NULL){
        printf("Error in rename. File not found\n");
        return -ENOENT;
    }
    //Renaming file
    memset(entry->name, 0, MAX_FILE_NAME);
    strcpy(entry->name, &newpath[len+1]);

    unsigned char *char_dir = (unsigned char*)malloc(sizeof(*dir));
    memcpy(char_dir, dir, sizeof(*dir));
    device_write_block(char_dir, dir_block);
    free(char_dir);
    return 0;
}

int d7fs_unlink(const char *path){
    printf("d7fs_unlink(path=%s)\n", path);

    struct Directory * dir = NULL;
    int dir_block;
    int len;
    for(len = strlen(path) - 1; len >= 1 && path[len] != '/'; len--);
    if(len == 0){
        dir = &root;
        dir_block = metadata.root_ptr;
    }
    else{
        char dirname[MAX_FILE_NAME];
        memset(dirname, 0, MAX_FILE_NAME);
        memcpy(dirname, &path[1], len-1);
        struct Entry * direntry = d7fs_get_dir_entry(dirname, &root);
        if(direntry == NULL){
            printf("ERROR FATAL\n");
            return -ENOENT;
        }
        dir = d7fs_load_dir(direntry->index_block);
        dir_block = direntry->index_block;
    }
    struct Entry* entry = d7fs_lookup_entry(&path[len+1], dir);
    if(entry==NULL){
        printf("Error in unlink. File not found\n");
        return -ENOENT;
    }
    //Setting index block to 0
    d7fs_delete_entry(entry);

    unsigned char *char_dir = (unsigned char*)malloc(sizeof(*dir));
    memcpy(char_dir, dir, sizeof(*dir));
    device_write_block(char_dir, dir_block);
    free(char_dir);
    return 0;
}

int d7fs_mkdir(const char *path, mode_t mode){
    printf("d7fs_mkdir(path=%s)\n", path);
    struct Entry* entry = d7fs_get_entry(path);
    if(entry!=NULL){
        printf("Ya existe el entry\n");
        return -EEXIST;
    }

    struct Directory * dir = NULL;
    int dir_block;
    int len;
    for(len = strlen(path) - 1; len >= 1 && path[len] != '/'; len--);
    if(len == 0){
        dir = &root;
        dir_block = metadata.root_ptr;
    }
    else{
        char dirname[MAX_FILE_NAME];
        memset(dirname, 0, MAX_FILE_NAME);
        memcpy(dirname, &path[1], len-1);
        struct Entry * direntry = d7fs_get_dir_entry(dirname, &root);
        if(direntry == NULL){
            printf("ERROR FATAL\n");
            return -ENOSPC;
        }
        dir = d7fs_load_dir(direntry->index_block);
        dir_block = direntry->index_block;
    }
    int j = 0;
    while(j<MAX_DIRECTORY_ENTRIES){    
        if(dir->entries[j].index_block==0)
            break;
        j++;
    }

    int free_block = d7fs_get_free_block(bitmap, metadata.bitmap_size);

    if(j>=MAX_DIRECTORY_ENTRIES || free_block==-1){
        printf("No more available space\n");
        return -ENOSPC;
    }

    if(strlen(&path[len+1])>=MAX_FILE_NAME)
        return -ENAMETOOLONG;


    struct timeval now;
    gettimeofday(&now, NULL);

    long created_at = now.tv_sec * 1000;

    strcpy(dir->entries[j].name, &path[len+1]);
    dir->entries[j].is_dir = 1;
    dir->entries[j].size = 0;
    dir->entries[j].created_at = created_at;
    dir->entries[j].last_modified = created_at;
    dir->entries[j].index_block = free_block;//assign new block
    d7fs_set_bit(bitmap, free_block, 0);
    d7fs_update_map();

    unsigned char *char_dir = (unsigned char*)malloc(sizeof(*dir));
    memcpy(char_dir, dir, sizeof(*dir));
    device_write_block(char_dir, dir_block);
    free(char_dir);

    unsigned char *char_new_dir = (unsigned char*)malloc(BLOCK_SIZE);
    memset(char_new_dir, 0, BLOCK_SIZE);
    device_write_block(char_new_dir, free_block);
    free(char_new_dir);

    return 0;
}

int d7fs_rmdir(const char *path){
    printf("d7fs_rmdir(path=%s)\n", path);
    struct Directory * dir = NULL;
    int len;
    for(len = strlen(path) - 1; len >= 1 && path[len] != '/'; len--);
    if(len == 0){
        dir = &root;
    }
    else{
        char dirname[MAX_FILE_NAME];
        memset(dirname, 0, MAX_FILE_NAME);
        memcpy(dirname, &path[1], len-1);
        struct Entry * direntry = d7fs_get_dir_entry(dirname, &root);
        if(direntry == NULL){
            printf("ERROR FATAL\n");
            return -ENOENT;
        }
        dir = d7fs_load_dir(direntry->index_block);
    }
    struct Entry* entry = d7fs_lookup_entry(&path[len+1], dir);
    d7fs_delete_entry(entry);
    return 0;
}

void d7fs_delete_entry(struct Entry* entry){
    if(entry->is_dir){
        struct Directory * dir = d7fs_load_dir(entry->index_block);
        for(int i = 0; i<MAX_DIRECTORY_ENTRIES; i++){
            if(dir->entries[i].index_block!=0)
                d7fs_delete_entry(&(dir->entries[i]));
        }
    }
    else{
        int index_block_l1[BLOCK_SIZE/sizeof(int)];
        int index_block_l2[BLOCK_SIZE/sizeof(int)];

        device_read_block((unsigned char*) index_block_l1, entry->index_block);

        for(int i = 0; index_block_l1[i] != 0 && i < MAX_BLOCKS_PER_INDEX_BLOCK; i++){
            device_read_block((unsigned char*) index_block_l2, index_block_l1[i]);
            for(int j = 0; index_block_l2[j] != 0 && i < MAX_BLOCKS_PER_INDEX_BLOCK; j++){
                int ib2 = entry->index_block;
                entry->index_block = 0;
                d7fs_set_bit(bitmap, ib2, 1);
            }
            int ib1 = entry->index_block;
            entry->index_block = 0;
            d7fs_set_bit(bitmap, ib1, 1);
        }
    }
    int iblock = entry->index_block;
    entry->index_block = 0;
    d7fs_set_bit(bitmap, iblock, 1);
    d7fs_update_map();
}

int d7fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo){
    printf("d7fs_write(path=%s)\n", path);
    struct Entry * entry = d7fs_get_entry(path);
   
    if(entry==NULL){
        return -ENOENT;
    }

    int index_block_l1[BLOCK_SIZE/sizeof(int)];
    int index_block_l2[BLOCK_SIZE/sizeof(int)];

    device_read_block((unsigned char*) index_block_l1, entry->index_block);

    int start_block = floor(((double)offset)/BLOCK_SIZE);
    int blocks_to_write = ceil(((double)size)/BLOCK_SIZE);

    int x;
    for(x=0; x < blocks_to_write; x++){
        int i = (start_block+x)/MAX_BLOCKS_PER_INDEX_BLOCK;
        int j = (start_block+x)%MAX_BLOCKS_PER_INDEX_BLOCK;
        int wblock = (i*MAX_BLOCKS_PER_INDEX_BLOCK) + j;
        if(wblock>=MAX_BLOCKS_PER_FILE){
            return -EFBIG;
        }
        if(index_block_l1[i] == 0){
            int new_block = d7fs_get_free_block(bitmap, metadata.bitmap_size);
            if(new_block==-1){
                return -ENOSPC;
            }
            d7fs_set_bit(bitmap, new_block, 0);
            d7fs_update_map();
            index_block_l1[i] = new_block;
            device_write_block((unsigned char*) index_block_l1, entry->index_block);
        }
        device_read_block((unsigned char*) index_block_l2, index_block_l1[i]);

        if(index_block_l2[j] != 0){
            unsigned char *block_info=(unsigned char*)malloc(BLOCK_SIZE);
            memset(block_info, 0, BLOCK_SIZE);
            
            device_read_block(block_info, index_block_l2[j]);

            long my_offset=(long)offset;
            if(my_offset>=(BLOCK_SIZE*(wblock)))
                my_offset-=(BLOCK_SIZE*(wblock));

            memset(&block_info[my_offset], 0, (int)size);
            memcpy(&block_info[my_offset], buf, (int)size+1);
            device_write_block(block_info,  index_block_l2[j]);

            free(block_info);
        }
        else{
            int new_block = d7fs_get_free_block(bitmap, metadata.bitmap_size);

            if(new_block==-1)
                return -ENOSPC;

            d7fs_set_bit(bitmap, new_block, 0);
            d7fs_update_map();

            index_block_l2[j] = new_block;

            device_write_block((unsigned char*) index_block_l2, index_block_l1[i]);

            unsigned char *block_info = (unsigned char *)malloc(BLOCK_SIZE);
            memset(block_info, 0, BLOCK_SIZE);

            memcpy(block_info, buf, size);
            device_write_block(block_info,  new_block);

            free(block_info);
        }
    }
    return size;
}

int d7fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo){
    printf("d7fs_read(path=%s)\n", path);

    struct Entry * entry = d7fs_get_entry(path);

    if(entry == NULL){
        return -ENOENT;
    }

    int index_block_l1[BLOCK_SIZE/sizeof(int)];
    int index_block_l2[BLOCK_SIZE/sizeof(int)];

    device_read_block((unsigned char*) index_block_l1, entry->index_block);

    int start_block = floor(((double)offset)/BLOCK_SIZE);
    int blocks_to_read = ceil(((double)size)/BLOCK_SIZE);

    unsigned char * info = (unsigned char*)malloc((int)size);
    memset(info, 0, (int)size);

    int my_offset = 0;

    int x;
    for(x=0; x<blocks_to_read; x++){
        int i = (start_block+x)/MAX_BLOCKS_PER_INDEX_BLOCK;
        int j = (start_block+x)%MAX_BLOCKS_PER_INDEX_BLOCK;
        int rblock = (i*MAX_BLOCKS_PER_INDEX_BLOCK) + j;
        
        if(rblock >= MAX_BLOCKS_PER_FILE)
            return 0;

        if(index_block_l1[i] == 0)
            break;
        device_read_block((unsigned char*) index_block_l2, index_block_l1[i]);

        if(index_block_l2[j] != 0){
            unsigned char *block_info = (unsigned char*)malloc(BLOCK_SIZE);
            device_read_block(block_info, index_block_l2[j]);

            memcpy(&info[my_offset], block_info, BLOCK_SIZE);

            my_offset+=BLOCK_SIZE;

            free(block_info);
        }
        else if(x == 0) return 0;
        else break;
    }
    memcpy(buf, info, size);
    free(info);
    return size;
}

int d7fs_truncate(const char *path, off_t newSize){
    printf("d7fs_truncate(path=%s)\n", path);
    return 0;
}