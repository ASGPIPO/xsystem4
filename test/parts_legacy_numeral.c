#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "xsystem4.h"
#define parts_numeral_font_init inspect_font_init
#include "../src/parts/save.c"
#undef parts_numeral_font_init

static int initialized;
void inspect_font_init(struct parts_numeral_font *font)
{
	(void)font;
	initialized++;
}

static void check(const char *label, int type, int cg_no, const int input[12], const int expected[12])
{
	struct iarray_writer writer;
	iarray_init_writer(&writer, "XPE");
	iarray_write(&writer, 1);
	iarray_write(&writer, type);
	iarray_write(&writer, cg_no);
	for (int i = 0; i < 12; i++)
		iarray_write(&writer, input[i]);
	struct page *page = iarray_to_page(&writer);
	struct iarray_reader reader;
	assert(iarray_init_reader(&reader, page, "XPE"));
	int previous = initialized;
	load_numeral_fonts(&reader);
	assert(!reader.error && initialized == previous + 1);
	if (memcmp(parts_numeral_fonts[0].width, expected, 12 * sizeof(int))) {
		fprintf(stderr, "FAIL %s widths:", label);
		for (int i = 0; i < 12; i++)
			fprintf(stderr, " %d", parts_numeral_fonts[0].width[i]);
		fputc('\n', stderr);
		abort();
	}
	struct iarray_writer saved;
	iarray_init_writer(&saved, "XPE");
	save_numeral_fonts(&saved);
	for (int i = 0; i < 12; i++)
		assert(saved.data[saved.size - 12 + i] == expected[i]);
	printf("PASS %s serialized widths\n", label);
	iarray_free_writer(&saved);
	free(parts_numeral_fonts);
	parts_numeral_fonts = NULL;
	parts_nr_numeral_fonts = 0;
	delete_page_vars(page);
	free_page(page);
	iarray_free_writer(&writer);
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	asset_manager_init();
	const int zero[12] = {0};
	const int restored[12] = {17,17,17,17,17,17,17,17,17,17,0,0};
	const int partial[12] = {17,0,0,0,0,0,0,0,0,0,0,0};
	const int negative[12] = {-1,0,0,0,0,0,0,0,0,0,0,0};
	const int valid[12] = {14,14,14,14,14,14,14,14,14,14,15,15};
	check("legacy ten-digit CG2739", PARTS_NUMERAL_FONT_COMBINED, 2739, zero, restored);
	check("legacy ten-digit CG2738", PARTS_NUMERAL_FONT_COMBINED, 2738, zero, restored);
	const int restored15[12] = {15,15,15,15,15,15,15,15,15,15,0,0};
	check("legacy width-15 numeral CG", PARTS_NUMERAL_FONT_COMBINED, 2735, zero, restored15);
	check("partial width preserved", PARTS_NUMERAL_FONT_COMBINED, 2739, partial, partial);
	check("negative sentinel preserved", PARTS_NUMERAL_FONT_COMBINED, 2739, negative, negative);
	check("valid widths preserved", PARTS_NUMERAL_FONT_COMBINED, 2739, valid, valid);
	check("separate preserved", PARTS_NUMERAL_FONT_SEPARATE, 2739, zero, zero);
	check("separate2 preserved", PARTS_NUMERAL_FONT_SEPARATE2, 2739, zero, zero);
	check("missing CG preserved", PARTS_NUMERAL_FONT_COMBINED, 999999, zero, zero);
	check("other numeral CG preserved", PARTS_NUMERAL_FONT_COMBINED, 2722, zero, zero);
	puts("PASS production load_numeral_fonts XPE read and write");
	return 0;
}
