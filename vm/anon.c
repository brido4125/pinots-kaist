/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/string.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
// 이 기능에서는 스왑 디스크를 설정해야 합니다. 또한 스왑 디스크에서 사용 가능한 영역과 사용된 영역을 관리하기 위한 데이터 구조가 필요합니다. 스왑 영역도 PGSIZE(4096바이트) 단위로 관리됩니다.

void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);
	

	// (SECTORS_PER_PAGE = 8byte)  
	// SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE; 8 = 4096 / 512
	swap_size = disk_size(swap_disk)/SECTORS_PER_PAGE; 
	// 디스크 섹터는 하드 디스크 내 정보를 저장하는 단위로, 자체적으로 주소를 갖는 storage의 단위다. 즉, 한 디스크 당 몇 개의 섹터가 들어가는지를 나눈 값을 swap_size로 지칭한다. 즉, 해당 swap_disk를 swap할 때 필요한 섹터 수가 결국 swap_size.

	// swap size 크기만큼 swap_table을 비트맵으로 생성
    swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
// anon_initializer는 프로세스가 uninit page에 접근해서 page fault가 일어나면, page fault handler에 의해 호출되는 함수다.

bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	struct uninit_page* uninit_page = &page->uninit;
	memset(uninit_page,0,sizeof(struct uninit_page));
	/* Set up the handler */
	page->operations = &anon_ops; // operations를 anon-ops로 지정

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_sector = -1;
	
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
// anon_swap_out()은 anonymous page를 디스크 내 swap 공간으로 내리는 작업을 수행하는 함수이다.
// 비트맵을 순회해 false 값을 갖는(=해당 swap slot이 비어있다는 표시) 비트를 찾는다. 이어서 해당 섹터에 페이지 크기만큼 써줘야 하니 필요한 섹터 수 만큼 disk_write()을 통해 입력해준다. write 작업이 끝나면 해당 스왑 공간에 페이지가 채워졌으니 bitmap_set()으로 slot이 찼다고 표시해준다. 그리고 pml4_clear_page()로 물리 프레임에 올라와 있던 페이지를 지운다.
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	int empty_slot = bitmap_scan (swap_table, 0, 1, false);

	if ((empty_slot) == BITMAP_ERROR) {
        return false;
    }

    /* 
    한 페이지를 디스크에 써주기 위해 SECTORS_PER_PAGE 개의 섹터에 저장해야 한다.
    이때 디스크에 각 섹터 크기의 DISK_SECTOR_SIZE만큼 써준다.
	SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE; 8 = 4096 / 512
	swap_size = disk_size(swap_disk)/SECTORS_PER_PAGE; 
    */
   	for (int i = 0; i<SECTORS_PER_PAGE; i++){
		printf("empty_slot, %d\n",empty_slot );
		printf("empty_slot*SECTORS_PER_PAGE+i, %d\n",empty_slot*SECTORS_PER_PAGE+i );
		disk_write(swap_disk, empty_slot*SECTORS_PER_PAGE+i, page->va+DISK_SECTOR_SIZE*i);
	}


	// bitmap_set(struct bitmap *b, size_t idx, true);
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
