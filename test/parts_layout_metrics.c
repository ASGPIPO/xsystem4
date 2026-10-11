/* Real AIN/libffi nested pending-layout regression; no game messages or images. */
#include <assert.h>
#include <ffi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "xsystem4.h"
#include "../src/parts/parts_internal.h"
#include "../src/hll/hll.h"

extern struct static_library lib_PartsEngine;

static void *bound(const char *name)
{
	for (int i = 0; lib_PartsEngine.functions[i].name; i++)
		if (!strcmp(name, lib_PartsEngine.functions[i].name))
			return lib_PartsEngine.functions[i].fun;
	return NULL;
}

static int invoke(const char *name, int *values, int count)
{
	int lib = ain_get_library(ain, "PartsEngine");
	int fn = ain_get_library_function(ain, lib, name);
	assert(lib == 21 && fn >= 0 && bound(name));
	struct ain_hll_function *h = &ain->libraries[lib].functions[fn];
	assert(h->nr_arguments == count && count <= 9);
	ffi_type *types[9];
	void *args[9];
	for (int i = 0; i < count; i++) {
		assert(h->arguments[i].type.data == AIN_INT);
		types[i] = &ffi_type_sint32;
		args[i] = &values[i];
	}
	assert(h->return_type.data == AIN_VOID || h->return_type.data == AIN_INT
			|| h->return_type.data == AIN_BOOL);
	ffi_cif cif;
	ffi_arg result = 0;
	ffi_type *ret = h->return_type.data == AIN_VOID ? &ffi_type_void : &ffi_type_sint32;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, count, ret, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound(name)), &result, args);
	return (int)result;
}

static void canvas(int no, int w, int h)
{
	int create[] = {no, w, h, 1};
	assert(invoke("Parts_AddCreatePixelOnlyToPartsConstructionProcess", create, 4));
	int build[] = {no, 1};
	assert(invoke("Parts_BuildPartsConstructionProcess", build, 2));
}

static int width(int no)
{
	int args[] = {no, 1};
	return invoke("Parts_GetPartsWidth", args, 2);
}

static int height(int no)
{
	int args[] = {no, 1};
	return invoke("Parts_GetPartsHeight", args, 2);
}

static void child(int parent, int no)
{
	int args[] = {parent, no};
	invoke("AddChild", args, 2);
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "Private numeric layout regression";
	config.view_width = 1024;
	config.view_height = 768;
	config.font_paths[0] = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
	config.font_paths[1] = config.font_paths[0];
	asset_manager_init();
	((void (*)(void))bound("_PreLink"))();
	PE_Init();

	int outer = PE_GetFreeNumber();
	int vertical[] = {outer, 1};
	invoke("SetLayoutBoxLayoutType", vertical, 2);
	int inner = PE_GetFreeNumber();
	int horizontal[] = {inner, 2};
	invoke("SetLayoutBoxLayoutType", horizontal, 2);
	int text = PE_GetFreeNumber();
	canvas(text, 473, 22);
	int separator = PE_GetFreeNumber();
	canvas(separator, 497, 2);
	int button = PE_GetFreeNumber();
	int button_size[] = {button, 150, 35};
	invoke("SetButtonSize", button_size, 3);
	child(inner, button);
	child(outer, text);
	child(outer, separator);
	child(outer, inner);
	assert(!parts_try_get(text)->parent);
	assert(parts_try_get(text)->pending_parent == outer);

	int w = width(outer), h = height(outer);
	printf("AIN pending nested layout width=%d height=%d expected=497x59\n", w, h);
	if (w != 497 || h != 59)
		return 1;
	assert(width(outer) == w && height(outer) == h);
	assert(width(inner) == 150 && height(inner) == 35);
	assert(parts_try_get(text)->parent == parts_try_get(outer));
	assert(parts_try_get(button)->parent == parts_try_get(inner));
	assert(parts_try_get(text)->pending_parent == -1);
	PE_UpdateComponent(0);
	assert(width(outer) == w && height(outer) == h);
	parts_release_all();
	puts("PASS real AIN/libffi nested pending LayoutBox metrics and stable component update");
	return 0;
}
