#ifndef _DEVICE_H_
#define _DEVICE_H_

#define BLOCK_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

void device_new_disk(const char *path, int size_in_mb);
int device_open(const char *path);
void device_close();
int device_read_block(unsigned char buffer[], int sector);
int device_write_block(unsigned char buffer[], int sector);
void device_flush();

#ifdef __cplusplus
}
#endif

#endif
