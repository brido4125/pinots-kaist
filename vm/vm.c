/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "include/userprog/process.h"
#include "threads/mmu.h"

uint64_t my_hash_function (const struct hash_elem *e, void *aux);
bool my_less_func (const struct hash_elem *a,const struct hash_elem *b,void *aux);
void hash_copy_func(struct hash_elem* elem, void *aux);
void spt_dealloc(struct hash_elem *e, void *aux);
bool vm_handle_wp(struct page *page);

struct list frame_table; // project3 vm_get_frame()
struct list_elem* clock_ref; // project3 vm_get_victim()
struct lock frame_table_lock;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */ 
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	clock_ref = list_begin(&frame_table);
	lock_init(&frame_table_lock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
//  전달된 vm_type에 따라 적절한 initializer를 가져와서 uninit_new를 호출하는 역할이다.
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		struct page* new_page = (struct page*)malloc(sizeof(struct page));

		// uninit.h 참조
		/* Initiate the struct page and maps the pa to the va */
		typedef bool (*page_initializer) (struct page *, enum vm_type, void *kva);
		page_initializer new_initializer = NULL;

		//anon.h 참조
		// initailizer의 정의를 위에서 정의
		// bool anon_initializer (struct page *page, enum vm_type type, void *kva); --> anon_initailizer로 표현 가능

		//file.h 참조
		// initailizer의 정의를 위에서 정의
		// bool file_backed_initializer (struct page *page, enum vm_type type, void *kva); --> file_backed_initializer로 표현가능

		// vm_type에 따라 다른 initializer호출
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			new_initializer = anon_initializer;
			break;
		case VM_FILE:
			new_initializer = file_backed_initializer;
			break;
		default:
			break;
		}
		uninit_new(new_page,upage,init,type,aux,new_initializer);

		new_page->writable = writable;

		return spt_insert_page(spt,new_page);
	}
	// vm_alloc_page_with_initializer는 무조건 uninit type의 page를 만든다. 그 후에 uninit_new에서 받아온 type으로 이 uninit type이 어떤 type으로 변할지와 같은 정보들을 page 구조체에 채워준다.
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// SPT 에서 va에 해당하는 struct page를 찾는다 실패시 NULL 반환
// 굳이 dummy page를 생성하는 이유는 dummy page를 생성하면 거기에 hash_elem이 있고, 이걸 통해서 우리가 받아온 va(offset을 지운)로 접근하기 위해서이다.
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va ) {
	// project3
	/* TODO: Fill this function. */
	struct page* page = (struct page*)malloc(sizeof(struct page));	// dummy page 생성
	page->va = pg_round_down(va); // va가 가리키는 가상 page의 시작 포인트 (offset이 0으로 설정된 va)반환
	struct hash_elem* target = hash_find(&spt->spt_hash,&page->hash_elem);
	// SPT에서 hash_elem과 같은 요소를 검색해서 발견하면 elem 반환 아니면 NULL 반환
	free(page);

	if(target == NULL){
		return NULL;
	}
	return hash_entry(target,struct page,hash_elem);
}

/* Insert PAGE into spt with validation. */
// spt에 va가 있는지 없는지 check
bool spt_insert_page (struct supplemental_page_table *spt,struct page *page) {
	/* TODO: Fill this function. */
	if (hash_insert(&spt->spt_hash,&page->hash_elem) == NULL){
		return true;
	}
	return false;
}

/* Delete PAGE into spt with validation. */
// spt에 va가 있는지 없는지 check
bool spt_delete_page (struct supplemental_page_table *spt,struct page *page) {

	/* TODO: Fill this function. */
	if (hash_delete(&spt->spt_hash,&page->hash_elem) == NULL){
		return true;
	}
	return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
/* Project3 : Clock Algorithm */
//page table entry가 최근에 액세스된 경우(즉, pml4_is_accessed에서 true가 반환되면) pml4_set_accessed 함수에서 accessed bit을 0으로 설정한다. 그리고 만약에 pml4에 page table entry가 없는 경우 (pml4_is_accessed에서 false 반환) 바로 그 frame이 victim(쫓겨날 대상)이 된다. 그리고 그 대상이 start에 기록된다.

// 그리고 list의 맨 처음(list_begin)부터 한번 더 for문을 돌리면서 victim을 찾아낸다.

// 이 victim을 찾는 과정을 여러가지로 구현할 수 있지만 여기서는 clock algorithm으로 구현했다.
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	struct thread* curr = thread_current();
	lock_acquire(&frame_table_lock);
	for (clock_ref; clock_ref != list_end(&frame_table); clock_ref = list_next(clock_ref)){
		victim = list_entry(clock_ref,struct frame,frame_elem);
		//bit가 1인 경우
		if(pml4_is_accessed(curr->pml4,victim->page->va)){
			pml4_set_accessed(curr->pml4,victim->page->va,0);
		}else{
			lock_release(&frame_table_lock);
			return victim;
		}
	}

	struct list_elem* start = list_begin(&frame_table);

	for (start; start != list_end(&frame_table); start = list_next(start)){
		victim = list_entry(start,struct frame,frame_elem);
		//bit가 1인 경우
		if(pml4_is_accessed(curr->pml4,victim->page->va)){
			pml4_set_accessed(curr->pml4,victim->page->va,0);
		}else{
			lock_release(&frame_table_lock);
			return victim;
		}
	}
	lock_release(&frame_table_lock);
	ASSERT(clock_ref != NULL);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* Frame_Table에 할당받은 Frame을 추가해준다.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame)); // user_pool 에서 frame 가져오고, kva return해서 frame에 넣어준다.
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	if(frame->kva == NULL){ //frame에서 가용한 page가 없다면
		/* 해당 로직은 evict한 frame을 받아오기에 이미 Frame_Table 존재해서 list_push_back()할 필요 없음 */
		frame = vm_evict_frame(); // 쫓아냄
	}
	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table,&frame->frame_elem);
	lock_release(&frame_table_lock);
	frame->page = NULL; //새 frame을 가져왔으니 page의 멤버를 초기화
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	if (vm_alloc_page(VM_ANON|VM_MARKER_0, addr, 1)) {
		
		//vm_claim_page(addr);
		thread_current()->stack_bottom -= PGSIZE;
	}
}

/* Return true on success */
// 1. 유저 -> 커널 transition시 thread 내 구조체에 유저 스택 저장
// 2. fault 발생 주소 확인 (유저스택에 있는지 in Pinots 1MB)
//  1) user stack에서 밑으로 1MB 사이에 있는지 확인 [1MB = (1<<20)]
//  2) push 시 8byte씩만 주소값이 내려감 write시 8byte아래로 주소값이 들어가면 정상적이지 않다. (user_stack영역안에 있더라도)
// 3. 확인되면 vm_stack_growth 호출
bool
vm_try_handle_fault (struct intr_frame *f , void *addr ,bool user , bool write , bool not_present) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;

	if(is_kernel_vaddr(addr) || addr == NULL){
		return false;
	}
	
	if(not_present){
		// thread 구조체 내의 rsp_stack을 설정 
		struct thread* cur = thread_current();
		void *rsp_stack = !user ? cur->rsp_stack : f->rsp;

		if (rsp_stack-8 <= addr  && USER_STACK - 0x100000 <= addr && addr <= USER_STACK){
				vm_stack_growth(pg_round_down(addr));
		} 
		page = spt_find_page(spt,addr);
		if(page == NULL){
			return false;
		}
		if(write && !page->writable){
			return false;
		}
		if (!vm_do_claim_page(page)){
			return false;
		}
		return true;
	}
	// 부모 페이지가 원래 write 가능한지 check
	page = spt_find_page(spt,addr);
	if(write == true && page->writable == true && page){
		return vm_handle_wp(page);
	}
}

bool vm_handle_wp(struct page *page){
	//새로 물리메모리 할당
  	void* parent_kva = page->frame->kva;
	//새로 물리메모리 할당
	struct frame *new_frame = (struct frame*)malloc(sizeof(struct frame));
	/* TODO: Fill this function. */
	new_frame->kva = palloc_get_page(PAL_USER);  
	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table,&new_frame->frame_elem);
	lock_release(&frame_table_lock);
	page->frame = new_frame;
  	memcpy(page->frame->kva, parent_kva, PGSIZE);
  	pml4_set_page(thread_current()->pml4, page->va, page->frame->kva, page->writable);
  	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
// page에 va를 할당하도록 claim을 하는 것이다.
// claim이란 유저 virtual memory 공간의 page를 physical memory에 할당하는 것이다.
// 함수의 로직을 살펴보면 va를 가진 page를 spt에서 찾아서 그 page에 대해서 vm_do_claim_page(page)를 진행한다.

bool
vm_claim_page (void *va) {
	struct page *page;
	/* TODO: Fill this function */
	struct thread* curr = thread_current();
	page = spt_find_page(&curr->spt,va);
	if (page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	//  user virtual address(UPAGE)에서 kernel virtual address(KPAGE)로의 mapping을 page table에 추가해주는 함수다(pml4 mapping 진행).
	// 인자로 받는 writable이 true면 user process가 page를 수정할 수 있고, 그렇지 않으면 read-only이다. KPAGE는 user pool에서 가져온 page여야 한다. UPAGE가 이미 mapping되었거나, 메모리 할당이 실패하면 false를 반환한다. 성공하면 true를 반환한다. 성공시에 swap_in()함수가 실행된다.
	if(install_page(page->va,frame->kva,page->writable)){
		return swap_in (page, frame->kva);
	}
	return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->spt_hash,my_hash_function,my_less_func,NULL);
}
//project3
// page에 대한 hash value를 return 해준다.(hash값을 구해주는 함수의 pointer)
// key = va, value = hash_elem
uint64_t my_hash_function (const struct hash_elem *e, void *aux){
	struct page* page = hash_entry(e,struct page,hash_elem);
	return hash_bytes(&page->va,sizeof(page->va)); //va에서 시작하는 hash를 반환
}


bool my_less_func (const struct hash_elem *a,const struct hash_elem *b,void *aux){
	bool flag = false;
	/* Returns true if A is less than B, or false if A is greater than or equal to B */
	struct page* A = hash_entry(a,struct page,hash_elem);
	struct page* B = hash_entry(b,struct page,hash_elem);
	return A->va < B->va;
}

/* Copy supplemental page table from src to dst */
// src에서 dst로 추가 페이지 테이블을 복사합니다. 
// 이는 자식이 부모의 실행 컨텍스트를 상속해야 할 때 사용됩니다(예: fork()). 
// src의 추가 페이지 테이블에 있는 각 페이지를 반복하고 dst의 추가 페이지 테이블에 있는 항목의 정확한 복사본을 만듭니다. 
// uninit 페이지를 할당하고 즉시 요청해야 합니다.

bool supplemental_page_table_copy (struct supplemental_page_table *dst , struct supplemental_page_table *src ) {
    struct hash_iterator i;
    hash_first (&i, &src->spt_hash);
    while (hash_next (&i)) {	// src의 각각의 페이지를 반복문을 통해 복사
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);   // 현재 해시 테이블의 element 리턴
        enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
        void *upage = parent_page->va;						// 부모 페이지의 가상 주소
        bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
        vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
        void* aux = parent_page->uninit.aux;

        if(parent_page->operations->type == VM_UNINIT) {	// 부모 타입이 uninit인 경우
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux)) // 부모의 타입, 부모의 페이지 va, 부모의 writable, 부모의 uninit.init, 부모의 aux (container)
                return false;
        }
        else {
			if(!vm_alloc_page(type, upage, writable)){
				return false;
			}
			struct page* child_page = spt_find_page(dst, upage);
			if(parent_page->operations->type == VM_FILE){
            	if(!vm_claim_page(upage))
            	    return false;
            	memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
			}else{
				child_page->frame = parent_page->frame;
				child_page->writable = parent_page->writable;
				if(install_page(child_page->va,child_page->frame->kva,child_page->writable)){
					swap_in(child_page,child_page->frame->kva);
				}
				ASSERT(child_page->frame->kva != NULL);
				if (pml4_set_page(thread_current()->pml4, child_page->va, child_page->frame->kva, 0) == false){
					return false;
				}
        	}
    	}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator i;
	struct frame* frame;
    hash_first (&i, &spt->spt_hash);
	while (hash_next(&i)){
		struct page *target = hash_entry (hash_cur (&i), struct page, hash_elem);
		frame = target->frame;
		//file-backed file인 경우
		if(target->operations->type == VM_FILE){
			do_munmap(target->va);
		}
	}
	
	hash_destroy(&spt->spt_hash,spt_dealloc);
	free(frame);
}

void spt_dealloc(struct hash_elem *e, void *aux){
	struct page *page = hash_entry (e, struct page, hash_elem);
	// destroy(page);
	ASSERT(is_user_vaddr(page->va));
	ASSERT(is_kernel_vaddr(page));
	free(page);
}