#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <gmp.h>
#include "prime.h"

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

void factor(mpz_t N)
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


int main(void) {
	int res;
	char *str;
	mpz_t largenum;

	mpz_init_set_str(one, "1", 10);
	mpz_init_set_str(two, "2", 10);

	gmp_randinit_default(randstate);

	mpz_init(largenum);
	mpz_pow_ui(largenum, two, 20);


	while (1) {
		str = mpz_to_str(largenum);
		if (!str)
			return EXIT_FAILURE;

		/*
		 * We simply return the prime number itself if the base is prime.
		 * (We use the GMP probabilistic function with 10 repetitions).
		 */
		res = mpz_probab_prime_p(largenum, 10);
		if (res) {
			printf("%s is a prime number\n", str);
			free(str);
			mpz_add(largenum, largenum, one);
			continue;
		}

		printf("Prime factors for %s are:  ", str);
		free(str);

		factor(largenum);
		printf("\n");

		mpz_add(largenum, largenum, one);
	}

	return EXIT_SUCCESS;
}
