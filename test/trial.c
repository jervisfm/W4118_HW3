#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <gmp.h>
#include "prime.h"

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

int main(int argc, const char *argv[])
{
	mpz_t largenum;

	mpz_init_set_str(one, "1", 10);
	mpz_init_set_str(two, "2", 10);

	mpz_init(largenum);
	mpz_pow_ui(largenum, two, 20);

	while (1) {
		find_factors(largenum);
		mpz_add(largenum, largenum, one);
	}

	return EXIT_SUCCESS;
}
