#include <types.h>
#include <kern/errno.h>
#include <limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <syscall.h>

#include <current.h>
#include <lib.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <copyinout.h>

#include <fdtable.h>
#include "thread.h"
#include <vnode.h>

#include <synch.h>

int fdtable_init(void) 
{
    int result;
    char path[5];

    for(int i = 0; i < 3; ++i) {
        fd_init(i);
    }

    strcpy(path, "con:");
    result = vfs_open(path, O_RDONLY, 0, &curthread->fdtable[0]->vnode);
    if(result) {
        return result;
    }
    strcpy(path, "con:");
    result = vfs_open(path, O_WRONLY, 0, &curthread->fdtable[1]->vnode);
    if(result) {
        return result;
    }
    strcpy(path, "con:");
    result = vfs_open(path, O_WRONLY, 0, &curthread->fdtable[2]->vnode);
    if(result) {
        return result;
    }

    curthread->fdtable[0]->flags = O_RDONLY;
    curthread->fdtable[1]->flags = O_WRONLY;
    curthread->fdtable[2]->flags = O_WRONLY;

    return 0;
}


int fdtable_copy(struct fdtable *a[], struct fdtable *b[])
{
    int i;
    for(i = 0; i < OPEN_MAX; ++i) {
        if(a[i] != NULL) {
            lock_acquire(curthread->fdtable[i]->lk);
            ++*a[i]->dups;
            lock_release(curthread->fdtable[i]->lk);
            b[i] = kmalloc(sizeof(struct fdtable));
            b[i]->vnode = a[i]->vnode;
            b[i]->offset = a[i]->offset;
            b[i]->flags = a[i]->flags;
            b[i]->dups = a[i]->dups;
            b[i]->lk = a[i]->lk;
        }
    }
    return i;
}


void fdtable_destroy(struct fdtable *fdtable[])
{
    int i;
    for(i = 0; i < OPEN_MAX; ++i) {
        if(fdtable[i] != NULL) {
            lock_acquire(fdtable[i]->lk);

            if(*fdtable[i]->dups > 0) {
                --*fdtable[i]->dups;
                lock_release(fdtable[i]->lk);
            } else if(*fdtable[i]->dups == 0) {
                vfs_close(fdtable[i]->vnode);
                lock_release(fdtable[i]->lk);
                lock_destroy(fdtable[i]->lk);
                kfree(fdtable[i]);
            }

        }
    }
}


void fd_init(int fd)
{
    KASSERT(curthread->fdtable[fd] == NULL);
    curthread->fdtable[fd] = kmalloc(sizeof(struct fdtable));
    int *dup = kmalloc(sizeof(int));
    *dup = 0;
    curthread->fdtable[fd]->dups = dup;
    curthread->fdtable[fd]->offset = 0;
    curthread->fdtable[fd]->lk = lock_create("fdtlock");
    KASSERT(curthread->fdtable[fd] != NULL);
}
