#include <types.h>
#include <current.h>
#include <syscall.h>
#include <pid.h>
#include <proc.h>

int sys_getpid(int32_t* retval){
    *retval=(int32_t)(curproc->pi->pid);
    return 0;
}
