#ifndef _LINUX_ORIENTATION_H
#define _LINUX_ORIENTATION_H

struct dev_orientation {
	int azimuth; /* angle between the magnetic north
	                and the Y axis, around the Z axis
	                (0<=azimuth<360)
	                0=North, 90=East, 180=South, 270=West */
	int pitch;   /* rotation around the X-axis: -180<=pitch<=180 */
	int roll;    /* rotation around Y-axis: +Y == -roll,
	                -90<=roll<=90 */
};

struct dev_orientation k_orient;

#endif /* _LINUX_ORIENTATION_H */
