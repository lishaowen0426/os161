#include <pid.h>
#include <limits.h>

void pid_table_create(struct pid_table* pt){

    //arr should have PID_MAX+1 entries
    //so that pid is equal to index
    pt->arr=array_create();
    //set one more so that pid is directly related
    //to index ,i.e info of pid N can be retrieved
    //by bitmap[N]
    pt->bitmap=bitmap_create(PID_MAX+1);
    //mark the unavailable bits
    for(unsigned index=0;index<PID_MIN;++index)
        bitmap_mark(pt->bitmap,index);

    array_preallocate(pt->arr,PID_MAX+1);

    //setsize so later can use set
    array_setsize(pt->arr,PID_MAX+1);

    pt->lock=lock_create("l");
}

int get_pid(struct pid_table* pt, pid_t* pid){

    lock_acquire(pt->lock);
    int ret=bitmap_alloc(pt->bitmap,(unsigned int*)pid);

    //ret==0 means success
    //if(ret==0) bitmap_unmark(pt->bitmap,*pid);

    lock_release(pt->lock);
    return ret;
}

void add_pid(struct pid_table* pt, struct pid_info* pi){

    KASSERT(pi!=NULL);
    lock_acquire(pt->lock);
    //if(bitmap_isset(pt->bitmap,pi->pid)) result=1;
    //else{
    //bitmap_mark(pt->bitmap,pi->pid);
    array_set(pt->arr,pi->pid,pi);
        //result=0;
    //}
    lock_release(pt->lock);
    //return result;
}


struct pid_info* get_proc(struct pid_table* pt, pid_t pid){

    struct pid_info* ret;
    lock_acquire(pt->lock);
    if(!bitmap_isset(pt->bitmap,pid)) ret=NULL;
    else ret=(struct pid_info*)array_get(pt->arr,pid);
    lock_release(pt->lock);
    return ret;
}
