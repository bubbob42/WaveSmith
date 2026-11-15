/*
 * check.h -- the host tests' one macro. Every test file includes it once.
 */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int checks = 0, failed = 0;

#define CHECK(cond, what) \
	do { \
		checks++; \
		if (!(cond)) { \
			failed++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (what)); \
		} \
	} while (0)

#define CHECK_DONE(name) \
	(printf("%s: %d checks, %d failed\n", (name), checks, failed), failed ? 1 : 0)

#endif
