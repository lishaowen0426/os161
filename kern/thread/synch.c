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
        
        
        // initialize wait channel for lock
        lock->lk_wchan = wchan_create(lock->lk_name);
        //check if success
        if(lock->lk_wchan == NULL){
            kfree(lock->lk_name);
            kfree(lock);
            return NULL;
        }
        
        // initialize spinlock for wait channel
        spinlock_init(&lock->lk_slock);
        
        
        // initialize such that no cpu holds the lock
        lock->lk_holder = NULL;
        
        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
        
        // cleanup spinlock
        spinlock_cleanup(&lock->lk_slock);
        
        // destroy wait channel
        wchan_destroy(lock->lk_wchan);
        
        // free the fields
        kfree(lock->lk_name);
        kfree(lock->lk_holder);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        // Write this

        (void)lock;  // suppress warning until code gets written
        
        //check if lock exists
        KASSERT(lock != NULL);
        
        //check interrupt
        KASSERT(curthread->t_in_interrupt == false);
        
        //grab spinlock to protect wait channel 
        spinlock_acquire(&lock->lk_slock);
        
        //check for deadlock	    
        if(lock->lk_holder == curthread->t_name){
             panic("Deadlock on lock %p\n", lock);
        }	    
        	    
        //check if someone holds the lock already
        //put the thread to sleep and wait	    
        while(lock->lk_holder != NULL){
            wchan_sleep(lock->lk_wchan, &lock->lk_slock);
        
        }

        //make sure no one hold the lock
        KASSERT(lock->lk_holder == NULL);
        
        //change the lock holder to the current thread
        lock->lk_holder = curthread->t_name;
        
        //release the spinlock for the wait channel
        spinlock_release(&lock->lk_slock);        

}

void
lock_release(struct lock *lock)
{
        // Write this

        (void)lock;  // suppress warning until code gets written
        
        //check if lock exist
        KASSERT(lock != NULL);
        
        //make sure current thread holds the lock
        KASSERT(lock_do_i_hold(lock));
        
        //grab spinlock to protect wait channel
        spinlock_acquire(&lock->lk_slock);
        
        //change lock holder to no one
        lock->lk_holder = NULL;
        
        //make sure it is changed successfully
        KASSERT(lock->lk_holder == NULL);
        
        //wake a waiting thread from the wait channel
        wchan_wakeone(lock->lk_wchan, &lock->lk_slock);
        
        //release the spinlock for the wait channel
        spinlock_release(&lock->lk_slock);
        
}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this

        (void)lock;  // suppress warning until code gets written

        //return true; // dummy until code gets written
        
        //make sure current cpu exists
        if(!CURCPU_EXISTS()){
            return true;
        }
        
        //check the lock holder field
        //make sure there is no interrupt while checking
        spinlock_acquire(&lock->lk_slock);
        bool i_hold = (lock->lk_holder == curthread->t_name);
        spinlock_release(&lock->lk_slock);
        
        return i_hold;
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
      
        // initialize wait channel for cv
        cv->cv_wchan = wchan_create(cv->cv_name);
        //check if success
        if(cv->cv_wchan == NULL){
            kfree(cv->cv_name);
            kfree(cv);
            return NULL;
        }
        
        // initialize spinlock for wait channel
        spinlock_init(&cv->cv_slock);


        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
   
        // cleanup spinlock
        spinlock_cleanup(&cv->cv_slock);
        
        // destroy wait channel
        wchan_destroy(cv->cv_wchan);


        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
        (void)cv;    // suppress warning until code gets written
        (void)lock;  // suppress warning until code gets written
        
        //check if cv and lock exist
        KASSERT(cv != NULL);
        KASSERT(lock != NULL);
        
        //make sure current thread holds the lock
        KASSERT(lock_do_i_hold(lock));
        
        //release the lock and make the current thread goes to sleep in atomic operation
        spinlock_acquire(&cv->cv_slock);
        
        lock_release(lock);
        wchan_sleep(cv->cv_wchan, &cv->cv_slock);
        
        //release the spinlock so that another spinlock can be used by lock
        //it will not cause race condition because lock provides synchronization
        spinlock_release(&cv->cv_slock);
        
        //the thread is waken, grab the lock again
        lock_acquire(lock);
        
        //check if the lock is acquired successfully
        KASSERT(lock_do_i_hold(lock));
        
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
	
	//make sure cv and lock exist
	KASSERT(lock != NULL);
	KASSERT(cv != NULL);
	
	//wake up a thread from the wait channel
	//grab the spinlock to protect the wait channel
	spinlock_acquire(&cv->cv_slock);
	wchan_wakeone(cv->cv_wchan, &cv->cv_slock);
	spinlock_release(&cv->cv_slock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
	
	//make sure cv and lock exist
	KASSERT(lock != NULL);
	KASSERT(cv != NULL);
	
	//wake up all threads in the wait channel
	//grab the spinlock to protect the wait channel
	spinlock_acquire(&cv->cv_slock);
	wchan_wakeall(cv->cv_wchan, &cv->cv_slock);
	spinlock_release(&cv->cv_slock);
}
