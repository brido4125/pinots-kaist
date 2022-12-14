#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
#include <list.h>


struct bitmap;

void inode_init (void);
bool inode_create (disk_sector_t, off_t);
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
