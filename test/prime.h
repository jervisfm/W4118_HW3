
static inline int verify_is_prime(mpz_t num)
{
	mpz_t i;
	mpz_t sqrt;
	mpz_t one;
	mpz_t two;

	mpz_init_set_str(one, "1", 10);
	mpz_init_set_str(two, "2", 10);

	mpz_init_set_str(i, "2", 10);
	mpz_init(sqrt);
	mpz_sqrt(sqrt, num);

	if (mpz_cmp(num, one) == 0)
		return 0;

	if (mpz_cmp(num, two) == 0)
		return 1;

	do {
		if (mpz_divisible_p(num, i))
			return 0;
		mpz_add(i, i, one);
	} while (mpz_cmp(i, sqrt) <= 0);

	return 1;
}

static inline char *mpz_to_str(mpz_t val)
{
	char *str;

	str = mpz_get_str(NULL, 10, val);
	if (!str)
		printf("Cannot allocate string\n");
	return str;
}
