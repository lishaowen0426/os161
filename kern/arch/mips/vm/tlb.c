#include <mips/tlb.h>
#include <spl.h>
#include <vm.h>
#include <bitmap.h>
#include <kern/iovec.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <array.h>

void tlb_evict(uint32_t ehi, uint32_t elo){
  static unsigned int tlb_next_evict=0;

  uint32_t entry_ehi, entry_elo;
  int spl;

  spl=splhigh();
  for(unsigned int i=0;i<NUM_TLB;++i){
    tlb_read(&entry_ehi,&entry_elo,i);
    if(entry_elo&TLBLO_VALID) continue;

    tlb_write(ehi,elo,i);
    splx(spl);
    return ;
  }

  splx(spl);

   /*
    *write to disk, update frame, addrspace, update tlb
    *
    */
  tlb_read(&entry_ehi.&entry_elo,tlb_next_evict);

  paddr_t paddr=entry_elo&TLBLO_PPAGE;
  vaddr_t vaddr=entry_ehi&TLBHI_VPAGE;
  int as_id= (entry_ehi&TLBHI_PID)>>6;
  int8_t flag=0;
  spinlock_acquire(frametable_lock);

  /*write to disk*/
  void* buf=(void*)vaddr;//cannot use kmalloc, the ram itself may full

  lock_acquire(swap_disk_lock);
  unsigned swap_disk_index;
  int err=bitmap_alloc(swap_disk_bitmap,&swap_disk_index);
  if(err) panic("Swap disk is full!");

  struct iovec iov;
  struct uio ku;
  uio_kinit(&iov,&ku,buf,PAGE_SIZE,(off_t)(swap_disk_index*PAGE_SIZE),UIO_WRITE);
  err=VOP_WRITE(swap_disk,&ku);
  if(err) panic("Cannot write to swap disk!");
  lock_release(swap_disk_lock);
  /**/

  /*update addrspace*/
  struct array* as_arr=frametable_ptr[FRAME_INDEX(paddr)].as_arr;
  KASSERT(vaddr==frametable_ptr[FRAME_INDEX(paddr)].vaddr);
  KASSERT(as_arr!=NULL);
  KASSERT(array_num(as_arr)!=0);

  for(unsigned int i=0;i<array_num(as_arr);++i){
    struct addrspace* as=(struct addrspace*)array_get(as_arr,i);
    lock_acquire(as->as_lock);
    if(as.as_id==as_id) flag=1;
    KASSERT(((as->dir)[EXTRACT_DIR_INDEX(vaddr)].pte)[EXTRACT_PTE_INDEX(vaddr)].valid==1);
    KASSERT(((as->dir)[EXTRACT_DIR_INDEX(vaddr)].pte)[EXTRACT_PTE_INDEX(vaddr)].allocated==1);
    KASSERT(((as->dir)[EXTRACT_DIR_INDEX(vaddr)].pte)[EXTRACT_PTE_INDEX(vaddr)].present=1);

    ((as->dir)[EXTRACT_DIR_INDEX(vaddr)].pte)[EXTRACT_PTE_INDEX(vaddr)].present=0;
    ((as->dir)[EXTRACT_DIR_INDEX(vaddr)].pte)[EXTRACT_PTE_INDEX(vaddr)].swap_disk_offset=swap_disk_index;
    lock_release(as->as_lock);
  }

  KASSERT(flag==1);

  spinlock_release(frametable_lock);
  /**/

  /*tlb update*/
  spl=splhigh();
  tlb_write(ehi,elo,tlb_next_evict);
  tlb_next_evict=(++tlb_next_evict)%NUM_TLB;
  splx(spl);
  return;
}


int disk_copyback(vaddr_t vaddr,paddr_t paddr, off_t swap_disk_offset){

  KASSERT((paddr&PAGE_FRAME)==paddr);

  spinlock_acquire(frametable_lock);
  KASSERT(frametable_ptr[FRAME_INDEX(paddr)].used==0);
  lock_acquire(swap_disk_lock);
  struct iovec iov;
  struct uio ku;
  void* buf=(void*)vaddr;
  uio_kinit(&iov,&ku, buf,PAGE_SIZE,(off_t)swap_disk_offset*PAGE_SIZE,UIO_READ);
  int err=VOP_READ(swap_disk,&ku);
  lock_release(swap_disk_lock);
  spinlock_release(frametable_lock);
  return err;

}
