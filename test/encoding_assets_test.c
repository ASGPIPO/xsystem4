
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/string.h"
#include "system4/utfsjis.h"

static void check(const char *raw, const char *label, int index)
{
	char *utf = sjis2utf(raw, 0);
	char *back = utf2sjis(utf, 0);
	if (strcmp(raw, back)) {
		fprintf(stderr, "%s[%d] failed round trip\n", label, index);
		abort();
	}
	free(back);
	free(utf);
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	int error;
	struct ain *a = ain_open(argv[1], &error);
	assert(a);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	for (int i = 0; i < a->nr_strings; i++)
		check(a->strings[i]->text, "STR0", i);
	for (int i = 0; i < a->nr_messages; i++)
		check(a->messages[i]->text, "MSG1", i);
	char *sample = sjis2utf(a->strings[704]->text, 0);
	assert(!strcmp(sample, "CommonSystemData存档文件读入失败\n"));
	printf("PASS actual STR0[704]: %s", sample);
	free(sample);
	sample = sjis2utf(a->messages[85]->text, 0);
	assert(!strcmp(sample, "　不过我也是第一次来。」"));
	printf("PASS actual MSG1[85]: %s\n", sample);
	free(sample);
	printf("PASS byte-preserving CP936 round trip: %d STR0 + %d MSG1 strings\n",
		a->nr_strings, a->nr_messages);
	ain_free(a);
	return 0;
}

