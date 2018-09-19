#include "d7fs.h"
#include <fuse.h>
#include <stdio.h>

struct fuse_operations d7fs_oper;

int main(int argc, char *argv[]) {
	int i, fuse_stat;

	d7fs_oper.getattr = d7fs_getattr;
	d7fs_oper.readlink = NULL; //d7fs_readlink;
	d7fs_oper.getdir = NULL;
	d7fs_oper.mknod = d7fs_mknod;
	d7fs_oper.mkdir = d7fs_mkdir;
	d7fs_oper.unlink = d7fs_unlink;
	d7fs_oper.rmdir = d7fs_rmdir;
	d7fs_oper.symlink = NULL; //d7fs_symlink;
	d7fs_oper.rename = d7fs_rename;
	d7fs_oper.link = NULL; //d7fs_link;
	d7fs_oper.chmod = NULL; //d7fs_chmod;
	d7fs_oper.chown = NULL; //d7fs_chown;
	d7fs_oper.truncate = d7fs_truncate;// might need
	d7fs_oper.utime = d7fs_utime;
	d7fs_oper.open = NULL; //d7fs_open;
	d7fs_oper.read = d7fs_read;
	d7fs_oper.write = d7fs_write;
	d7fs_oper.statfs = NULL;//d7fs_statfs;
	d7fs_oper.flush = d7fs_flush;
	d7fs_oper.release = d7fs_release;
	d7fs_oper.fsync = NULL;//d7fs_fsync; //might need
	d7fs_oper.opendir = d7fs_opendir;
	d7fs_oper.readdir = d7fs_readdir;
	d7fs_oper.releasedir = d7fs_releasedir;
	d7fs_oper.fsyncdir = NULL; //d7fs_fsyncdir;
	d7fs_oper.init = d7fs_init;

	for(i = 1; i < argc && (argv[i][0] == '-'); i++) {
		if(i == argc) {
			return (-1);
		}
	}

	if(strcmp(argv[1], "newdisk") == 0){
		if(argc <= 3){
			printf("Missing arguments\n");
			return -1;
		}
		device_new_disk(argv[2], atoi(argv[3]));
		return 0;
	}
	
	printf("mounting file system...\n");
	
	

	//realpath(...) returns the canonicalized absolute pathname
	if (!device_open(realpath(argv[i], NULL)) ) {
	    printf("Cannot open device file %s\n", argv[i]);
	    return 1;
	}

	for(; i < argc; i++) {
		argv[i] = argv[i+1];
	}
	argc--;

	fuse_stat = fuse_main(argc, argv, &d7fs_oper, NULL);

    device_close();
    
	printf("fuse_main returned %d\n", fuse_stat);

	return fuse_stat;
	
	//device_new_disk("disk.hex", 20);
}


