/*
 * envelope.c -- the level of the sound (envelope.h)
 */

#include <exec/types.h>

#include "envelope.h"

/* (1 - cos(pi i / 256)) / 2 in 4.12, i 0 .. 256 */
static const UWORD scurve[257] = {
	0, 0, 1, 1, 2, 4, 6, 8, 10, 12, 15, 19,
	22, 26, 30, 35, 39, 44, 50, 55, 61, 68, 74, 81,
	88, 96, 103, 111, 120, 128, 137, 146, 156, 166, 176, 186,
	197, 208, 219, 230, 242, 254, 266, 279, 291, 304, 318, 331,
	345, 359, 374, 388, 403, 418, 433, 449, 465, 481, 497, 514,
	531, 548, 565, 582, 600, 618, 636, 654, 673, 691, 710, 729,
	749, 768, 788, 808, 828, 848, 869, 889, 910, 931, 952, 974,
	995, 1017, 1039, 1060, 1083, 1105, 1127, 1150, 1172, 1195, 1218, 1241,
	1264, 1288, 1311, 1334, 1358, 1382, 1406, 1429, 1453, 1478, 1502, 1526,
	1550, 1575, 1599, 1624, 1648, 1673, 1698, 1723, 1747, 1772, 1797, 1822,
	1847, 1872, 1897, 1922, 1948, 1973, 1998, 2023, 2048, 2073, 2098, 2123,
	2148, 2174, 2199, 2224, 2249, 2274, 2299, 2324, 2349, 2373, 2398, 2423,
	2448, 2472, 2497, 2521, 2546, 2570, 2594, 2618, 2643, 2667, 2690, 2714,
	2738, 2762, 2785, 2808, 2832, 2855, 2878, 2901, 2924, 2946, 2969, 2991,
	3013, 3036, 3057, 3079, 3101, 3122, 3144, 3165, 3186, 3207, 3227, 3248,
	3268, 3288, 3308, 3328, 3347, 3367, 3386, 3405, 3423, 3442, 3460, 3478,
	3496, 3514, 3531, 3548, 3565, 3582, 3599, 3615, 3631, 3647, 3663, 3678,
	3693, 3708, 3722, 3737, 3751, 3765, 3778, 3792, 3805, 3817, 3830, 3842,
	3854, 3866, 3877, 3888, 3899, 3910, 3920, 3930, 3940, 3950, 3959, 3968,
	3976, 3985, 3993, 4000, 4008, 4015, 4022, 4028, 4035, 4041, 4046, 4052,
	4057, 4061, 4066, 4070, 4074, 4077, 4081, 4084, 4086, 4088, 4090, 4092,
	4094, 4095, 4095, 4096, 4096
};

/* 10^(d / 20) in 4.12 for whole dB d from -24 to +18 */
static const UWORD whole_db[43] = {
	258, 290, 325, 365, 410, 460, 516, 579, 649, 728, 817, 917, 1029, 1154, 1295,
	1453, 1631, 1830, 2053, 2303, 2584, 2900, 3254, 3651, 4096, 4596, 5157, 5786,
	6492, 7284, 8173, 9170, 10289, 11544, 12953, 14533, 16306, 18296, 20529, 23034,
	25844, 28997, 32536
};

/* 10^(r / 200) in 1.15 for r tenths, 0 .. 9 */
static const UWORD tenth_up[10] = {
	32768U, 33147U, 33531U, 33920U, 34312U, 34710U, 35112U, 35518U, 35929U, 36345U
};

UWORD Env_Curve(UWORD shape, UWORD u)
{
	ULONG i, f;

	switch (shape) {
	case ENV_SHAPE_LINEAR:
		return (UWORD) (((ULONG) u * ENV_UNITY + ENV_U_MAX / 2) / ENV_U_MAX);
	case ENV_SHAPE_SCURVE:
		/* the table's 256 steps, the place between two of them by
		   straight line */
		i = (ULONG) u >> 8;
		f = (ULONG) u & 255UL;
		if (i >= 256) return (UWORD) ENV_UNITY;
		return (UWORD) (scurve[i] + (((ULONG) (scurve[i + 1] - scurve[i]) * f + 128UL) >> 8));
	default:
		/* Even: u squared, 16 x 16 bits into 32, down to 4.12 -- halved
		   first, or the rounding's half passes 32 bits near the top */
		if (u == ENV_U_MAX) return (UWORD) ENV_UNITY;
		return (UWORD) (((((ULONG) u * (ULONG) u) >> 1) + (1UL << 18)) >> 19);
	}
}

UWORD Env_FromTenths(WORD tenths)
{
	LONG t = tenths, d, r;
	ULONG v;

	if (t < ENV_TENTHS_MIN) t = ENV_TENTHS_MIN;
	if (t > ENV_TENTHS_MAX) t = ENV_TENTHS_MAX;
	/* the whole dB below, the tenths above it */
	d = t >= 0 ? t / 10 : -((-t + 9) / 10);
	r = t - d * 10;
	v = ((ULONG) whole_db[d + 24] * (ULONG) tenth_up[r] + 16384UL) >> 15;
	return (UWORD) (v > ENV_MAX ? ENV_MAX : v);
}

WORD Env_Tenths(UWORD level)
{
	WORD lo = ENV_TENTHS_MIN, hi = ENV_TENTHS_MAX, mid;

	if (!level) return (WORD) (ENV_TENTHS_MIN - 1);
	if (level <= Env_FromTenths(lo)) return lo;
	if (level >= Env_FromTenths(hi)) return hi;
	/* the steps rise: the last at or below level, then the nearer one */
	while (hi - lo > 1) {
		mid = (WORD) ((lo + hi) / 2);
		if (Env_FromTenths(mid) <= level) lo = mid;
		else hi = mid;
	}
	return (WORD) (level - Env_FromTenths(lo) <= Env_FromTenths(hi) - level ? lo : hi);
}

UWORD Env_Mul(UWORD a, UWORD b)
{
	ULONG v = ((ULONG) a * (ULONG) b + 2048UL) >> 12;

	return (UWORD) (v > ENV_MAX ? ENV_MAX : v);
}

void Env_Apply(struct EnvApply *ea)
{
	UBYTE *p = ea->ea_Buf;
	ULONG n = ea->ea_Frames * ea->ea_Channels;
	WORD level = (WORD) ea->ea_Level, s;
	LONG v;

	if (ea->ea_Level == ENV_UNITY) return;
	while (n--) {
		s = (WORD) (p[0] | ((UWORD) p[1] << 8));
		/* 16 x 16 into 32 bits: a MULS.W, also on a 68000 */
		v = ((LONG) s * (LONG) level + 2048L) >> 12;
		if (v > 32767L) {
			v = 32767L;
			ea->ea_Clipped++;
		} else if (v < -32768L) {
			v = -32768L;
			ea->ea_Clipped++;
		}
		p[0] = (UBYTE) v;
		p[1] = (UBYTE) ((UWORD) v >> 8);
		p += 2;
	}
}

UBYTE Env_LookDb(UBYTE look, UWORD level)
{
	LONG db;
	WORD t;

	if (look == ENV_LOOK_NONE || level == ENV_UNITY) return look;
	if (!level) return ENV_LOOK_NONE;
	/* dB below full scale, less the level's tenths (rounded to whole dB) */
	t = Env_Tenths(level);
	db = (LONG) look - ((LONG) t + (t >= 0 ? 5 : -5)) / 10;
	if (db < 0) db = 0;
	if (db > 90) db = 90;
	return (UBYTE) db;
}
