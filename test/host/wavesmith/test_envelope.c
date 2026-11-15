/*
 * test_envelope.c -- envelope.c's fixed point against the same sums in
 * doubles: the three curves at 2049 places (ends exact, rising, near the
 * formula), every tenth of a dB both ways, the clamp of a product, and
 * the samples -- in their byte order, every channel, held at full scale
 * and counted -- against a reference rounded the same way, with the level
 * ENV_UNITY leaving every byte alone.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "envelope.h"

static double formula(UWORD shape, double x)
{
	if (shape == ENV_SHAPE_LINEAR) return x;
	if (shape == ENV_SHAPE_EVEN) return x * x;
	return (1.0 - cos(M_PI * x)) / 2.0;
}

int main(void)
{
	static UBYTE buf[4096], ref[4096];
	struct EnvApply ea;
	UWORD shape, prev, v, l;
	ULONG u, i, clipped, k;
	int worst = 0, d, bad;
	WORD t;
	double want;
	long s, w;

	/* the curves */
	for (shape = 0; shape < ENV_SHAPES; shape++) {
		CHECK(Env_Curve(shape, 0) == 0, "a curve starts at nothing");
		CHECK(Env_Curve(shape, ENV_U_MAX) == ENV_UNITY, "a curve ends as recorded");
		prev = 0;
		bad = 0;
		worst = 0;
		for (u = 0; u <= ENV_U_MAX; u += 32) {
			v = Env_Curve(shape, (UWORD) u);
			if (v < prev) bad++;
			prev = v;
			d = (int) v - (int) lround(4096.0 * formula(shape, (double) u / ENV_U_MAX));
			if (abs(d) > worst) worst = abs(d);
		}
		CHECK(!bad, "a curve never falls");
		CHECK(worst <= 2, "a curve within 2 of 4096 of its formula");
		printf("  shape %u: at most %d off its formula\n", shape, worst);
	}
	CHECK(Env_Curve(ENV_SHAPE_EVEN, 32768) == 1024, "Even at the middle: a quarter");
	CHECK(abs((int) Env_Curve(ENV_SHAPE_SCURVE, 32768) - 2048) <= 1, "the S-curve at the middle: a half");

	/* the dB steps */
	CHECK(Env_FromTenths(0) == ENV_UNITY, "0 dB as recorded");
	bad = 0;
	worst = 0;
	prev = 0;
	for (t = ENV_TENTHS_MIN; t <= ENV_TENTHS_MAX; t++) {
		v = Env_FromTenths(t);
		want = 4096.0 * pow(10.0, t / 200.0);
		d = (int) v - (int) lround(want);
		if (abs(d) > worst) worst = abs(d);
		if (v <= prev) bad++;
		prev = v;
		if (Env_Tenths(v) != t) bad++;
	}
	CHECK(worst <= 1, "every tenth within 1 of 4096 of 10^(t/200)");
	CHECK(!bad, "every tenth a step of its own, read back as itself");
	CHECK(Env_FromTenths(ENV_TENTHS_MAX + 50) == Env_FromTenths(ENV_TENTHS_MAX), "above the range: +18 dB");
	CHECK(Env_FromTenths(ENV_TENTHS_MIN - 50) == Env_FromTenths(ENV_TENTHS_MIN), "below the range: -24 dB");
	CHECK(Env_Tenths(0) == ENV_TENTHS_MIN - 1, "nothing at all below the range");
	CHECK(Env_Tenths(ENV_MAX) == ENV_TENTHS_MAX, "the most: +18 dB");
	CHECK(Env_Tenths(8192) == 60, "twice the amplitude: +6.0 dB");

	/* the product */
	CHECK(Env_Mul(ENV_UNITY, 1234) == 1234 && Env_Mul(1234, ENV_UNITY) == 1234, "as recorded times x: x");
	CHECK(Env_Mul(8192, 8192) == 16384, "twice twice: four times");
	CHECK(Env_Mul(ENV_MAX, ENV_MAX) == ENV_MAX, "the product held at the most");
	CHECK(Env_Mul(2048, 0) == 0, "times nothing: nothing");

	/* the samples: the level ENV_UNITY touches nothing */
	srand(1063);
	for (i = 0; i < sizeof(buf); i++) buf[i] = (UBYTE) rand();
	memcpy(ref, buf, sizeof(buf));
	ea.ea_Buf = buf;
	ea.ea_Frames = sizeof(buf) / 4;
	ea.ea_Channels = 2;
	ea.ea_Level = ENV_UNITY;
	ea.ea_Clipped = 0;
	Env_Apply(&ea);
	CHECK(!memcmp(buf, ref, sizeof(buf)) && !ea.ea_Clipped, "as recorded: every byte as it was");

	/* any other level: each sample, little-endian, rounded half up,
	   held at full scale and counted */
	for (k = 0; k < 400; k++) {
		l = (UWORD) (k == 0 ? 8192 : k == 1 ? ENV_MAX : k == 2 ? 1 : rand() % 32768);
		for (i = 0; i < sizeof(buf); i++) buf[i] = (UBYTE) rand();
		memcpy(ref, buf, sizeof(buf));
		ea.ea_Buf = buf;
		ea.ea_Frames = (ULONG) (k % 3 == 0 ? sizeof(buf) / 2 : sizeof(buf) / 4);
		ea.ea_Channels = (UWORD) (k % 3 == 0 ? 1 : 2);
		ea.ea_Level = l;
		ea.ea_Clipped = 0;
		Env_Apply(&ea);
		bad = 0;
		clipped = 0;
		for (i = 0; i < sizeof(buf); i += 2) {
			s = (short) (ref[i] | (ref[i + 1] << 8));
			w = (long) floor((s * (double) l + 2048.0) / 4096.0);
			if (w > 32767) { w = 32767; clipped++; }
			if (w < -32768) { w = -32768; clipped++; }
			if ((short) (buf[i] | (buf[i + 1] << 8)) != (short) w) bad++;
		}
		if (bad || clipped != ea.ea_Clipped) {
			printf("  level %u: %d samples wrong, %lu clipped, %lu counted\n", l, bad,
			       (unsigned long) clipped, (unsigned long) ea.ea_Clipped);
			CHECK(0, "the samples at a level");
			break;
		}
	}
	CHECK(k == 400, "400 random levels: every sample as the reference, the clipped counted");

	return CHECK_DONE("test_envelope");
}
