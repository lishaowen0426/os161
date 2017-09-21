/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

//added library
#include <cpu.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

        // add stuff here as needed
        
        /*
        // initialize wait channel for lock
        lock->lk_wchan = wchan_create(lock->lk_name);
        //check if success
        if(lock->lk_wchan == NULL){
            kfree(lock->lk_name);
            kfree(lock);
            return NULL;
        }
        */
        // initialize spinlock for wait channel
        //spinlock_init(&lock->lk_slock);
        
        
        // initialize such that no cpu holds the lock
        lock->lk_holder = NULL;
        
        lock->lk_sem = sem_create(name, 1);
        
        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
        // cleanup wait channel
        
        //spinlock_cleanup(&lock->lk_slock);
        /*
        wchan_destroy(lock->lk_wchan);
        */
        sem_destroy(lock->lk_sem);
        
        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        // Write this

        (void)lock;  // suppress warning until code gets written
        
        KASSERT(lock != NULL);
        
        KASSERT(curthread->t_in_interrupt == false);
        
        /*
        struct cpu *mycpu;
        //splraise(IPL_NONE, IPL_HIGH);
        splhigh();
        
        //get the pointer to the current cpu
        if (CURCPU_EXISTS()) {
		    mycpu = curcpu->c_self;
		    if (lock->lk_holder == mycpu) {
		        //current cpu is already holding the lock
			    panic("Deadlock on lock %p\n", lock);
		    }
		    //mycpu->c_spinlocks++;
	     }
	     else {
		    mycpu = NULL;
	     }
	     */
	    //grab spinlock to protect wait channel 
	    //spinlock_acquire(&lock->lk_slock);
	    /*
        while(lock->lk_holder != NULL){
            wchan_sleep(lock->lk_wchan, &lock->wchan_lock);
        
        }
        
        KASSERT(lock->lk_holder == NULL);
        lock->lk_holder = mycpu;
        spinlock_release(&lock->wchan_lock);
        */
        /* 
	     if(!lock->lk_holder){
	         membar_store_any();
	         lock->lk_holder = mycpu;
	     }else{
	         //add cpu to queue, put it to sleep
	     }
	     */
	     
	     P(lock->lk_sem);
	     lock->lk_holder = curcpu->c_self;
	     
	     //spinlock_release(&lock->lk_slock);
}

void
lock_release(struct lock *lock)
{
        // Write this

        (void)lock;  // suppress warning until code gets written
        
        KASSERT(lock != NULL);
        KASSERT(lock->lk_holder == curcpu->c_self);
        
        
        //spinlock_acquire(&lock->lk_slock);
        /*
        lock->lk_holder = NULL;
        
        KASSERT(lock->lk_holder == NULL);
        wchan_wakeone(lock->lk_wchan, &lock->wchan_lock);
        
        spinlock_release(&lock->wchan_lock);
        */
        
        /*
        if (CURCPU_EXISTS()) {
		KASSERT(lock->lk_holder == curcpu->c_self);
		//KASSERT(curcpu->c_spinlocks > 0);
		//curcpu->c_spinlocks--;
	    }
	    
        //check wait queue, if not empty
	    lock->lk_holder = NULL;
	    membar_any_store();
	    //spinlock_data_set(&splk->splk_lock, 0);
	    spllower(IPL_HIGH, IPL_NONE);
	    */
	    
	    V(lock->lk_sem);
	    lock->lk_holder = NULL;
	    
	    //spinlock_release(&lock->lk_slock);
}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this

        (void)lock;  // suppress warning until code gets written

        //return true; // dummy until code gets written
        
        if(!CURCPU_EXISTS()){
            return true;
        }
        
        return (lock->lk_holder == curcpu->c_self);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }

        // add stuff here as needed

        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed

        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
        (void)cv;    // suppress warning until code gets written
        (void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}
