#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "utils.h"

static void print_message(FILE *output_stream, const char *prefix,
		const char *fmt, va_list varargs);
static NORETURN void default_exit_routine(int status);
static NORETURN void (*exit_routine)(int) = default_exit_routine;

NORETURN void BUG(const char *fmt, ...)
{
	va_list varargs;

	va_start(varargs, fmt);
	print_message(stderr, "BUG: ", fmt, varargs);
	va_end(varargs);

	exit_routine(EXIT_FAILURE);
}

NORETURN void FATAL(const char *fmt, ...)
{
	va_list varargs;

	va_start(varargs, fmt);
	print_message(stderr, "Fatal Error: ", fmt, varargs);
	va_end(varargs);

	exit_routine(EXIT_FAILURE);
}

NORETURN void DIE(const char *fmt, ...)
{
	va_list varargs;

	va_start(varargs, fmt);
	print_message(stderr, "", fmt, varargs);
	va_end(varargs);

	exit_routine(EXIT_FAILURE);
}

void WARN(const char *fmt, ...)
{
	va_list varargs;

	va_start(varargs, fmt);
	print_message(stderr, "Warn: ", fmt, varargs);
	va_end(varargs);
}

static void print_message(FILE *output_stream, const char *prefix,
		const char *fmt, va_list varargs)
{
	fprintf(output_stream, "%s", prefix);
	vfprintf(output_stream, fmt, varargs);
	fprintf(output_stream, "\n");

	if (errno > 0)
		fprintf(stderr, "%s\n", strerror(errno));
}

void set_exit_routine(NORETURN void (*new_exit_routine)(int))
{
	exit_routine = new_exit_routine;
}

static NORETURN void default_exit_routine(int status)
{
	exit(status);
}

ssize_t recoverable_read(int fd, void *buf, size_t len)
{
	int errsv = errno;

	ssize_t bytes_read = 0;
	while(1) {
		bytes_read = read(fd, buf, len);
		if ((bytes_read < 0) && (errno == EAGAIN || errno == EINTR)) {
			errno = errsv;
			continue;
		}

		break;
	}

	return bytes_read;
}

ssize_t recoverable_write(int fd, const void *buf, size_t len)
{
	int errsv = errno;

	ssize_t bytes_written = 0;
	while(1) {
		bytes_written = write(fd, buf, len);
		if ((bytes_written < 0) && (errno == EAGAIN || errno == EINTR)) {
			errno = errsv;
			continue;
		}

		break;
	}

	return bytes_written;
}
