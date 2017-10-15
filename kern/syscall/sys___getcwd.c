#include <types.h>
#include <syscall.h>
#include <uio.h>
#include <kern/iovec.h>
#include <vfs.h>
#include <lib.h>
#include <kern/errno.h>
#include <copyinout.h>

/*
 * Get the current working directory. Store it in buff. Return the actual length
 * of the copied String.
 */
int sys___getcwd(char *buf, size_t buflen, int32_t* retval){

    //check if buf null
    if(!buf){
        return EFAULT;
    }
    
    //allocate a buffer in kernel space to store result
    char *copied_buf = kmalloc(sizeof(char *));
    int err;

    struct uio cwd;
    struct iovec iov;
    
    //initialize a uio to store the path
    uio_kinit(&iov, &cwd, copied_buf, buflen, 0, UIO_READ);
    
    //get path from virtual file system
    err = vfs_getcwd(&cwd);
    
    if(err){
        panic("sys___getcwd error\n");
        return err;
    }
    
    //copy the pointer from kernel space back to user space
    err = copyout((const void *)copied_buf, (userptr_t)buf, (size_t)(sizeof(char *)));
    kfree(copied_buf);
    //user pointer error
    if(err){
        return err;
    }
    
    //store copied length of path
    *retval = cwd.uio_offset;
       
    return 0;
}
