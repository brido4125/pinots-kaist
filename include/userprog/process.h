#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
bool install_page (void *upage, void *kpage, bool writable);

/* Project3 - Anon Page */
/*해당 구조체는 디스크에 저장되어 있지 않기 때문에, 특정 파일을 불러 오는 것이 아님 */
struct container{
    struct file *file;
    off_t offset;
    size_t read_bytes;
};

#endif /* userprog/process.h */
