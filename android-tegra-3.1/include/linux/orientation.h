#ifndef _LINUX_ORIENTATION_H
#define _LINUX_ORIENTATION_H

#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/sched.h>


/* Types for lock entry */
#define READER_ENTRY 0
#define WRITER_ENTRY 1

/* Orientation range vallue constants */
#define MIN_PITCH -180
#define MAX_PITCH 180
#define MIN_ROLL -90
#define MAX_ROLL 90
#define MIN_AZIMUTH 0
#define MAX_AZIMUTH 360

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
	struct list_head list; /* Waiters list */
	struct list_head granted_list;
	int type; /* 0 for read 1 for write */
	int pid; /* pid of process that runs this */
};

LIST_HEAD(waiters_list);
LIST_HEAD(granted_list);

spinlock_t WAITERS_LOCK;
spinlock_t GRANTED_LOCK;
spinlock_t SET_LOCK;

DECLARE_WAIT_QUEUE_HEAD(sleepers);
#endif /* _LINUX_ORIENTATION_H */
