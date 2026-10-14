/* Real AIN width parsing, known numeral assets, drawing, and in-memory XPE. */
#define VM_PRIVATE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/cg.h"
#include "system4/dasm.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "debugger.h"
#include "gfx/gfx.h"
#include "parts.h"
#include "vm.h"
#include "vm/heap.h"
#include "vm/page.h"
#include "xsystem4.h"
#include "../src/parts/parts_internal.h"

static int width_from_real_function(struct string *name)
{
	assert(stack_ptr == 0 && call_stack_ptr == 0);
	stack_push(vm_string_ref(name));
	vm_call(7754, -1);
	assert(call_stack_ptr == 0 && stack_ptr == 1);
	int result = stack_pop().i;
	assert(stack_ptr == 0);
	return result;
}

static int visible(Texture *t)
{
	assert(t->handle && t->w > 0 && t->h > 0);
	uint8_t *pixels = gfx_get_pixels(t);
	assert(pixels);
	int count = 0;
	for (int i = 0; i < t->w * t->h; i++)
		count += pixels[4 * i + 3] != 0;
	free(pixels);
	return count;
}

static void check_render(int number, int width)
{
	struct parts *p = parts_try_get(number);
	assert(p && p->states[0].type == PARTS_NUMERAL);
	struct parts_numeral *n = &p->states[0].num;
	assert(n->font_no >= 0 && n->font_no < parts_nr_numeral_fonts);
	struct parts_numeral_font *f = &parts_numeral_fonts[n->font_no];
	for (int i = 0; i < 10; i++) {
		assert(f->width[i] == width && f->cg[i].w == width);
		assert(visible(&f->cg[i]) > 0);
	}
	assert(n->common.w == width * 5);
	assert(visible(&n->common.texture) > 0);
	printf("RENDER part=%d width=%d texture=%dx%d visible=1\n",
		number, width, n->common.w, n->common.h);
}

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain && ain->version == 6);
	initialize_instructions(ain->version);
	assert(ain->functions[7754].nr_args == 1);
	assert(ain->functions[7754].vars[0].type.data == AIN_STRING);
	assert(ain->functions[7754].return_type.data == AIN_INT);
	/* This leaf function only examines the font name. It must not enter gameplay. */
	struct dasm d;
	dasm_init(&d, ain);
	dasm_jump(&d, ain->functions[7754].address);
	while (!dasm_eof(&d) && dasm_addr(&d) < (unsigned)ain->functions[7755].address) {
		int op = dasm_opcode(&d);
		assert(op != _MSG && op != CALLHLL && op != CALLFUNC
			&& op != CALLMETHOD && op != CALLSYS);
		dasm_next(&d);
	}
	heap_init();
	stack = xcalloc(4096, sizeof(*stack));
	stack_ptr = call_stack_ptr = 0;
#ifdef DEBUGGER_ENABLED
	dbg_enabled = false;
#endif
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "HUD numeral contract";
	config.view_width = 320;
	config.view_height = 240;
	asset_manager_init();
	if (argc == 2 && !strcmp(argv[1], "--font-metadata")) {
		const int ids[] = {2744,2726,2737,2748,2735,2723,2755,2722,2730,2742,
			2759,2765,2762,2761,2736,2740,2751,2750,2739,2738,2746,2734,2725,
			2743,2729,2733,2757};
		for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
			struct archive_data *data = asset_get(ASSET_CG, ids[i]);
			struct cg_metrics m;
			assert(data && data->name && cg_get_metrics_data(data, &m));
			char *name = sjis2utf(data->name, 0);
			assert(!strncmp(name, "系统／数字／", strlen("系统／数字／")));
			printf("FONT cg=%d width=%d height=%d name=%s\n", ids[i], m.w, m.h, name);
			free(name);
			archive_free_data(data);
		}
		return 0;
	}

	struct string *names[2];
	names[0] = string_ref(ain->strings[1149]);
	char *expected = utf2sjis("系统／数字／幅２１／白", 0);
	assert(!strcmp(names[0]->text, expected));
	free(expected);
	struct archive_data *data = asset_get(ASSET_CG, 2739);
	assert(data && data->name);
	names[1] = cstr_to_string(data->name);
	archive_free_data(data);
	const int expected_width[2] = {21, 17};
	int widths[2];
	for (int i = 0; i < 2; i++) {
		widths[i] = width_from_real_function(names[i]);
		printf("AIN_WIDTH font=%d expected=%d actual=%d\n", i, expected_width[i], widths[i]);
		assert(widths[i] == expected_width[i]);
	}
	assert(PE_Init());
	for (int i = 0; i < 2; i++) {
		int w = widths[i];
		assert(PE_SetNumeralLinkedCGNumberWidthWidthList(i + 1, names[i],
			w,w,w,w,w,w,w,w,w,w,w,w,1));
		assert(PE_SetNumeralNumber(i + 1, 12345, 1));
		check_render(i + 1, w);
		free_string(names[i]);
	}
	struct page *saved = NULL;
	assert(PE_Save(&saved));
	assert(saved);
	assert(PE_Load(&saved));
	for (int i = 0; i < 2; i++)
		check_render(i + 1, widths[i]);
	delete_page_vars(saved);
	free_page(saved);
	PE_Reset();
	free(stack);
	stack = NULL;
	ain_free(ain);
	ain = NULL;
	puts("PASS real AIN width, ten visible glyphs, numeral texture, XPE round trip");
	return 0;
}
