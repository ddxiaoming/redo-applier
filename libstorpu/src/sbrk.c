#include <errno.h>
#include "storpu/storpu.h"

extern char _end;
static void* _brksize = &_end;

extern int sys_brk(void* addr) __attribute__((weak));

int brk(void* addr)
{
    int ret;

    ret = sys_brk(addr);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    _brksize = addr;
    return 0;
}

void* sbrk(ptrdiff_t nbytes)
{
    char *oldsize = (char *)_brksize, *newsize = (char *)_brksize + nbytes;

    if ((nbytes < 0 && newsize > oldsize) || (nbytes > 0 && newsize < oldsize))
        return (void*)(-1);
    if (brk(newsize) == 0)
        return (void*)oldsize;
    else
        return (void*)(-1);
}
