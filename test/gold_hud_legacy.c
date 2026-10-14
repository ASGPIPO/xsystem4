/* Reproduce the inherited HUD's all-zero font table without entering gameplay. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "gfx/gfx.h"
#include "parts.h"
#include "vm/heap.h"
#include "xsystem4.h"
#include "../src/parts/save.c"

static int visible(Texture *texture)
{
	assert(texture->handle && texture->w > 0 && texture->h > 0);
	uint8_t *pixels = gfx_get_pixels(texture);
	assert(pixels);
	int count = 0;
	for (int i = 0; i < texture->w * texture->h; i++)
		count += pixels[4 * i + 3] != 0;
	free(pixels);
	return count;
}

static void set_font(int part, int cg, int width)
{
	assert(PE_SetNumeralLinkedCGNumberWidthWidthList_by_index(part, cg,
		width,width,width,width,width,width,width,width,width,width,0,0,1));
}

static void check_part(int part, int width, int number)
{
	struct parts *p = parts_try_get(part);
	assert(p);
	struct parts_numeral *n = &p->states[0].num;
	assert(n->num == number && n->have_num);
	assert(n->common.w == width * (number == 3756 ? 4 : 1));
	assert(visible(&n->common.texture) > 0);
	printf("PART part=%d number=%d width=%d texture=%dx%d visible=1\n",
		part, number, width, n->common.w, n->common.h);
}

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	assert(argc == 2);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	initialize_instructions(ain->version);
	heap_init();
	config.game_name = "Inherited HUD numeral regression";
	config.ain_filename = "Rance03.ain";
	config.game_dir = "/root/rance03";
	config.view_width = 320;
	config.view_height = 240;
	asset_manager_init();
	assert(PE_Init());

	if (!strcmp(argv[1], "cache")) {
		set_font(1, 2744, 0);
		int old = parts_try_get(1)->states[0].num.font_no;
		set_font(2, 2744, 21);
		int current = parts_try_get(2)->states[0].num.font_no;
		printf("CACHE old=%d new=%d expected_different=1\n", old, current);
		assert(current != old);
		set_font(3, 2744, 21);
		assert(parts_try_get(3)->states[0].num.font_no == current);
		assert(PE_SetNumeralNumber(2, 3756, 1));
		check_part(2, 21, 3756);
	} else {
		assert(!strcmp(argv[1], "legacy"));
		/* CGs and zero widths captured from the inherited slot07 runtime. */
		const int cg[] = {2744,2726,2737,2748,2723,2755,2730,2742,2762,
			2761,2736,2740,2751,2750,2739,2738};
		const int width[] = {21,12,16,23,10,35,13,18,7,6,15,18,28,26,17,17};
		const int count = sizeof(cg) / sizeof(cg[0]);
		struct iarray_writer writer;
		iarray_init_writer(&writer, "XPE");
		iarray_write(&writer, count);
		for (int i = 0; i < count; i++) {
			iarray_write(&writer, PARTS_NUMERAL_FONT_COMBINED);
			iarray_write(&writer, cg[i]);
			for (int digit = 0; digit < 12; digit++)
				iarray_write(&writer, 0);
		}
		struct page *page = iarray_to_page(&writer);
		struct iarray_reader reader;
		assert(iarray_init_reader(&reader, page, "XPE"));
		load_numeral_fonts(&reader);
		assert(!reader.error && parts_nr_numeral_fonts == count);
		for (int i = 0; i < count; i++) {
			struct parts_numeral_font *font = &parts_numeral_fonts[i];
			printf("LEGACY cg=%d expected=%d actual=%d\n", cg[i], width[i], font->width[0]);
			for (int digit = 0; digit < 10; digit++) {
				assert(font->width[digit] == width[i]);
				assert(visible(&font->cg[digit]) > 0);
			}
			assert(!font->width[10] && !font->width[11]);
			assert(!font->cg[10].handle && !font->cg[11].handle);
			set_font(i + 1, cg[i], width[i]);
			assert(parts_try_get(i + 1)->states[0].num.font_no == i);
			int number = i == 0 ? 3756 : 1;
			assert(PE_SetNumeralNumber(i + 1, number, 1));
			check_part(i + 1, width[i], number);
		}
		struct page *saved = NULL;
		assert(PE_Save(&saved) && PE_Load(&saved));
		for (int i = 0; i < count; i++)
			check_part(i + 1, width[i], i == 0 ? 3756 : 1);
		delete_page_vars(saved);
		free_page(saved);
		delete_page_vars(page);
		free_page(page);
		iarray_free_writer(&writer);
	}
	PE_Reset();
	ain_free(ain);
	ain = NULL;
	puts("PASS inherited numeral regression");
	return 0;
}
