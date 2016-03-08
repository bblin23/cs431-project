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
#include <types.h>
#include <synch.h>
#include <vfs.h>
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

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
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
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
