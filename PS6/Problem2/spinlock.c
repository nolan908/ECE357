#include <sched.h>
#include "tas.h"
#include "spinlock.h"

void spinlock_init(spinlock_t *lock)
{
    *lock = 0;
}

void spin_lock(spinlock_t *lock)
{
    while (tas((volatile char *)lock) != 0)
        sched_yield();
}

void spin_unlock(spinlock_t *lock)
{
    *lock = 0;
}