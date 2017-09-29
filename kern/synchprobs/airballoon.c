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
typedef struct rope_map{
    //the current status of all ropes, 1 if still attached, 0 otherwise 
    bool rmap_status[NROPES];
    
    //record the connected rope of the stake such that
    //rmap_rope_num[n] = k
    //means stake #k is connected to rope #n
    int rmap_rope_num[NROPES];
} balloon_rope_map;

balloon_rope_map map;
// = malloc(sizeof(balloon_rope_map));


// Synchronization primitives
//#define status_lk_name "rmap_status"
//#define stake_lk_name "stake_lock"
static struct lock* rope_status_lock[NROPES];
static struct lock* rope_num_lock;

static struct lock* balloon_lock;
static struct cv* escape_cv;
static struct cv* main_program_cv;

/* Implement this! */

/*
 * Describe your design and any invariants or locking protocols 
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?  
 */

//

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Dandelion thread starting\n");

	// Implement this function
	while(ropes_left != 0){
	    int rand_rope = random() % NROPES;
	    
	    /*
	    lock_acquire(rope_status_lock[0]);
	    
	    if(ropes_left == 0){
	        lock_release(rope_status_lock[0]);
	        break;
	    }
	    */
	    lock_acquire(rope_status_lock[rand_rope]);
	    
	    while(map.rmap_status[rand_rope] == 0){
	        lock_release(rope_status_lock[rand_rope]);
	        rand_rope = random() % NROPES;
	        lock_acquire(rope_status_lock[rand_rope]);
	        //kprintf("rand = %d", rand_rope);
	        if(ropes_left == 0){
	            lock_release(rope_status_lock[rand_rope]);
	            goto dandelion_done;
	        }
	    }
	    
	    map.rmap_status[rand_rope] = 0;
	    
	    lock_acquire(rope_num_lock);
	    ropes_left--;
	    //kprintf("ropes left = %d\n", ropes_left);
	    kprintf("Dandelion severed rope %d\n", rand_rope);
	    
	    lock_release(rope_num_lock);
	    
	    lock_release(rope_status_lock[rand_rope]);
	    
	    
	    thread_yield();
	}

dandelion_done:	
	kprintf("Dandelion thread done\n");
	
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
	while(ropes_left != 0){
	    int rand_rope = random() % NROPES;
	    int rand_stake = map.rmap_rope_num[rand_rope];
	    
	    lock_acquire(rope_status_lock[rand_rope]);
	    
	    /*
	    lock_acquire(rope_status_lock[0]);
	    //lock_acquire(stake_lock);
	    
	    if(ropes_left == 0){
	        lock_release(rope_status_lock[0]);
	        //lock_release(stake_lock);
	        break;
	    }
	    */
	    
	    while(map.rmap_status[rand_rope] == 0){
	        lock_release(rope_status_lock[rand_rope]);
	        
	        rand_rope = random() % NROPES;
	        rand_stake = map.rmap_rope_num[rand_rope];
	        
	        lock_acquire(rope_status_lock[rand_rope]);
	        //kprintf("rand = %d", rand_rope);
	        if(ropes_left == 0){
	            lock_release(rope_status_lock[rand_rope]);
	            goto marigold_done;
	        }
	    }
	    
	    map.rmap_status[rand_rope] = 0;
	    
	    lock_acquire(rope_num_lock);
	    ropes_left--;
	    //kprintf("ropes_left = %d\n", ropes_left);
	    kprintf("Marigold severed rope %d from stake %d\n", rand_rope, rand_stake);
	    lock_release(rope_num_lock);
	    
	    lock_release(rope_status_lock[rand_rope]);
	    //lock_release(stake_lock);
	    
	    
	    thread_yield();
	
	}
	
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
	
	while(ropes_left != 0){
	    int rand_rope = random() % NROPES;
	    int rand_stake = random() % NROPES;
	    int present_stake = map.rmap_rope_num[rand_rope];
	    
	    while(rand_stake == present_stake){
	        rand_stake = random () % NROPES;
	    }
	    
	    /*
	    lock_acquire(rope_status_lock[0]);
	    //lock_acquire(stake_lock);
	    
	    if(ropes_left == 0){
	        
	        //lock_release(stake_lock);
	        lock_release(rope_status_lock[0]);
	        break;
	    }
	    */
	    
	    lock_acquire(rope_status_lock[rand_rope]);
	    
	    while(map.rmap_status[rand_rope] == 0){
	        lock_release(rope_status_lock[rand_rope]);
	        
	        rand_rope = random() % NROPES;
	        rand_stake = random() % NROPES;
	        present_stake = map.rmap_rope_num[rand_rope];
	        
	        while(rand_stake == present_stake){
	            rand_stake = random () % NROPES;
	        }
	        
	        lock_acquire(rope_status_lock[rand_rope]);
	        
	        if(ropes_left == 0){
	            lock_release(rope_status_lock[rand_rope]);
	            goto flowerkiller_done;
	        }
	    }
	
	    map.rmap_rope_num[rand_rope] = rand_stake;
	
	    kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", rand_rope, present_stake, rand_stake);
	    
	    //lock_release(stake_lock);
	    lock_release(rope_status_lock[rand_rope]); 
	    
	    thread_yield();   
	    
	}
	
	
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
	lock_acquire(balloon_lock);
	cv_wait(escape_cv, balloon_lock);
	lock_release(balloon_lock);
	
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	
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
	
	//initialize data structure
	ropes_left = NROPES;
	
	
	int count = 0;
	
	for(count = 0; count < NROPES; count++){
	    rope_status_lock[count] = lock_create("rmap_status_lock_" + count);
	}
	
	rope_num_lock = lock_create("rope_num_lock");
	
	escape_cv = cv_create("escape_cv");
	main_program_cv = cv_create("main_program_cv");
	balloon_lock = lock_create("balloon lock");
	
	
	for(count = 0; count < NROPES; count ++){
	    map.rmap_status[count] = 1;
        map.rmap_rope_num[count] = count;
	}
	
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
    lock_acquire(balloon_lock);
	cv_wait(main_program_cv, balloon_lock);
	lock_release(balloon_lock);
    
    //destroy locks and cvs
    for(count = 0; count < NROPES; count++){
        lock_destroy(rope_status_lock[count]);
    }
    
    lock_destroy(balloon_lock);
    cv_destroy(escape_cv);
    cv_destroy(main_program_cv);

    kprintf("Main thread done\n");
    
	return 0;
}
