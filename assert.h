// Copyright 1999-2014 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

An assert function which contains a breakpoint, for ease of debugging.

*/

#pragma once

void internal_error() {
	fprintf(stderr,
		"\nShrinkler has encountered an internal error.\n"
		"Please send a bug report to blueberry@loonies.dk,\n"
		"providing the file you tried to compress.\n"
		"\n"
		"Thanks, and apologies for the inconvenience.\n\n");
	fflush(stderr);
	exit(1);
}

#ifndef NDEBUG
#include <stdio.h>
static void _assert_func(const char *file, int line, const char *func, const char *exp) {
	fprintf(stderr, "\nassertion \"%s\" failed: file \"%s\", line %d, function: %s\n",
			exp, file, line, func);
	fflush(stderr);
#ifdef DEBUG
	__asm volatile ("int3;");
#endif
	internal_error();
}
#undef assert
#define assert(__e) ((__e) ? (void)0 : _assert_func (__FILE__, __LINE__, __PRETTY_FUNCTION__, #__e))
#endif

