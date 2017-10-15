#include <types.h>
#include <syscall.h>
#include <vfs.h>
#include <kern/errno.h>
#include <lib.h>
#include <copyinout.h>

/*
 * Change the current directory to pathname, return 0 if no error
 */
int sys_chdir(const char *pathname, int32_t* retval){
    
    //check if pathname null
    if(!pathname){
        return EFAULT;
    }
    
    //check validity of user passed pointer
    char *checkptr = kmalloc(sizeof(char *));
    int err = copyin((const_userptr_t)pathname, (void *)checkptr, sizeof(char *));
    kfree(checkptr);
    
    if(err){
        return err;
    }
    
    //check if empty string
    int len = strlen(pathname);
    
    if(len == 0){
        return EINVAL;
    }
    
    //copy the path string from userspace to kernel
    char *path = kmalloc(sizeof(char *));
    size_t *got_len = kmalloc(sizeof(size_t *));
    err = copyinstr((const_userptr_t)pathname, path, (size_t)(len+1), got_len);
    
    kfree(got_len);
    
    if(err){
        kfree(path);
        return err;
    }

    //use virtual file system's change directory
    err = vfs_chdir(path);

    //return -1 if there is an error, return 0 otherwise
    if(err){
        *retval = (int32_t)-1;
    }else{
        *retval = (int32_t)0;
    }
    
    return err;
}
