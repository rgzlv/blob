#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

unsigned char *blob;
size_t blobsz;

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

	blob = realloc(blob, ++blobsz);
	if (!blob)
		abort();
	blob[blobsz - 1] = value;
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

int
main(void)
{
	int c;
	char *line = NULL;
	int i = 0;

	while ((c = fgetc(stdin)) != EOF) {
		if (c == '\n') {
			if (line) {
				blob_line(line);
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

	if (fwrite(blob, 1, blobsz, stdout) != blobsz) {
		perror("can't write output");
		exit(1);
	}

	return 0;
}
