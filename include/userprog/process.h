#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/vm.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
bool install_page (void *upage, void *kpage, bool writable);
struct page* check2_address(void* addr);
bool lazy_load_segment (struct page *page, void *aux);
void check_address(void *addr);

/* Project3 - Anon Page */
/*해당 구조체는 디스크에 저장되어 있지 않기 때문에, 특정 파일을 불러 오는 것이 아님 */
// 우리는 현재 lazy loading 방식을 취하고 있고 이는 파일 전체를 다 읽어오지 않는다. 그때 그때 필요할 때만 읽어오는데, 그걸 위해서는 우리가 어떤 파일의 어떤 위치에서 읽어와야 할지 알아야 하고 그 정보가 container안에 들어가 있다
struct container{
    struct file *file;
    off_t offset;
    size_t read_bytes;
};

#endif /* userprog/process.h */
