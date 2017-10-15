#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include <types.h>
#include <vnode.h>
#include <synch.h>
#include <array.h>

struct file {
    unsigned refcount;
    unsigned status;
    off_t offset;
    struct vnode* vn;
    struct lock* l;
    unsigned valid;
};

struct filetable{
    struct  array* arr;
};

void filetable_init(struct filetable* ft);
void filetable_insert(struct filetable* ft, struct file* f);

extern struct filetable filetable;
#endif
