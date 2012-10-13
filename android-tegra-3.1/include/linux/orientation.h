#ifndef _LINUX_ORIENTATION_H
#define _LINUX_ORIENTATION_H

#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>

/* Types for lock entry */
#define READER_ENTRY 0
#define WRITER_ENTRY 1

struct dev_orientation {
	int azimuth; /* angle between the magnetic north
	                and the Y axis, around the Z axis
	                (0<=azimuth<360)
	                0=North, 90=East, 180=South, 270=West */
	int pitch;   /* rotation around the X-axis: -180<=pitch<=180 */
	int roll;    /* rotation around Y-axis: +Y == -roll,
	                -90<=roll<=90 */
};

struct dev_orientation current_orient;

struct orientation_range {
	struct dev_orientation orient;  /* device orientation */
	unsigned int azimuth_range;     /* +/- degrees around Z-axis */
	unsigned int pitch_range;       /* +/- degrees around X-axis */
	unsigned int roll_range;        /* +/- degrees around Y-axis */
};

struct lock_entry {
	struct orientation_range *range;
	atomic_t granted;
	list_head list; /* Waiters list */
	list_head granted_list;
	const int type; /* 0 for read 1 for write */
};

LIST_HEAD(waiters_list);
LIST_HEAD(granted_list);

spinlock_t WAITERS_LOCK = 0;

DECLARE_WAIT_QUEUE_HEAD(sleepers);

#endif /* _LINUX_ORIENTATION_H */
