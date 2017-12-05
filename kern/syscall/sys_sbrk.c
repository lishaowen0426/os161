#include <types.h>
#include <kern/errno.h>
#include <current.h>
#include <proc.h>
#include <syscall.h>
#include <addrspace.h>
#include <elf.h>
#include <mips/tlb.h>
#include <vm.h>
#include <spl.h>

int sys_sbrk(intptr_t amount, vaddr_t *retval){
    
    //kprintf("sbrk called with %d\n", (int)amount);
    struct addrspace* addr = curproc->p_addrspace;
    
    //check if heap is initialized
    if(addr->heapregion == NULL){
        as_define_heap(addr, (vaddr_t *)retval);
        *retval = (vaddr_t)addr->heaptop;
        //kprintf("heapbase is now %x\n", (int)retval);
        KASSERT(retval);
    }else{
        *retval = (vaddr_t)addr->heaptop;
        //kprintf("heapbase is now %x\n", (int)retval);
        KASSERT(retval);
    }
    
    //check if don't need to alloc
    if(amount == 0){
        return 0;
    }
    
    //check possible heap overflow
    if((addr->heaptop + amount < addr->stackbase) && 
        (addr->heaptop + amount >= addr->vtop2) &&
        (addr->heaptop + amount >= addr->vtop1)){
        
        lock_acquire(addr->as_lock);
        
        if(amount < 0){
            int free_page_num = -amount/PAGE_SIZE;
            int total_page_index = (addr->heaptop - addr->heapbase)/PAGE_SIZE - 1;
            int lower_bound = total_page_index - free_page_num;
            
            for(int i = total_page_index; i > lower_bound; i --){
               //clear pte
               struct pte* pte = addr->heapregion + i;
               if(pte->allocated==0){
                   continue;
               }
               
               if(pte->present==1){
                    spinlock_acquire(coremap_lock);
                    KASSERT(pte->ppagenum!=0);
                    struct frame* f=coremap+COREMAP_INDEX(pte->ppagenum);
                    KASSERT(f->used==1);
                    KASSERT(f->as==addr);
                    KASSERT(f->kernel==0);
                    KASSERT(f->vaddr==pte->vpagenum);
                    KASSERT(f->size==1);

                    bzero((void*)PADDR_TO_KVADDR(pte->ppagenum),PAGE_SIZE);
                    f->as=NULL;
                    f->used=0;
                    f->kernel=0;
                    f->vaddr=0;
                    f->size=0;
                    spinlock_release(coremap_lock);
               }
               else if(pte->present==0){
                    lock_acquire(swap_disk_lock);
                    bitmap_unmark(swap_disk_bitmap, pte->swap_disk_offset);
                    lock_release(swap_disk_lock);
               }

               //update entry in page table
               pte->ppagenum=0;
               pte->allocated=0;
               pte->present=0;
               pte->swap_disk_offset=0;
            }
            
            //clear tlb
            int spl=splhigh();
            for(int i=0;i<NUM_TLB;++i){
                tlb_write(TLBHI_INVALID(i),TLBLO_INVALID(),i);
            }
            splx(spl);
        }
        
        //kprintf("heaptop = %x\n", (int)addr->heaptop);
        //move heap pointer
        addr->heaptop = addr->heaptop + amount;
        //kprintf("heaptop = %x\n", (int)addr->heaptop);
        
        lock_release(addr->as_lock);
    }else{
        kprintf("amount is %x, but heap base %x, heaptop %x\n", (int)amount, (int)(addr->heapbase), (int)(addr->heaptop));
        return EINVAL;
    }
    
    return 0;
}




