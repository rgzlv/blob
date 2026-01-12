#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *
base_prefix(int base)
{
	switch (base) {
	case 2:
		return "0b";
	case 8:
		return "0o";
	case 10:
		return "";
	case 16:
		return "0x";
	}
	abort();
}

static void
blob_value(char *value_str, int base, FILE *outfp)
{
	unsigned long value;
	char *end;

	errno = 0;
	value = strtoul(value_str, &end, base);
	if (errno == ERANGE) {
		fprintf(stderr, "value \"%s%s\" doesn't fit in a byte\n", base_prefix(base), value_str);
		exit(1);
	}
	if (end == value_str) {
		fprintf(stderr, "can't parse value \"%s%s\"\n", base_prefix(base), value_str);
		exit(1);
	}
	if (value > 0xff) {
		fprintf(stderr, "value \"%s%s\" doesn't fit in a byte\n", base_prefix(base), value_str);
		exit(1);
	}

	if (fwrite(&value, 1, 1, outfp) != 1) {
		perror("can't write output");
		exit(1);
	}
}

static void
blob_word(char *word, FILE *outfp)
{
	int base;

	if (!strncmp(word, "0b", 2))
		base = 2;
	else if (!strncmp(word, "0o", 2))
		base = 8;
	else if (!strncmp(word, "0x", 2))
		base = 16;
	else
		base = 10;

	blob_value(base == 10 ? word : word + 2, base, outfp);
}

static void
blob_line(char *line, FILE *outfp)
{
	char *c;
	char *word = NULL;
	int i = 0;

	for (c = line; *c; c++)
		if (*c == ';')
			*c = 0;

	for (c = line;; c++) {
		if (*c == ' ' || !*c) {
			if (!word) {
				if (!*c)
					break;
				continue;
			}
			blob_word(word, outfp);
			free(word);
			if (!*c)
				break;
			word = NULL;
			i = 0;
			continue;
		}
		word = realloc(word, i + 2);
		if (!word)
			abort();
		word[i++] = *c;
		word[i] = 0;
	}
}

static void
blob_file(FILE *infp, FILE *outfp)
{
	int c;
	char *line = NULL;
	int i = 0;

	while ((c = fgetc(infp)) != EOF) {
		if (c == '\n') {
			if (line) {
				blob_line(line, outfp);
				free(line);
				line = NULL;
				i = 0;
			}
			continue;
		}
		line = realloc(line, i + 2);
		if (!line)
			abort();
		line[i++] = c;
		line[i] = 0;
	}
}

int
main(int argc, char **argv)
{
	char *argv0 = *argv;
	int opt;
	FILE *infp = NULL;
	FILE *outfp = NULL;

	while ((opt = getopt(argc, argv, "o:")) != -1) {
		switch (opt) {
		case 'o':
			outfp = fopen(optarg, "wb");
			if (!outfp) {
				perror(optarg);
				exit(1);
			}
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc) {
		infp = fopen(*argv, "rb");
		if (!infp) {
			perror(*argv);
			exit(1);
		}
		argc--;
		argv++;
	}

	blob_file(infp ? infp : stdin, outfp ? outfp : stdout);

	return 0;

usage:
	fprintf(stderr, "usage: %s [-o out] [in]\n", argv0);
	return 1;
}
