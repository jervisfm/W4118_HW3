/*
 * orient_lock.h
 *
 *  Created on: Oct 13, 2012
 *      Author: w4118
 */

#ifndef ORIENT_LOCK_H_
#define ORIENT_LOCK_H_

#include "../android-tegra-3.1/arch/arm/include/asm/unistd.h"
#include <unistd.h>

/* Include other struct needed to test program.
 * These were copied from include/linux/orientation.h
 */
struct dev_orientation {
	int azimuth; /* angle between the magnetic north
			and the Y axis, around the Z axis
			(0<=azimuth<360)
			0=North, 90=East, 180=South, 270=West */
	int pitch;   /* rotation around the X-axis: -180<=pitch<=180 */
	int roll;    /* rotation around Y-axis: +Y == -roll,
			-90<=roll<=90 */
};

struct orientation_range {
	struct dev_orientation orient;  /* device orientation */
	unsigned int azimuth_range;     /* +/- degrees around Z-axis */
	unsigned int pitch_range;       /* +/- degrees around X-axis */
	unsigned int roll_range;        /* +/- degrees around Y-axis */
};

/* Keeps on attempting to acquire a Read lock until we succeed */
void orient_read_lock(struct orientation_range *lock);

/* Keeps on attempting to release the read lock until we succeed */
void orient_read_unlock(struct orientation_range *lock);


/* Keeps on attempting to acquire a write lock until we succeed */
void orient_write_lock(struct orientation_range *lock);

/* Keeps on attempting to release the write lock until we succeed */
void orient_write_unlock(struct orientation_range *lock);


#endif /* ORIENT_LOCK_H_ */
