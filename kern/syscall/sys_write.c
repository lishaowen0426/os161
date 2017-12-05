#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <vnode.h>
#include <uio.h>
#include <kern/iovec.h>
#include <array.h>
#include <current.h>
#include <filetable.h>
#include <proc.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include  <copyinout.h>
int sys_write (int fd, const void *buf, size_t nbytes,int32_t* retVal){
    struct fd_entry* fe=get(curproc->fd_array,fd,NULL);
    if(fe==NULL) return EBADF;


    //
    if(!buf){
        return 0;
    }

    //check if buf is a valid pointer
    char* check_buf = kmalloc(sizeof(char *));
    int ptr_err = copyin((const_userptr_t)buf, (void *)check_buf, sizeof(char *));
    kfree(check_buf);
    if(ptr_err){
        return ptr_err;
    }

    //

    struct file* file=fe->f;

    int flags=(file->status)&O_ACCMODE;
    struct vnode* vn=file->vn;
    struct lock* l=file->l;

    if(flags==O_RDONLY) return EBADF;

    lock_acquire(l);

    struct uio uio;
    struct iovec iov;
    int err;
    char* c =kstrdup((const char*)buf);

    uio_kinit(&iov,&uio,c,nbytes,file->offset,UIO_WRITE);
    err=VOP_WRITE(vn,&uio);

    if(err){
        panic("sys_write error\n");
        return err;
    }

    file->offset=uio.uio_offset;
    *retVal=nbytes-uio.uio_resid;
    kfree(c);
    lock_release(l);
    return 0;
}
