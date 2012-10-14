#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <gmp.h>
#include "prime.h"
#include "orient_lock.h"

#define FILENAME "integer"

static mpz_t one;
static mpz_t two;

static void find_factors(mpz_t base) {
	char *str;
	int res;
	mpz_t i;
	mpz_t half;

	mpz_init_set_str(i, "2", 10);
	mpz_init(half);
	mpz_cdiv_q(half, base, two);

	str = mpz_to_str(base);
	if (!str)
		return;

	/*
	 * We simply return the prime number itself if the base is prime.
	 * (We use the GMP probabilistic function with 10 repetitions).
	 */
	res = mpz_probab_prime_p(base, 10);
	if (res) {
		printf("%s is a prime number\n", str);
		free(str);
		return;
	}

	printf("Prime factors for %s are:  ", str);
	free(str);
	do {
		if (mpz_divisible_p(base, i) && verify_is_prime(i)) {
			str = mpz_to_str(i);
			if (!str)
				return;
			printf(" %s", str);
			free(str);
		}

		mpz_nextprime(i, i);
	} while (mpz_cmp(i, half) <= 0);
	printf("\n");
}

/**
 * Returns only 1 on success and 0 on failure
 */
static int read_integer(mpz_ptr result) {

	const char *filename = FILENAME;
	const int INT_LENGTH = 32;
	char integer_string[INT_LENGTH];
	int num_result = -1;
	int ret_code = 1;

	/* get read lock - only want to work when device is lying facedown */
	struct orientation_range read_lock;
	struct dev_orientation read_lock_orient;
	read_lock_orient.azimuth = 180;
	read_lock_orient.pitch = 180;
	read_lock_orient.roll = 0;

	read_lock.orient = read_lock_orient;
	read_lock.azimuth_range =  180;
	read_lock.roll_range = 10;
	read_lock.pitch_range = 10;

	printf("Attempting to take read lock...");
	orient_read_lock(&read_lock);
	printf("Acquired ! \n");
	FILE *integer_file = fopen(filename, "r");
	if (integer_file == NULL) {
		ret_code = 0;
		perror("Integer file not found: '%s'\n");
	} else if (fgets(integer_string,INT_LENGTH, integer_file) != NULL) {
		num_result = atoi(integer_string);
		printf("Read Integer: %d\n", num_result);
		mpz_init_set_d(result, num_result);
		ret_code = 1;
	} else {
		printf("Warning: Reading integer file failed\n");
		ret_code = 0;
	}
	fclose(integer_file);
	printf("Attempting to release read lock...");
	orient_read_unlock(&read_lock);
	printf("Released ! \n");
	return ret_code;
}


int main(int argc, const char *argv[])
{
	mpz_t integer;
	mpz_t largenum;

	mpz_init_set_str(one, "1", 10);
	mpz_init_set_str(two, "2", 10);
	mpz_init_set_d(integer, 2020);

	mpz_init(largenum);
	mpz_pow_ui(largenum, two, 20);

	while (1) {
		if (read_integer(integer))
			find_factors(integer);
		//mpz_add(largenum, largenum, one);
	}
	return EXIT_SUCCESS;
}
