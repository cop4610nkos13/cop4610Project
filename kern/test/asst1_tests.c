#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <wchan.h>
#include <test.h>

static struct lock *lockTest;
static struct cv *cvTest;
static struct semaphore *semaph;
static struct semaphore *semaph_locks;
static volatile unsigned long testNum;

#define NTHREADS      32

//cvTests

static void init_cv(void)
{
    testNum = 0;

    if (lockTest==NULL) {
        lockTest = lock_create("lockTest");
        if (lockTest == NULL) {
            panic("synchtest: lock_create failed\n");
        }
    }
    if (cvTest==NULL) {
        cvTest = cv_create("lockTest");
        if (cvTest == NULL) {
            panic("synchtest: cv_create failed\n");
        }
    }
    if (semaph==NULL) {
        semaph = sem_create("semaph", 0);
        if (semaph == NULL) {
            panic("synchtest: sem_create failed\n");
        }
    }
}

static void testThread_cv(void *ignore, unsigned long x)
{
    (void)ignore;

    lock_acquire(lockTest);
    V(semaph);
    while(testNum != x)
        cv_wait(cvTest, lockTest);
    V(semaph);
    lock_release(lockTest);
}

int testWait_cv(int nargs, char **args)
{
    (void)nargs;
    (void)args;

    int newFork;
    init_cv();

    kprintf("Starting wait test for cv...\n");
    
    //Wait until testNum == 1
    newFork = new_thread_fork("cvtest", NULL, NULL, testThread_cv, NULL, 1);
    if (newFork) {
        panic("testWait_cv: thread_fork failed: %s\n",
              strerror(newFork));
    }
    P(semaph);

    lock_acquire(lockTest);
        
    //increment testNum
    testNum++;
    cv_signal(cvTest, lockTest);
    lock_release(lockTest);
    P(semaph);

    
    kprintf("Test wait test for cv done\n");
    return 0;
}

int testSignal_cv(int nargs, char **args)
{
    (void)nargs;
    (void)args;

    int newFork;
    init_cv();
    kprintf("Starting signal test for cv...\n");
    
    //Wait for testNum == 1
    newFork = new_thread_fork("cvtest", NULL, NULL, testThread_cv, NULL, 1);
    if (newFork) {
        panic("testSignal_cv: thread_fork failed: %s\n",
              strerror(newFork));
    }
    newFork = new_thread_fork("cvtest", NULL, NULL, testThread_cv, NULL, 1);
    if (newFork) {
        panic("testSignal_cv: thread_fork failed: %s\n",
              strerror(newFork));
    }
    P(semaph);
    P(semaph);
    
    //increment testNum
    lock_acquire(lockTest);
    testNum++;
    cv_signal(cvTest, lockTest);
    lock_release(lockTest);
    P(semaph);
    
    lock_acquire(lockTest);
        
    cv_signal(cvTest, lockTest);
    lock_release(lockTest);
    P(semaph);
    
    
    kprintf("Signal test for cv done\n");
    return 0;
}

int testBroadcast_cv(int nargs, char **args)
{
    (void)nargs;
    (void)args;

    int newFork;
    init_cv();
    kprintf("Starting broadcast test for cv...\n");
    
    //wait on testNum == 1
    newFork = new_thread_fork("cvtest", NULL, NULL, testThread_cv, NULL, 1);
    if (newFork) {
        panic("cvtest: thread_fork failed: %s\n",
              strerror(newFork));
    }
    newFork = new_thread_fork("cvtest", NULL, NULL, testThread_cv, NULL, 1);
    if (newFork) {
        panic("cvtest: thread_fork failed: %s\n",
              strerror(newFork));
    }
    
    //wait on testNum == 2
    newFork = new_thread_fork("cvtest", NULL, NULL, testThread_cv, NULL, 2);
    if (newFork) {
        panic("cvtest: thread_fork failed: %s\n",
              strerror(newFork));
    }

    //waiting
    P(semaph);
    P(semaph);
    P(semaph);
    
    //increment testNum
    lock_acquire(lockTest);
        testNum++;
    //broadcast the change
        cv_broadcast(cvTest, lockTest);
    lock_release(lockTest);
    P(semaph);
    P(semaph);
    
    lock_acquire(lockTest);
        
    //increment testNum
    testNum++;
    cv_broadcast(cvTest, lockTest);
    lock_release(lockTest);
    P(semaph);
    
    kprintf("Broadcast for cv test done\n");
    return 0;
}


//lockTests

static int errorMsg(const char *error, unsigned long x)
{
    kprintf("thread %lu: %s\n ",x, error);
    kprintf("Test failed\n");

    lock_release(lockTest);

    V(semaph_locks);
    thread_exit();
}

static void helperTest(void *ignore, unsigned long x){

    (void)ignore;

    //test to make sure we're not the holder
    if(lock_do_i_hold(lockTest)){
        errorMsg("I should not hold the lock", x);
    }
	
    //check if we have the lock when we should
    lock_acquire(lockTest);
        if(!lock_do_i_hold(lockTest)){
            errorMsg("I don't hold the lock but should", x);
        }

        thread_yield();
 
        if(!lock_do_i_hold(lockTest)){
            errorMsg("postyield: I don't hold the lock but should", x);
        }
    lock_release(lockTest);

    //shouldn't have the lock
    if(lock_do_i_hold(lockTest)){
        errorMsg("I have the lock but shouldn't", x);
    }

    V(semaph_locks);
}

static void multiHoldTest()
{
    int newFork;   
    semaph_locks = sem_create("semaph_locks",0);

    for (int i=0; i<NTHREADS; i++) {
        newFork = new_thread_fork("multiHoldTest", NULL, NULL, helperTest, NULL, i);
        if (newFork) {
            panic("thread_fork failed: %s\n",
                  strerror(newFork));
        }
    }
    for (int i=0; i<NTHREADS; i++) {
        P(semaph_locks);
    }

}



static void threadRunner(void* p, unsigned long n)
{
	(void)p;
	kprintf("Hello from thread %ld\n", n);
}

static void joinTest(void)
{
	struct thread *children[NTHREADS];
	int ret;
	
	for(int i=0; i < NTHREADS; i++)
		new_thread_fork("child", &(children[i]), NULL, &threadRunner, NULL, i);

	for(int i=0; i < NTHREADS; i++)
	{
		thread_join(children[i], &ret);
		kprintf("Thread %d returned with %d\n", i, ret);
	}

	kprintf("Main thread done.\n");
}

int ourThreadTest(int nargs, char **args)
{
    (void) nargs;
    (void) args;

    kprintf("Starting join thread test...\n");
    joinTest();
    kprintf("\nThread join thread test done.\n");

    return 0;
}


int asst1_tests(int nargs, char **args)
{
	(void) nargs;
	(void) args;
	KASSERT(1);
	KASSERT(testWait_cv(nargs, args) == 0);
        KASSERT(testSignal_cv(nargs, args) == 0);
        KASSERT(testBroadcast_cv(nargs, args) == 0);
	multiHoldTest(nargs, args);
    KASSERT(ourThreadTest(nargs, args)==0);

    return 0;
}

