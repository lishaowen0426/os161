#include <types.h>
#include <syscall.h>
#include <current.h>
#include <limits.h>
#include <vnode.h>
#include <vfs.h>
#include <lib.h>
#include <filetable.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>

int sys_close (int fd){
    lock_acquire(curproc->array_lock);
    int index=-1;
    struct fd_entry* fe=get(curproc->fd_array,fd,&index);
    if(fe==NULL){
        lock_release(curproc->array_lock);
        return EBADF;
    }
    KASSERT(index!=-1);
    struct file* file=fe->f;
    struct vnode* vn=file->vn;
    struct lock* l=file->l;


    lock_acquire(l);
    KASSERT(file->refcount>=1);

    --(file->refcount);

    if(file->refcount==0){
        VOP_DECREF(vn);
        file->valid=0;
        file->vn=NULL;
        lock_release(l);
        lock_destroy(l);

        fe->f=NULL;
        array_remove(curproc->fd_array,index);
        kfree(fe);
        lock_release(curproc->array_lock);

        return 0;
    }
    else {
        fe->f=NULL;
        array_remove(curproc->fd_array,index);
        kfree(fe);
        lock_release(l);
        lock_release(curproc->array_lock);

        return 0;
    }


}
