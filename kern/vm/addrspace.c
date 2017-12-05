#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <elf.h>
#include <vfs.h>
#include <mips/tlb.h>
#include <spl.h>
#include <current.h>
#include <proc.h>
#include <cpu.h>
#include <uio.h>
#include <vnode.h>
#include <bitmap.h>
#define CLEVERVM_STACKPAGES    18
#define CLEVERVM_HEAPPAGES     18

//initialize each region to 0
struct addrspace*
as_create(void)
{
  struct addrspace * as;
  as=(struct addrspace*) kmalloc(sizeof(struct addrspace));
  if(as==NULL) return NULL;

  as->vbase1=0;
  as->vtop1=0;
  as->region1=NULL;

  as->vbase2=0;
  as->vtop2=0;
  as->region2=NULL;

  as->stacktop=0;
  as->stackbase=0;
  as->stackregion=NULL;

  as->heapbase = 0;
  as->heaptop = 0;
  as->heapregion = NULL;

  as->as_lock=lock_create("l");
  return as;
}

//just define the vaddr for each region
//but not allocate any physical page for it
//only allocate ppage in vm fault
int
 as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,int readable,int writeable,int executable)
{
  lock_acquire(as->as_lock);
  sz += vaddr & ~(vaddr_t)PAGE_FRAME;
  vaddr &= PAGE_FRAME;

  /* ...and now the length. */
  sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

  size_t npages=sz/PAGE_SIZE;

  if(as->region1==NULL){
    as->vbase1=vaddr;
    as->vtop1=vaddr+npages*PAGE_SIZE;
    as->region1=(struct pte*) kmalloc(npages*sizeof(struct pte));

    for(unsigned int i=0;i<npages;++i){
      (as->region1)[i].vpagenum=vaddr+i*PAGE_SIZE;
      (as->region1)[i].ppagenum=0;
      (as->region1)[i].allocated=0;
      (as->region1)[i].present=0;
      (as->region1)[i].flag=readable|writeable|executable;
      (as->region1)[i].swap_disk_offset=0;
    }
    lock_release(as->as_lock);
    return 0;
  }
  else if (as->region2==NULL){
    as->vbase2=vaddr;
    as->vtop2=vaddr+npages*PAGE_SIZE;
    as->region2=(struct pte*) kmalloc(npages*sizeof(struct pte));
    for(unsigned int i=0;i<npages;++i){
      (as->region2)[i].vpagenum=vaddr+i*PAGE_SIZE;
      (as->region2)[i].ppagenum=0;
      (as->region2)[i].allocated=0;
      (as->region2)[i].present=0;
      (as->region2)[i].flag=readable|writeable|executable;
      (as->region2)[i].swap_disk_offset=0;

    }
    lock_release(as->as_lock);
    return 0;
  }

  panic("as define region: try to define more than 2 regions");
}

int
as_prepare_load(struct addrspace *as)
{
  /*
   * Write this.
   */

  (void)as;
  return 0;
}

int
as_complete_load(struct addrspace *as)
{
  /*
   * Write this.
   */

  (void)as;
  return 0;
}

//define the stack region,which is much similiar to the other region
//the stack region has to be writable
int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
  /*
   * Write this.
   */
  lock_acquire(as->as_lock);
  as->stacktop=USERSTACK;
  as->stackbase=USERSTACK-STACK_PAGES*PAGE_SIZE;

  as->stackregion=(struct pte*)kmalloc(STACK_PAGES*sizeof(struct pte));

  for(unsigned int i=0;i<STACK_PAGES;++i){
      (as->stackregion)[i].vpagenum=as->stackbase+i*PAGE_SIZE;
      (as->stackregion)[i].ppagenum=0;
      (as->stackregion)[i].allocated=0;
      (as->stackregion)[i].present=0;
      (as->stackregion)[i].flag=PF_W|PF_R;
      (as->stackregion)[i].swap_disk_offset=0;
    }
  *stackptr=USERSTACK;
  lock_release(as->as_lock);
  return 0;
}

//when context switch
//flush the whole tlb
void
as_deactivate(void)
{
  int spl = splhigh();

  for(unsigned int i = 0; i < NUM_TLB; i++){
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
  }

  splx(spl);
}

//when context switch
//flush the whole tlb
void as_activate(void){

  //clear the whole tlb
  int spl = splhigh();

  for(unsigned int i = 0; i < NUM_TLB; i++){
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
  }

  splx(spl);
}

//when destroy a pte
//1. free the physical page,update the coremap entry
//2.if it is on disk, also free the disk page
static
void
destroy_pte(struct addrspace* as,struct pte* pte){
  if(pte->allocated==0) return;

  if(pte->present==1){
    spinlock_acquire(coremap_lock);
    KASSERT(pte->ppagenum!=0);
    struct frame* f=coremap+COREMAP_INDEX(pte->ppagenum);
    KASSERT(f->used==1);
    KASSERT(f->as==as);
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

  pte->vpagenum=0;
  pte->ppagenum=0;
  pte->allocated=0;
  pte->present=0;
  pte->flag=0;
  pte->swap_disk_offset=0;
}

void
as_destroy(struct addrspace* as)
{

  lock_acquire(as->as_lock);
  for(unsigned int i=0;i<(as->vtop1-as->vbase1)/PAGE_SIZE;++i){
    destroy_pte(as,as->region1+i);
  }
  //kprintf("clear region 1\n");
  kfree(as->region1);
  as->region1=NULL;
  for(unsigned int i=0;i<(as->vtop2-as->vbase2)/PAGE_SIZE;++i){
    destroy_pte(as,as->region2+i);
  }
  kfree(as->region2);
  as->region2=NULL;
  for(unsigned int i=0;i<(as->stacktop-as->stackbase)/PAGE_SIZE;++i){
    destroy_pte(as,as->stackregion+i);
  }
  kfree(as->stackregion);
  as->stackregion=NULL;

  //add heap support
  for(unsigned int i=0;i<(as->heaptop-as->heapbase)/PAGE_SIZE;++i){
    destroy_pte(as,as->heapregion+i);
  }
  kfree(as->heapregion);
  as->stackregion=NULL;

  int spl=splhigh();
  for(int i=0;i<NUM_TLB;++i){
      tlb_write(TLBHI_INVALID(i),TLBLO_INVALID(),i);
  }
  splx(spl);

  lock_release(as->as_lock);
  lock_destroy(as->as_lock);
  as->as_lock=NULL;
  kfree(as);

}

//for simplicity, when copy a page
//we find a new physical page for it and copy the old page to the new one
static
void
pte_copy(struct pte* old, struct pte* new,struct addrspace* newas)
{
    new->vpagenum=old->vpagenum;

    if(old->allocated==1){

      if(old->present==1){
        paddr_t paddr=0;
        paddr=get_single_ppage(new->vpagenum,newas);
        if(paddr==0) paddr=ram_evict(new->vpagenum,newas);
        new->ppagenum=paddr;
        memmove((void*)PADDR_TO_KVADDR(new->ppagenum),(const void*)PADDR_TO_KVADDR(old->ppagenum),PAGE_SIZE);

        new->allocated=1;
        new->present=1;
        new->flag=old->flag;
        new->swap_disk_offset=0;
      }
      else{
        KASSERT(old->present==0);
        paddr_t paddr=0;
        paddr=get_single_ppage(new->vpagenum,newas);
        if(paddr==0) paddr=ram_evict(new->vpagenum,newas);

        new->ppagenum=paddr;
        lock_acquire(swap_disk_lock);
        struct iovec iov;
        struct uio ku;
        void* buf= (void*)PADDR_TO_KVADDR(new->ppagenum);
        uio_kinit(&iov,&ku,buf,PAGE_SIZE,(off_t)(old->swap_disk_offset*PAGE_SIZE),UIO_READ);
        int err=VOP_READ(swap_disk,&ku);
        lock_release(swap_disk_lock);
        if(err) panic("pte_copy: read from disk failed");

        new->allocated=1;
        new->present=1;
        new->flag=old->flag;
        new->swap_disk_offset=0;
      }
    }
    else{
      //old->allocated==0
      new->ppagenum=0;
      new->allocated=0;
      new->present=0;
      new->flag=old->flag;
      new->swap_disk_offset=0;

    }


};



int
as_copy(struct addrspace *old, struct addrspace **ret)
{
  struct addrspace* newas;
  newas=as_create();
  if(newas==NULL) return ENOMEM;

  newas->as_lock=lock_create("l");

  newas->vbase1=old->vbase1;
  newas->vtop1=old->vtop1;
  size_t region1_sz=(old->vtop1-old->vbase1)/PAGE_SIZE;
  newas->region1=(struct pte*)kmalloc(region1_sz*sizeof(struct pte));

  newas->vbase2=old->vbase2;
  newas->vtop2=old->vtop2;
  size_t region2_sz=(old->vtop2-old->vbase2)/PAGE_SIZE;
  newas->region2=(struct pte*)kmalloc(region2_sz*sizeof(struct pte));

  newas->stacktop=old->stacktop;
  newas->stackbase=old->stackbase;
  size_t stack_sz=(old->stacktop-old->stackbase)/PAGE_SIZE;
  newas->stackregion=(struct pte*)kmalloc(stack_sz*sizeof(struct pte));

  //add heap support
  newas->heaptop = old->heaptop;
  newas->heapbase = old->heapbase;
  size_t heap_sz = 0;
  newas->heapregion = NULL;
  //if(old->heapregion != NULL){
    heap_sz = (old->heaptop - old->heapbase)/PAGE_SIZE;
    newas->heapregion = (struct pte *)kmalloc(heap_sz * sizeof(struct pte));
  //}

  lock_acquire(old->as_lock);
  for(unsigned int i=0;i<region1_sz;++i){
    pte_copy(old->region1+i,newas->region1+i,newas);
  }

  for(unsigned int i=0;i<region2_sz;++i){
    pte_copy(old->region2+i,newas->region2+i,newas);
  }

  //if(old->heapregion != NULL){
    for(unsigned int i=0;i<stack_sz;++i){
      pte_copy(old->stackregion+i,newas->stackregion+i,newas);
    }
  //}
  for(unsigned int i = 0; i < heap_sz; ++i){
    pte_copy(old->heapregion + i, newas->heapregion + i, newas);
  }


  lock_release(old->as_lock);
  *ret = newas;
  return 0;

}

//add heap support
int as_define_heap(struct addrspace *as, vaddr_t *heapptr){
    lock_acquire(as->as_lock);

    if(as->vtop1 > as->vtop2){
	    as->heapbase = as->vtop1;
	    //kprintf("using vtop1 %x\n", (int)as->heapbase);
	}else{
	    as->heapbase = as->vtop2;
	    //kprintf("using vtop2 %x\n", (int)as->heapbase);
	}


    as->heaptop = as->heapbase;
    as->heapregion=(struct pte*)kmalloc(CLEVERVM_HEAPPAGES*sizeof(struct pte));

    for(unsigned int i=0;i<CLEVERVM_HEAPPAGES;++i){
        (as->heapregion)[i].vpagenum=as->heapbase+i*PAGE_SIZE;
        (as->heapregion)[i].ppagenum=0;
        (as->heapregion)[i].allocated=0;
        (as->heapregion)[i].present=0;
        (as->heapregion)[i].flag=PF_W|PF_R;
        (as->heapregion)[i].swap_disk_offset=0;
    }
    *heapptr=as->heapbase;

    lock_release(as->as_lock);
    return 0;
}

