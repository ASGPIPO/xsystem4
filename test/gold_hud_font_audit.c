/* Inspect only system numeral assets; reuse the real-AIN rendering fixture. */
#define main original_numeral_contract_main
#include "gold_hud_numbers.c"
#undef main
#include "system4/afa.h"
#include "../src/parts/save.c"

static void round_trip(int count, const int *widths)
{
	struct page *saved = NULL;
	assert(PE_Save(&saved) && PE_Load(&saved));
	for (int i = 0; i < count; i++)
		check_render(i + 1, widths[i]);
	delete_page_vars(saved);
	free_page(saved);
}

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain && ain->version == 6 && ain->nr_functions > 7755);
	initialize_instructions(ain->version);
	assert(ain->functions[7754].nr_args == 1);
	assert(ain->functions[7754].vars[0].type.data == AIN_STRING);
	assert(ain->functions[7754].return_type.data == AIN_INT);
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
	config.game_name = "All numeral width families";
	config.view_width = 320;
	config.view_height = 240;
	asset_manager_init();
	struct afa_archive *ar = afa_open("/root/rance03/Rance03CG.afa", ARCHIVE_MMAP, &error);
	assert(ar);
	char *prefix = utf2sjis("系统／数字／", 0);
	char *width_prefix = utf2sjis("系统／数字／幅", 0);
	struct string *names[512];
	int cg[512], widths[512], count = 0, special = 0, explicit_width = 0;
	for (unsigned i = 0; i < ar->nr_files; i++) {
		const char *name = ar->files[i].name->text;
		if (strncmp(name, prefix, strlen(prefix)))
			continue;
		if (strncmp(name, width_prefix, strlen(width_prefix))) {
			int id;
			struct cg_metrics metrics;
			assert(asset_exists_by_name(ASSET_CG, name, &id));
			assert(asset_cg_get_metrics(id, &metrics));
			char *utf = sjis2utf(name, 0);
			printf("SPECIAL cg=%d strip=%dx%d name=%s\n", id, metrics.w, metrics.h, utf);
			free(utf);
			special++;
			continue;
		}
		assert(count < 512);
		names[count] = cstr_to_string(name);
		assert(asset_exists_by_name(ASSET_CG, name, &cg[count]));
		int automatic = width_from_real_function(names[count]);
		struct string *token = cstr_to_string(name + strlen(width_prefix));
		widths[count] = string_to_integer(token);
		free_string(token);
		explicit_width += automatic != widths[count];
		struct cg_metrics metrics;
		assert(asset_cg_get_metrics(cg[count], &metrics));
		assert(widths[count] > 0 && widths[count] <= INT_MAX / 10);
		char *utf = sjis2utf(name, 0);
		printf("FAMILY cg=%d width_token=%d auto_width=%d strip=%dx%d name=%s\n",
			cg[count], widths[count], automatic, metrics.w, metrics.h, utf);
		free(utf);
		assert(metrics.w == widths[count] * 10 && metrics.h > 0);
		count++;
	}
	assert(count > 0);
	if (argc == 2 && !strcmp(argv[1], "--metadata"))
		return 0;
	archive_free(&ar->ar);
	free(prefix);
	free(width_prefix);
	assert(PE_Init());
	for (int i = 0; i < count; i++) {
		int w = widths[i];
		assert(PE_SetNumeralLinkedCGNumberWidthWidthList(i + 1, names[i],
			w,w,w,w,w,w,w,w,w,w,w,w,1));
		assert(PE_SetNumeralNumber(i + 1, 12345, 1));
		check_render(i + 1, w);
		free_string(names[i]);
	}
	round_trip(count, widths);
	parts_release_all();
	struct iarray_writer writer;
	iarray_init_writer(&writer, "XPE");
	iarray_write(&writer, count);
	for (int i = 0; i < count; i++) {
		iarray_write(&writer, PARTS_NUMERAL_FONT_COMBINED);
		iarray_write(&writer, cg[i]);
		for (int j = 0; j < 12; j++)
			iarray_write(&writer, 0);
	}
	struct page *page = iarray_to_page(&writer);
	struct iarray_reader reader;
	assert(iarray_init_reader(&reader, page, "XPE"));
	load_numeral_fonts(&reader);
	assert(!reader.error && parts_nr_numeral_fonts == count);
	for (int i = 0; i < count; i++) {
		int w = widths[i];
		assert(PE_SetNumeralLinkedCGNumberWidthWidthList_by_index(i + 1, cg[i],
			w,w,w,w,w,w,w,w,w,w,0,0,1));
		assert(parts_try_get(i + 1)->states[0].num.font_no == i);
		assert(PE_SetNumeralNumber(i + 1, 12345, 1));
		check_render(i + 1, w);
	}
	round_trip(count, widths);
	delete_page_vars(page);
	free_page(page);
	iarray_free_writer(&writer);
	parts_release_all();
	const int special_cg[] = {2722, 2764, 2765};
	const int special_width[] = {42, 42, 55};
	assert(special == 3);
	for (int i = 0; i < special; i++) {
		int w = special_width[i];
		assert(PE_SetNumeralLinkedCGNumberWidthWidthList_by_index(i + 1, special_cg[i],
			w,w,w,w,w,w,w,w,w,w,w,w,1));
		assert(PE_SetNumeralNumber(i + 1, 12345, 1));
		check_render(i + 1, w);
	}
	round_trip(special, special_width);
	PE_Reset();
	free(stack);
	stack = NULL;
	ain_free(ain);
	ain = NULL;
	printf("PASS all_width_families=%d special_families=%d require_explicit_width=%d fresh_and_legacy=1 special_rendered=3 roundtrips=3\n",
		count, special, explicit_width);
	return 0;
}
