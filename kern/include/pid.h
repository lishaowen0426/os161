#ifndef _PID_H_
#define _PID_H_
#include <types.h>
#include <array.h>
#include <bitmap.h>
#include <synch.h>
#include <proc.h>

struct pid_info{
    pid_t pid;
    pid_t parent_pid;//-1 means no parent
    unsigned int proc_exited;
    unsigned int exit_code;
    unsigned int waited;//0 no one is waiting, 1 it is being waited
    struct array* child_pid_info; //store pointer to pid_info of child
    struct lock* pid_lock;
    struct cv* pid_cv;
    struct proc* p;
};


struct pid_table{
    struct array* arr;//store pointers to struct pid_info
    struct bitmap* bitmap;
    struct lock* lock;
};

void pid_table_create(struct pid_table* pt);

//get the next available pid but not set it
//it should be set when it is actually used
//return 0 if success
int get_pid(struct pid_table* pt,pid_t* pid);

//return 1 if not success
void add_pid(struct pid_table* pt, struct pid_info* pi);

struct pid_info* get_proc(struct pid_table* pt, pid_t pid);

//1. free the pid_info resource
//2. unmark the bitmap
//3. do not remove the array
void clear_pid (struct pid_table* pt, struct pid_info* pi);


extern struct pid_table pidtable;

#endif
