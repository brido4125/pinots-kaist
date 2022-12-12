#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "lib/string.h"
#include "threads/palloc.h"
#include "vm/vm.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int exec (const char *cmd_line);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
struct file* find_file(int fd);
int fork (const char *thread_name,struct intr_frame* if_);
int wait (int pid);
void close (int fd);
void seek (int fd, unsigned position);
unsigned tell (int fd);
int add_file(struct file *file);
int dup2(int oldfd, int newfd);
void remove_file(int fd);
void * mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write);
struct page* check_address(void *addr);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

static struct lock lock;


void
syscall_init (void) {
	lock_init(&lock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t number = f->R.rax;
	thread_current()->rsp_stack = f->rsp;
	switch (number)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi,f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 1);
		f->R.rax = read(f->R.rdi,f->R.rsi,f->R.rdx);
		break;
	case SYS_WRITE:
		check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 0);
		f->R.rax = write(f->R.rdi,f->R.rsi,f->R.rdx);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi,f);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_SEEK:
		seek(f->R.rdi,f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_DUP2:	// project2 - extra
		f->R.rax = dup2(f->R.rdi, f->R.rsi);
		break;
	case SYS_MMAP:
	    f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	default:
		thread_exit();
		break;
	}
}

int dup2(int oldfd, int newfd) {
	// 기존의 파일 디스크립터 oldfd를 새로운 newfd로 복제하여 생성하는 함수.
	// newfd가 이전에 열렸다면, 재사용하기 전에 자동으로 닫힌다.
	// oldfd가 불분명하면 이 시스템 콜은 실패하며 -1을 리턴, newfd는 닫히지 않는다.
	// oldfd가 명확하고 newfd가 oldfd와 같은 값을 가진다면, dup2() 함수는 실행되지 않고 newfd값을 그대로 반환
	struct file *file = find_file(oldfd);
	if (file == NULL){
		return -1;
	}
	if (oldfd == newfd){
		return newfd;
	}

	struct thread *curr = thread_current();
	struct file **curr_fd_table = curr->fd_table;
	if (file == STDIN){
		curr->stdin_count++;
	}else if(file == STDOUT){
		curr->stdout_count++;
	}else{
		file->dup_count++;
	}

	close(newfd);
	curr_fd_table[newfd] = file;
	return newfd;
}

/* Project2-2 User Memory Access */
// void check_address(void* addr){
// 	struct thread* curr = thread_current();
// 	if(!is_user_vaddr(addr) || addr == NULL || pml4_get_page(curr->pml4,addr) == NULL){
// 		exit(-1);
// 	}
// }
/* Project3  */
struct page * check_address(void * addr) {
	if (addr == NULL || is_kernel_vaddr(addr)) {
		exit(-1);
	}

	return spt_find_page(&thread_current()->spt, addr);
}

/* Project2-3 System Call */
void halt(void){
	power_off();
}

/* Project2-3 System Call */
void exit (int status){
	/* status가 1로 넘어온 경우는 정상 종료 */
	struct thread* curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n",curr->name, status);
	thread_exit();
}

/* Project2-3 System Call */
bool create(const char *file, unsigned initial_size){
	// if(!file){
	// 	exit(-1);
	// }
	check_address(file);
	return filesys_create(file,initial_size);
}

/* Project2-3 System Call */
bool remove(const char *file){
	check_address(file);
	if(filesys_remove(file)){
		return true;
	}
	return false;
}

/* Project2-3 System Call */
int exec (const char *cmd_line){
	check_address(cmd_line);
	char* copy = palloc_get_page(PAL_ZERO);
	if(copy == NULL){
		exit(-1);
	}
	strlcpy(copy,cmd_line,strlen(cmd_line) + 1);
	struct thread* curr = thread_current();
	if (process_exec(copy) == -1){
		return -1;
	}
	NOT_REACHED();
	return 0;
}

/* Project2-3 System Call */
int open (const char *file){
	
	check_address(file);
	// if(file ==NULL){
	// 	return -1;
	// }

	lock_acquire(&lock);
	struct file *fileobj = filesys_open(file);

	if (fileobj == NULL) {
		return -1;
	}

	int fd = add_file(fileobj); // fdt : file data table

	// fd table이 가득 찼다면
	if (fd == -1) {
		file_close(fileobj);
	}
	lock_release(&lock);
	return fd;
}

int add_file(struct file *file){
	struct thread *cur = thread_current();
	struct file **fdt = cur->fd_table;

	// int idx = cur->fd_idx;
	while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx]) {
		cur->fd_idx++;
	}

	// fdt가 가득 찼다면
	if (cur->fd_idx >= FDCOUNT_LIMIT)
		return -1;

	fdt[cur->fd_idx] = file;
	return cur->fd_idx;
}

/* Project2-3 System Call */
int filesize (int fd){
	if(fd < 0 || fd >= FDCOUNT_LIMIT){
		return -1;
	}
	struct file *file = find_file(fd);
	return file_length(file);
}

/* Project2-3 System Call */
int read (int fd, void *buffer, unsigned size){
	check_address(buffer);
	off_t char_count = 0;
	struct thread *cur = thread_current();
	struct file *file = find_file(fd);

	if (fd == NULL){
		return -1;
	}

	if (file == NULL || file == STDOUT){
		return -1;
	}

	/* Keyboard 입력 처리 */
	if(file == STDIN){
		if (cur->stdin_count == 0){
			// 더이상 열려있는 stdin fd가 없다.
			NOT_REACHED();
			remove_file(fd);
			return -1;
		}
		while (char_count < size)
		{
			char key = input_getc();
			*(char*)buffer = key;
			char_count++;
			(char*)buffer++;
			if (key == '\0'){
				break;
			}
		}
		
	}
	else{
		lock_acquire(&lock);
		char_count = file_read(file,buffer,size);
		lock_release(&lock);
	}
	return char_count;
}

/* Project2-3 System Call */
int write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	off_t write_size = 0;
	struct thread *cur = thread_current();
	struct file *file = find_file(fd);

	if (fd == NULL){
		return -1;
	}

	if (file == NULL || file == STDIN){
		return -1;
	}


    if (file == STDOUT) {
		if (cur->stdout_count == 0){
			remove_file(fd);
			return -1;
		}
		putbuf(buffer, size);
		return size;
  	}else{
		lock_acquire(&lock);
		write_size = file_write(file,buffer,size);
		lock_release(&lock);
	} 
	return write_size;
}

/* Project2-3 System Call */
struct file* find_file(int fd){
	struct thread* curr = thread_current();
	if(fd < 0 || fd >= FDCOUNT_LIMIT){
		return NULL;
	}
	return curr->fd_table[fd];
}

/* Project2-3 System Call */
int fork (const char *thread_name,struct intr_frame* if_){
	check_address(thread_name);
	return process_fork(thread_name,if_);
}

/* Project2-3 System Call */
int wait (int pid){
	return process_wait(pid);
}

/* Project2-3 System Call */
void close (int fd){
	struct file* close_file = find_file(fd);
	if (close_file == NULL) {
		return;
	}

	struct thread *curr = thread_current();

	if(fd==0 || close_file==STDIN)
		curr->stdin_count--;
	else if(fd==1 || close_file==STDOUT)
		curr->stdout_count--;

	remove_file(fd);


	if(fd < 2 || close_file <= 2){
		return;
	}

	if(close_file->dup_count == 0){
		file_close(close_file);
	}
	else{
		close_file->dup_count--;

	}
}

void remove_file(int fd)
{
	struct thread *cur = thread_current();
	
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	cur->fd_table[fd] = NULL;
}


/* Project2-3 System Call */
void seek (int fd, unsigned position){
	struct file* file = find_file(fd);
	if (file <= 2) {
		return;
	}
	file_seek(file,position);
}

unsigned tell (int fd){
	struct file* file = find_file(fd);
	if (file <= 2) {
		return;
	}
	return file_tell(file);
}

// addr = start
// mmap()이 파일에 가상 페이지 매핑을 해줘도 적합한지를 체크해주는 함수
void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {

	// 파일의 시작점(offset)이 page-align되지 않았을 때
	if(offset % PGSIZE != 0){
		return NULL;
	}
	// 가상 유저 page 시작 주소가 page-align되어있지 않을 때
	/* failure case 2: 해당 주소의 시작점이 page-align되어 있는지 & user 영역인지 & 주소값이 null인지 & length가 0이하인지*/
	if(pg_round_down(addr)!= addr || is_kernel_vaddr(addr) || addr == NULL || (long long)length <= 0){
		return NULL;
	}
	// 매핑하려는 페이지가 이미 존재하는 페이지와 겹칠 때(==SPT에 존재하는 페이지일 때)
	
	if(spt_find_page(&thread_current()->spt,addr)){
		return NULL;
	}
	
	// 콘솔 입출력과 연관된 파일 디스크립터 값(0: STDIN, 1:STDOUT)일 때
	if(fd == 0 || fd == 1){
		exit(-1);
	}
	// 찾는 파일이 디스크에 없는경우
	struct file * target = find_file(fd);
	if (target==NULL){
		return NULL;
	}

	return do_mmap(addr, length, writable, target, offset);
}

//  유저 가상 페이지의 변경 사항을 디스크 파일에 업데이트한 뒤, 매핑 정보를 지운다. 여기서 중요한 점은 페이지를 지우는 게 아니라 present bit을 0으로 만들어준다는 점이다. 따라서 munmap() 함수는 정확히는 지정된 주소 범위 addr에 대한 매핑을 해제하는 함수

// Dirty bit은 해당 페이지가 변경되었는지 여부를 저장하는 비트이다. 페이지가 변경될 때마다 이 비트는 1이 되고, 디스크에 변경 내용을 기록하고 나면 해당 페이지의 dirty bit는 다시 0으로 초기화해야 한다. 즉, 변경하는 동안에 dirty bit가 1이 된다.

// Present Bit
// 해당 페이지가 물리 메모리에 매핑되어 있는지 아니면 swap out되었는지를 가리킨다. Swap in/out에서 더 다룰 것. present bit이 1이면 해당 페이지가 물리 메모리 어딘가에 매핑되어 있다는 말이며 0이면 디스크에 내려가(swap out)있다는 말이다. 이렇게 present bit이 0인, 물리 메모리에 존재하지 않는 페이지에 접근하는 과정이 file-backed page에서의 page fault이다.
void
munmap (void *addr) {
	do_munmap(addr);
}

//project 3 add
void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write){
	if (buffer <= USER_STACK && buffer >= rsp)
		return;
	
	for(int i=0; i<size; i++){
        struct page* page = check_address(buffer + i);
        if(page == NULL)
            exit(-1);
        if(to_write == true && page->writable == false)
            exit(-1);
    }
}