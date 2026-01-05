#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgpte(void)
{
  uint64 va;
  struct proc *p;  

  p = myproc();
  argaddr(0, &va);
  pte_t *pte = pgpte(p->pagetable, va);
  if(pte != 0) {
      return (uint64) *pte;
  }
  return 0;
}
#endif

#ifdef LAB_PGTBL
int
sys_kpgtbl(void)
{
  struct proc *p;  

  p = myproc();
  vmprint(p->pagetable);
  return 0;
}
#endif


uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Trong kernel/sysproc.c

uint64
sys_pgaccess(void)
{
  uint64 base;      // Địa chỉ ảo bắt đầu (user truyền vào)
  int len;          // Số lượng trang cần kiểm tra
  uint64 bitmask;   // Địa chỉ buffer của user để chứa kết quả
  struct proc *p = myproc(); // Lấy tiến trình hiện tại

  // 1. Lấy các tham số từ thanh ghi (argument parsing) [cite: 82]
  if(argaddr(0, &base) < 0 || argint(1, &len) < 0 || argaddr(2, &bitmask) < 0)
    return -1;

  // Giới hạn số trang để tránh loop quá lâu (ví dụ 64 trang - tương ứng 64 bit) [cite: 85]
  if(len > 64) 
    return -1;

  uint64 mask_result = 0; // Biến tạm để lưu kết quả bitmask

  // 2. Duyệt qua từng trang
  for(int i = 0; i < len; i++) {
    // Tính địa chỉ ảo của trang thứ i
    uint64 va = base + i * PGSIZE; 

    // 3. Tìm PTE tương ứng với địa chỉ ảo này 
    // walk trả về con trỏ tới PTE. Tham số cuối là 0 vì ta chỉ tìm, không cấp phát mới.
    pte_t *pte = walk(p->pagetable, va, 0);

    // 4. Kiểm tra PTE
    // Kiểm tra xem PTE có tồn tại không và bit PTE_A có bật không
    if(pte != 0 && (*pte & PTE_V) && (*pte & PTE_A)) {
      
      // Nếu đã access: Bật bit thứ i trong biến kết quả
      mask_result = mask_result | (1L << i);

      // QUAN TRỌNG: Xóa bit A để reset cho lần kiểm tra sau 
      // Dùng phép AND với phủ định của PTE_A (dạng ...11011...)
      *pte = *pte & ~PTE_A; 
    }
  }

  // 5. Copy kết quả từ kernel (mask_result) ra user space (bitmask) 
  if(copyout(p->pagetable, bitmask, (char *)&mask_result, sizeof(mask_result)) < 0)
    return -1;

  return 0;
}