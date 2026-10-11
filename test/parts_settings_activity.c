/* Real ordinary Rance03 settings PACT activity contract. */
#include <assert.h>
#include <ffi.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "input.h"
#include "parts.h"
#include "xsystem4.h"
#include "vm.h"
#include "../src/hll/hll.h"
#include "../src/parts/parts_internal.h"

extern struct static_library lib_PartsEngine;
static bool pressed;
static int pointer_x, pointer_y;

bool __wrap_key_is_down(enum sact_keycode key)
{
	return key == VK_LBUTTON && pressed;
}

void __wrap_mouse_get_pos(int *x, int *y)
{
	*x = pointer_x;
	*y = pointer_y;
}

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
	assert(fno >= 0 && bound(name));
	struct ain_hll_function *f = &ain->libraries[lib].functions[fno];
	assert(f->return_type.data == ret && f->nr_arguments == argc);
	assert(f->arguments[0].type.data == AIN_INT);
	for (int i = 1; i < argc; i++) {
		int expected = !strcmp(name, "CheckBoxChecked") ? AIN_BOOL
			: !strcmp(name, "SetHScrollbarScrollRate") ? AIN_FLOAT : AIN_INT;
		assert(f->arguments[i].type.data == expected);
	}
}

static bool checked(int number)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32};
	void *args[] = {&number};
	ffi_arg result = 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint32, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("IsCheckBoxChecked")), &result, args);
	return result != 0;
}

static void set_checked(int number, bool value)
{
	ffi_cif cif;
	int check = value;
	ffi_type *types[] = {&ffi_type_sint32, &ffi_type_sint32};
	void *args[] = {&number, &check};
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("CheckBoxChecked")), NULL, args);
}

static void set_color(int number, int red, int green, int blue)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32, &ffi_type_sint32, &ffi_type_sint32, &ffi_type_sint32};
	void *args[] = {&number, &red, &green, &blue};
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("SetCheckBoxColor")), NULL, args);
}

static float rate(int number)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32};
	void *args[] = {&number};
	float result = -1.0f;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_float, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("GetHScrollbarScrollRate")), &result, args);
	return result;
}

static void set_rate(int number, float value)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32, &ffi_type_float};
	void *args[] = {&number, &value};
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("SetHScrollbarScrollRate")), NULL, args);
}

static int part(struct string *activity, const char *name)
{
	struct string *key = cstr_to_string(name);
	int number = PE_GetActivityPartsNumber(activity, key);
	free_string(key);
	assert(number >= 0);
	return number;
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
	config.game_name = "Private settings activity regression";
	config.view_width = 1024;
	config.view_height = 768;
	asset_manager_init();
	PE_Init();
	struct string *name = cstr_to_string("settings-contract");
	struct string *file = cstr_to_string("\xc5\xe4\xd6\xc3\xa3\xaf\xa3\xb0\xa3\xb1\xa3\xaf\xb6\xd4\xb0\xd7\xcb\xd9\xb6\xc8\xd2\xb3.pact");
	struct archive_data *page = asset_get_by_name(ASSET_PACT, file->text, NULL);
	assert(page && page->size == 3446);
	archive_free_data(page);
	assert(PE_ReadActivityFile(name, file));
	assert(PE_NumofActivityParts(name) == 36);
	signature("GetHScrollbarScrollRate", AIN_FLOAT, 1);
	signature("SetHScrollbarScrollRate", AIN_VOID, 2);
	signature("IsCheckBoxChecked", AIN_BOOL, 1);
	signature("CheckBoxChecked", AIN_VOID, 2);
	signature("SetCheckBoxColor", AIN_VOID, 4);
	int root = part(name, "\xb8\xf9\xb2\xbf\xbc\xfe");
	assert(PE_GetComponentType(root, 1) == 8);
	int speed = part(name, "\x53\x59\x53\x5f\xb6\xd4\xb0\xd7\xcb\xd9\xb6\xc8\xbb\xac\xbf\xe9");
	int auto_bar = part(name, "\x53\x59\x53\x5f\xd7\xd4\xb6\xaf\xc4\xa3\xca\xbd\xbb\xac\xbf\xe9");
	int skip_bar = part(name, "\x53\x59\x53\x5f\xcc\xf8\xb9\xfd\xc4\xa3\xca\xbd\xbb\xac\xbf\xe9");
	assert(PE_GetPartsWidth(speed, 1) == 430 && PE_GetPartsHeight(speed, 1) == 43);
	assert(fabsf(rate(speed) - 55.0f/99.0f) < 0.001f);
	assert(rate(auto_bar) == 0.0f && rate(skip_bar) == 0.0f);
	set_rate(auto_bar, 0.75f);
	assert(fabsf(rate(auto_bar) - 0.75f) < 0.01f);
	int first = part(name, "\x53\x59\x53\x5f\xd1\xdd\xb3\xf6\xc6\xac\xb6\xce");
	int second = part(name, "\x53\x59\x53\x5f\xca\xbe\xc0\xfd\xb6\xd4\xb0\xd7\xcf\xd4\xca\xbe");
	assert(PE_GetPartsWidth(first, 1) == 50 && PE_GetPartsHeight(first, 1) == 41);
	assert(!checked(first) && checked(second));
	struct parts *first_part = parts_try_get(first), *second_part = parts_try_get(second);
	assert(first_part && second_part);
	assert(first_part->states[0].common.texture.handle && second_part->states[0].common.texture.handle);
	uint8_t *unchecked_pixels = gfx_get_pixels(&first_part->states[0].common.texture);
	uint8_t *checked_pixels = gfx_get_pixels(&second_part->states[0].common.texture);
	assert(unchecked_pixels && checked_pixels && memcmp(unchecked_pixels, checked_pixels, 50*41*4));
	free(unchecked_pixels);
	free(checked_pixels);
	set_checked(first, true);
	assert(checked(first));
	set_checked(first, false);
	assert(!checked(first));
	set_color(first, 31, 63, 95);
	int red, green, blue;
	PE_GetMultiplyColor(first, &red, &green, &blue);
	assert(red == 31 && green == 63 && blue == 95);
	for (int i = 0; i < 36; i++) {
		struct string *part_name = cstr_to_string("");
		int number;
		assert(PE_GetActivityParts(i, name, &part_name, &number));
		if (number != first) PE_SetPassCursor(number, true);
		free_string(part_name);
	}
	pointer_x = PE_GetPartsUpperLeftPosX(first, 1) + 25;
	pointer_y = PE_GetPartsUpperLeftPosY(first, 1) + 20;
	PE_BeginInput();
	pressed = true;
	PE_UpdateInputState(16);
	PE_UpdateInputState(16);
	pressed = false;
	PE_UpdateInputState(16);
	PE_UpdateInputState(16);
	assert(PE_GetClickPartsNumber() == first);
	assert(checked(first));
	PE_EndInput();
	assert(PE_ReleaseActivity(name, NULL));
	assert(!PE_IsExistActivity(name));
	printf("PASS real 3446B settings PACT; 36 controls; 3 rates; 2 drawn checkbox states; AIN ABI/setters/click/release\n");
	free_string(name);
	free_string(file);
	PE_Reset();
	return 0;
}
