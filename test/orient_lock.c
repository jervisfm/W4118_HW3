/*
 * orient_lock.c
 *
 *  Created on: Oct 13, 2012
 *      Author: w4118
 */
#include "orient_lock.h"


/*
 *
 * System Calls
 *
__NR_orientlock_write
__NR_orientunlock_read
__NR_orientunlock_write
__NR_orientlock_read
 */

/* Keeps on attempting to acquire a Read lock until we succeed */
void orient_read_lock(struct orientation_range *lock) {
	int ret = -1;
	do {
		ret = syscall(__NR_orientlock_read, lock);
	} while (ret != 0);
}

/* Keeps on attempting to release the read lock until we succeed */
void orient_read_unlock(struct orientation_range *lock) {
	int ret = -1;
	do {
		ret = syscall(__NR_orientunlock_read, lock);
	} while (ret != 0);
}

/* Keeps on attempting to acquire a write lock until we succeed */
void orient_write_lock(struct orientation_range *lock) {
	int ret = -1;
	do {
		ret = syscall(__NR_orientlock_write, lock);
	} while (ret != 0);
}

/* Keeps on attempting to release the write lock until we succeed */
void orient_write_unlock(struct orientation_range *lock) {
	int ret = -1;
	do {
		ret = syscall(__NR_orientunlock_write, lock);
	} while (ret != 0);
}
