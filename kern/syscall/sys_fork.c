#include <types.h>
#include <spl.h>
#include <lib.h>
#include <kern/errno.h>
#include <addrspace.h>
#include <current.h>
#include <proc.h>
#include <pid.h>
#include <thread.h>
#include <mips/trapframe.h>
#include <limits.h>
#include <synch.h>
#include <syscall.h>
#include <vm.h>

int sys_fork(struct trapframe* tf,void (*entrypoint)(void *data1, unsigned long data2),int32_t* retval){

    //kprintf("enter fork\n");

    int spl=splhigh();

    int err;

    //copy the trapframe to the kernel heap
    struct trapframe* tf_copy=(struct trapframe*)kmalloc(sizeof(struct trapframe));
    memmove(tf_copy,tf,sizeof(struct trapframe));
    //splx(s);
    struct proc* newproc;
    //create new proc
    newproc=(struct proc*)kmalloc(sizeof(struct proc));
    if(newproc==NULL){
        kfree(tf_copy);
        return ENOMEM;
    }
    newproc->p_name=kstrdup("proc");
    if(newproc->p_name==NULL){
        kfree(newproc);
        kfree(tf_copy);
        return ENOMEM;
    }

    /*find new pid */
    /*
    pid_t child_pid=-1;
    err=get_pid(&pidtable,&child_pid);
    if(err){
        kfree(tf_copy);
        proc_destroy(newproc);
        return err;
    }
    KASSERT(child_pid>=PID_MIN&&child_pid<=PID_MAX);
    //initialize a new pid info struct
    struct pid_info* pi=(struct pid_info*)kmalloc(sizeof(struct pid_info));
    if(pi==NULL){
        kfree(tf_copy);
        proc_destroy(newproc);
        return ENPROC;
    }

    pi->pid=child_pid;
    pi->parent_pid=curproc->pi->pid;
    pi->proc_exited=0;
    pi->waited=0;
    pi->pid_lock=lock_create("l");
    pi->pid_cv=cv_create("cv");
    pi->child_pid_info=array_create();
    pi->p=newproc;
    newproc->pi=pi;
    add_pid(&pidtable,pi);

    //insert child pid info to its parent's array
    lock_acquire(curproc->pi->pid_lock);
    array_add(curproc->pi->child_pid_info,pi,NULL);
    lock_release(curproc->pi->pid_lock);
    */
    /**/


    threadarray_init(&newproc->p_threads);
    spinlock_init(&newproc->p_lock);
    newproc->p_addrspace=NULL;
    newproc->p_cwd=NULL;
    newproc->fd_array=array_create();
	newproc->array_lock=lock_create("l");
    //copy address space

    //kprintf("parent pid is:%d\n",curproc->pi->pid);
    //kprintf("child pid is :%d\n",child_pid);
    err=as_copy(curproc->p_addrspace,&(newproc->p_addrspace));

    if(err){
        kfree(tf_copy);
        proc_destroy(newproc);
        return err;
    }
    //copy the whole fd array
    fd_array_copy(curproc,newproc);


    spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

    //acquire a new pid

    pid_t child_pid=-1;
    err=get_pid(&pidtable,&child_pid);
    if(err){
        kfree(tf_copy);
        proc_destroy(newproc);
        return err;
    }
    KASSERT(child_pid>=PID_MIN&&child_pid<=PID_MAX);
    //initialize a new pid info struct
    struct pid_info* pi=(struct pid_info*)kmalloc(sizeof(struct pid_info));
    if(pi==NULL){
        kfree(tf_copy);
        proc_destroy(newproc);
        return ENPROC;
    }

    pi->pid=child_pid;
    pi->parent_pid=curproc->pi->pid;
    pi->proc_exited=0;
    pi->waited=0;
    pi->pid_lock=lock_create("l");
    pi->pid_cv=cv_create("cv");
    pi->child_pid_info=array_create();
    newproc->pi=pi;
    add_pid(&pidtable,pi);

    //insert child pid info to its parent's array
    lock_acquire(curproc->pi->pid_lock);
    array_add(curproc->pi->child_pid_info,pi,NULL);
    lock_release(curproc->pi->pid_lock);

    //newproc->p_addrspace->as_id=child_pid;

    //tweak the trapframe inside entrypoint
    thread_fork("proc",newproc,entrypoint,tf_copy,0/*second param is not useful*/);

    //return the child pid to parent
    *retval=child_pid;

    splx(spl);
    return 0;

}
