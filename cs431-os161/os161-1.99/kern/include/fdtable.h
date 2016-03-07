#ifndef _FDTABLE_H_
#define _FDTABLE_H_

#include <limits.h>
//#include <synch.h>

struct vnode;


struct fdtable {
    int flags;
    int *dups;
    struct vnode *vnode;
    struct lock *lk;
    int offset;
};


int fdtable_init(void);
int fdtable_copy(struct fdtable *a[], struct fdtable *b[]);
void fdtable_destroy(struct fdtable *fdtable[]);
void fd_init(int fd);

#endif
