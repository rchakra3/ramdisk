all:
	gcc -Wall myfs.c myfs_helper.c `pkg-config fuse --cflags --libs` -o ramdisk
