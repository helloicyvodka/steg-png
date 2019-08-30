#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse-options.h"
#include "utils.h"

#define USAGE_OPTIONS_WIDTH		24
#define USAGE_OPTIONS_GAP		2


static int parse_long_option(int argc, char *argv[], int arg_index,
		const struct command_option options[]);
static int parse_short_option(int argc, char *argv[], int arg_index,
		const struct command_option options[]);
static inline void array_shift(char *argv[], int arg_index, int *len);

int parse_options(int argc, char *argv[], const struct command_option options[],
		int skip_first, int stop_on_unknown) {
	int new_len = argc;
	if (argc < 0)
		BUG("parse_options argument vector length is negative");

	// shift to remove first argument, which is name of program
	if (skip_first)
		array_shift(argv, 0, &new_len);

	int arg_index = 0;
	while (arg_index < new_len) {
		char *arg = argv[arg_index];

		// if argument is equal to `--`, stop processing
		if (!strcmp(arg, "--")) {
			array_shift(argv, arg_index, &new_len);
			return new_len;
		}

		int shifted_args = 0;
		if (!strncmp(arg, "--", 2)) {
			// if argument is prefixed with `--`, it is a long argument
			shifted_args = parse_long_option(new_len, argv, arg_index, options);
		} else if (arg[0] == '-') {
			// if argument is prefixed by `-`, it is short argument (may be multiple args combined combined)
			shifted_args = parse_short_option(new_len, argv, arg_index, options);
		}

		new_len -= shifted_args;
		if (!shifted_args && stop_on_unknown)
			return new_len;
		if (!shifted_args)
			arg_index++;
	}

	return new_len;
}

static int parse_long_option(int argc, char *argv[], int arg_index,
		const struct command_option options[]) {
	// skip '--' prefix
	char *arg = argv[arg_index] + 2;
	int new_len = argc;

	for (const struct command_option *op = options; op->type != OPTION_END; op++) {
		if (!op->l_flag)
			continue;

		// if the argument is a boolean arg, set value to one and return
		if (op->type == OPTION_BOOL_T && !strcmp(arg, op->l_flag)) {
			array_shift(argv, arg_index, &new_len);
			*(int *) op->arg_value = 1;
			break;
		}

		/*
		 * Check to see if this argument matches the flag, taking into account
		 * that the argument might be suffixed with =<value>.
		 * */
		size_t flag_len = strlen(op->l_flag);
		if (!op->l_flag || strncmp(arg, op->l_flag, flag_len) != 0)
			continue;

		if (strlen(arg) == flag_len) {
			// check that argument value is in the next argument
			if (arg_index + 1 >= argc)
				continue;

			arg = argv[arg_index + 1];

			if (op->type == OPTION_INT_T) {
				char *tailptr = NULL;
				long arg_value = strtol(arg, &tailptr, 10);

				// verify that integer was parsed successfully
				if (tailptr == (arg + strlen(arg))) {
					array_shift(argv, arg_index, &new_len);
					array_shift(argv, arg_index, &new_len);
					*(long *) op->arg_value = arg_value;
					break;
				}
			} else if (op->type == OPTION_STRING_T) {
				array_shift(argv, arg_index, &new_len);
				array_shift(argv, arg_index, &new_len);

				*(char **) op->arg_value = arg;
				break;
			}
		} else if (strlen(arg) > flag_len && arg[flag_len] == '=') {
			// argument value is attached to the argument with '='
			arg = arg + flag_len + 1;

			if (op->type == OPTION_INT_T) {
				char *tailptr = NULL;
				long arg_value = strtol(arg, &tailptr, 10);

				// verify that integer was parsed successfully
				if (tailptr == (arg + strlen(arg))) {
					array_shift(argv, arg_index, &new_len);
					*(long *) op->arg_value = arg_value;
					break;
				}
			} else if (op->type == OPTION_STRING_T) {
				array_shift(argv, arg_index, &new_len);

				*(char **) op->arg_value = arg;
				break;
			}
		}
	}

	return argc - new_len;
}

static int parse_short_option(int argc, char *argv[], int arg_index,
		const struct command_option options[]) {
	int new_len = argc;

	// skip '-' prefix
	for (char *arg = argv[arg_index] + 1; *arg; arg++) {
		for (const struct command_option *op = options; op->type != OPTION_END; op++) {
			if (!op->s_flag || op->s_flag != *arg)
				continue;

			if (op->type == OPTION_BOOL_T) {
				*(int *) op->arg_value = 1;
				break;
			}

			if (op->type == OPTION_INT_T || op->type == OPTION_STRING_T) {
				// intermediate options that accept values are disallowed
				if (*(arg + 1))
					return argc - new_len;

				// if no value given
				if (arg_index + 1 >= argc)
					return argc - new_len;
			}

			if (op->type == OPTION_INT_T) {
				arg++;
				char *tailptr = NULL;
				long arg_value = strtol(arg, &tailptr, 10);

				// verify that integer was parsed successfully
				if (tailptr == (arg + strlen(arg))) {
					array_shift(argv, arg_index, &new_len);
					array_shift(argv, arg_index, &new_len);

					*(long *) op->arg_value = arg_value;
					return argc - new_len;
				}
			}

			if (op->type == OPTION_STRING_T) {
				arg++;
				array_shift(argv, arg_index, &new_len);
				array_shift(argv, arg_index, &new_len);

				*(char **) op->arg_value = arg;
				return argc - new_len;
			}
		}
	}

	array_shift(argv, arg_index, &new_len);
	return argc - new_len;
}

void show_usage(const struct usage_string cmd_usage[], int err,
		const char *optional_message_format, ...)
{
	va_list varargs;
	va_start(varargs, optional_message_format);

	variadic_show_usage(cmd_usage, optional_message_format, varargs, err);

	va_end(varargs);
}

void variadic_show_usage(const struct usage_string cmd_usage[],
		const char *optional_message_format, va_list varargs, int err)
{
	FILE *fp = err ? stderr : stdout;

	if (optional_message_format != NULL) {
		vfprintf(fp, optional_message_format, varargs);
		fprintf(fp, "\n");
	}

	size_t index = 0;
	while (cmd_usage[index].usage_desc != NULL) {
		if (index == 0)
			fprintf(fp, "%s %s\n", "usage:", cmd_usage[index].usage_desc);
		else
			fprintf(fp, "%s %s\n", "   or:", cmd_usage[index].usage_desc);

		index++;
	}

	fprintf(fp, "\n");
}

void show_options(const struct command_option opts[], int err)
{
	FILE *fp = err ? stderr : stdout;

	size_t index = 0;
	while (opts[index].type != OPTION_END) {
		struct command_option opt = opts[index++];
		int printed_chars = 0;

		if (opt.type == OPTION_GROUP_T) {
			if (index > 1)
				fprintf(fp, "\n");

			fprintf(fp, "%s:\n", opt.desc);
			continue;
		}

		printed_chars += fprintf(fp, "    ");
		if (opt.type == OPTION_BOOL_T) {
			if (opt.s_flag)
				printed_chars += fprintf(fp, "-%c", opt.s_flag);

			if (opt.s_flag && opt.l_flag)
				printed_chars += fprintf(fp, ", --%s", opt.l_flag);
			else if (opt.l_flag)
				printed_chars += fprintf(fp, "--%s", opt.l_flag);
		} else if (opt.type == OPTION_INT_T) {
			if (opt.s_flag)
				printed_chars += fprintf(fp, "-%c=<n>", opt.s_flag);

			if (opt.s_flag && opt.l_flag)
				printed_chars += fprintf(fp, ", --%s=<n>", opt.l_flag);
			else if (opt.l_flag)
				printed_chars += fprintf(fp, "--%s=<n>", opt.l_flag);
		} else if (opt.type == OPTION_STRING_T) {
			if (opt.s_flag)
				printed_chars += fprintf(fp, "-%c", opt.s_flag);

			if (opt.s_flag && opt.l_flag)
				printed_chars += fprintf(fp, ", --%s", opt.l_flag);
			else if (opt.l_flag)
				printed_chars += fprintf(fp, "--%s", opt.l_flag);

			printed_chars += fprintf(fp, " <%s>", opt.str_name);
		} else if (opt.type == OPTION_COMMAND_T) {
			printed_chars += fprintf(fp, "%s", opt.str_name);
		}

		if (printed_chars >= (USAGE_OPTIONS_WIDTH - USAGE_OPTIONS_GAP))
			fprintf(fp, "\n%*s%s\n", USAGE_OPTIONS_WIDTH, "", opt.desc);
		else
			fprintf(fp, "%*s%s\n", USAGE_OPTIONS_WIDTH - printed_chars, "",
					opt.desc);
	}

	fprintf(fp, "\n");
}

void show_usage_with_options(const struct usage_string cmd_usage[],
		const struct command_option opts[], int err,
		const char *optional_message_format, ...)
{
	va_list varargs;
	va_start(varargs, optional_message_format);

	variadic_show_usage_with_options(cmd_usage, opts, optional_message_format,
									 varargs, err);

	va_end(varargs);
}

void variadic_show_usage_with_options(const struct usage_string cmd_usage[],
		const struct command_option opts[],
		const char *optional_message_format, va_list varargs, int err)
{
	variadic_show_usage(cmd_usage, optional_message_format, varargs, err);
	show_options(opts, err);
}

static inline void array_shift(char *argv[], int arg_index, int *len) {
	if (*len <= 0 || arg_index >= *len)
		return;

	for (int index = arg_index; index < *len - 1; index++)
		argv[index] = argv[index + 1];
	*len = *len - 1;
	argv[*len] = NULL;
}