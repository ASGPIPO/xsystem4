/* Execute the real Rance03 wrapper with an in-memory RGBA asset fixture. */
#define VM_PRIVATE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/cg.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "debugger.h"
#include "parts.h"
#include "vm/heap.h"
#include "vm/page.h"
#include "xsystem4.h"
#include "../src/hll/hll.h"
#include "../src/parts/parts_internal.h"

static const char fixture_name[] = "__construction_alpha_fixture__";
static const unsigned char background[] = {23, 41, 67};
static const unsigned char alpha[] = {17, 0, 128, 255, 64, 200};
extern struct static_library lib_PartsEngine;

bool __real_asset_exists_by_name(enum asset_type type, const char *name, int *id);
struct cg *__real_asset_cg_load_by_name(const char *name, int *id);

/* Only the asset storage is replaced. VM, HLL, queue and GPU remain real. */
bool __wrap_asset_exists_by_name(enum asset_type type, const char *name, int *id)
{
	if (type == ASSET_CG && !strcmp(name, fixture_name)) {
		if (id) *id = 1;
		return true;
	}
	return __real_asset_exists_by_name(type, name, id);
}

static void source_pixel(unsigned char *p, int x, int y)
{
	p[0] = 170 + x;
	p[1] = 80 + y * 10;
	p[2] = 210 - x;
	p[3] = alpha[x];
}

struct cg *__wrap_asset_cg_load_by_name(const char *name, int *id)
{
	if (strcmp(name, fixture_name))
		return __real_asset_cg_load_by_name(name, id);
	struct cg *cg = calloc(1, sizeof(*cg));
	assert(cg);
	cg->type = ALCG_QNT;
	cg->metrics = (struct cg_metrics) {
		.w = 6, .h = 4, .bpp = 32, .has_pixel = true, .has_alpha = true,
		.pixel_pitch = 24, .alpha_pitch = 6
	};
	unsigned char *pixels = calloc(6 * 4, 4);
	assert(pixels);
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 6; x++)
			source_pixel(pixels + (y * 6 + x) * 4, x, y);
	cg->pixels = pixels;
	if (id) *id = 1;
	return cg;
}

static int op_count(struct parts_construction_process *cp)
{
	int count = 0;
	struct parts_cp_op *op;
	TAILQ_FOREACH(op, &cp->ops, entry)
		count++;
	return count;
}

static void invoke_real_wrapper(int number, int state)
{
	/* F3963 invokes F110, F4355 and the actual HLL21:541 through ffi.c. */
	struct string *name = cstr_to_string(fixture_name);
	assert(stack_ptr == 0 && call_stack_ptr == 0);
	stack_push(number);
	stack_push(vm_string_ref(name));
	stack_push(2); /* destination x */
	stack_push(3); /* destination y */
	stack_push(1); /* source x */
	stack_push(1); /* source y */
	stack_push(3); /* equal-scale width */
	stack_push(2); /* equal-scale height */
	stack_push(state);
	vm_call(3963, -1);
	assert(stack_ptr == 0 && call_stack_ptr == 0);
	free_string(name);
}

static void check_pixels(struct parts *p, int state, int destination_alpha)
{
	Texture *texture = &p->states[state - 1].common.texture;
	assert(texture->w == 10 && texture->h == 8);
	unsigned char *pixels = gfx_get_pixels(texture);
	assert(pixels);
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 10; x++) {
			unsigned char *actual = pixels + (y * 10 + x) * 4;
			int inside = x >= 2 && x < 5 && y >= 3 && y < 5;
			unsigned char src[4] = {0};
			if (inside)
				source_pixel(src, x - 1, y - 2);
			for (int c = 0; c < 3; c++) {
				int expected = inside
					? (src[c] * src[3] + background[c] * (255 - src[3]) + 127) / 255
					: background[c];
				assert(abs(actual[c] - expected) <= (inside ? 1 : 0));
			}
			assert(actual[3] == destination_alpha);
		}
	}
	free(pixels);
	printf("PIXELS state=%d dst_alpha=%d src_alpha=0/128/255 crop=PASS\n",
		state, destination_alpha);
}

/*
 * F3963 receives a 状態 (state) argument but the real bytecode ignores it and
 * always pushes literal State=1 into AFL_Parts_AddConstructProcess, so the
 * command must land in parts state 1 regardless of the 状態 argument.  The
 * real caller (F6547 SceneMirrorPieceView@initMirrorPiece) also passes 1.
 */
static int check_case(int number, int state_arg, int destination_alpha)
{
	assert(PE_AddCreateToPartsConstructionProcess(number, 10, 8, 1));
	assert(PE_AddFillWithAlphaToPartsConstructionProcess(number, 0, 0, 10, 8,
		background[0], background[1], background[2], destination_alpha, 1));
	assert(PE_BuildPartsConstructionProcess(number, 1));
	struct parts *p = parts_try_get(number);
	assert(p && op_count(&p->states[0].cproc) == 2);
	invoke_real_wrapper(number, state_arg);
	struct parts_construction_process *cp = &p->states[0].cproc;
	int count = op_count(cp);
	printf("AIN_WRAPPER number=%d state_arg=%d operations=%d expected=3\n", number, state_arg, count);
	if (count != 3) {
		fputs("FAIL real AIN wrapper dropped command19\n", stderr);
		return 1;
	}
	struct parts_cp_op *op = NULL, *cursor;
	TAILQ_FOREACH(cursor, &cp->ops, entry)
		op = cursor;
	assert(op);
	assert(op->type == PARTS_CP_DRAW_CUT_CG);
	assert(op->cut_cg.dx == 2 && op->cut_cg.dy == 3);
	assert(op->cut_cg.dw == 3 && op->cut_cg.dh == 2);
	assert(op->cut_cg.sx == 1 && op->cut_cg.sy == 1);
	assert(op->cut_cg.sw == 3 && op->cut_cg.sh == 2);
	assert(op->cut_cg.interp_type == 1);
	assert(!strcmp(op->cut_cg.cg_name->text, fixture_name));
	assert(op->cut_cg.cg_name->ref >= 1);
	for (int s = 1; s < PARTS_NR_STATES; s++)
		assert(p->states[s].type == PARTS_UNINITIALIZED);
	assert(PE_BuildPartsConstructionProcess(number, 1));
	check_pixels(p, 1, destination_alpha);
	return 0;
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain && ain->version == 6);
	initialize_instructions(ain->version);
	assert(ain->functions[3963].nr_args == 9);
	assert(ain->functions[3963].return_type.data == AIN_VOID);
	struct ain_hll_function *signature = &ain->libraries[21].functions[541];
	assert(!strcmp(signature->name, "AddPartsConstructionProcess"));
	assert(signature->return_type.data == AIN_VOID && signature->nr_arguments == 3);
	assert(signature->arguments[0].type.data == AIN_REF_ARRAY_INT);
	assert(signature->arguments[1].type.data == AIN_REF_ARRAY_FLOAT);
	assert(signature->arguments[2].type.data == AIN_REF_ARRAY_STRING);
	heap_init();
	stack = calloc(4096, sizeof(*stack));
	assert(stack);
	stack_ptr = call_stack_ptr = 0;
#ifdef DEBUGGER_ENABLED
	dbg_enabled = false;
#endif
	config.save_dir = getenv("DRAWING_TEST_SAVE_DIR");
	assert(config.save_dir && config.save_dir[0] == '/');
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "Drawing construction regression";
	config.view_width = 320;
	config.view_height = 240;
	asset_manager_init();
	init_libraries();
	assert(PE_Init());
	const int destination_alpha[] = {0, 73, 255};
	for (int i = 0; i < 3; i++)
		if (check_case(5001 + i, i + 1, destination_alpha[i]))
			return 1;
	struct page *saved = NULL;
	assert(PE_Save(&saved) && PE_Load(&saved));
	for (int i = 0; i < 3; i++)
		check_pixels(parts_try_get(5001 + i), 1, destination_alpha[i]);
	delete_page_vars(saved);
	free_page(saved);
	for (int i = 0; lib_PartsEngine.functions[i].name; i++)
		assert(strcmp(lib_PartsEngine.functions[i].name, "GetPartsConstructionProcess"));
	PE_Reset();
	puts("PASS real AIN construction wrapper, queue, reference lifetime, pixels and XPE");
	return 0;
}
