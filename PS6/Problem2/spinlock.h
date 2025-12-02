#ifndef __SPINLOCK_H
#define __SPINLOCK_H

typedef volatile int spinlock_t;

void spinlock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

#endif