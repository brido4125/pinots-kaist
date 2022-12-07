/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
// addr부터 시작하는 연속된 유저 가상 메모리 공간에 page들을 만들어 file의 offset부터 length에 해당하는 file의 정보를 각 page마다 저장한다. 프로세스가 이 page에 접근해서 page fault를 발생시키면 physical frame과 mapping하여 (claim) disk에서 file data를 frame에 복사함

실질적으로 가상 페이지를 할당해주는 함수이다. 인자로 받은 addr부터 시작하는 연속적인 유저 가상 메모리 공간에 페이지를 생성해 file의 offset부터 length에 해당하는 크기만큼 파일의 정보를 각 페이지마다 저장한다. 프로세스가 이 페이지에 접근해서 page fault가 뜨면 물리 프레임과 매핑(이때 claim을 사용한다)해 디스크에서 파일 데이터를 프레임에 복사한다.

이전에 다뤘던 load_segment()와 유사한 로직이다. 차이점은

1) load_segment()가 파일의 정보를 담은 uninit 타입 페이지를 만들 때 파일 타입을 VM_FILE로 선언해주는 것과

2) 매핑이 끝난 후 연속된 유저 페이지 나열의 첫 주소를 리턴한다는 점

이 있다.
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
		
		struct file * get_file = file_reopen(file);

		vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, file);

}

/* Do the munmap */
void
do_munmap (void *addr) {
}
