#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

/* Formats the file system. */
static void do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();

    // '.' '..' 파일 추가
    if (!dir_create(ROOT_DIR_SECTOR, 16)) {
        PANIC("root directory creation failed");
    }
        
    struct dir* root_dir = dir_open_root();
    dir_add(root_dir, ".", ROOT_DIR_SECTOR);
    dir_add(root_dir, "..", ROOT_DIR_SECTOR);
    dir_close(root_dir);

	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();
	lock_init(&file_rw_lock);

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
	//루트 디렉토리설정
	thread_current()->cur_dir = dir_open_root(); 
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool filesys_create (const char *name, off_t initial_size) {
    bool success = false;
#ifdef EFILESYS

    // name의 파일경로를 cp_name에 복사
    char *cp_name = (char *)malloc(strlen(name) + 1);
    strlcpy(cp_name, name, strlen(name) + 1);

    // cp_name의 경로분석
    char *file_name = (char *)malloc(strlen(name) + 1);
    struct dir *dir = parse_path(cp_name, file_name);

    cluster_t inode_cluster = fat_create_chain(0);

    success = (dir != NULL
               // 파일의 inode를 생성하고 디렉토리에 추가한다
               && inode_create(inode_cluster, initial_size, 0)
               && dir_add(dir, file_name, inode_cluster));

    if (!success && inode_cluster != 0) {
        fat_remove_chain(inode_cluster, 0);
    }

    dir_close(dir);
    free(cp_name);
    free(file_name);
    return success;

#else
	disk_sector_t inode_sector = 0;
	struct dir *dir = dir_open_root ();
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size, 0)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	dir_close (dir);

	return success;

#endif
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *filesys_open (const char *name) {
    #ifdef EFILESYS

    // name의 파일경로를 cp_name에 복사
    char* cp_name = (char *)malloc(strlen(name) + 1);
    char* file_name = (char *)malloc(strlen(name) + 1);

    struct dir* dir = NULL;
    struct inode *inode = NULL;

    while(true) {
        strlcpy(cp_name, name, strlen(name) + 1);
        // cp_name의경로분석
        dir = parse_path(cp_name, file_name);

        if (dir != NULL) {
            dir_lookup(dir, file_name, &inode);
            if(inode && inode->data.is_link) {   // 파일이 존재하고, 링크 파일인 경우
                dir_close(dir);
                name = inode->data.link_name;
                continue;
            }
        }
        free(cp_name);
        free(file_name);
        dir_close(dir);
        break;
    }
    return file_open(inode);

    #else

	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode);
	dir_close (dir);

	return file_open (inode);

    #endif
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool filesys_remove (const char *name) {
    #ifdef EFILESYS

    // name의 파일경로를 cp_name에 복사
    char* cp_name = (char *)malloc(strlen(name) + 1);
    strlcpy(cp_name, name, strlen(name) + 1);

    // cp_name의 경로분석
    char* file_name = (char *)malloc(strlen(name) + 1);
    struct dir* dir = parse_path(cp_name, file_name);

    struct inode *inode = NULL;
    bool success = false;

    if (dir != NULL) {
        dir_lookup(dir, file_name, &inode);

        if(inode_is_dir(inode)) {   // 디렉토리인 경우
            struct dir* cur_dir = dir_open(inode);
            char* tmp = (char *)malloc(NAME_MAX + 1);
            dir_seek(cur_dir, 2 * sizeof(struct dir_entry));

            if(!dir_readdir(cur_dir, tmp)) {   // 디렉토리가 비었다
                // 현재 디렉토리가 아니면 지우게 한다
                if(inode_get_inumber(dir_get_inode(thread_current()->cur_dir)) != inode_get_inumber(dir_get_inode(cur_dir)))
                    success = dir_remove(dir, file_name);
            }

            else {   // 디렉토리가 비지 않았다.
                // 찾은 디렉토리에서 지운다
                success = dir_remove(cur_dir, file_name);
            }
            
            dir_close(cur_dir);
            free(tmp);
        }
        else {   // 파일인 경우
            inode_close(inode);
            success = dir_remove(dir, file_name);
        }
    }

    dir_close(dir);
    free(cp_name);
    free(file_name);

    return success;

    #else

	struct dir *dir = dir_open_root ();
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;

    #endif
