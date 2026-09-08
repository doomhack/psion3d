/*  pcclock.c - the monotonic clock, isolated from the SIBO type universe.

    Deliberately includes no plib.h. See pcclock.h for why. */

#include "pcclock.h"

#ifdef _WIN32

#include <windows.h>

double pcMonotonicMs(void)
{
	static LARGE_INTEGER freq;
	LARGE_INTEGER now;

	if(freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);

	QueryPerformanceCounter(&now);

	return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

#else

#include <time.h>

double pcMonotonicMs(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

#endif
