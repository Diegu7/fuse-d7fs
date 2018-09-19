#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "device.h"
#include "d7fs.h"

static const char *device_path;
static FILE *f;

void device_new_disk(const char *path, int size_in_mb){
    printf("%s\n", __FUNCTION__);

    if(size_in_mb < 2){
        printf("Size too small, defaulting to 20mb\n");
        size_in_mb = 20;
    }

    struct MetaBlock meta;
    meta.total_blocks = (size_in_mb * 1024 * 1024)/BLOCK_SIZE;
    meta.bitmap_size = (int)ceil((double)meta.total_blocks/(double)BLOCK_SIZE/(double)BITS_IN_BYTE);
    meta.bitmap_ptr = 1;
    meta.root_ptr = meta.bitmap_size + 1;
    meta.available_blocks = meta.total_blocks - (meta.root_ptr + 1);
    meta.disk_size_in_mb = size_in_mb;
    memset(meta.empty_space, 0, BLOCK_SIZE - METABLOCK_SIZE);

    device_path = path;
    
    f=fopen(path, "w+");
    if(f == NULL){
        perror("fopen");
        return;
    }

    unsigned char *char_meta=(unsigned char*)malloc(BLOCK_SIZE);
    memcpy(char_meta, &meta, BLOCK_SIZE);
    device_write_block(char_meta, 0);
    free(char_meta);
    
    unsigned char bitmap[BLOCK_SIZE * meta.bitmap_size];
    int i;
    for(i=0; i<BLOCK_SIZE * meta.bitmap_size; i++)
       bitmap[i]=0xFF;

    for(i = 0; i < meta.root_ptr + 1; i++){
        int free_block = d7fs_get_free_block((char*)bitmap, meta.bitmap_size);
        d7fs_set_bit((char*)bitmap, free_block, 0);
    }
    for(i = 0; i < meta.bitmap_size; i++)
        device_write_block(&bitmap[BLOCK_SIZE*i], i + meta.bitmap_ptr);

    unsigned char *fat=(unsigned char*)malloc(BLOCK_SIZE);
    memset(fat, 0, BLOCK_SIZE);
    device_write_block(&fat[0], meta.root_ptr);

    fseek(f, meta.total_blocks*BLOCK_SIZE, SEEK_SET);
    free(fat);
}

int device_open(const char *path){
    printf("%s\n", __FUNCTION__);
    device_path = path;
    f = fopen(path, "r+");
    return (f != NULL); 
}

void device_close(){
    printf("%s\n", __FUNCTION__);
    fflush(f);
    fclose(f);
}

int device_read_block(unsigned char buffer[], int block){
    fseek(f, block*BLOCK_SIZE, SEEK_SET);
    return ( fread(buffer, 1, BLOCK_SIZE, f) == BLOCK_SIZE );
}

int device_write_block(unsigned char buffer[], int block){
    printf("%s\n", __FUNCTION__);
    if(fseek(f, block*BLOCK_SIZE, SEEK_SET) != 0) printf("Error seeking ...\n");
    return ( fwrite(buffer, 1, BLOCK_SIZE, f) == BLOCK_SIZE );
}

void device_flush(){
    fflush(f);
}
