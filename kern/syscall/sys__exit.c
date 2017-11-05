#include <types.h>
#include <syscall.h>
#include <pid.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <spl.h>
#include <vnode.h>
#include <addrspace.h>
#include <kern/wait.h>
void sys__exit(int32_t exit_code){



    struct pid_info* info=curproc->pi;
    KASSERT(info!=NULL);

    KASSERT(curthread->t_proc==curproc);

    lock_acquire(info->pid_lock);

    /*set fields in pid_info*/
    info->proc_exited=1;

    int exit_reg=_MKWAIT_EXIT(exit_code);
    info->exit_code=exit_reg;


    /*set all child's parent_pid to -1*/
    for(unsigned index=0;index<array_num(info->child_pid_info);++index){


        struct pid_info* child_info=(struct pid_info*)array_get(info->child_pid_info,index);
        //1. set all child's parent pid to -1
        //2. check if child has already exited, if so, clear its pid_info
        //since when the child of this proc exit, it may keep its pid_info
        //due to the existance of this proc and now this proc is exiting
        //we have to clear its exited children
        lock_acquire(child_info->pid_lock);
        if(child_info->proc_exited){
            array_cleanup(child_info->child_pid_info);
            cv_broadcast(child_info->pid_cv,child_info->pid_lock);//incase,not necessary
            cv_destroy(child_info->pid_cv);
            lock_acquire(pidtable.lock);
            bitmap_unmark(pidtable.bitmap,child_info->pid);
            lock_release(pidtable.lock);
            lock_release(child_info->pid_lock);
            lock_destroy(child_info->pid_lock);
            kfree(child_info);
        }
        else{
            child_info->parent_pid=-1;
            lock_release(child_info->pid_lock);
        }


    }


    //after done with child array, clear it
    while(array_num(info->child_pid_info)>0) array_remove(info->child_pid_info,0);

   /*check if parent exists*/
    if(info->parent_pid==-1){
        //no one is or will be possibly waiting for this proc
        //so we can clear this pid_info, unsset the bitmap,but don't call remove on pittable.arr
        //since it will change the index to pid relation
        array_cleanup(info->child_pid_info);
        cv_broadcast(info->pid_cv,info->pid_lock);//in case, not necessary
        cv_destroy(info->pid_cv);

        lock_acquire(pidtable.lock);
        bitmap_unmark(pidtable.bitmap,info->pid);
        lock_release(pidtable.lock);

        lock_release(info->pid_lock);
        lock_destroy(info->pid_lock);
        kfree(info);
    }
    else{

        //parent may wait
        //keep this pid_info and signal parent
        cv_broadcast(info->pid_cv,info->pid_lock);
        lock_release(info->pid_lock);
    }


    /*destroy process*/
    struct proc* p=curproc;

    //close all file
    if(p->fd_array!=NULL){
		//lock_acquire(proc->array_lock);
		while(array_num(p->fd_array)>0){
			struct fd_entry* fe=(struct fd_entry*)array_get(p->fd_array,0);
			KASSERT(fe!=NULL);
			sys_close(fe->fd);
		}
		KASSERT(array_num(p->fd_array)==0);
		array_cleanup(p->fd_array);
		//lock_release(proc->array_lock);
		lock_destroy(p->array_lock);
	}


    if(p->p_cwd){
        VOP_DECREF(p->p_cwd);
        p->p_cwd=NULL;
    }
    KASSERT(p!=NULL);
    if(p->p_addrspace){
        struct addrspace *as;
        as=p->p_addrspace;
        p->p_addrspace=NULL;
        as_destroy(as);
    }
    proc_remthread(curthread);
    KASSERT(p!=NULL);
    KASSERT(threadarray_num(&p->p_threads)==0);
    threadarray_cleanup(&p->p_threads);
    spinlock_cleanup(&p->p_lock);
    kfree(p->p_name);
    kfree(p);
    /**/
    thread_exit();

}

//this function is called only in kill_curthread
//since when the kernel kills curthread
//the exit code would be signal not exit
//this is the only difference
void kexit(int32_t exit_code){

    struct pid_info* info=curproc->pi;
    KASSERT(info!=NULL);

    KASSERT(curthread->t_proc==curproc);

    lock_acquire(info->pid_lock);

    /*set fields in pid_info*/
    info->proc_exited=1;

    int exit_reg=_MKWAIT_SIG(exit_code);
    info->exit_code=exit_reg;


    /*set all child's parent_pid to -1*/
    for(unsigned index=0;index<array_num(info->child_pid_info);++index){


        struct pid_info* child_info=(struct pid_info*)array_get(info->child_pid_info,index);
        //1. set all child's parent pid to -1
        //2. check if child has already exited, if so, clear its pid_info
        //since when the child of this proc exit, it may keep its pid_info
        //due to the existance of this proc
        lock_acquire(child_info->pid_lock);
        if(child_info->proc_exited){
            array_cleanup(child_info->child_pid_info);
            cv_broadcast(child_info->pid_cv,child_info->pid_lock);//incase,not necessary
            cv_destroy(child_info->pid_cv);
            lock_acquire(pidtable.lock);
            bitmap_unmark(pidtable.bitmap,child_info->pid);
            lock_release(pidtable.lock);
            lock_release(child_info->pid_lock);
            lock_destroy(child_info->pid_lock);
            kfree(child_info);
        }
        else{
            child_info->parent_pid=-1;
            lock_release(child_info->pid_lock);
        }


    }


    //after done with child array, clear it
    while(array_num(info->child_pid_info)>0) array_remove(info->child_pid_info,0);
    /*check if parent exists*/

    if(info->parent_pid==-1){
        //no one is or will be possibly waiting for this proc
        //so we can clear this pid_info, unsset the bitmap,but don't call remove on pittable.arr
        //since it will change the index_pid relation
        //if(array_num(info->child_pid_info)=0) kprintf("child array\n");
        array_cleanup(info->child_pid_info);
        cv_broadcast(info->pid_cv,info->pid_lock);//in case, not necessary
        cv_destroy(info->pid_cv);

        lock_acquire(pidtable.lock);
        bitmap_unmark(pidtable.bitmap,info->pid);
        lock_release(pidtable.lock);

        lock_release(info->pid_lock);
        lock_destroy(info->pid_lock);
        kfree(info);
    }
    else{

        //parent may wait
        //keep this pid_info and signal parent
        cv_broadcast(info->pid_cv,info->pid_lock);
        lock_release(info->pid_lock);
    }


    /*destroy process*/
    struct proc* p=curproc;

    if(p->fd_array!=NULL){
		//lock_acquire(proc->array_lock);
		while(array_num(p->fd_array)>0){
			struct fd_entry* fe=(struct fd_entry*)array_get(p->fd_array,0);
			KASSERT(fe!=NULL);
			sys_close(fe->fd);
		}
		KASSERT(array_num(p->fd_array)==0);
		array_cleanup(p->fd_array);
		//lock_release(proc->array_lock);
		lock_destroy(p->array_lock);
	}


    if(p->p_cwd){
        VOP_DECREF(p->p_cwd);
        p->p_cwd=NULL;
    }
    KASSERT(p!=NULL);
    if(p->p_addrspace){
        struct addrspace *as;
        as=p->p_addrspace;
        p->p_addrspace=NULL;
        as_destroy(as);
    }
    proc_remthread(curthread);
    KASSERT(p!=NULL);
    KASSERT(threadarray_num(&p->p_threads)==0);
    threadarray_cleanup(&p->p_threads);
    spinlock_cleanup(&p->p_lock);
    kfree(p->p_name);
    kfree(p);
    /**/
    thread_exit();
}
