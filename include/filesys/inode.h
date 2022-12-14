#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
#include "filesys/file.h"
#include "fat.h"
#include "include/lib/kernel/list.h"
#include "include/filesys/off_t.h"

struct bitmap;

struct inode_disk {
	disk_sector_t start;                /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	bool isdir;		// 디렉토리 구분 변수
	uint32_t unused[125];               /* Not used. */
	uint32_t is_link;
	char link_name[492];
};

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};



void inode_init (void);
bool inode_create (disk_sector_t, off_t, bool);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
cluster_t sector_to_cluster(disk_sector_t sector);
//add
bool link_inode_create (disk_sector_t sector, char* path_name);
struct cluster_t *sys_inumber(int fd);
struct dir* parse_path(char *path_name, char *file_name);
bool filesys_create_dir(const char* name);
bool sys_mkdir(const char *dir);
bool sys_chdir(const char *path_name);
bool inode_is_dir(const struct inode* inode);

#endif /* filesys/inode.h */
