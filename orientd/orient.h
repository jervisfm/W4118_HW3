/*
 * orient.h
 *
 *  Created on: Oct 14, 2012
 *      Author: w4118
 */

/**
 * Data structures for the storing the orientation information
 * Based on linux/orientation.h file
 */
#ifndef ORIENT_H_
#define ORIENT_H_


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



#endif /* ORIENT_H_ */
