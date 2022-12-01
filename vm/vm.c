/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

uint64_t my_hash_function (const struct hash_elem *e, void *aux);
bool my_less_func (const struct hash_elem *a,const struct hash_elem *b,void *aux);

struct list frame_table;
struct list_elem* clock_ref;

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
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va ) {
	/* TODO: Fill this function. */
	struct page* page = (struct page*)malloc(sizeof(page));		
	page->va = va;
	va = pg_round_down(va);//?
	struct hash_elem* target = hash_find(&spt->spt_hash,&page->hash_elem);
	free(page);
	if(target == NULL){
		return NULL;
	}
	return hash_entry(target,struct page,hash_elem);
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page (struct supplemental_page_table *spt,struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_insert(&spt->spt_hash,&page->hash_elem) == NULL){
		succ = true;
		return succ;
	}
	return succ;
}

/* Insert PAGE into spt with validation. */
bool spt_delete_page (struct supplemental_page_table *spt,struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_delete(&spt->spt_hash,&page->hash_elem) == NULL){
		return succ;
	}
	succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
/* Project3 : Clock Algorithm */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	struct thread* curr = thread_current();
	for (clock_ref; clock_ref != list_end(&frame_table); list_next(clock_ref)){
		victim = list_entry(clock_ref,struct frame,frame_elem);
		//bit가 1인 경우
		if(pml4_is_accessed(curr->pml4,victim->page->va)){
			pml4_set_accessed(curr->pml4,victim->page->va,0);
		}else{
			return victim;
		}
	}

	struct list_elem* start = list_begin(&frame_table);

	for (start; start != list_end(&frame_table); list_next(start)){
		victim = list_entry(start,struct frame,frame_elem);
		//bit가 1인 경우
		if(pml4_is_accessed(curr->pml4,victim->page->va)){
			pml4_set_accessed(curr->pml4,victim->page->va,0);
		}else{
			return victim;
		}
	}
	return NULL;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* Frame_Table에 할당받은 Frame을 추가해준다.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame*)malloc(sizeof(frame));
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	if(frame->kva == NULL){
		/* 해당 로직은 evict한 frame을 받아오기에 이미 Frame_Table 존재해서 list_push_back()할 필요 없음 */
		frame = vm_evict_frame();
		frame->page = NULL;
		return frame;
	}
	list_push_back(&frame_table,&frame->frame_elem);
	frame->page = NULL;
	//ASSERT (frame != NULL);
	//ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread* curr = thread_current();
	page = spt_find_page(&curr->spt,va);
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
	install_page(page->va,frame->kva,page->writable);//추후 확인 필요
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->spt_hash,my_hash_function,my_less_func,NULL);
}

uint64_t my_hash_function (const struct hash_elem *e, void *aux){
	struct page* page = hash_entry(e,struct page,hash_elem);
	return hash_bytes(page->va,sizeof(page->va));
}

bool my_less_func (const struct hash_elem *a,const struct hash_elem *b,void *aux){
	bool flag = false;
	/* Returns true if A is less than B, or false if A is greater than or equal to B */
	struct page* A = hash_entry(a,struct page,hash_elem);
	struct page* B = hash_entry(b,struct page,hash_elem);
	if (A->va>B->va){
		return !flag;
	}
	else 
		return flag;
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
