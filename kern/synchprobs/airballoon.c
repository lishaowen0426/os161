/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>

#include <synch.h>

#define NROPES 16
static int ropes_left = NROPES;

// Data structures for rope mappings

/* Implement this! */

/*
 * A balloon rope map for maintaining the status of all ropes and all stakes
 */
typedef struct rope_map{
    // The current status of all ropes, 1 if still attached, 0 otherwise. 
    // e.g. rmap_status[3] == true means that the third rope is still attached.
    volatile bool rmap_status[NROPES];
    
    // Record the mapping of all ropes to stakes such that
    // rmap_stake_num[n] = k means rope #n is connected to stake #k.
    // Ropes and stakes don't need to have a 1:1 ratio.
    // e.g. rmap_stake_num[3] == 7 && rmap_stake_num[5] == 7 means that both the 
    // 3rd rope and the 5th rope are connected to the 7th stake
    volatile int rmap_stake_num[NROPES];
    
} balloon_rope_map;

// declare a balloon rope map for this class
balloon_rope_map map;


// Synchronization primitives

// This is the array of locks on each rope
static struct lock* rope_status_lock[NROPES];

// This is the lock on the global variable ropes_left
static struct lock* rope_num_lock;

// This is the lock on the hot airballoon status (prince escaped or not)
static struct lock* balloon_lock;
// This is the conditional variable for the hot airballoon thread
static struct cv* escape_cv;
// This is the conditional variable for the main thread
static struct cv* main_program_cv;

/* Implement this! */

/*
 * Describe your design and any invariants or locking protocols 
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?  
 */

/* The airballoon is implemented by maintaining (1)the number of ropes left, 
 * (2) the status of each rope (attached or severed), and (3)the corresponding 
 * stake number of each rope.
 *
 * (1) The global variable ropes_left is maintained such that every time a rope is 
 * severed, regardless of whom, ropes_left will be decremented by 1. This variable
 * is protected by the lock rope_num_lock. Everytime a thread need to access this
 * global variable, it needs to grab this lock first. Even though this is a global
 * lock, it is not used for large critical section (it is only used to protect the
 * counter); it should not have a significant negative impact on performance.
 *
 * (2) The boolean array in the balloon_rope_map struct: rmap_status[NROPES] 
 * maintains the status of each rope (true for attached and false for severed) 
 * such that everytime a rope's status is changed, the corresponding element in 
 * the array is updated to the new status. Every element of this array is protected
 * by separate locks; map.rmap_status[i] is protected by rope_status_lock[i], i 
 * ranges from 0 to NROPES. Everytime a thread tries to sever a rope, it needs to
 * grab the corresponding lock to the rope.
 *
 * (3) The int array in the balloon_rope_map struct: rmap_stake_num[NROPES]
 * maintains the corresponding stake number of each rope (rmap_stake_num[n] = k
 * means rope n is connected to stake k) such that everytime the connected stake 
 * of a rope changes, the corresponding element in the array is updated to the new 
 * stake number. map.rmap_stake_num[i] is also protected by rope_status_lock[i].
 * Everytime FlowerKiller thread tries to move a rope to a new stake, it also needs
 * to grab the corresponding lock. Thus the rope will not be moved in the middle of
 * being cut by Dandelion or Marigold.
 *
 * Dandelion, Marigold and FlowerKill exits when ropes_left == 0 (all ropes are
 * cut). When Dandelion exits, it notifies the hot airballoon thread that he escaped
 * through the conditional variable escape_cv. The balloon thread then knows that
 * prince has escaped, thus prints the statement and exit. The balloon thread is
 * the last thread to exit, therefore it also needs to notify the main thread that
 * all forked threads have finished. This is done through cv_signal on the 
 * conditional variable main_program_cv. These two conditional variables are protected
 * by the lock balloon_lock.
 *
 * Rep invariant: 
 * sum(map.rmap_status[i])(i from 0 to NROPES) = ropes_left;
 * map.rmap_stake_num[i] = k (0 <= k <= NROPES);
 * 0 <= ropes_left <= NROPES;
 * None of the structures or entries in the arrays are null.
 *
 *
 * -------------------------------------------------------------------------------
 * (Ignore this part if the design is considered sufficient for this lab)
 *
 * Note on the Marigold & FlowerKiller thread implementation:
 * Since Marigold & FlowerKiller needs to iterate through the loop for the rope 
 * number, they need to "do more work" to complete one operation compare to the
 * Dandelion thread. It is possible that Dandelion completes more work, especially
 * towards the end of the escaping process (Dandelion will cut rope consecutively).
 * Given enough rounds of testing, it will show that threads does complete work
 * interchangably. And Marigold can still cut rope correctly after it is switched 
 * to another stake. 
 * This design is memory sufficient that it only needs to allocate memory to store
 * one boolean array and one number array. It doesn't need to dynamically allocate
 * space for the switched stake or maintain a 2d square boolean array of size NROPES to 
 * make sure that the relationship between every rope and stake is marked. It only
 * needs lock on every rope; doesn't need extra lock on every stake. There is no 
 * busy-wait or coarse lock on a large critical section. The solution is simple 
 * enough to prevent race condition or deadlock. The performance seems OK (around
 * 2 seconds). However, the "unbalanced workload" as metioned above is a problem.
 * I believe that this is a fair trade-off for the memory conservation since
 * we are operating on a very limited memory space.
 * 
 * The specifications seem odd that Marigold and FlowerKiller can't select "random 
 * rope" instead of "random stake". Even though they are standing on the ground, they
 * still need to grab on a rope to cut it or move it to another stake. They don't 
 * have to "know the rope number". They can just grab one rope by random.
 * I completed an implementation that both Marigold and FlowerKiller select random
 * rope. The work load is balanced across all three threads, so it seems like that
 * it is much more unlikely to print consecutive outputs from one thread. I don't 
 * know if selecting random rope implementation is better than this one.
 *
 * Check commit 1a9d18f for "select random rope" implementation.
 * 
 * Thank you so much for reading thus far. It means a lot to me. Have a nice day :)
 *-------------------------------------------------------------------------------- 
 */ 

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Dandelion thread starting\n");

	// Implement this function
	// exit when all ropes are cut
	while(ropes_left != 0){
	    // choose a random rope
	    int rand_rope = random() % NROPES;
	    
	    // grab the lock for this rope
	    lock_acquire(rope_status_lock[rand_rope]);
	    
	    // check if the rope was cut already
	    while(map.rmap_status[rand_rope] == 0){
	        // the rope was cut already, release the lock on this rope and get
	        // a new random rope
	        lock_release(rope_status_lock[rand_rope]);
	        rand_rope = random() % NROPES;
	        
	        // grab the lock for the new random rope
	        lock_acquire(rope_status_lock[rand_rope]);
	        
	        // check if exit condition is met
	        if(ropes_left == 0){
	            lock_release(rope_status_lock[rand_rope]);
	            goto dandelion_done;
	        }
	        
	    }
	    
	    // the rope was not cut, now cut it
	    map.rmap_status[rand_rope] = 0;
	    
	    // grab the lock to protect total number of ropes
	    lock_acquire(rope_num_lock);
	    // decrement the total number of ropes
	    ropes_left--;
	    
	    // acknowledge the action
	    kprintf("Dandelion severed rope %d\n", rand_rope);
	   
	    // release the lock on total number of ropes
	    lock_release(rope_num_lock);
	    
	    // release the lock on the cut rope
	    lock_release(rope_status_lock[rand_rope]);
	    
	    // give control to other threads
	    thread_yield();
	}

// Dandelion thread exit
dandelion_done:	
	kprintf("Dandelion thread done\n");
	
	// notify the balloon that prince has escaped
	cv_signal(escape_cv, balloon_lock);
}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Marigold thread starting\n");

	// Implement this function
	// exit when all ropes are cut
	while(ropes_left != 0){
	    //lock_acquire(rope_num_lock);
	    // get a random stake
	    int rand_stake = random() % NROPES;
	    
	    // look for the corresponding rope number
	    int index = 0;
	    int rand_rope = 0;
	    
	    // check if exit condition is met
	    if(ropes_left == 0){
	        goto marigold_done;
	    }
	        
	    for(index = 0; index < NROPES; index++){
	        // lock on the current rope
	        lock_acquire(rope_status_lock[index]);
	        
	        // check if the rope is attached to the stake and if it is free
	        if(map.rmap_stake_num[index] == rand_stake && map.rmap_status[index] == 1){
	            // this is the rope that we are looking for, mark this rope
	            rand_rope = index;
	                    
	            // the rope was not cut, now cut it
	            map.rmap_status[rand_rope] = 0;
	    
	            // change the total number of ropes left
	            lock_acquire(rope_num_lock);
	            ropes_left--;
	    
	            kprintf("Marigold severed rope %d from stake %d\n", rand_rope, rand_stake);
	            lock_release(rope_num_lock);
	    
                lock_release(rope_status_lock[rand_rope]);
	    
                // give control to another thread
	            thread_yield();
	                    
	            break;
	                    
	        }else{
	            // this is not the rope that we are looking for, keep looking
	            lock_release(rope_status_lock[index]);
	        }
	   
        }
	        
	    //  choose a new stake for the next iteration 
	    rand_stake = random() % NROPES;

	}

// Marigold thread exit	
marigold_done:	
	kprintf("Marigold thread done\n");
	
}

static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Lord FlowerKiller thread starting\n");

	// Implement this function
	
	// exit when all ropes are cut
	while(ropes_left != 0){
	
	    // get a random stake to move the rope from
	    int present_stake = random() % NROPES;
	    
	    // get a random stake to move the rope to
	    int rand_stake = random() % NROPES;
	    
	    // make sure they are not the same stake
	    while(rand_stake == present_stake){
	        rand_stake = random() % NROPES;
	    }
	    
	    // search for a rope that is on this stake
	    int rand_rope = 0;
	    int index = 0;
	    
	    for(index = 0; index < NROPES; index++){
	        // grab the lock on this rope
	        lock_acquire(rope_status_lock[index]);
	        
	        // check if this rope is attached to the chosen stake and if it is uncut
	        if(map.rmap_stake_num[index] == present_stake && map.rmap_status[index] == 1){
             
                // this is the rope that we are looking for, mark it
	            rand_rope = index;
	            
	            // now switch it to the new stake
	            map.rmap_stake_num[rand_rope] = rand_stake;
	
        	    kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", rand_rope, present_stake, rand_stake);
	    
	            lock_release(rope_status_lock[rand_rope]); 
	    
	            // give control to another thread
	            thread_yield();  
	            break;
	            
	        }else{
	            // the rope is not what we are looking for, keep searching
	            lock_release(rope_status_lock[index]);
	        }
        }
	        
	    // check if exit condition is met
	    if(ropes_left == 0){
	        goto flowerkiller_done;
	    }
	        
	   // choose a new stake for the next iteration     
	    rand_stake = random() % NROPES;
	        
	}
	
// FlowerKiller exit	
flowerkiller_done:	
	kprintf("Lord FlowerKiller thread done\n");
}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Balloon thread starting\n");

	// Implement this function
	// wait until notified
	lock_acquire(balloon_lock);
	cv_wait(escape_cv, balloon_lock);
	lock_release(balloon_lock);
	
	// acknowledge that all ropes are cut
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	
	// signal the main thread to continue executing
	cv_signal(main_program_cv, balloon_lock);
}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{

	int err = 0;

	(void)nargs;
	(void)args;
	(void)ropes_left;
		
	// all ropes are attached 
	ropes_left = NROPES;
	
	
	int count = 0;
	
	// create all synchronization variables
	// create locks for each rope
	for(count = 0; count < NROPES; count++){
	    rope_status_lock[count] = lock_create("rmap_status_lock_" + count);
	}
	
	// create lock for the total number of ropes left
	rope_num_lock = lock_create("rope_num_lock");
	
	// create cv for the balloon
	escape_cv = cv_create("escape_cv");
	// create cv for the main thread
	main_program_cv = cv_create("main_program_cv");
	balloon_lock = lock_create("balloon lock");
	
	// initialize data structure
	for(count = 0; count < NROPES; count ++){
	    //rope is still attached
	    map.rmap_status[count] = 1;
	    //rope i is connected to stake i
	    map.rmap_stake_num[count] = count;
	}
	
	// fork threads
	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Lord FlowerKiller Thread",
			  NULL, flowerkiller, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));
	
done:
    // wait until notified
    lock_acquire(balloon_lock);
    cv_wait(main_program_cv, balloon_lock);
    lock_release(balloon_lock);
    
    // all threads done, destroy all locks and cvs
    for(count = 0; count < NROPES; count++){
        lock_destroy(rope_status_lock[count]);
    }
    
    lock_destroy(balloon_lock);
    cv_destroy(escape_cv);
    cv_destroy(main_program_cv);
    
    // main thread exits
    kprintf("Main thread done\n");
    
    return 0;
}
