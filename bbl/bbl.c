#include "kernel_elf.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include <string.h>

static kernel_elf_info info;
static volatile int elf_loaded;

static void supervisor_vm_init()
{
  uintptr_t highest_va = DRAM_BASE - first_free_paddr;
  mem_size = MIN(mem_size, highest_va - info.first_user_vaddr) & -MEGAPAGE_SIZE;

  pte_t* sbi_pt = (pte_t*)(info.first_user_vaddr - (3 * RISCV_PGSIZE));
  memset(sbi_pt, 0, RISCV_PGSIZE);
  pte_t* middle_pt = (void*)sbi_pt + RISCV_PGSIZE;
#if !defined(__riscv64) && __riscv_xlen != 64
  size_t num_middle_pts = 1;
  pte_t* root_pt = middle_pt;
  memset(root_pt, 0, RISCV_PGSIZE);
#else
  size_t num_middle_pts = mem_size / GIGAPAGE_SIZE + 1;
  pte_t* root_pt = (void*)middle_pt + num_middle_pts * RISCV_PGSIZE;
  memset(middle_pt, 0, num_middle_pts * RISCV_PGSIZE);

  for (size_t i = 0; i < num_middle_pts; i++) {
    size_t l1_idx = (info.first_user_vaddr + (i * GIGAPAGE_SIZE)) >> ((2 * RISCV_PGLEVEL_BITS) + RISCV_PGSHIFT);
    root_pt[l1_idx] = ptd_create(((uintptr_t)middle_pt >> RISCV_PGSHIFT) + i);
  }
#endif

  for (uintptr_t vaddr = info.first_user_vaddr, paddr = vaddr;
       paddr < DRAM_BASE + mem_size; vaddr += MEGAPAGE_SIZE, paddr += MEGAPAGE_SIZE) {
    int l2_shift = RISCV_PGLEVEL_BITS + RISCV_PGSHIFT;
    size_t l2_idx = (info.first_user_vaddr >> l2_shift) & ((1 << RISCV_PGLEVEL_BITS)-1);
    l2_idx += ((vaddr - info.first_user_vaddr) >> l2_shift);
    middle_pt[l2_idx] = pte_create(paddr >> RISCV_PGSHIFT, PTE_G | PTE_R | PTE_W | PTE_X);
  }

  // map SBI at top of vaddr space
  extern char _sbi_end;
  uintptr_t num_sbi_pages = ((uintptr_t)&_sbi_end - DRAM_BASE - 1) / RISCV_PGSIZE + 1;
  assert(num_sbi_pages <= (1 << RISCV_PGLEVEL_BITS));
  for (uintptr_t i = 0; i < num_sbi_pages; i++) {
    uintptr_t idx = (1 << RISCV_PGLEVEL_BITS) - num_sbi_pages + i;
    sbi_pt[idx] = pte_create((DRAM_BASE / RISCV_PGSIZE) + i, PTE_G | PTE_R | PTE_X);
  }
  pte_t* sbi_pte = middle_pt + ((num_middle_pts << RISCV_PGLEVEL_BITS)-1);
  assert(!*sbi_pte);
  *sbi_pte = ptd_create((uintptr_t)sbi_pt >> RISCV_PGSHIFT);

  mb();
  root_page_table = root_pt;
  write_csr(sptbr, (uintptr_t)root_pt >> RISCV_PGSHIFT);
}

void print_logo();

void boot_loader()
{
  extern char _payload_start, _payload_end;
  //print_logo();

  load_kernel_elf(&_payload_start, &_payload_end - &_payload_start, &info);
  supervisor_vm_init();
  mb();
  elf_loaded = 1;
  printm("starting bootstrap at %lx ...\n", info.entry);
  asm volatile ("li a0, 0xDEADBE10");
  enter_supervisor_mode((void *)info.entry, 0);
}

void boot_other_hart()
{
  while (!elf_loaded)
    ;
  mb();
  enter_supervisor_mode((void *)info.entry, 0);
}
