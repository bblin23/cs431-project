#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include <synch.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2

  lock_acquire(p->p_info->pinfolock);
  exitcode = _MKWAIT_EXIT(exitcode);
  p->p_info->exited = true;
  p->p_info->exitcode = exitcode;

#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
#endif

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

#if OPT_A2
  cv_broadcast(p->p_info->waitcv, p->p_info->pinfolock);
  lock_release(p->p_info->pinfolock);
#endif
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
#if OPT_A2

  /* If status pointer is not valid (points to NULL or kernelspace) */
  if(status == NULL || (vaddr_t)status >= (vaddr_t)USERSPACETOP)
  {
      *retval = -1;
      return EFAULT;
  }

  /* If status pointer is not properly aligned */
  if((vaddr_t)status % 4 != 0)
  {
      *retval = -1;
      return EFAULT;
  }

  /* Does the waited pid exist/valid? */
  if(pid > PID_MAX || pid < PID_MIN || ptable->plist[pid % NPROCS_MAX] == NULL){
      *retval = -1;
      return ESRCH;
  }

  /* If process does not exist */
  if (ptable->plist[pid % NPROCS_MAX]->proc == NULL){
      *retval = -1;
      return ESRCH;
  }

  /* Are we allowed to wait for it? (MUST BE WAITING FOR CHILD) */
  if (curproc->p_info->pid != ptable->plist[pid % NPROCS_MAX]->ppid)
  {
    *retval = -1;
    return ECHILD;
  }

  struct pinfo* childpinfo = ptable->plist[pid % NPROCS_MAX];

  /* Wait until the chlid has exited */
  if(childpinfo->exited == false){
    lock_acquire(childpinfo->pinfolock);
    express_interest(pid);
    cv_wait(childpinfo->waitcv, childpinfo->pinfolock);
    uninterested(pid);
  }

  exitstatus = childpinfo->exitcode;
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }

#if OPT_A2

  lock_release(childpinfo->pinfolock);
  if(!check_interest(pid)){
    cv_destroy(childpinfo->waitcv);
    lock_destroy(childpinfo->pinfolock);
    kfree(childpinfo);
  }

  kfree(&childpinfo);
#endif
  *retval = pid;
  return(0);
}

