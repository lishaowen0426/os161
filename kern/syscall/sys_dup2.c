#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <filetable.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <array.h>
#include <limits.h>
int sys_dup2(int oldfd, int newfd,int32_t* retval){
    if(newfd>=OPEN_MAX) return EBADF;
    lock_acquire(curproc->array_lock);
    struct fd_entry* fe=get(curproc->fd_array,oldfd,NULL);
    if(fe==NULL||newfd<0){
        lock_release(curproc->array_lock);
        return EBADF;
    }

    if(oldfd==newfd){
        *retval=newfd;
        lock_release(curproc->array_lock);
        return 0;
    }

    if(get(curproc->fd_array,newfd,NULL)!=NULL){
        lock_release(curproc->array_lock);
        sys_close(newfd);
        lock_acquire(curproc->array_lock);
    }

    if(array_num(curproc->fd_array)>=OPEN_MAX){
        lock_release(curproc->array_lock);
        return EMFILE;;
    }

    struct fd_entry* new_fdentry=kmalloc(sizeof(struct fd_entry));
    if(new_fdentry==NULL) return ENFILE;

    new_fdentry->fd=newfd;
    new_fdentry->f=fe->f;
    ++((fe->f)->refcount);
    VOP_INCREF(fe->f->vn);
    array_add(curproc->fd_array,new_fdentry,NULL);
    *retval=newfd;
    lock_release(curproc->array_lock);
    return 0;

}
