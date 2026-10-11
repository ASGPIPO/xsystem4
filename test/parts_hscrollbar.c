/* Real ordinary configuration EX/CG and AIN/libffi scrollbar regression. */
#include <assert.h>
#include <ffi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/archive.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "input.h"
#include "parts.h"
#include "xsystem4.h"
#include "../src/hll/hll.h"
#include "../src/parts/parts_internal.h"

extern struct static_library lib_PartsEngine;
extern bool PE_CreateHScrollbar(int, struct string *, int, int, int, int, int, int, int, int)
	__attribute__((weak));
static bool pressed;
static Point pointer;
bool __wrap_key_is_down(enum sact_keycode key)
{ return key == VK_LBUTTON && pressed; }
void __wrap_mouse_get_pos(int *x, int *y)
{ *x = pointer.x; *y = pointer.y; }

static void *bound(const char *name)
{
	for (int i = 0; lib_PartsEngine.functions[i].name; i++)
		if (!strcmp(name, lib_PartsEngine.functions[i].name))
			return lib_PartsEngine.functions[i].fun;
	return NULL;
}
static void signature(const char *name, enum ain_data_type ret, int argc)
{
	int lib = ain_get_library(ain, "PartsEngine");
	int fno = ain_get_library_function(ain, lib, name);
	assert(fno >= 0);
	struct ain_hll_function *f = &ain->libraries[lib].functions[fno];
	assert(f->return_type.data == ret && f->nr_arguments == argc);
	assert(f->arguments[0].type.data == AIN_INT);
	if (argc == 2) assert(f->arguments[1].type.data == AIN_FLOAT);
	printf("real_AIN_HLL %s return=%d argc=%d bound=%d\n",
			name, ret, argc, bound(name) != NULL);
}
static float get_rate(int number)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32};
	void *args[] = {&number};
	float result = -1.0f;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_float, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("GetHScrollbarScrollRate")), &result, args);
	return result;
}
static void set_rate(int number, float rate)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32, &ffi_type_float};
	void *args[] = {&number, &rate};
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("SetHScrollbarScrollRate")), NULL, args);
}
static struct ex_tree *child(struct ex_tree *tree, const char *key)
{ return tree && !tree->is_leaf ? ex_tree_get_child(tree, key) : NULL; }
static int integer(struct ex_tree *tree, const char *key)
{
	struct ex_tree *v = child(tree, key);
	assert(v && v->is_leaf && v->leaf.value.type == EX_INT);
	return v->leaf.value.i;
}
static struct string *text(struct ex_tree *tree, const char *key)
{
	struct ex_tree *v = child(tree, key);
	assert(v && v->is_leaf && v->leaf.value.type == EX_STRING);
	return v->leaf.value.s;
}
static struct ex_tree *bars[3];
static unsigned nr_bars;
static void find_bars(struct ex_tree *tree)
{
	struct ex_tree *kind = child(tree, "\xb7\x4e\xee\x90\x84\x65\xc7\xe9\x88\xf3");
	if (kind && integer(kind, "\xb2\xbf\xbc\xfe\xc0\xe0\xd0\xcd") == 3) {
		assert(nr_bars < 3);
		bars[nr_bars++] = kind;
	}
	struct ex_tree *children = child(tree, "\xd7\xd3\xb2\xbf\xbc\xfe");
	if (children && !children->is_leaf)
		for (unsigned i = 0; i < children->nr_children; i++)
			find_bars(&children->children[i]);
}
static bool create_bar(int number, struct ex_tree *kind)
{
	return PE_CreateHScrollbar(number, text(kind, "CG\xc3\xfb"),
			integer(kind, "\xb3\xa4\xb6\xc8"), integer(kind, "\xb7\xf9"),
			integer(kind, "\xc7\xb0\xb3\xdf\xb4\xe7"), integer(kind, "\xb4\xce\xb3\xdf\xb4\xe7"),
			integer(kind, "\xc8\xab\xcc\xe5\xa5\xb9\xa5\xaf\xa5\xed\xa9\x60\xa5\xeb\xc1\xbf"),
			integer(kind, "\xcf\xd4\xca\xbe\xc1\xbf"), integer(kind, "\xb9\xf6\xb6\xaf\xce\xbb\xd6\xc3"),
			integer(kind, "\xb0\xb4\xc5\xa5\xb5\xe3\xbb\xf7\xd2\xc6\xb6\xaf\xc1\xbf"));
}
static Texture skin_texture(struct string *base, const char *suffix)
{
	struct string *name = string_dup(base);
	string_append_cstr(&name, suffix, strlen(suffix));
	struct cg *cg = asset_cg_load_by_name(name->text, NULL);
	assert(cg);
	Texture texture = {0};
	gfx_init_texture_with_cg(&texture, cg);
	assert(texture.handle);
	cg_free(cg);
	free_string(name);
	return texture;
}
static void pixels(int number, struct string *skin, int pos)
{
	Texture bg = skin_texture(skin, "\xa3\xaf\xb1\xb3\xbe\xb0");
	assert(bg.w == 430 && bg.h == 43);
	const char *suffixes[] = {
		"\xa3\xaf\xcc\xf5\xa3\xaf\xcd\xa8\xb3\xa3",
		"\xa3\xaf\xcc\xf5\xa3\xaf\xbf\xaa",
		"\xa3\xaf\xcc\xf5\xa3\xaf\xbb\xf7\xb5\xb9"
	};
	uint8_t *background = gfx_get_pixels(&bg);
	int offset = 406 * pos / 99;
	struct parts *p = parts_try_get(number);
	for (int state = 0; state < 3; state++) {
		Texture thumb = skin_texture(skin, suffixes[state]);
		assert(thumb.w == 24 && thumb.h == 43);
		uint8_t *foreground = gfx_get_pixels(&thumb);
		uint8_t *actual = gfx_get_pixels(&p->states[state].common.texture);
		unsigned bg_pixels = 0, thumb_pixels = 0;
		for (int y = 0; y < 43; y++) for (int x = 0; x < 430; x++) {
			int target = (y*430+x)*4;
			if (x >= offset && x < offset+24) {
				int source = (y*24+x-offset)*4;
				if (foreground[source+3] == 255) {
					assert(!memcmp(actual+target, foreground+source, 4));
					thumb_pixels++;
				}
			} else if (background[target+3] == 255) {
				assert(!memcmp(actual+target, background+target, 4));
				bg_pixels++;
			}
		}
		assert(bg_pixels && thumb_pixels);
		printf("real_skin_pixels state=%d pos=%d bg=%u thumb=%u\n",
				state+1, pos, bg_pixels, thumb_pixels);
		free(foreground);
		free(actual);
		gfx_delete_texture(&thumb);
	}
	free(background);
	gfx_delete_texture(&bg);
}
int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	signature("GetHScrollbarScrollRate", AIN_FLOAT, 1);
	signature("SetHScrollbarScrollRate", AIN_VOID, 2);
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "Private ordinary scrollbar regression";
	config.view_width = 1024;
	config.view_height = 768;
	asset_manager_init();
	const char *page = "\xc5\xe4\xd6\xc3\xa3\xaf\xa3\xb0\xa3\xb1\xa3\xaf\xb6\xd4\xb0\xd7\xcb\xd9\xb6\xc8\xd2\xb3.pact";
	struct archive_data *data = asset_get_by_name(ASSET_PACT, page, NULL);
	assert(data && data->size == 3446);
	struct ex *ex = ex_read(data->data, data->size);
	assert(ex);
	archive_free_data(data);
	struct ex_tree *activity = ex_get_tree(ex, "\xbb\xee\xb6\xaf");
	assert(activity && !activity->is_leaf);
	for (unsigned i = 0; i < activity->nr_children; i++)
		find_bars(&activity->children[i]);
	assert(nr_bars == 3);
	for (unsigned i = 0; i < nr_bars; i++) {
		assert(integer(bars[i], "\xb3\xa4\xb6\xc8") == 430);
		assert(integer(bars[i], "\xb7\xf9") == 43);
		assert(integer(bars[i], "\xc8\xab\xcc\xe5\xa5\xb9\xa5\xaf\xa5\xed\xa9\x60\xa5\xeb\xc1\xbf") == 100);
		assert(integer(bars[i], "\xcf\xd4\xca\xbe\xc1\xbf") == 1);
		assert(integer(bars[i], "\xb9\xf6\xb6\xaf\xce\xbb\xd6\xc3") == (i ? 0 : 55));
		struct string *skin = text(bars[i], "CG\xc3\xfb");
		char *name = sjis2utf(skin->text, 0);
		printf("ordinary_skin %u %s width=430 height=43 total=100 view=1 pos=%d\n", i, name, i ? 0 : 55);
		free(name);
	}
	struct afa_archive *cg = afa_open("/root/rance03/Rance03CG.afa", 0, &error);
	assert(cg);
	struct string *base = text(bars[0], "CG\xc3\xfb");
	for (unsigned i = 0; i < cg->nr_files; i++) {
		const char *n = cg->files[i].name->text;
		if (strncmp(n, base->text, base->size)) continue;
		struct cg_metrics m;
		assert(asset_cg_get_metrics_by_name(n, &m));
		char *u = sjis2utf(n, 0);
		printf("ordinary_skin_asset %s %dx%d\n", u, m.w, m.h);
		free(u);
	}
	if (!bound("GetHScrollbarScrollRate") || !bound("SetHScrollbarScrollRate")) {
		fprintf(stderr, "FAIL actual ordinary configuration requires unbound HScrollbar rate ABI\n");
		return 1;
	}
	archive_free(&cg->ar);
	assert(PE_CreateHScrollbar);
	((void (*)(void))bound("_PreLink"))();
	PE_Init();
	struct string *skin = text(bars[0], "CG\xc3\xfb");
	int number = PE_GetFreeNumber();
	assert(create_bar(number, bars[0]));
	PE_SetPos(number, 50, 60);
	PE_SetDelegateIndex(number, 42);
	PE_UpdateComponent(0);
	assert(fabsf(get_rate(number) - 55.0f / 99.0f) < 0.000001f);
	struct parts *p = parts_try_get(number);
	assert(p && p->states[0].common.w == 430 && p->states[0].common.h == 43);
	assert(p->states[0].common.hitbox.w == 430 && p->states[0].common.hitbox.h == 43);
	pixels(number, skin, 55);
	assert(get_rate(-12345) == 0.0f);
	set_rate(number, 1.5f);
	assert(get_rate(number) == 1.0f);
	pixels(number, skin, 99);
	assert(PE_GetMessageType() == PARTS_MSG_SCROLL);
	assert(PE_GetMessageDelegateIndex() == 42 && PE_GetMessagePartsNumber() == number);
	assert(PE_GetMessageVariableCount() == 2 && PE_GetMessageVariableInt(0) == 99
			&& PE_GetMessageVariableInt(1) == 100);
	PE_ReleaseMessage();
	set_rate(number, 1.0f);
	assert(PE_GetMessageType() == 0);
	set_rate(number, -0.5f);
	assert(get_rate(number) == 0.0f);
	PE_ReleaseMessage();
	pixels(number, skin, 0);
	set_rate(number, NAN);
	assert(get_rate(number) == 0.0f && PE_GetMessageType() == 0);
	PE_BeginInput();
	pointer = (Point){50 + 215, 60 + 21};
	pressed = true;
	PE_UpdateInputState(16);
	assert(get_rate(number) > 0.4f && get_rate(number) < 0.6f);
	pointer.x = 49;
	PE_UpdateInputState(16);
	assert(get_rate(number) == 0.0f);
	pointer.x = 50 + 431;
	PE_UpdateInputState(16);
	assert(get_rate(number) == 1.0f);
	pressed = false;
	PE_UpdateInputState(16);
	PE_EndInput();
	set_rate(number, 0.0f);
	PE_BeginInput();
	pointer = (Point){50 + 215, 60 + 21};
	pressed = true;
	PE_UpdateInputState(16);
	float canceled = get_rate(number);
	PE_EndInput();
	pressed = false;
	pointer.x = 50 + 431;
	PE_UpdateInputState(16);
	PE_BeginInput();
	pressed = true;
	PE_UpdateInputState(16);
	if (get_rate(number) != canceled) {
		fprintf(stderr, "FAIL canceled scrollbar drag survives EndInput\n");
		return 1;
	}
	pressed = false;
	PE_UpdateInputState(16);
	PE_EndInput();
	assert(PE_CreateHScrollbar(number, skin, 430, 43, 0, 0, 1, 1, 55, 1));
	assert(get_rate(number) == 0.0f);
	PE_ReleaseParts(number);
	PE_Reset();
	ex_free(ex);
	puts("PASS real ordinary EX; libffi float/void; actual three-state pixels; geometry; 55/99; bounds; messages; track/drag endpoints; canceled drag reset; empty range");
	return 0;
}
