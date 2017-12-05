#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <limits.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <elf.h>
#include <uio.h>
#include <vnode.h>
#include <kern/stat.h>
#include <cpu.h>
#include <array.h>
#include <mainbus.h>
#include <thread.h>
#include <threadlist.h>
#include <threadprivate.h>
#include "opt-synchprobs.h"
#include <wchan.h>
#include <signal.h>
#include <syscall.h>

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
struct spinlock* coremap_lock;
int vm_initialized=0;
struct frame* coremap;
paddr_t freepage;
struct vnode* swap_disk;
unsigned long total_frame_num;
struct bitmap* swap_disk_bitmap;
struct lock* swap_disk_lock;

void
vm_bootstrap(void)
{
  coremap_lock=(struct spinlock*) kmalloc(sizeof(struct spinlock));
  spinlock_init(coremap_lock);

  char buf[10];
  strcpy(buf,"lhd0raw:");
  int err=vfs_open(buf, O_RDWR,0664, &swap_disk);
  if(err){
    panic("Can not open swap disk");
  }

  //get stat of swap disk
  struct stat s;
  VOP_STAT(swap_disk,&s);
  off_t swap_disk_npages=s.st_size/PAGE_SIZE;
  swap_disk_bitmap=bitmap_create(swap_disk_npages);
  KASSERT(swap_disk_bitmap!=NULL);
  swap_disk_lock=lock_create("l");

  total_frame_num=ram_getsize()/PAGE_SIZE;
  size_t coremap_sz=total_frame_num*sizeof(struct frame);
  size_t coremap_npages=(coremap_sz+PAGE_SIZE-1)/PAGE_SIZE;

  //get physical mem for the coremap
  spinlock_acquire(&stealmem_lock);
  paddr_t coremap_paddr=ram_stealmem(coremap_npages);
  spinlock_release(&stealmem_lock);
  KASSERT(coremap_paddr!=0);

  coremap=(struct frame*)PADDR_TO_KVADDR(coremap_paddr);
  bzero(coremap,coremap_npages*PAGE_SIZE);

  paddr_t last=ram_getsize();
  freepage=ram_getfirstfree();
  bzero((void*)PADDR_TO_KVADDR(freepage),(last-freepage));
  KASSERT(freepage!=0);

  //initialize the coremap
  unsigned long index=0;
  for(;index<COREMAP_INDEX(freepage);++index){
    coremap[index].as=NULL;
    coremap[index].used=1;
    coremap[index].kernel=1;
    coremap[index].vaddr=PADDR_TO_KVADDR(index*PAGE_SIZE);
    coremap[index].size=1;
  }

  for(;index<total_frame_num;++index){
    coremap[index].as=NULL;
    coremap[index].used=0;
    coremap[index].kernel=0;
    coremap[index].vaddr=0;
    coremap[index].size=0;
  }

  vm_initialized=1;

  int spl = splhigh();

  //flush tlb
  for (int i=0; i<NUM_TLB; i++) {
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
  }

  splx(spl);
}



//find physical pages for kernel
//since the address in kseg0 and kseg1 is not tlb mapped
//it must be contiguous
static paddr_t find_contiguous_ppages(unsigned long npages){

  KASSERT(npages != 0);

  unsigned long found = npages;


  int first_index = 0;



  for(unsigned long index=0; index < total_frame_num; index ++){
    if(found == 0){
      //enough pages are found
      break;
    }else if(coremap[index].used == 0){
      //found one page
      found--;
    }else if(index == total_frame_num - 1){
      //reach the end but didn't find enough pages
      break;
    }else{
      //check the next page in the next iteration
      found = npages;
      first_index = index+1;
    }

  }



  if(found == 0){
    //prepare the pages before return
    bzero((void*)PADDR_TO_KVADDR(first_index*PAGE_SIZE),PAGE_SIZE*npages);

    for(unsigned long i = 0; i < npages; i++){

      KASSERT((first_index + i) < total_frame_num);
      KASSERT(coremap[first_index + i].used == 0);

      //prepare the coremap
      coremap[first_index + i].as = NULL;
      coremap[first_index + i].used = 1;
      coremap[first_index + i].kernel = 1;
      coremap[first_index + i].vaddr = PADDR_TO_KVADDR((first_index+i)*PAGE_SIZE);

      if(i == 0){
        coremap[first_index + i].size = (size_t)npages;
      }else{
        coremap[first_index + i].size = 0;
      }
    }

  }else{
    panic("find_contiguous_ppages: no enough ram\n");
  }



  return (paddr_t)(first_index*PAGE_SIZE);
}


//when address in kuseg needs physical page
//but the ram is full
//call this function to evict one page in ram
paddr_t
ram_evict(vaddr_t newvaddr, struct addrspace* newas)
{
  //kprintf("Enter ram evict\n");

  //spinlock_acquire(coremap_lock);
  static unsigned int start_idx=0;
  for(unsigned int j=0;j<total_frame_num;++j){

    unsigned int i=(start_idx+j)%total_frame_num;

    if(coremap[i].kernel==0){
      KASSERT(coremap[i].used==1);
      KASSERT(coremap[i].as!=NULL);
      KASSERT(coremap[i].vaddr!=0);
      KASSERT(coremap[i].size==1);

      struct addrspace * as=coremap[i].as;
      struct pte* pte=NULL;
      vaddr_t vaddr=coremap[i].vaddr;

      //write the evicted page to disk
      //and update its addrspace
      if(vaddr>=as->vbase1&&vaddr<as->vtop1)
        pte=as->region1+(vaddr-as->vbase1)/PAGE_SIZE;
      else if(vaddr>=as->vbase2&&vaddr<as->vtop2)
        pte=as->region2+(vaddr-as->vbase2)/PAGE_SIZE;
      else if(vaddr>=as->stackbase&&vaddr<as->stacktop)
        pte=as->stackregion+(vaddr-as->stackbase)/PAGE_SIZE;
      //add heap
      else if(vaddr>=as->heapbase && vaddr < as->heaptop)
        pte = as->heapregion + (vaddr - as->heapbase)/PAGE_SIZE;

      else panic("ram_evict:frame's vaddr is not in the valid region");

      KASSERT(pte!=NULL);
      KASSERT(pte->ppagenum==i*PAGE_SIZE);
      KASSERT(pte->allocated==1);
      KASSERT(pte->present=1);

      void *buf=(void*) PADDR_TO_KVADDR(i*PAGE_SIZE);

      lock_acquire(swap_disk_lock);
      unsigned swap_disk_index;
      int err=bitmap_alloc(swap_disk_bitmap,&swap_disk_index);
      lock_release(swap_disk_lock);
      if(err) panic("ram_evict: swap_disk is full");

      struct iovec iov;
      struct uio ku;
      uio_kinit(&iov,&ku,buf,PAGE_SIZE,(off_t)(swap_disk_index*PAGE_SIZE),UIO_WRITE);
      err=VOP_WRITE(swap_disk,&ku);
      if(err) panic("ram_evict: cannot write to swap disk");

      pte->ppagenum=0;
      pte->present=0;
      pte->swap_disk_offset=swap_disk_index;



      bzero((void*)PADDR_TO_KVADDR(i*PAGE_SIZE),PAGE_SIZE);
      coremap[i].as=newas;
      coremap[i].used=1;
      coremap[i].kernel=0;
      coremap[i].vaddr=newvaddr;
      coremap[i].size=1;

      //spinlock_release(coremap_lock);

      uint32_t ehi,elo;
      int spl=splhigh();
      for(int j=0;j<NUM_TLB;++j){
        tlb_read(&ehi,&elo,j);
        if((ehi&TLBHI_VPAGE)==vaddr)
          tlb_write(TLBHI_INVALID(j),TLBLO_INVALID(),j);
      }
      splx(spl);

      //kprintf("Return ram evict\n");
      start_idx=(i+1)%total_frame_num;
      return (paddr_t)(i*PAGE_SIZE);
    }
  }

  panic("ram_evict:no user page in physical memory");
}


//called by kmalloc
static
paddr_t
getppages(unsigned long npages)
{
  paddr_t paddr;

  if(vm_initialized==0){
    spinlock_acquire(&stealmem_lock);
    paddr=ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }
  else{
    spinlock_acquire(coremap_lock);
    paddr=find_contiguous_ppages(npages);
    spinlock_release(coremap_lock);
  }

  KASSERT(paddr!=0);
  return paddr;
}

//copy a page on disk back to physical ram
static
void disk_copyback(paddr_t paddr, off_t swap_disk_offset){

  KASSERT((paddr&PAGE_FRAME)==paddr);

  lock_acquire(swap_disk_lock);
  struct iovec iov;
  struct uio ku;
  void* buf=(void*)PADDR_TO_KVADDR(paddr);
  uio_kinit(&iov,&ku,buf,PAGE_SIZE,(off_t)(swap_disk_offset*PAGE_SIZE),UIO_READ);
  int err =VOP_READ(swap_disk,&ku);
  if(err) panic("disk_copyback: read from disk failed");

  bitmap_unmark(swap_disk_bitmap,swap_disk_offset);
  lock_release(swap_disk_lock);
}


//called by kmalloc
vaddr_t
alloc_kpages(unsigned npages)
{
  paddr_t paddr=0;
  paddr=getppages(npages);
  if(paddr==0) panic("alloc_kpages: cannot get paddr");

  return PADDR_TO_KVADDR(paddr);
}


void free_kpages(vaddr_t vaddr)
{
  KASSERT(vaddr>=MIPS_KSEG0);

  paddr_t paddr=vaddr-MIPS_KSEG0;
  unsigned int first_index=COREMAP_INDEX(paddr);

  spinlock_acquire(coremap_lock);
  KASSERT(coremap[first_index].kernel==1);
  KASSERT(coremap[first_index].as==NULL);
  KASSERT(coremap[first_index].vaddr=vaddr);
  KASSERT(coremap[first_index].size!=0);

  size_t sz=coremap[first_index].size;

  for(unsigned int i=first_index;i<first_index+sz;++i){
    KASSERT(coremap[i].kernel==1);
    KASSERT(coremap[i].as==NULL);
    KASSERT(coremap[i].vaddr==i*PAGE_SIZE+MIPS_KSEG0);
    KASSERT(coremap[i].used==1);

    coremap[i].used=0;
    coremap[i].kernel=0;
    coremap[i].vaddr=0;
    coremap[i].size=0;
  }

  bzero((void*)vaddr,PAGE_SIZE*sz);

  spinlock_release(coremap_lock);
}

//since we flush the whole tlb on context switch
//nothing to do in tlbshootdown
void vm_tlbshootdown_all(void){

};

void vm_tlbshootdown(const struct tlbshootdown* t){
  (void) t;
}


//get one physical page for address in kuseg
paddr_t
get_single_ppage(vaddr_t vaddr, struct addrspace* as)
{

  //kprintf("Enter get single ppage\n");

  KASSERT((vaddr&PAGE_FRAME)==vaddr);

  spinlock_acquire(coremap_lock);
  for(unsigned int i=0;i<total_frame_num;++i){
    if(coremap[i].used==0){
      KASSERT(coremap[i].as==NULL);
      KASSERT(coremap[i].used==0);
      KASSERT(coremap[i].kernel==0);
      KASSERT(coremap[i].vaddr==0);
      KASSERT(coremap[i].size==0);

      bzero((void*)PADDR_TO_KVADDR(i*PAGE_SIZE),PAGE_SIZE);

      coremap[i].as=as;
      coremap[i].used=1;
      coremap[i].kernel=0;
      coremap[i].vaddr=vaddr;
      coremap[i].size=1;
      spinlock_release(coremap_lock);


      return (paddr_t)(i*PAGE_SIZE);
    }
  }

  spinlock_release(coremap_lock);

  //if cannot find one in ram
  //return 0 and then call ram_evict
  return 0;
}

//if there is space in tlb, write the new entry into tlb directly
//otherwise, flush the whole tlb
static
int tlb_evict(uint32_t ehi, uint32_t elo){

  uint32_t entry_ehi, entry_elo;

  //turn off interrupt before manipulating the tlb
  int spl = splhigh();
  for(unsigned int i = 0; i < NUM_TLB; i++){
    //check if there are available empty spot in the tlb
    tlb_read(&entry_ehi, &entry_elo, i);
    if(entry_elo&TLBLO_VALID){
      continue;
    }else{
      //write to the empty spot
      tlb_write(ehi, elo, i);
      //turn on interrupt
      splx(spl);
      return 0;
    }
  }

  //the tlb was full, now clear the whole tlb
  for(unsigned int i = 0; i < NUM_TLB; i++){
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
  }

  //write the new entry to the first spot
  tlb_write(ehi, elo, 0);
  splx(spl);

  return 0;
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{

  faultaddress=faultaddress&PAGE_FRAME;

  switch(faulttype){
    case VM_FAULT_READONLY:
    case VM_FAULT_READ:  break;
    case VM_FAULT_WRITE: break;
    default: return EINVAL;
  }

  if(curproc==NULL) return EFAULT;
  struct addrspace* as=proc_getas();
  if(as==NULL) return EFAULT;

  //locate the page entry
  struct pte* pte=NULL;
  if(faultaddress>=as->vbase1&&faultaddress<as->vtop1){
    pte=as->region1+(faultaddress-as->vbase1)/PAGE_SIZE;
  }
  else if(faultaddress>=as->vbase2&&faultaddress<as->vtop2){
    pte=as->region2+(faultaddress-as->vbase2)/PAGE_SIZE;
  }
  else if(faultaddress>=as->stackbase&&faultaddress<as->stacktop){
    pte=as->stackregion+(faultaddress-as->stackbase)/PAGE_SIZE;
  }

  //add heap
  else if(faultaddress >= as->heapbase && faultaddress < as->heaptop){
    //kprintf("heap\n");
    pte=as->heapregion+(faultaddress-as->heapbase)/PAGE_SIZE;
  }

  else{
    kprintf("Fatal user mode trap signal 11\n");
    kexit(SIGSEGV);
    //panic("vm_fault: faultaddress is in invalid region");
  }

  KASSERT(pte!=NULL);
  KASSERT(pte->vpagenum==faultaddress);

  uint32_t ehi,elo;

  //if the vaddr is just defined, it won't
  //be in ram or disk,we need to find a physical page
  //for it first and then write it to tlb
  if(pte->allocated==0){
    KASSERT(pte->present==0);
    paddr_t paddr=0;
    paddr=get_single_ppage(pte->vpagenum, as);
    if(paddr==0) paddr=ram_evict(pte->vpagenum,as);

    ehi=faultaddress;
    elo=paddr|TLBLO_DIRTY|TLBLO_VALID;
    tlb_evict(ehi,elo);

    pte->allocated=1;
    pte->ppagenum=paddr;
    pte->present=1;
    return 0;
  }
  //the physical page is in ram
  //but not in tlb
  //just update the tlb
  else if(pte->present==1){
    KASSERT(pte->vpagenum==faultaddress);
    KASSERT(pte->ppagenum!=0);
    ehi=pte->vpagenum;
    elo=pte->ppagenum|TLBLO_DIRTY|TLBLO_VALID;
    tlb_evict(ehi,elo);
    return 0;
  }
  //the physical page is on disk
  //we need to find a physical page for it
  //can copy it back from disk
  else{
    KASSERT(pte->allocated==1&&pte->present==0);
    KASSERT(pte->ppagenum==0);
    // on disk
    paddr_t paddr=0;
    paddr=get_single_ppage(pte->vpagenum,as);
    if(paddr==0) paddr=ram_evict(pte->vpagenum,as);
    disk_copyback(paddr,pte->swap_disk_offset);

    pte->ppagenum=paddr;
    pte->present=1;

    ehi=pte->vpagenum;
    elo=pte->ppagenum|TLBLO_DIRTY|TLBLO_VALID;
    tlb_evict(ehi,elo);
    return 0;
  }

}

