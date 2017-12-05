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
#include <uio.h>
#include <kern/iovec.h>
#include <kern/fcntl.h>
#include <copyinout.h>

int sys_read (int fd, void *buf, size_t buflen,int32_t* retVal){

    if(!buf){
        return 0;
    }

    //check if buf is a valid pointer
    /*
    char* check_buf = kmalloc(sizeof(char*));
    int ptr_err = copyin((const_userptr_t)buf, (void *)check_buf, sizeof(char *));
    kfree(check_buf);
    if(ptr_err){
        return ptr_err;
    }
    */
    struct fd_entry* fe=get(curproc->fd_array,fd,NULL);
    if(fe==NULL) return EBADF;

    struct file* file=fe->f;
    struct vnode* vn=file->vn;
    int flags=(file->status)&O_ACCMODE;

    if(flags&O_WRONLY) return EBADF;

    struct uio ku;
    struct iovec iov;
    int err;
    void* kbuf=kmalloc(buflen);
    uio_kinit(&iov,&ku,kbuf,buflen,file->offset,UIO_READ);
    /**/
    /**/
    err=VOP_READ(vn,&ku);
    if(err){
        panic("sys read error\n");
        return err;
    }

    file->offset=ku.uio_offset;
    *retVal=buflen-ku.uio_resid;

    if(buf){
        err=copyout(kbuf,(userptr_t)buf,buflen);
        if(err){
            kfree(kbuf);
            return err;
        }
    }
    kfree(kbuf);
    return 0;
}
