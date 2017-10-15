#include <filetable.h>

void filetable_init(struct filetable* ft){
    ft->arr=array_create();
}

void filetable_insert(struct filetable* ft, struct file* f){
    struct file* temp;
    for(unsigned index=0;index<array_num(ft->arr);++index){
        temp=(struct file*)array_get(ft->arr,index);
        if(!temp->valid){
            array_set(ft->arr,index,f);
            return;
        }
    }
    array_add(ft->arr,f,NULL);
}
