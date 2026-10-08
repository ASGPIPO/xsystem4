#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/string.h"

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s game.ain out.bin\n", argv[0]);
		return 1;
	}
	int error;
	struct ain *a = ain_open(argv[1], &error);
	if (!a) {
		fprintf(stderr, "ain_open failed: %d\n", error);
		return 1;
	}
	FILE *out = fopen(argv[2], "wb");
	if (!out) {
		perror("fopen");
		return 1;
	}
	uint32_t total = a->nr_strings + a->nr_messages;
	fwrite(&total, 4, 1, out);
	for (int i = 0; i < a->nr_strings; i++) {
		uint32_t len = a->strings[i]->size;
		fwrite(&len, 4, 1, out);
		fwrite(a->strings[i]->text, 1, len, out);
	}
	for (int i = 0; i < a->nr_messages; i++) {
		uint32_t len = a->messages[i]->size;
		fwrite(&len, 4, 1, out);
		fwrite(a->messages[i]->text, 1, len, out);
	}
	fclose(out);
	printf("dumped %u real strings (%d STR0 + %d MSG1)\n",
		total, a->nr_strings, a->nr_messages);
	ain_free(a);
	return 0;
}
