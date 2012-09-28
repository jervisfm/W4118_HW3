/* sensorsim.c
 * Columbia University
 * COMS W4118 Fall 2010
 * Android emulator, command-line sensor simulator (data injection)
 *
 */
#include <bionic/errno.h>

#define __USE_GNU
#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cutils/native_handle.h>
#include <cutils/sockets.h>

#include <hardware/sensors.h>
#include <hardware/qemud.h>

#if 0
	#define dbg(fmt, ...) printf("[D:%s:%d] " fmt "\n", __FUNCTION__, \
					__LINE__, ## __VA_ARGS__)
	#define info(fmt, ...) printf("[I:%s:%d] " fmt "\n", __FUNCTION__, \
					__LINE__, ## __VA_ARGS__)
	#define err(fmt, ...) printf("[E:%s:%d] " fmt "\n", __FUNCTION__, \
					__LINE__, ## __VA_ARGS__)
#else
	#define dbg(...)
	#define info(fmt, ...) printf("[I] " fmt "\n", ## __VA_ARGS__)
	#define err(fmt, ...) printf("[E] " fmt "\n", ## __VA_ARGS__)
#endif

/* defines the rate at which sensorsim will poll the
   sensorsimulator.java application across the network */
#define SENSOR_UPDATE_PERIOD_MS 500

#define DAEMON_LOG_FILE "/data/anr/sensorsim.log"

/*
 * QEMU tokens and macros
 */
#define SENSORS_SERVICE_NAME "sensors"

#define MAX_NUM_SENSORS 4

#define SUPPORTED_SENSORS  ((1<<MAX_NUM_SENSORS)-1)

#define ID_BASE           SENSORS_HANDLE_BASE
#define ID_ACCELERATION   (ID_BASE+0)
#define ID_MAGNETIC_FIELD (ID_BASE+1)
#define ID_ORIENTATION    (ID_BASE+2)
#define ID_TEMPERATURE    (ID_BASE+3)

#define SENSORS_ACCELERATION    (1 << ID_ACCELERATION)
#define SENSORS_MAGNETIC_FIELD  (1 << ID_MAGNETIC_FIELD)
#define SENSORS_ORIENTATION     (1 << ID_ORIENTATION)
#define SENSORS_TEMPERATURE     (1 << ID_TEMPERATURE)

#define ID_CHECK(x) ((unsigned)((x)-ID_BASE) < MAX_NUM_SENSORS && (x) >= 0)

#define SENSORS_LIST  \
	SENSOR_(ACCELERATION, "acceleration") \
	SENSOR_(MAGNETIC_FIELD, "magnetic field") \
	SENSOR_(ORIENTATION, "orientation") \
	SENSOR_(TEMPERATURE, "temperature") \

/*
 * structure which defines sensor names and IDs
 * (must match the one in sensors_qemu.c)
 */
static const struct {
	const char *name;
	int         id;
} s_sensorIds[MAX_NUM_SENSORS] = {
	#define SENSOR_(x, y)  { y, ID_##x },
	SENSORS_LIST
	#undef  SENSOR_
};

/* informational description of each sensor */
static const struct sensor_t s_availableSensors[];

/*
 * sensorsimulator interface functions
 */
static int sensorsimulator_open_socket(char *ip, int port);
static int sensorsimulator_enable_sensor(int sock, int sensor_id);
static int sensorsimulator_get_params(int sock, int sensor_id,
				      float *x, float *y, float *z);

static const char *sensorId_to_name(int id)
{
	int nn;
	for (nn = 0; nn < MAX_NUM_SENSORS; nn++)
		if (id == s_sensorIds[nn].id)
			return s_sensorIds[nn].name;
	return "<UNKNOWN>";
}

#if 0
static int sensorId_from_name(const char *name)
{
	int nn;

	if (name == NULL)
		return -1;

	for (nn = 0; nn < MAX_NUM_SENSORS; nn++)
		if (!strcmp(name, s_sensorIds[nn].name))
			return s_sensorIds[nn].id;

	return -1;
}
#endif


#define get_param(PROMPT, LINE, FMT, VAR_PTR) \
	do { \
		char c; \
		int idx = 0; \
		printf(PROMPT); \
		fflush(NULL); \
		do { \
			c = getchar(); \
			(LINE)[idx++] = c; \
		} while (c != '\n' && c != EOF && \
			 idx < sizeof(LINE)); \
		if (c == EOF || idx <= 1) \
			return -1; \
		(LINE)[--idx] = 0; \
		idx = sscanf(LINE, FMT, VAR_PTR); \
		if (idx < 1) \
			return -1; \
	} while (0);

int get_parameters(int *sensor_id, float *x, float *y, float *z)
{
	char line[64]; /* only holds numeric input */
	/* sensor ID */
	get_param("SensorID: ", line, "%d", sensor_id);
	get_param("   |-> X: ", line, "%g", x);
	get_param("   |-> Y: ", line, "%g", y);
	get_param("   `-> Z: ", line, "%g", z);
	return 0;
}


static void emu_sensors_list(int qfd)
{
	char buffer[12];
	int  mask, nn, count;

	int  ret;
	if (qfd < 0) {
		err("no qemud connection");
		return;
	}
	ret = qemud_channel_send(qfd, "list-sensors", -1);
	if (ret < 0) {
		err("could not query sensor list: %s", strerror(errno));
		return;
	}
	ret = qemud_channel_recv(qfd, buffer, sizeof(buffer)-1);
	if (ret < 0) {
		err("could not receive sensor list: %s", strerror(errno));
		return;
	}
	buffer[ret] = 0;

	/* the result is a integer used as a mask for available sensors */
	mask  = atoi(buffer);
	count = 0;
	for (nn = 0; nn < MAX_NUM_SENSORS; nn++) {
		const struct sensor_t *s;
		if (((1 << nn) & mask) == 0)
			continue;
		/* print info about the enabled sensor */
		s = &s_availableSensors[nn];
		info("\tSensorID: %d", s->handle);
		info("\t%s (%s)", s->name, s->vendor);
		info("\tmax=%0.2f, resolution=%0.2f\n", s->maxRange,
			s->resolution);
	}

	return;
}

void daemonize(void)
{
	int rc = fork();
	if (rc < 0) {
		err("Failed to daemonize (%s)...\n", strerror(errno));
		return;
	}
	if (rc > 0) {
		info("Daemonized.\n");
		exit(EXIT_SUCCESS);
	}
	/* re-open STDIN/STDOUT file handles */
	rc = open(DAEMON_LOG_FILE, O_CREAT | O_APPEND | O_RDWR);
	if (rc < 0) {
		err("Could not open log file!");
		return;
	}
	dup2(rc, STDIN_FILENO);
	dup2(rc, STDOUT_FILENO);
	dup2(rc, STDERR_FILENO);
	close(rc);
	{
		char tbuf[32];
		time_t tm = time(NULL);
		info("sensorsim starting: %s\n", ctime_r(&tm, tbuf));
	}
}

static int s_should_exit;
static void sighandler(int sig);
extern sighandler_t sysv_signal(int, sighandler_t);
static inline sighandler_t bionic_signal(int s, sighandler_t f)
{
	return sysv_signal(s, f);
}


/*
 * Main sensor sim routine
 */
int main(int argc, char **argv)
{
	int rc;
	int qfd;
	char command[128];
	char ip[16];
	int port;
	int ssock = -1; /* simulator socket */
	int sensor_id = -1;

	(void)bionic_signal(SIGPIPE, sighandler);
	(void)bionic_signal(SIGKILL, sighandler);
	(void)bionic_signal(SIGQUIT, sighandler);

	qfd = qemud_channel_open(SENSORS_SERVICE_NAME);
	if (qfd < 0) {
		err("Can't get a connection to qemud!\n");
		return EXIT_FAILURE;
	}

	/* list available / enabled sensors */
	info("Emulator-enabled sensors:\n");
	emu_sensors_list(qfd);

	/* if we're passed arguments, use them to connect to
	   a sensorsimulator.java instance somewhere on the network */
	if (argc > 2) {
		strncpy(ip, argv[1], sizeof(ip));
		port = atoi(argv[2]);
		ssock = sensorsimulator_open_socket(ip, port);
		if (ssock < 0) {
			err("Error connecting to %s:%d (%s)", ip, port,
				strerror(errno));
			goto exit_socket_failure;
		}
		if (argc > 3)
			sensor_id = atoi(argv[3]);
		else {
			/* ask the user for a sensor ID */
			char line[64];
			get_param("SensorID: ", line, "%d", &sensor_id);
		}
		if (!ID_CHECK(sensor_id)) {
			err("Invalid SensorID");
			goto exit_data_error;
		}
		if (sensorsimulator_enable_sensor(ssock, sensor_id) < 0) {
			err("Error enabling SensorID=%d (%s)", sensor_id,
			     strerror(errno));
			goto exit_data_error;
		}
		daemonize();
	}

	while (!s_should_exit) {
		float x, y, z;
		char *sensor_name;

		if (ssock > 0) {
			usleep(SENSOR_UPDATE_PERIOD_MS * 1000);
			if (sensorsimulator_get_params(ssock, sensor_id,
							&x, &y, &z) < 0) {
				err("Error receiving data from sensorsimulator"
					" (%s)", strerror(errno));
				goto exit_data_error;
			}
		} else {
			if (get_parameters(&sensor_id, &x, &y, &z) < 0) {
				err("Error receiving parameters from cmdline");
				continue;
			}
			if (!ID_CHECK(sensor_id)) {
				err("Invalid SensorID");
				continue;
			}
		}

		sensor_name = (char *)sensorId_to_name(sensor_id);

		if (ssock < 0 && sensor_id == ID_ORIENTATION) {
			/* the user just input X,Y,Z (pitch,roll,azimuth)
			   the qemu pipe is expecting Z,X,Y (azimuth,pitch,roll)
			 */
			float tmpX = x;
			float tmpY = y;
			x = z;
			y = tmpX;
			z = tmpY;
		}

		snprintf(command, sizeof(command), "force-sensor:%d:%g:%g:%g:",
			 sensor_id, x, y, z);

		dbg("simulating %s event: %s\n", sensor_name, command);

		rc = qemud_channel_send(qfd, command, -1);
		if (rc < 0) {
			err("Error sending emulator command: %s\n",
				strerror(errno));
			sleep(2); /* let the user know there was a problem */
		}
	}

	{
		char tbuf[32];
		time_t tm = time(NULL);
		info("sensorsim exiting at %s\n", ctime_r(&tm, tbuf));
	}

	close(ssock);
	close(qfd);
	return EXIT_SUCCESS;

exit_data_error:
	close(ssock);
exit_socket_failure:
	close(qfd);
	return EXIT_FAILURE;
}

void sighandler(int sig)
{
	switch (sig) {
	default:
		break;
	case SIGPIPE:
	case SIGKILL:
	case SIGQUIT:
		info("Caught %d signal: flagging exit.", sig);
		s_should_exit = 1;
		break;
	}
}

/*
 * an inefficient way to read a line from a socket
 */
int socket_readline(int sockfd, char **line, size_t *linesz)
{
	static const int INIT_BUF_SZ = 128;
	int pos = 0;
	if (*line == NULL) {
		*line = malloc(INIT_BUF_SZ);
		*linesz = INIT_BUF_SZ;
	}

	while (1) {
		char ch;
		if (recv(sockfd, &ch, sizeof(ch), 0) < 0)
			return -1;

		if (pos == *linesz) {
			*linesz *= 2;
			*line = realloc(*line, *linesz);
		}

		if (ch == '\n') {
			/* overwrite \r (combines \r\n in one!) */
			if ((*line)[pos-1] == '\r')
				--pos;
			(*line)[pos] = '\0';
			return pos; /* don't include the NULL (strlen style) */
		}

		(*line)[pos++] = ch;
	}
}

static int sensorsimulator_open_socket(char *ip, int port)
{
	struct sockaddr_in addr;
	char *line = NULL;
	size_t linesz = 0;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	info("Connecting to %s:%d...\n", ip, port);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_aton(ip, &addr.sin_addr) < 0)
		goto sock_err;

	if (connect(sock, &addr, sizeof(addr)) < 0)
		goto sock_err;

	/* read the server response string to verify that we've
	   connected to the sensorsimulator application */
	if (socket_readline(sock, &line, &linesz) < 0)
		goto sock_err;

	if (strcmp(line, "SensorSimulator") != 0) {
		err("Received garbled server response.\n"
			"\tExpected: 'SensorSimulator'\n\tReceived: %s", line);
		goto sock_err;
	}

	return sock;

sock_err:
	free(line);
	close(sock);
	return -1;
}

static int sensorsimulator_enable_sensor(int sock, int sensor_id)
{
	char cmd[128];
	char *line = NULL;
	size_t linesz = 0;

	snprintf(cmd, sizeof(cmd), "enableSensor()\n%s\n",
		 sensorId_to_name(sensor_id));

	if (send(sock, cmd, strlen(cmd), 0) < 0)
		return -1;

	if (socket_readline(sock, &line, &linesz) < 0)
		return -1;

	dbg("[Enable %d]: Previous state was: %s\n", sensor_id, line);
	free(line);
	return 0;
}

static int sensorsimulator_get_params(int sock, int sensor_id,
				float *x, float *y, float *z)
{
	char cmd[128];
	char *line = NULL;
	size_t linesz = 0;
	int numvals = 0, n = 0, rc = 0;

	snprintf(cmd, sizeof(cmd), "readSensor()\n%s\n",
		 sensorId_to_name(sensor_id));

	if (send(sock, cmd, strlen(cmd), 0) < 0)
		return -1;

	if (socket_readline(sock, &line, &linesz) < 0)
		return -1;

	numvals = atoi(line);
	if (numvals > 3 ||
	    (numvals < 3 && (sensor_id != ID_TEMPERATURE))) {
		err("Invalid number of data points for sensor %d, "
			"the server sent me %d\n", sensor_id, numvals);
		errno = EINVAL;
		rc = -1;
		goto exit_getparms;
	}
	for (n = 0; n < numvals; ++n) {
		float val;
		if (socket_readline(sock, &line, &linesz) < 0) {
			rc = -1;
			goto exit_getparms;
		}
		if (sscanf(line, "%g", &val) < 1) {
			err("Invalid data line: '%s'", line);
			errno = EINVAL;
			rc = -1;
			goto exit_getparms;
		}
		switch (n) {
		case 0:
			*x = val;
			break;
		case 1:
			*y = val;
			break;
		case 2:
			*z = val;
			break;
		default:
			break;
		}
	}

	dbg("Sensor values: X=%g, Y=%g, Z=%g\n", *x, *y, *z);

exit_getparms:
	free(line);
	return rc;
}


/* List of all supported sensors (taken from sensors_qemu.c in Android SDK)
 * This is used to print interesting information about enabled sensors.
 */
static const struct sensor_t s_availableSensors[] = {
	{ .name	   = "Goldfish 3-axis Accelerometer",
	  .vendor	 = "The Android Open Source Project",
	  .version	= 1,
	  .handle	 = ID_ACCELERATION,
	  .type	   = SENSOR_TYPE_ACCELEROMETER,
	  .maxRange   = 2.8f,
	  .resolution = 1.0f/4032.0f,
	  .power	  = 3.0f,
	  .reserved   = {}
	},
	{ .name	   = "Goldfish 3-axis Magnetic field sensor",
	  .vendor	 = "The Android Open Source Project",
	  .version	= 1,
	  .handle	 = ID_MAGNETIC_FIELD,
	  .type	   = SENSOR_TYPE_MAGNETIC_FIELD,
	  .maxRange   = 2000.0f,
	  .resolution = 1.0f,
	  .power	  = 6.7f,
	  .reserved   = {}
	},
	{ .name	   = "Goldfish Orientation sensor",
	  .vendor	 = "The Android Open Source Project",
	  .version	= 1,
	  .handle	 = ID_ORIENTATION,
	  .type	   = SENSOR_TYPE_ORIENTATION,
	  .maxRange   = 360.0f,
	  .resolution = 1.0f,
	  .power	  = 9.7f,
	  .reserved   = {}
	},
	{ .name	   = "Goldfish Temperature sensor",
	  .vendor	 = "The Android Open Source Project",
	  .version	= 1,
	  .handle	 = ID_TEMPERATURE,
	  .type	   = SENSOR_TYPE_TEMPERATURE,
	  .maxRange   = 80.0f,
	  .resolution = 1.0f,
	  .power	  = 0.0f,
	  .reserved   = {}
	},
};
