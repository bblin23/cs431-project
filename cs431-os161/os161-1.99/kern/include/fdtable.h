#ifndef _FDTABLE_H_
#define _FDTABLE_H_

#include <limits.h>
#include <synch.h>

struct vnode;


struct fdtable {
    int flags;
    int *dups;
    struct vnode *vnode;
    struct lock *lk;
    off_t offset;
};


void fdtable_init();
int fdtable_copy(struct fdtable *a[], struct fdtable *b[]);
int fdtable_destroy(struct fdtable *table[]);
void fd_init(int fd);

#endif
