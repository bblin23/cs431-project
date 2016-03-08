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

#include <kern/fcntl.h>
#include <vfs.h>
#include <synch.h>
#include "opt-A2.h"
#include <spl.h>
#include <mips/trapframe.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2
  lock_acquire(p->p_info->pinfolock);
  DEBUG(DB_SYSCALL, "IN sys__exit\n");
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
  DEBUG(DB_SYSCALL,"removed thread.\n");

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

#if OPT_A2
  cv_broadcast(p->p_info->waitcv, p->p_info->pinfolock);
  lock_release(p->p_info->pinfolock);
#endif
  DEBUG(DB_SYSCALL,"exiting thread.\n");
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
  //DEBUG(DB_SYSCALL,"%d ",curproc->p_info->pid);
  *retval = curproc->p_info->pid;
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
#endif
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

  DEBUG(DB_SYSCALL,"===IN WAIT===\n");

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

  /* If process does not already exited */
  if (ptable->plist[pid % NPROCS_MAX]->exited == true){
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

#endif
  *retval = pid;
  return(0);
}


#if OPT_A2

int sys_execv(char *progname, char **uargs)
{
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    if(progname == NULL || uargs == NULL) {
        return EFAULT;
    }

    char *name;
    size_t size;

    lock_acquire(exec_lk);   

    // Copy in program name from userspace
    name = kmalloc(sizeof(char) * PATH_MAX);
    result = copyinstr((const_userptr_t) progname, name, PATH_MAX, &size);
    if(result) {
        kfree(name);
        return EFAULT;
    }
    if(size == 1) {
        kfree(name);
        return EINVAL;
    }

    char **args = kmalloc(sizeof(char **));
    /*
    result = copyin((const_userptr_t) uargs, args, sizeof(char **));
    if(result) {
        kfree(name);
        return EFAULT;
    }
    */

    // Copy userargs from userspace
    int i;
    for(i = 0; uargs[i] != NULL; ++i) {
        args[i] = kmalloc(sizeof(char) * PATH_MAX);
        result = copyinstr((const_userptr_t) uargs[i], args[i], PATH_MAX, &size);
        if(result) {
            kfree(name);
            kfree(args);
            return EFAULT;
        }
    }
    args[i] = NULL;
    
    // Open program file
    result = vfs_open(name, O_RDONLY, 0, &v);
    if(result) {
        kfree(name);
        kfree(args);
        return result;
    }

    // Create address space
    if(curproc_getas() != NULL) {
        as_destroy(curproc->p_addrspace);
        curproc->p_addrspace = NULL;
    }

    as = as_create();
    if(as == NULL) {
        kfree(name);
        kfree(args);
        vfs_close(v);
        return ENOMEM;
    }

    curproc_setas(as);
    as_activate();

    // Load executable
    result = load_elf(v, &entrypoint);
    if(result) {
        kfree(name);
        kfree(args);
        vfs_close(v);
        return result;
    }
    vfs_close(v);

    // Define the user stack in the address space
    result = as_define_stack(as, &stackptr);
    if(result) {
        kfree(name);
        kfree(args);
        return result;
    }

    // Copy arguments to userspace
    for(i = 0; args[i] != NULL; ++i) {
        char *arg;
        int len = strlen(args[i]) + 1;

        // Make sure stackptr is aligned
        if(len % 4 != 0) {
            len = len + (4 - len % 4);
        }
        arg = kmalloc(sizeof(len));
        arg = kstrdup(args[i]);

        stackptr -= len;

        result = copyout((const void *) arg, (userptr_t) stackptr, (size_t) len);
        kfree(arg);
        if(result) {
            kfree(name);
            kfree(args);
            return result;
        }
        
        args[i] = (char *)stackptr;
    }

    if(args[i] == NULL) {
        stackptr -= 4 * sizeof(char);
    }

    // Copy argument pointers to userspace
    for(int j = i-1; j >= 0; --j) {
        stackptr = stackptr - sizeof(char *);
        result = copyout((const void *) args+j, (userptr_t) stackptr, (sizeof(char *)));
        if(result) {
            kfree(name);
            kfree(args);
            return result;
        }
    }

    lock_release(exec_lk);
    enter_new_process(i, (userptr_t) stackptr, stackptr, entrypoint);

    // enter_new_process does not return
	panic("enter_new_process returned\n");
    return EINVAL;
}

#endif
#if OPT_A2

/*
  sys_fork needs to do these things:
    1. copy the parent's trapframe, give it to child
    2. copy parent's as
    3. create child's process and thread (using thread_fork)
    4. copy parent's file table into child (we can ignore this for now)
    5. make sure child returns from fork with 0
  We can use the helper function in syscall.c, enter_forked_process to
  handle a few of those things.
 */
int
sys_fork(struct trapframe *parenttf, 
  pid_t *retval)
{

  /* WE NEED TO DISABLE INTERRUPTS BEFORE FORKING*/
  int spl;
  spl = splhigh();
  DEBUG(DB_SYSCALL,"\n====IN SYS_FORK====\n");

  struct proc *parentproc = curproc;

  int err;

  if (getproc_count() >= NPROCS_MAX){
    spl = spl0();
    splx(spl);
    return ENPROC;
  }
  DEBUG(DB_SYSCALL,"nprocs: %d/%d\n",getproc_count(), NPROCS_MAX);


  /* Allocate space for child trapframe */
  struct trapframe *childtf = kmalloc(sizeof(struct trapframe));
  if (childtf == NULL){
    spl = spl0();
    DEBUG(DB_SYSCALL,"NO MORE MEMORY\n");
    return ENOMEM;
  }
  /* childtf is a copy of parenttf */
  memcpy((void *)childtf, (const void *)parenttf, (size_t) sizeof(struct trapframe));

  /* Allocate space for child process */
  struct proc *childproc = kmalloc(sizeof(struct proc));
  /* Use memcpy to copy the parent's process attributes into child's process */ 
  memcpy((void *)childproc, (const void *)parentproc, (size_t)sizeof(struct proc));

  lock_acquire(childproc->p_lock2);

  struct addrspace *childads = kmalloc(sizeof(struct addrspace));
  /* as_copy the parent's address space into the child's */
  err = as_copy(parentproc->p_addrspace, &childads);
  if (err){
    spl = spl0();
    return err;
  }
  DEBUG(DB_SYSCALL, "childads: 0x%x\nparentads: 0x%x\n",(int)childads,(int)parentproc->p_addrspace);


  /* Forked processes have only one thread of control, the thread that called it.
     So we clean up the thread array, initialize it again, then fork the curthread
     using thread_fork which will add the new thread into the child process' thread
     array */
  // threadarray_cleanup(&childproc->p_threads);
  // DEBUG(DB_SYSCALL,"Cleaned up childproc's threadarray.\n");
  // threadarray_init(&childproc->p_threads);

  err = thread_fork("child thread", childproc, enter_forked_process, 
    childtf, (unsigned long) childads);
  if (err){
    spl = spl0();
    return err;
  }

  kfree(childtf);

  /* childproc's pid was copied from its parent, insert_ptable will handle the case
  where if a p_info already exists, it will just use the copied pid as its ppid, and
  generate a new pid.*/
  childproc->p_info = insert_ptable(childproc);
  *retval = childproc->p_info->pid;
  DEBUG(DB_SYSCALL,"childpid: %d\nchildppid: %d\n", childproc->p_info->pid, childproc->p_info->ppid);

  /* ENABLE INTERRUPTS */
  cv_signal(childproc->p_waitcv, childproc->p_lock2);
  lock_release(childproc->p_lock2);
  spl = spl0();
  return 0;

}
#endif
