#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "system4/ain.h"
#include "system4/cg.h"
#include "system4/instructions.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "gfx/gfx.h"
#include "parts.h"
#include "vm/heap.h"
#include "xsystem4.h"
#include "../src/parts/save.c"

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	initialize_instructions(ain->version);
	heap_init();
	config.game_name = utf2sjis("兰斯03", 0);
	config.ain_filename = "Rance03.ain";
	config.game_dir = "/root/rance03";
	config.view_width = 320;
	config.view_height = 240;
	config.default_volume = 100;
	char *channels[] = {"Master", "BGM", "SE", "Voice"};
	int volumes[] = {100, 100, 100, 100};
	config.mixer_nr_channels = 4;
	config.mixer_channels = channels;
	config.mixer_volumes = volumes;
	asset_manager_init();
	assert(PE_Init());

	struct iarray_writer writer;
	iarray_init_writer(&writer, "XPE");
	iarray_write(&writer, 1);
	iarray_write(&writer, PARTS_NUMERAL_FONT_COMBINED);
	iarray_write(&writer, 2739);
	for (int i = 0; i < 12; i++)
		iarray_write(&writer, 0);
	struct page *page = iarray_to_page(&writer);
	struct iarray_reader reader;
	assert(iarray_init_reader(&reader, page, "XPE"));
	load_numeral_fonts(&reader);
	assert(!reader.error);
	struct parts_numeral_font *font = &parts_numeral_fonts[0];
	struct cg_metrics metrics;
	assert(asset_cg_get_metrics(2739, &metrics));
	assert(metrics.w == 170 && metrics.h == 26);
	int x = 0;
	for (int i = 0; i < 10; i++) {
		assert(font->width[i] == 17);
		assert(x + font->width[i] <= metrics.w);
		assert(font->cg[i].handle && font->cg[i].w == 17 && font->cg[i].h == 26);
		x += font->width[i];
	}
	assert(x == metrics.w);
	for (int i = 10; i < 12; i++)
		assert(font->width[i] == 0 && !font->cg[i].handle);
	uint8_t *pixels = gfx_get_pixels(&font->cg[1]);
	assert(pixels);
	int visible = 0;
	for (int i = 0; i < 17 * 26; i++)
		if (pixels[i * 4 + 3] > 0)
			visible++;
	printf("digit1 visible RGBA pixels=%d; ten slices fit source width=%d; minus/comma absent\n",
			visible, metrics.w);
	assert(visible > 0);
	free(pixels);
	parts_release_all();
	delete_page_vars(page);
	free_page(page);
	iarray_free_writer(&writer);
	ain_free(ain);
	ain = NULL;
	return 0;
}
