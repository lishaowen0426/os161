#include <types.h>
#include <kern/errno.h>
#include <limits.h>
#include <current.h>
#include <addrspace.h>
#include <copyinout.h>
#include <syscall.h>
#include <vfs.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vm.h>
#include <proc.h>
int sys_execv(const char* program,char** args){
    //check if program and args pointer null
    //kprintf("enter exec\n");
    //kprintf("exec curproc is:%d\n",curproc->pi->pid);
    if(!program||!args){
        return EFAULT;
    }
    //check if program is a valid user space pointer
    char ** check_ptr=kmalloc(sizeof(void*));
    int ptr_err=copyin((const_userptr_t)program,check_ptr,sizeof(void*));
    kfree(check_ptr);
    if(ptr_err){
        return ptr_err;
    }
    //get a new address space
    struct addrspace* new_as=as_create();
    if(new_as==NULL) return ENOMEM;
    //get the old address space
    struct addrspace* old_as=proc_getas();
    //new_as->as_id=old_as->as_id;
    KASSERT(old_as!=NULL);
    int err;
    char* name=NULL;
    struct vnode* v;
    //get program name
    name=kstrdup(program);
    //open the program file
    err=vfs_open(name,O_RDONLY,0,&v);
    if(err){
        as_destroy(new_as);
        kfree(name);
        return err;
    }
    kfree(name);
    /*switch to new proc to load elf*/
    vaddr_t entrypoint,stackptr;
    proc_setas(new_as);
    as_activate();
    //kprintf("before load elf in exec\n");
    err=load_elf(v,&entrypoint);
    if(err){
        as_deactivate();
        proc_setas(old_as);
        as_activate();

        as_destroy(new_as);
        vfs_close(v);
        return err;
    }

    //vfs_close(v);
    //make a stack in the new address space
    err=as_define_stack(new_as,&stackptr);
    if(err){
        as_deactivate();
        proc_setas(old_as);
        as_activate();
        vfs_close(v);
        as_destroy(new_as);
        return err;
    }
    //kprintf("after define stack in exec\n");
    //set back to copyinout
    as_deactivate();
    proc_setas(old_as);
    as_activate();

    //I can copy the arg to new addrspace stack
    //count number of arguments and check if each pointer valid
    unsigned argc=0;//argv[argc]==NULL
    size_t p_size=sizeof(char*);
    char** kaddr=(char**)kmalloc(p_size);
    while(1){
        err=copyin((const_userptr_t)(args+argc),kaddr,p_size);
        if(err){
            //the pointer is not valid
            as_destroy(new_as);
            kfree(kaddr);
            return err;
        }
        //finish copying all argument pointers
        if(*kaddr==NULL) break;
        ++argc;
    }
    //check if there are too many arguments(it will overflow the stack)
    if((argc*p_size+sizeof((void*)0))>ARG_MAX){
        as_destroy(new_as);
        kfree(kaddr);
        return E2BIG;
    }

    size_t total_len=0;
    size_t instr_len;
    size_t outstr_len;
    //make a large space to store the unknown length string argument
    char* kstr=(char*)kmalloc(sizeof(char)*ARG_MAX);
    //loop through all arguments to count the total length of all strings
    for(unsigned index=0;index<argc;++index){
        //copy in and check the length of this string
        copyin((const_userptr_t)(args+index),kaddr,p_size);
        err=copyinstr((const_userptr_t)*kaddr,kstr,ARG_MAX,&instr_len);
        if(err){
            //encountered an error while copyinstr
            //set back to old address space and return
            as_destroy(new_as);
            kfree(kaddr);
            kfree(kstr);
            return EFAULT;
        }
        total_len+=instr_len;
    }
    total_len=(total_len/4+1)*4;
    //compute the starting address of argument pointers based on stack pointer,
    //number of arguments(plus the null pointer), and total length of all strings
    vaddr_t arr_base=(stackptr-(argc+1)*sizeof(void*)-total_len);
    //compute the starting address of argument strings based on total length of all strings
    vaddr_t str_base=(stackptr-total_len);
    userptr_t argv= (userptr_t)arr_base;
    stackptr=arr_base;

    //copy every argument to the new address space
    for(unsigned index=0;index<argc;++index){
        //copy in this argument from old address space to the kernel
        copyin((const_userptr_t)(args+index),kaddr,p_size);
        err=copyinstr((const_userptr_t)*kaddr,kstr,ARG_MAX,&instr_len);
        if(err){
            kprintf("enter err\n");
            as_destroy(new_as);
            kfree(kaddr);
            kfree(kstr);
            return EFAULT;
        }
        //switch to the new address space
        as_deactivate();
        proc_setas(new_as);
        as_activate();

        //copy out this argument pointer
        err=copyout((const void*)&str_base,(userptr_t)arr_base,sizeof(void*));
        if(err){
            as_deactivate();
            proc_setas(old_as);
            as_activate();
            as_destroy(new_as);
            kfree(kaddr);
            kfree(kstr);
            return EFAULT;
        }

        //copy out this argument string
        err=copyoutstr(kstr,(userptr_t)str_base,ARG_MAX,&outstr_len);
        if(err){
            as_deactivate();
            proc_setas(old_as);
            as_activate();
            as_destroy(new_as);
            kfree(kaddr);
            kfree(kstr);
            return EFAULT;
        }

        //make sure the string copied in is the same as the one copied out
        KASSERT(outstr_len==instr_len);
        arr_base+=sizeof(void*);
        str_base+=instr_len;

        //switch back to the old address space to prepare for copying the next argument
        as_deactivate();
        proc_setas(old_as);
        as_activate();
    }

    //swith to the new address space
    as_deactivate();
    proc_setas(new_as);
    as_activate();

    //copy out the null pointer at the end of all argument pointers
    *kaddr=NULL;
    err=copyout(kaddr,(userptr_t)arr_base,p_size);
    if(err){
        kprintf("enter err\n");
        as_deactivate();
        proc_setas(old_as);
        as_activate();

        kfree(kaddr);
        kfree(kstr);
        as_destroy(old_as);
        return EFAULT;
    }

    //finished copy everything
    //now we can destroy the old address space

    //kprintf("before destroy old as\n");
    as_destroy(old_as);
    //kprintf("after destroy old as\n");
    //switch to the new process
    //kprintf("before enter new procc\n");
    enter_new_process(argc,(userptr_t)argv,NULL,stackptr,entrypoint);
}
