#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct sym {
	char *name;
	char *value;
};

FILE *infp;
FILE *outfp;
struct sym *syms;
size_t symssz;

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
blob_value(char *value_str, int base)
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
blob_word(char *word)
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

	blob_value(base == 10 ? word : word + 2, base);
}

static void
blob_line(char *line)
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
			blob_word(word);
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
sym_def(char *name, char *value)
{
	struct sym *sym;
	size_t sz;

	syms = realloc(syms, sizeof(*syms) * ++symssz);
	if (!syms)
		abort();
	sym = syms + symssz - 1;

	sz = strlen(name) + 1;
	sym->name = malloc(sz);
	if (!sym->name)
		abort();
	memcpy(sym->name, name, sz);

	if (value) {
		sz = strlen(value) + 1;
		sym->value = malloc(sz);
		if (!sym->value)
			abort();
		memcpy(sym->value, value, sz);
	}
}

static void
eval_ifdef(char *arg)
{
	char *c;
	int argsz;
	struct sym *sym = NULL;

	for (c = arg; *c && *c == ' '; c++)
		;
	if (!*c) {
		fputs("missing argument for `#ifdef`\n", stderr);
		exit(1);
	}
	arg = c;
	for (c++; *c && *c != ' '; c++)
		;
	argsz = c - arg;

	for (size_t i = 0; i < symssz; i++) {
		if (strncmp(syms[i].name, arg, argsz))
			continue;
		if (strlen(syms[i].name) == argsz) {
			sym = &syms[i];
			break;
		}
	}
	fprintf(stderr, "#ifdef %s: %s\n", arg, sym ? "true" : "false");
}

static void
eval_pproc(char *dir)
{
	char *c;
	int dirsz = 0;

	for (c = dir; *c && *c != ' '; c++)
		dirsz++;
	if (c == dir) {
		fputs("preprocessor directive ended early\n", stderr);
		exit(1);
	}

	fprintf(stderr, "dirsz: %d\n", dirsz);
	if (dirsz == 5) {
		if (!memcmp(dir, "ifdef", 5)) {
			eval_ifdef(dir + 5);
			return;
		} else
			goto bad;
	}

bad:
	fputs("bad preprocessor directive\n", stderr);
	exit(1);
}

static void
eval_line(char *line)
{
	while (*line && *line == ' ')
		line++;
	if (!*line)
		return;
	if (*line == '#')
		eval_pproc(++line);
	else
		blob_line(line);
}

static void
blob_file(void)
{
	int c;
	char *line = NULL;
	int i = 0;

	while ((c = fgetc(infp)) != EOF) {
		if (c == '\n') {
			if (line) {
				eval_line(line);
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

	while ((opt = getopt(argc, argv, "o:D:")) != -1) {
		switch (opt) {
		case 'o':
			outfp = fopen(optarg, "wb");
			if (!outfp) {
				perror(optarg);
				exit(1);
			}
			break;
		case 'D': {
			char *c;
			char *name;
			char *value;

			name = optarg;
			for (c = name; *c && *c != '=' && *c != ' '; c++)
				;
			if (c == name) {
				fputs("missing name for macro definition\n", stderr);
				exit(1);
			}
			if (*c == ' ') {
				fputs("can't have spaces in a macro definition\n", stderr);
				exit(1);
			}

			value = c;
			if (!*value)
				sym_def(name, NULL);
			*value = 0;
			sym_def(name, value + 1);

			break;
		}
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

	if (!infp)
		infp = stdin;
	if (!outfp)
		outfp = stdout;

	blob_file();

	return 0;

usage:
	fprintf(stderr, "usage: %s [-o out] [in]\n", argv0);
	return 1;
}
