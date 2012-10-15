#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <gmp.h>
#include "prime.h"

#define FILENAME "integer"

static mpz_t one;
static mpz_t two;
static gmp_randstate_t randstate;

static void rho(mpz_t R, mpz_t N)
{
	mpz_t divisor;
	mpz_t c;
	mpz_t x;
	mpz_t xx;
	mpz_t abs;

	mpz_init(divisor);
	mpz_init(c);
	mpz_init(x);
	mpz_init(xx);
	mpz_init(abs);

	mpz_urandomm(c, randstate, N);
	mpz_urandomm(x, randstate, N);
	mpz_set(xx, x);


	/* check divisibility by 2 */
	if (mpz_divisible_p(N, two)) {
		mpz_set(R, two);
		return;
	}

	do {
		/* Do this with x */
		mpz_mul(x, x, x);
		mpz_mod(x, x, N);
		mpz_add(x, x, c);
		mpz_mod(x, x, N);

		/* First time with xx */
		mpz_mul(xx, xx, xx);
		mpz_mod(xx, xx, N);
		mpz_add(xx, xx, c);
		mpz_mod(xx, xx, N);

		/* Second time with xx */
		mpz_mul(xx, xx, xx);
		mpz_mod(xx, xx, N);
		mpz_add(xx, xx, c);
		mpz_mod(xx, xx, N);

		mpz_sub(abs, x, xx);
		mpz_abs(abs, abs);
		mpz_gcd(divisor, abs, N);
	} while (mpz_cmp(divisor, one) == 0);

	mpz_set(R, divisor);
}

static void factor(mpz_t N)
{
	int res;
	char *str;
	mpz_t divisor;
	mpz_t next;

	mpz_init(divisor);
	mpz_init(next);


	if (mpz_cmp(N, one) == 0)
		return;

	res = mpz_probab_prime_p(N, 10);
	if (res) {
		str = mpz_to_str(N);
		if (!str)
			return;

		printf(" %s", str);
		free(str);
		return;
	}

	rho(divisor, N);

	factor(divisor);
	mpz_tdiv_q(next, N, divisor);
	factor(next);
}



static void print_error(const char *err)
{
	printf("error: %s\n", err);
}

/*
 * Reads line from given FILE
 * Caller responsible for freeing returned line string
 */
static char *read_line(FILE* input)
{
	char c = '\0';
	char *line = (char *) calloc(1, sizeof(char));

	/*memory allocation failed*/
	if (line == NULL) {
		print_error("ReadLine memory allocation failed");
		return NULL;
	}

	/*current index into line*/
	int i = 0;

	/* this includes the null character.
	 * so an empty string has size of 1
	 */
	int string_size = 1;

	while (fscanf(input,"%c", &c) > 0 && c != '\n') {
		++string_size;
		int new_buffersize = sizeof(char) * (string_size);
		line = realloc(line, new_buffersize);
		if (line == NULL) {
			print_error("ReadLine memory reallocation failed");
			return NULL;
		}
		line[i] = c;
		line[i + 1] = '\0';
		++i;
	}
	if (c == '\0') {
		print_error("Reading character from line failed");
		return NULL;
	}
	return line;
}


/**
 * Returns only 1 on success and 0 on failure
 */
static int read_integer(mpz_ptr result) {

	const char *filename = FILENAME;
	char *integer_string = NULL;
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
	} else if ( (integer_string = read_line(integer_file)) != NULL) {
		printf("Read Integer: %s\n", integer_string);
		mpz_init_set_str(result, integer_string, 10);
		ret_code = 1;
		free(integer_string);
	} else {
		printf("Warning: Reading integer file failed\n");
		ret_code = 0;
	}
	if (integer_file != NULL)
		fclose(integer_file);

	printf("Attempting to release read lock...");
	orient_read_unlock(&read_lock);
	printf("Released ! \n");
	return ret_code;
}

int main(void) {
	int res;
	char *str;
	mpz_t largenum;
	mpz_t result;

	mpz_init_set_str(one, "1", 10);
	mpz_init_set_str(two, "2", 10);

	gmp_randinit_default(randstate);

	mpz_init(largenum);
	mpz_pow_ui(largenum, two, 20);


	while (1) {
		if (!read_integer(result))
			continue;

		str = mpz_to_str(result);
		if (!str)
			return EXIT_FAILURE;

		/*
		 * We simply return the prime number itself if the base is prime.
		 * (We use the GMP probabilistic function with 10 repetitions).
		 */
		res = mpz_probab_prime_p(result, 10);
		if (res) {
			printf("%s is a prime number\n", str);
			free(str);
			// mpz_add(largenum, largenum, one);
			continue;
		}

		printf("Prime factors for %s are:  ", str);
		free(str);

		factor(result);
		printf("\n");

		//mpz_add(largenum, largenum, one);
	}

	return EXIT_SUCCESS;
}
