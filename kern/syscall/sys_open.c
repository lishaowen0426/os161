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
#include <array.h>
#include <vnode.h>
#include <kern/stat.h>
#include <kern/fcntl.h>
#include  <copyinout.h>

int sys_open (const char *filename, int flags, mode_t mode,int32_t* retVal){

    //
        // check if filename is null
        if(!filename){
            return EFAULT;
        }

        // check if filename is a valid pointer
        char* check_name=kmalloc(sizeof(char *));
        int ptr_err = copyin((const_userptr_t)filename, (void *)check_name, sizeof(char *));
        kfree(check_name);
        if(ptr_err){
            return ptr_err;
        }




    //

    lock_acquire(curproc->array_lock);
    if(array_num(curproc->fd_array)>=OPEN_MAX){
        lock_release(curproc->array_lock);
        return EMFILE;
    }
    unsigned fd=0;
    while(get(curproc->fd_array,fd,NULL)!=NULL) ++fd;

    struct vnode* vn;
    struct fd_entry* fe;
    struct file* file;
    int err;
    char * name=kstrdup(filename);
    err=vfs_open(name,flags,mode,&vn);
    if(err) {
        lock_release(curproc->array_lock);
        return err;
    }

    file=kmalloc(sizeof(struct file));
    if(file==NULL){
        lock_release(curproc->array_lock);
        return ENFILE;
    }


    file->refcount=1;
    file->status=flags;
    file->vn=vn;
    file->l=lock_create("l");
    file->valid=1;
    if(flags&O_APPEND){
        struct stat* s=kmalloc(sizeof(struct stat));
        KASSERT(s!=NULL);
        VOP_STAT(vn,s);
        file->offset=s->st_size;
    }
    else{
        file->offset=0;
    }
    fe=kmalloc(sizeof(struct fd_entry));
    if(fe==NULL) {
        lock_release(curproc->array_lock);
        return EMFILE;
    }

    fe->fd=fd;
    fe->f=file;

    filetable_insert(&filetable,file);
    array_add(curproc->fd_array,fe,NULL);
    *retVal=fd;
    lock_release(curproc->array_lock);
    return 0;
}
