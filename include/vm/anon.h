#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"

#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"

#define DISK_SECTOR_SIZE 512
struct page;
enum vm_type;

struct anon_page {
};

// [include>vm>anon.h] 추가
// 스왑 디스크에서 사용 가능한 영역과 사용된 영역을 관리하기 위한 자료구조로 bitmap 사용
// 스왑 영역은 PGSIZE 단위로 관리 => 기본적으로 스왑 영역은 디스크이니 섹터로 관리하는데
// 이를 페이지 단위로 관리하려면 섹터 단위를 페이지 단위로 바꿔줄 필요가 있음.
// 이 단위가 SECTORS_PER_PAGE! (8섹터 당 1페이지 관리)

struct bitmap *swap_table; // 0 - empty, 1 - filled
int swap_size;

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
