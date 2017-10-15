#include <types.h>
#include <syscall.h>
#include <current.h>
#include <filetable.h>
#include <proc.h>
#include <array.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <kern/errno.h>
#include <kern/stattypes.h>

int sys_lseek(int fd, off_t pos, int whence,off_t* retval){
    struct fd_entry* fe=get(curproc->fd_array,fd,NULL);

    if(fe==NULL) return EBADF;

    mode_t type;
    struct file* file=fe->f;
    struct vnode* vn=file->vn;
    struct lock* l=file->l;
    off_t newoffset;

    VOP_GETTYPE(vn,&type);
    type=type&_S_IFMT;
    if(type==_S_IFCHR||type==_S_IFBLK) return ESPIPE;

    lock_acquire(l);

    if(whence==SEEK_SET){
        newoffset=pos;
    }
    else if(whence==SEEK_CUR){
        newoffset=file->offset+pos;
    }
    else if (whence==SEEK_END){
        struct stat* s=kmalloc(sizeof(struct stat));
        KASSERT(s!=NULL);
        VOP_STAT(vn,s);
        newoffset=s->st_size+pos;
        kfree(s);
    }
    else{
        lock_release(l);
        return EINVAL;
    }

    if(newoffset<0){
        lock_release(l);
        return EINVAL;
    }

    file->offset=newoffset;
    *retval=newoffset;
    lock_release(l);
    return 0;
}
