/*
 * Columbia University
 * COMS W4118 Fall 2010
 * Homework 3 - orientd
 * teamN: UNI, UNI, UNI
 *
 */
#include <bionic/errno.h> /* Google does things a little different...*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../linux-2.6.35-cm/include/linux/akm8976.h"

#include <hardware/hardware.h>
#include <hardware/sensors.h> /* <-- This is a good place to look! */

/* from sensors.c */
#define ID_ACCELERATION   (0)
#define ID_MAGNETIC_FIELD (1)
#define ID_ORIENTATION	  (2)
#define ID_TEMPERATURE	  (3)

#define SENSORS_ACCELERATION   (1<<ID_ACCELERATION)
#define SENSORS_MAGNETIC_FIELD (1<<ID_MAGNETIC_FIELD)
#define SENSORS_ORIENTATION    (1<<ID_ORIENTATION)
#define SENSORS_TEMPERATURE    (1<<ID_TEMPERATURE)

/* the period at which the orientation sensor will update */
#define ORIENTATION_UPDATE_PERIOD_MS 500

/* set to 1 for a bit of debug output */
#if 1
	#define dbg_compass(fmt, ...) printf("compass: " fmt, ## __VA_ARGS__)
#else
	#define dbg_compass(fmt, ...)
#endif

/* helper functions which you should use */
static int open_compass(const struct hw_module_t **hw_module,
			struct sensors_control_device_t **ctrl,
			struct sensors_data_device_t **data);

static void close_compass(struct sensors_control_device_t *ctrl,
			  struct sensors_data_device_t *data);

static void enumerate_sensors(const struct sensors_module_t *sensors);

static int poll_sensor_data(struct sensors_data_device_t *compass_data)
{
	int rc;
	sensors_data_t data;

	rc = compass_data->poll(compass_data, &data);

	if (rc == (int)0x7FFFFFFF) {
		/* wake() was called: someone else is using the device, so
		   we should probably think about letting go for just a bit */
		dbg_compass("wake() called [sleeping a bit]...\n");
		sleep(1);
	} else if (rc < 0) {
		printf("[E]: %d\n", rc);
		return -1;
	}

	if (data.sensor != SENSORS_ORIENTATION)
		return -1;

	/* At this point we should have valid data */
	dbg_compass("Orientation: azimuth=%0.2f, pitch=%0.2f, "
			"roll=%0.2f\n", data.orientation.azimuth,
		data.orientation.pitch, data.orientation.roll);

	return 0;
}


/* entry point of orientd: fill in daemon implementation
   where indicated */
int main(int argc, char **argv)
{
	const struct sensors_module_t *hw_sensors;
	struct sensors_control_device_t *compass_ctrl = NULL;
	struct sensors_data_device_t *compass_data = NULL;

	/* open and initialize the orientation sensor */
	printf("Opening sensors...\n");
	if (open_compass((const struct hw_module_t **)&hw_sensors,
			 &compass_ctrl, &compass_data) < 0)
		return EXIT_FAILURE;

	enumerate_sensors(hw_sensors);


	while (poll_sensor_data(compass_data));

	printf("turn me into a daemon!\n");

	close_compass(compass_ctrl, compass_data);

	/* HINT: you should poll (using the sensors_data_device_t method)
	   for data from a sensor whose sensors_data_t.sensor value is
	   equal to SENSORS_ORIENTATION */

	return EXIT_SUCCESS;
}

/*                DO NOT MODIFY BELOW THIS LINE                    */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int open_compass(const struct hw_module_t **hw_module,
			struct sensors_control_device_t **ctrl,
			struct sensors_data_device_t **data)
{
	int rc;
	native_handle_t *nhandle;

	rc = hw_get_module("sensors", hw_module);
	if (rc < 0) {
		printf("Failed to open sensors module: %d (%s)\n",
			rc, strerror(errno));
		return -1;
	}
	dbg_compass("%s/%s: tag:%d, version:%d.%d author:%s\n",
				(*hw_module)->id, (*hw_module)->name,
				(*hw_module)->tag, (*hw_module)->version_major,
				(*hw_module)->version_minor,
				(*hw_module)->author);

	/* ignore return value here due to bug in
	   sensors.trout module! */
	sensors_control_open(*hw_module, ctrl);

	if (*ctrl == NULL) {
		printf("Unable to open sensor ctrl device (%s)\n",
			strerror(errno));
		return -1;
	}
	dbg_compass("HW Device tag: %d, version: %d\n", (*ctrl)->common.tag,
				(*ctrl)->common.version);


	/* set the event delay period */
	(*ctrl)->set_delay(*ctrl, ORIENTATION_UPDATE_PERIOD_MS);

	/* activate the orientation sensor
	   (requires accelerometer and magnetic field) */
	(*ctrl)->activate(*ctrl, ID_ORIENTATION, 1);
	(*ctrl)->activate(*ctrl, ID_ACCELERATION, 1);
	(*ctrl)->activate(*ctrl, ID_MAGNETIC_FIELD, 1);

	/* open the data device */
	nhandle = (*ctrl)->open_data_source(*ctrl);
	if (nhandle == NULL) {
		printf("Unable to open compass data source (%s)\n",
			strerror(errno));
		return -1;
	}

	/* initialize the data device structure */
	sensors_data_open(*hw_module, data);
	if (*data == NULL) {
		printf("Unable to init sensor data module (%s)\n",
			strerror(errno));
		return -1;
	}

	/* re-assign the data device handle to the data module */
	if ((*data)->data_open(*data, nhandle) < 0) {
		printf("Unable to open compass data handle (%s)\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

static void close_compass(struct sensors_control_device_t *ctrl,
			  struct sensors_data_device_t *data)
{
	sensors_data_close(data);
	sensors_control_close(ctrl);
}

static void enumerate_sensors(const struct sensors_module_t *sensors)
{
	int nr, s;
	const struct sensor_t *slist = NULL;

	nr = sensors->get_sensors_list((struct sensors_module_t *)sensors,
					&slist);
	if (nr < 1 || slist == NULL) {
		printf("no sensors!\n");
		return;
	}

	for (s = 0; s < nr; s++) {
		printf("%s (%s) v%d\n\tHandle:%d, type:%d, max:%0.2f, "
			"resolution:%0.2f\n", slist[s].name, slist[s].vendor,
			slist[s].version, slist[s].handle, slist[s].type,
			slist[s].maxRange, slist[s].resolution);
	}
}

