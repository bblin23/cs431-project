#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <uio.h>
#include <syscall.h>
#include <vnode.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>

#include <fdtable.h>
#include <synch.h>
#include <copyinout.h>

#include "opt-A2.h"

#if OPT_A2
int
sys_write(int fdesc,userptr_t ubuf,unsigned int nbytes,int *retval)
{
  struct iovec iov;
  struct uio u;
  int res;

  if(fdesc < 0 || fdesc > OPEN_MAX) {
    return EBADF;
  }
  if(curthread->fdtable[fdesc] == NULL) {
    return EBADF;
  }
  if(ubuf == NULL || curthread->fdtable[fdesc]->vnode == NULL) {
    return EFAULT;
  }

  lock_acquire(curthread->fdtable[fdesc]->lk);

  /* set up a uio structure to refer to the user program's buffer (ubuf) */
  iov.iov_ubase = ubuf;
  iov.iov_len = nbytes;
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_offset = 0;  /* not needed for the console */
  u.uio_resid = nbytes;
  u.uio_segflg = UIO_USERSPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  res = VOP_WRITE(curthread->fdtable[fdesc]->vnode, &u);
  if (res) {
    lock_release(curthread->fdtable[fdesc]->lk);
    return res;
  }

  /* pass back the number of bytes actually written */
  *retval = nbytes - u.uio_resid;
  lock_release(curthread->fdtable[fdesc]->lk);
  KASSERT(*retval >= 0);
  return 0;
}

int
sys_read(int fdesc, userptr_t ubuf, unsigned int nbytes, int *retval)
{
  struct iovec iov;
  struct uio u;
  int res;

  if(fdesc < 0 || fdesc > OPEN_MAX) {
    return EBADF;
  }
  if(curthread->fdtable[fdesc] == NULL) {
    return EBADF;
  }
  if(ubuf == NULL || curthread->fdtable[fdesc]->vnode == NULL) {
    return EFAULT;
  }

  lock_acquire(curthread->fdtable[fdesc]->lk);

  /* set up a uio structure to refer to the user program's buffer (ubuf) */
  iov.iov_ubase = ubuf;
  iov.iov_len = nbytes;
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_offset = 0;  /* not needed for the console */
  u.uio_resid = nbytes;
  u.uio_segflg = UIO_USERSPACE;
  u.uio_rw = UIO_READ;
  u.uio_space = curproc->p_addrspace;

  res = VOP_READ(curthread->fdtable[fdesc]->vnode, &u);
  if (res) {
    lock_release(curthread->fdtable[fdesc]->lk);
    return res;
  }

  /* pass back the number of bytes actually written */
  *retval = nbytes - u.uio_resid;
  lock_release(curthread->fdtable[fdesc]->lk);
  KASSERT(*retval >= 0);
  return 0;
}

int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
  int result;
  int fdesc;

  struct vnode *vn;
  char *name = kmalloc(sizeof(char) * PATH_MAX);
  copyinstr(filename, name, PATH_MAX, NULL);
  
  for(fdesc = 3; fdesc < OPEN_MAX; ++fdesc) {
    if(curthread->fdtable[fdesc] == NULL) {
        break;
    }
  }
  if(fdesc == OPEN_MAX) {
    return ENFILE;
  }

  fd_init(fdesc);
  
  result = vfs_open(name, flags, mode, &vn);
  if(result) {
    return result;
  }

  curthread->fdtable[fdesc]->vnode = vn;
  curthread->fdtable[fdesc]->flags = flags;
  curthread->fdtable[fdesc]->offset = 0;
  curthread->fdtable[fdesc]->lk = lock_create(name);
  *retval = fdesc;
  kfree(name);

  return 0;
}

int
sys_close(int fdesc)
{
  if(fdesc < 0 || fdesc >= OPEN_MAX) {
    return EBADF;
  }
  if(curthread->fdtable[fdesc] == NULL) {
    return EBADF;
  }
  lock_acquire(curthread->fdtable[fdesc]->lk);
  if(*curthread->fdtable[fdesc]->dups > 0) {
    --*curthread->fdtable[fdesc]->dups;
    lock_release(curthread->fdtable[fdesc]->lk);
    curthread->fdtable[fdesc] = NULL;
  }
  else if(*curthread->fdtable[fdesc]->dups == 0) {
    vfs_close(curthread->fdtable[fdesc]->vnode);
    lock_release(curthread->fdtable[fdesc]->lk);
    lock_destroy(curthread->fdtable[fdesc]->lk);
    kfree(curthread->fdtable[fdesc]->dups);
    kfree(curthread->fdtable[fdesc]);
    curthread->fdtable[fdesc] = NULL;
  }

  return 0;
}


#else

int
sys_write(int fdesc,userptr_t ubuf,unsigned int nbytes,int *retval)
{
  struct iovec iov;
  struct uio u;
  int res;

  //DEBUG(DB_SYSCALL,"Syscall: write(%d,%x,%d)\n",fdesc,(unsigned int)ubuf,nbytes);
  
  /* only stdout and stderr writes are currently implemented */
  if (!((fdesc==STDOUT_FILENO)||(fdesc==STDERR_FILENO))) {
    return EUNIMP;
  }
  KASSERT(curproc != NULL);
  KASSERT(curproc->console != NULL);
  KASSERT(curproc->p_addrspace != NULL);

  /* set up a uio structure to refer to the user program's buffer (ubuf) */
  iov.iov_ubase = ubuf;
  iov.iov_len = nbytes;
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_offset = 0;  /* not needed for the console */
  u.uio_resid = nbytes;
  u.uio_segflg = UIO_USERSPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  res = VOP_WRITE(curproc->console,&u);
  if (res) {
    return res;
  }

  /* pass back the number of bytes actually written */
  *retval = nbytes - u.uio_resid;
  KASSERT(*retval >= 0);
  return 0;
}

#endif
