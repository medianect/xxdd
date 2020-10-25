#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include "vendor/optparse/optparse.h"

enum format {
	HEX,
	GO,
	GO_HEX,
};

struct parameters {
	FILE		*input;
	FILE		*output;
	unsigned long	bytes;
	unsigned long	count;
	unsigned long	columns;
	enum format	format;
	unsigned long	skip;
	unsigned long	seek;
	bool		trailing_comma;
	bool		uppercase;
};

int
xxdd_hex(struct parameters *params)
{
	uint8_t			b;
	static const char	lo[] = "0123456789abcdef";
	static const char	up[] = "0123456789ABCDEF";
	const char		*alf = params->uppercase? up: lo;

	for (off_t i=0; ; i++) {
		size_t r = fread(&b, 1, 1, params->input);

		if (r == 0) {
			if (ferror(params->input)) {
				fprintf(stderr, "Failed to read input file: %s\n", strerror(errno));
				return EXIT_FAILURE;
			}

			if (feof(params->input)) {
				if (i != 0) {
					if (params->trailing_comma)
						fprintf(params->output, ",");
					fprintf(params->output, "\n");
				}
				fflush(params->output);
				return EXIT_SUCCESS;
			}
		} else {
			if (params->columns && i % params->columns == 0) {
				if (i != 0)
					fprintf(params->output, ",\n");
				fprintf(params->output, "\t");
			} else
				fprintf(params->output, ", ");
			fprintf(params->output, "0x%c%c", alf[(b>>4)&0xf], alf[b&0xf]);
		}
	}
	fprintf(stderr, "File offset overflowed system limits\n");
	return EXIT_FAILURE;
}

int
xxdd_go(struct parameters *params)
{
	char			b[2048];
	static const char	t[] = "`+\"`\"+`";

	fwrite("`", 1, 1, params->output);
	for (;;) {
		size_t r = fread(&b, 1, 2048, params->input);

		for (size_t j=0; j<r; j++) {
			if (b[j] != '`')
				fwrite(&b[j], 1, 1, params->output);
			else
				fwrite(t, 1, strlen(t), params->output);
		}

		if (r < 2048) {
			if (ferror(params->input)) {
				fprintf(stderr, "Failed to read input file: %s\n", strerror(errno));
				return EXIT_FAILURE;
			}

			if (feof(params->input)) {
				fwrite("`", 1, 1, params->output);
				fflush(params->output);
				return EXIT_SUCCESS;
			}
		}
	}
	fwrite("`", 1, 1, params->output);
	// If here success is authorized
	return EXIT_SUCCESS;
}

int
xxdd_gohex(struct parameters *params)
{
	int ret = EXIT_FAILURE;

	fwrite("[]byte{", 1, 7, params->output);
	ret = xxdd_hex(params);
	fwrite("}", 1, 1, params->output);
	return ret;
}

static int
usage(const char *progname, int ret, const char *fmt, ...)
{
	if (fmt) {
		va_list ap;

		va_start(ap, fmt);
		vfprintf(ret == 0? stdout: stderr, fmt, ap);
		va_end(ap);
	}

	fprintf(ret == 0? stdout: stderr,
	       "usage: %s [-u,] [-i INPUT_FILE] [-o OUTPUT_FILE]\n"
	       "        [-f FORMAT]\n"
	       "	[-s SKIP_WORDS] [-S SEEK_WORDS] [-c WORDS_COUNT]\n"
	       "	[-C COLUMNS]\n"
	       "\n"
	       "    -,|--trailing-comma\n"
	       "	Put a comma after the last item.\n"
	       "    -u|--uppercase\n"
	       "	Use uppercase symbols if any (e.g. 0X rather than 0x).\n"
	       "\n"
	       "    -c|--count WORDS_COUNT\n"
	       "	Read and write WORDS_COUNT \"words\". 0 (default) means until end of input.\n"
	       "    -C|--cols COLUMNS\n"
	       "	Display COLUMNS number of \"words\" per line of output. Defaults to 8.\n"
	       "    -f|--format FORMAT\n"
	       "	Format output as hexadecimal (-f hex) or Golang literal string (-f go).\n",
		progname
	);

	exit(ret);
}

int
main(int argc, char *argv[])
{
	int			opt;
	struct optparse		options;
	struct optparse_long	longopts[] = {
		{"if",             'i', OPTPARSE_REQUIRED},
		{"input",          'i', OPTPARSE_REQUIRED},
		{"of",             'o', OPTPARSE_REQUIRED},
		{"output",         'o', OPTPARSE_REQUIRED},
		{"bs",             'b', OPTPARSE_REQUIRED},
		{"bytes",          'b', OPTPARSE_REQUIRED},
		{"count",          'c', OPTPARSE_REQUIRED},
		{"cols",           'C', OPTPARSE_REQUIRED},
		{"format",         'f', OPTPARSE_REQUIRED},
		{"skip",           's', OPTPARSE_REQUIRED},
		{"seek",           'S', OPTPARSE_REQUIRED},
		{"trailing-comma", ',', OPTPARSE_NONE},
		{"upper",          'u', OPTPARSE_NONE},
		{"help",           'h', OPTPARSE_NONE},
		{"version",        'V', OPTPARSE_NONE},
		{0}
 	};
	struct parameters	params;
	char			*end;

	(void)optparse_arg;
	(void)argc;

	params.input = stdin;
	params.output = stdout;
	params.count = 0;
	params.columns = 8;
	params.format = HEX;
	params.skip = 0;
	params.seek = 0;
	params.trailing_comma = false;
	params.uppercase = false;
	optparse_init(&options, argv);

	while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
		switch (opt) {
		case ',':
			params.trailing_comma = true;
			break;
		case 'b':
			errno = 0;
			params.bytes = strtoul(options.optarg, &end, 0);
			if (!*options.optarg || *end || params.bytes == ULONG_MAX) {
				if (!errno)
					errno = EINVAL;
				usage(argv[0], EXIT_FAILURE, "Failed to parse bytes option: %s\n", strerror(errno));
			}
			break;
		case 'c':
			errno = 0;
			params.count = strtoul(options.optarg, &end, 0);
			if (!*options.optarg || *end || params.count == ULONG_MAX) {
				if (!errno)
					errno = EINVAL;
				usage(argv[0], EXIT_FAILURE, "Failed to parse count option: %s\n", strerror(errno));
			}
			break;
		case 'C':
			errno = 0;
			params.columns = strtoul(options.optarg, &end, 0);
			if (!*options.optarg || *end || params.columns == ULONG_MAX) {
				if (!errno)
					errno = EINVAL;
				usage(argv[0], EXIT_FAILURE, "Failed to parse columns option: %s\n", strerror(errno));
			}
			break;
		case 'f':
			if (strcmp(options.optarg, "hex") == 0)
				params.format = HEX;
			else if (strcmp(options.optarg, "go") == 0)
				params.format = GO;
			else if (strcmp(options.optarg, "gob") == 0)
				params.format = GO_HEX;
			else
				usage(argv[0], EXIT_FAILURE, "Known formats comprised of \"hex\" or \"go\"\n");
			break;
		case 'i':
			if (options.optarg[0] == '-' && options.optarg[1] == 0)
				params.input = stdin;
			else
				params.input = fopen(options.optarg, "r");
			if (!params.input)
				usage(argv[0], EXIT_FAILURE, "Failed to open input file: %s\n", strerror(errno));
			break;
		case 'o':
			if (options.optarg[0] == '-' && options.optarg[1] == 0)
				params.output = stdout;
			else
				params.output = fopen(options.optarg, "w+");
			if (!params.output)
				usage(argv[0], EXIT_FAILURE, "Failed to open output file: %s\n", strerror(errno));
			break;
		case 's':
			errno = 0;
			params.skip = strtoul(options.optarg, &end, 0);
			if (!*options.optarg || *end || params.skip == ULONG_MAX) {
				if (!errno)
					errno = EINVAL;
				usage(argv[0], EXIT_FAILURE, "Failed to parse skip option: %s\n", strerror(errno));
			}
			break;
		case 'S':
			errno = 0;
			params.seek = strtoul(options.optarg, &end, 0);
			if (!*options.optarg || *end || params.seek == ULONG_MAX) {
				if (!errno)
					errno = EINVAL;
				usage(argv[0], EXIT_FAILURE, "Failed to parse seek option: %s\n", strerror(errno));
			}
			break;
		case 'u':
			params.uppercase = true;
			break;
		case '?':
		default:
			usage(argv[0], EXIT_FAILURE, options.errmsg);
		}
	}

	if (params.format == HEX)
		return xxdd_hex(&params);
	else if (params.format == GO)
		return xxdd_go(&params);
	else
		return xxdd_gohex(&params);
}
