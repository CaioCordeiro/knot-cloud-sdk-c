/*
 * This file is part of the KNOT Project
 *
 * Copyright (c) 2020, CESAR. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * Log source file
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <ell/ell.h>
#include <errno.h>

#include "log.h"

static int log_priority = L_LOG_ERR;

static void log_stderr_handler(int priority, const char *file, const char *line,
			       const char *func, const char *format, va_list ap)
{
	if (priority > log_priority)
		return;

	switch (priority) {
	case L_LOG_ERR:
		fprintf(stderr, "ERR: %s() ", func);
		break;
	case L_LOG_WARNING:
		fprintf(stderr, "WARN: ");
		break;
	case L_LOG_INFO:
		fprintf(stderr, "INFO: ");
		break;
	case L_LOG_DEBUG:
		fprintf(stderr, "DEBUG: ");
		break;
	default:
		return;
	}

	vfprintf(stderr, format, ap);
}

void log_ell_enable(void)
{
	l_log_set_handler(log_stderr_handler);
}
