#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>

#include "opt-A1.h"

#if OPT_A1

static struct lock **globalCatMouseLock;
static struct lock *lock;
static struct cv *catEating;
static struct cv *mouseEating;
static int numCatsEating;
static int numMiceEating;


void
catmouse_sync_init(int bowls)
{
  numCatsEating = 0;
  numMiceEating = 0;
  catEating = cv_create("catEating");
  mouseEating = cv_create("mouseEating");

  lock = lock_create("lock");

  globalCatMouseLock = kmalloc(bowls * sizeof(struct lock *));
  for(int i = 0; i < bowls; ++i) {
    globalCatMouseLock[i] = lock_create("globalCatMouseLock");
  }
  if (globalCatMouseLock == NULL) {
    panic("could not create CatMouse synchronization lock");
  }
  return;
}

void
catmouse_sync_cleanup(int bowls)
{
  KASSERT(globalCatMouseLock != NULL);

  for(int i = 0; i < bowls; ++i) {
    lock_destroy(globalCatMouseLock[i]);
  }
  for(int i = 0; i < bowls; ++i) {
    kfree(globalCatMouseLock[i]);
  }
  kfree(globalCatMouseLock);

  lock_destroy(lock);
  cv_destroy(catEating);
  cv_destroy(mouseEating);
}

void
cat_before_eating(unsigned int bowl)
{
  KASSERT(globalCatMouseLock != NULL);
  lock_acquire(globalCatMouseLock[bowl-1]);
  lock_acquire(lock);
  while(numMiceEating > 0) {
    lock_release(lock);
    cv_wait(mouseEating, globalCatMouseLock[bowl-1]);
    lock_acquire(lock);
  }
  ++numCatsEating;
  lock_release(lock);
}

void
cat_after_eating(unsigned int bowl)
{
  KASSERT(globalCatMouseLock != NULL);

  lock_acquire(lock);
  --numCatsEating;
  if(numCatsEating == 0) {
    cv_broadcast(catEating, globalCatMouseLock[bowl-1]);
  }
  lock_release(globalCatMouseLock[bowl-1]);
  lock_release(lock);
}

void
mouse_before_eating(unsigned int bowl)
{
  KASSERT(globalCatMouseLock != NULL);

  lock_acquire(globalCatMouseLock[bowl-1]);
  lock_acquire(lock);
  while(numCatsEating > 0) {
    lock_release(lock);
    cv_wait(catEating, globalCatMouseLock[bowl-1]);
    lock_acquire(lock);
  }
  ++numMiceEating;
  lock_release(lock);
}

void
mouse_after_eating(unsigned int bowl)
{
  KASSERT(globalCatMouseLock != NULL);

  lock_acquire(lock);
  --numMiceEating;
  if(numMiceEating == 0) {
    cv_broadcast(mouseEating, globalCatMouseLock[bowl-1]);
  }
  lock_release(globalCatMouseLock[bowl-1]);
  lock_release(lock);
}

#else
/* 
 * This simple default synchronization mechanism allows only creature at a time to
 * eat.   The globalCatMouseLock is used as a a lock.   We use a semaphore
 * rather than a lock so that this code will work even before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct semaphore *globalCatMouseSem;


/* 
 * The CatMouse simulation will call this function once before any cat or
 * mouse tries to each.
 *
 * You can use it to initialize synchronization and other variables.
 * 
 * parameters: the number of bowls
 */
void
catmouse_sync_init(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_init */

  (void)bowls; /* keep the compiler from complaining about unused parameters */
  globalCatMouseSem = sem_create("globalCatMouseSem",1);
  if (globalCatMouseSem == NULL) {
    panic("could not create global CatMouse synchronization semaphore");
  }
  return;
}

/* 
 * The CatMouse simulation will call this function once after all cat
 * and mouse simulations are finished.
 *
 * You can use it to clean up any synchronization and other variables.
 *
 * parameters: the number of bowls
 */
void
catmouse_sync_cleanup(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_cleanup */
  (void)bowls; /* keep the compiler from complaining about unused parameters */
  KASSERT(globalCatMouseSem != NULL);
  sem_destroy(globalCatMouseSem);
}


/*
 * The CatMouse simulation will call this function each time a cat wants
 * to eat, before it eats.
 * This function should cause the calling thread (a cat simulation thread)
 * to block until it is OK for a cat to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the cat is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_before_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_before_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseSem != NULL);
  P(globalCatMouseSem);
}

/*
 * The CatMouse simulation will call this function each time a cat finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this cat finished.
 *
 * parameter: the number of the bowl at which the cat is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_after_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseSem != NULL);
  V(globalCatMouseSem);
}

/*
 * The CatMouse simulation will call this function each time a mouse wants
 * to eat, before it eats.
 * This function should cause the calling thread (a mouse simulation thread)
 * to block until it is OK for a mouse to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the mouse is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_before_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_before_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseSem != NULL);
  P(globalCatMouseSem);
}

/*
 * The CatMouse simulation will call this function each time a mouse finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this mouse finished.
 *
 * parameter: the number of the bowl at which the mouse is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_after_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseSem != NULL);
  V(globalCatMouseSem);
}

#endif
