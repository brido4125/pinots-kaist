#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/fat.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define CLUSTER_SIZE DISK_SECTOR_SIZE * SECTORS_PER_CLUSTER

cluster_t sector_to_cluster(struct inode* inode);

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
	disk_sector_t start;                /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	uint32_t unused[125];               /* Not used. */
};

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t byte_to_sector (const struct inode *inode, off_t pos) {
	ASSERT (inode != NULL);
	#ifdef EFILESYS
		if (pos < inode->data.length)

			cluster_t cluster = sector_to_cluster(inode);
			for (size_t i = 0; i < pos / DISK_SECTOR_SIZE; i++){
				if(sector_to_cluster(i) == cluster){
					return cluster_to_sector(i);
				}
			}
		}else{
			return -1;
		}
	#else
		ASSERT (inode != NULL);
		if (pos < inode->data.length)
			return inode->data.start + pos / DISK_SECTOR_SIZE;
		else
			return -1;
	#endif
	
}

cluster_t sector_to_cluster(struct inode* inode){
	return inode->data.start - inode->sector * SECTORS_PER_CLUSTER;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
// create시에는 lengh = 무조건0?
bool 
inode_create (disk_sector_t sector, off_t length) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		#ifdef EFILESYS

			cluster_t clst = sector_to_cluster(sector);
			cluster_t newclst = clst; // 체이닝실패시 대비 저장

			if(sectors==0){
				disk_inode->start = cluster_to_sector(fat_create_chain(newclst));
			}
			// cluster_t start = fat_create_chain(0); // 추후확인  
			// 창섭보험
			for (int i = 0; i < sectors; 1++){
				newclst = fat_create_chain(newlist);
				if(newclst ==0){
					fat_remove_chain(clst,0);
					free(disk_inode);
					return false;
				}	
				if (i == 0){
					clst = newclst;
					disk_inode->start = cluster_to_sector(newclst); //파일의 시작 포인트
				}
			}

			disk_write (filesys_disk, sector, disk_inode);
				if (sectors > 0) {
					static char zeros[DISK_SECTOR_SIZE];
					size_t i;

					for (i = 0; i < sectors; i++) {
						disk_write (filesys_disk,cluster_to_sector(clst), zeros); // 연속적이지 않기 때문에 클러스터에 매칭되는 섹터를 찾아준다.
						clst=fat_get(clst);  // 다음 클러스터 == 섹터
				}
			}
				success = true; 
		#else
			if (free_map_allocate (sectors, &disk_inode->start)) {
				disk_write (filesys_disk, sector, disk_inode);
				if (sectors > 0) {
					static char zeros[DISK_SECTOR_SIZE];
					size_t i;

					for (i = 0; i < sectors; i++) 
						disk_write (filesys_disk, disk_inode->start + i, zeros); 
				}
				success = true; 
			} 
		#endif
		free (disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode; 
		}
	}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	disk_read (filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed) {
			free_map_release (inode->sector, 1);
			free_map_release (inode->data.start,
					bytes_to_sectors (inode->data.length)); 
		}

		free (inode); 
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;
	#ifdef ESYSFILE
			/* Sector to write, starting byte offset within sector. */
			disk_sector_t sector_idx = byte_to_sector (inode, offset);
			cluster_t current_cluster =  sector_to_cluster(sector_idx);
			int sector_ofs = offset % DISK_SECTOR_SIZE;

			/* Bytes left in inode, bytes left in sector, lesser of the two. */
			off_t inode_left = inode_length (inode) - offset;
			int sector_left = DISK_SECTOR_SIZE - sector_ofs;
			int min_left = inode_left < sector_left ? inode_left : sector_left;

			/* 쓰려는 공간이 남은 공간보다 더 큰 경우*/
			int left_size;//다른 클러스터에 저장
			if(size >= min_left){
				left_size = size - min_left;
			}
			int cluster_cnt;
			if(left_size > CLUSTER_SIZE){
				if(left_size % CLUSTER_SIZE == 0 ){
					cluster_cnt = left_size / CLUSTER_SIZE;
				}else{
					cluster_cnt = (left_size / CLUSTER_SIZE) + 1;
				}
			}else{
				cluster_cnt = 1;
			}

			/* Number of bytes to actually write into this sector. */
			int chunk_size = size < min_left ? size : min_left;
			if (chunk_size <= 0)
				break;
			//file의 처음부터 섹터 사이즈 만큼 write
			if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
				/* Write full sector directly to disk. */
				disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
			} else {
				/* We need a bounce buffer. */
				if (bounce == NULL) {
					bounce = malloc (DISK_SECTOR_SIZE);
					if (bounce == NULL)
						break;
				}
				/* If the sector contains data before or after the chunk
				we're writing, then we need to read in the sector
				first.  Otherwise we start with a sector of all zeros. */
				if (sector_ofs > 0 || chunk_size < sector_left) 
					disk_read (filesys_disk, sector_idx, bounce);
				else
					memset (bounce, 0, DISK_SECTOR_SIZE);
				memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
				disk_write (filesys_disk, sector_idx, bounce); 

				cluster_t start_cluster = sector_to_cluster(sector_idx);
				for (size_t i = 0; i < cluster_cnt; i++){
					cluster_t next = fat_create_chain(start_cluster);
					disk_sector_t next_sector = cluster_to_sector(next);
					disk_write (filesys_disk, next_sector, buffer + (chunk_size * (i + 1))); 
				}

			}
			/* Advance. */
			bytes_written += size;
	#else
		while (size > 0) {
			/* Sector to write, starting byte offset within sector. */
			disk_sector_t sector_idx = byte_to_sector (inode, offset);
			cluster_t current_cluster =  sector_to_cluster(sector_idx);
			int sector_ofs = offset % DISK_SECTOR_SIZE;

			/* Bytes left in inode, bytes left in sector, lesser of the two. */
			off_t inode_left = inode_length (inode) - offset;
			int sector_left = DISK_SECTOR_SIZE - sector_ofs;
			int min_left = inode_left < sector_left ? inode_left : sector_left;

			/* Number of bytes to actually write into this sector. */
			int chunk_size = size < min_left ? size : min_left;
			if (chunk_size <= 0)
				break;
			//file의 처음부터 섹터 사이즈 만큼 write
			if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
				/* Write full sector directly to disk. */
				disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
			} else {
				/* We need a bounce buffer. */
				if (bounce == NULL) {
					bounce = malloc (DISK_SECTOR_SIZE);
					if (bounce == NULL)
						break;
				}
				/* If the sector contains data before or after the chunk
				we're writing, then we need to read in the sector
				first.  Otherwise we start with a sector of all zeros. */
				if (sector_ofs > 0 || chunk_size < sector_left) 
					disk_read (filesys_disk, sector_idx, bounce);
				else
					memset (bounce, 0, DISK_SECTOR_SIZE);
				memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
				disk_write (filesys_disk, sector_idx, bounce); 
			}

			/* Advance. */
			size -= chunk_size;
			offset += chunk_size;
			bytes_written += chunk_size;
		}
	#endif
	free (bounce);

	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
	void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}
