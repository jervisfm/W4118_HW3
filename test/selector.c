/*
 * selector.c
 *
 *  Created on: Oct 13, 2012
 *      Author: w4118
 */

#include <stdio.h>
#include <ctype.h> /* for is_digit method */
#include <stdlib.h>
#include <math.h>
#include <gmp.h>
#include "orient_lock.h"


#define FILENAME "integer"

/*
 * Accepts a starting integer as the only argument. When run, your program must
 * take the write lock for when the device is lying face down and write the
 *  integer from the argument to a file called integer in the current working
 *   directory. When the integer has been written, the write lock must be
 *   released and and re-acquired before writing the last integer + 1 to the
same file (overwriting the content of the file). Your program should run until t
erminated using ctrl+c by the user. Before releasing the write lock, you should
 output the integer to standard out.
 *
 */


static mpz_t one;

static void usage() {
	printf("Invalid arguments.\n");
	printf("Usage: selector [integer]\n");
}

static int is_number(char *string)
{
	int i = 0;

	if (string == NULL)
		return 0;

	for (; string[i] != '\0'; ++i) {
		if (!isdigit(string[i]))
			return 0;
	}
	return 1;
}


int main(int argc, char **argv) {
	mpz_init_set_str(one, "1", 10);
	mpz_t counter;
	if (argc != 2 || !is_number(argv[1])) {
		usage();
		exit(-1);
	}

	mpz_init_set_str(counter, argv[1], 10);

	printf("Stepping up orient structs...\n");
	struct orientation_range write_lock;
	/* Only want this to work when device is lying facedown */
	struct dev_orientation write_lock_orient;
	write_lock_orient.azimuth = 180;
	write_lock_orient.pitch = 180;
	write_lock_orient.roll = 0;

	write_lock.orient = write_lock_orient;
	write_lock.azimuth_range =  180;
	write_lock.roll_range = 10;
	write_lock.pitch_range = 10;

	const char *filename = FILENAME;

	printf("About to enter while loop...\n");
	/* Disbale buffering on stdout */
	setvbuf(stdout, NULL, _IONBF, 0);
	while (1) {
		/* Take the write lock */
		printf("Attempting to take write lock...");

		orient_write_lock(&write_lock);
		printf(" Acquired !\n");

		FILE *integer_file = fopen(filename, "w");
		char *buf;
		/* NULL says the mpz_init_set_str function
		 * should auto-allocate the memory for buf */
		buf = mpz_get_str(NULL,10,counter);
		printf("Computed %s\n",buf);
		if(buf == NULL) {
			printf("Warning: Could not get integer\n");
			fclose(integer_file);
			orient_write_unlock(&write_lock);
			continue;
		}

		fprintf(integer_file, "%s", buf);
		printf("Writing %s to integer file\n", buf);

		/* Release write lock */
		fclose(integer_file);
		printf("Attempting to release write lock...");
		orient_write_unlock(&write_lock);
		printf(" Released !\n");

		/* Increment the MPZ number */
		mpz_add(counter, counter, one);

		free(buf);
	}
	printf("Exited while loop");
	return 0;
}
