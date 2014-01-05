// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

An assert function which contains a breakpoint, for ease of debugging.

*/

#pragma once

#ifndef NDEBUG
#include <stdio.h>
static void _assert_func(const char *file, int line, const char *func, const char *exp) {
	fprintf(stderr, "assertion \"%s\" failed: file \"%s\", line %d, function: %s\n",
			exp, file, line, func);
	fflush(stderr);
#ifdef AMIGA
	exit(1);
#else
	__asm volatile ("int3;");
#endif
}
#undef assert
#define assert(__e) ((__e) ? (void)0 : _assert_func (__FILE__, __LINE__, __PRETTY_FUNCTION__, #__e))
#endif
