#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "lib/kernel/bitmap.h"
struct page;
enum vm_type;

struct anon_page {
    struct page anon_p;
    int swap_sector; // swap된 내용이 저장되는 sector
};

// [include>vm>anon.h] 추가
struct bitmap *swap_table; // 0 - empty, 1 - filled
int bitcnt;

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
