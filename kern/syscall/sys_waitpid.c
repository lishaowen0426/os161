#include <types.h>
#include <current.h>
#include <proc.h>
#include <pid.h>
#include <kern/errno.h>
#include <syscall.h>
#include <synch.h>
#include <copyinout.h>
int sys_waitpid(pid_t pid,int* status,int options,int32_t* retval){

    if(options!=0) return EINVAL;

    struct pid_info* info=get_proc(&pidtable,pid);
    if(info==NULL) {
        return ESRCH;
    }


    lock_acquire(info->pid_lock);
    if(info->parent_pid!=-1&&info->parent_pid!=curproc->pi->pid){
        kprintf("!!!!!!!!!!");
        lock_release(info->pid_lock);
        return ECHILD;
    }
    while(info->proc_exited==0){
        cv_wait(info->pid_cv,info->pid_lock);
    }
    if(status){
        int* check_ptr=kmalloc(sizeof(void*));
        *check_ptr=info->exit_code;
        int ptr_err=copyout((const void*)check_ptr,(userptr_t)status,sizeof(int));
        kfree(check_ptr);

        if(ptr_err){
            lock_release(info->pid_lock);
            return ptr_err;
        }
    }
    *retval=pid;

    /*
    KASSERT(array_num(info->child_pid_info)==0);
    array_cleanup(info->child_pid_info);
    cv_destroy(info->pid_cv);
    lock_acquire(pidtable.lock);
    bitmap_unmark(pidtable.bitmap,info->pid);
    lock_release(pidtable.lock);
    for(unsigned index=0;index<array_num(curproc->pi->child_pid_info);++index){
        struct pid_info* temp=(struct pid_info*) array_get(curproc->pi->child_pid_info,index);
        if(temp->pid==pid){
            array_remove(curproc->pi->child_pid_info,index);
            break;
        }
    }
    lock_release(info->pid_lock);
    lock_destroy(info->pid_lock);
    kfree(info);
    */
    lock_release(info->pid_lock);
    return 0;
}

//Can only called by kernel
int kwaitpid(pid_t pid){

    struct pid_info* info=get_proc(&pidtable,pid);
    if(info==NULL) {
        return ESRCH;
    }


    lock_acquire(info->pid_lock);
    if(info->parent_pid!=-1&&info->parent_pid!=curproc->pi->pid){
        lock_release(info->pid_lock);
        return ECHILD;
    }

    while(info->proc_exited==0){
        cv_wait(info->pid_cv,info->pid_lock);
    }
    KASSERT(array_num(info->child_pid_info)==0);
    array_cleanup(info->child_pid_info);
    cv_destroy(info->pid_cv);
    lock_acquire(pidtable.lock);
    bitmap_unmark(pidtable.bitmap,info->pid);
    lock_release(pidtable.lock);
    for(unsigned index=0;index<array_num(curproc->pi->child_pid_info);++index){
        struct pid_info* temp=(struct pid_info*) array_get(curproc->pi->child_pid_info,index);
        if(temp->pid==pid){
            array_remove(curproc->pi->child_pid_info,index);
            break;
        }
    }
    lock_release(info->pid_lock);
    lock_destroy(info->pid_lock);
    kfree(info);

    return 0;
}
