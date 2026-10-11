/* Real Rance03 AIN/libffi and waveform-backed click-miss sound contract. */
#include <assert.h>
#include <ffi.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/afa.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "audio.h"
#include "input.h"
#include "parts.h"
#include "xsystem4.h"
#include "../src/hll/hll.h"

extern struct static_library lib_PartsEngine;
static bool pressed;
static int pointer_x = 900, pointer_y = 700;
static int plays, last_sound;
extern bool __real_audio_play_sound(int number);

bool __wrap_audio_play_sound(int number)
{
	if (number < 0)
		return false;
	plays++;
	last_sound = number;
	assert(__real_audio_play_sound(number));
	return true;
}

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

static void set_name(struct string *name)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_pointer};
	void *args[] = {&name};
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("SetClickMissSoundName")), NULL, args);
}

static bool set_number(int number)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32};
	void *args[] = {&number};
	ffi_arg result = 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint32, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(bound("SetClickMissSoundNumber")), &result, args);
	return result != 0;
}

static void click(bool active, int expected)
{
	int before = plays;
	if (active) PE_BeginInput();
	else PE_EndInput();
	pressed = true;
	PE_UpdateInputState(16);
	PE_UpdateInputState(16);
	assert(plays == before);
	pressed = false;
	PE_UpdateInputState(16);
	PE_UpdateInputState(16);
	assert(plays == before + expected);
	PE_EndInput();
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	int lib = ain_get_library(ain, "PartsEngine");
	int fno = ain_get_library_function(ain, lib, "SetClickMissSoundName");
	assert(fno >= 0);
	struct ain_hll_function *f = &ain->libraries[lib].functions[fno];
	printf("real_AIN_HLL name=SetClickMissSoundName return=%d argc=%d arg0=%d bound=%d\n",
		f->return_type.data, f->nr_arguments, f->arguments[0].type.data,
		bound("SetClickMissSoundName") != NULL);
	assert(f->return_type.data == AIN_VOID && f->nr_arguments == 1);
	assert(f->arguments[0].type.data == AIN_STRING);
	if (!bound("SetClickMissSoundName")) {
		fprintf(stderr, "FAIL actual ordinary dialog needs unbound SetClickMissSoundName\n");
		return 1;
	}
	assert(bound("SetClickMissSoundNumber"));
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "Private click-miss sound regression";
	config.view_width = 1024;
	config.view_height = 768;
	asset_manager_init();
	((void (*)(void))bound("_PreLink"))();
	PE_Init();
	audio_init();
	struct afa_archive *sounds = afa_open("/root/rance03/Rance03Sound.afa", 0, &error);
	assert(sounds && sounds->nr_files);
	struct string *known = NULL;
	for (unsigned i = 0; i < sounds->nr_files; i++) {
		struct archive_data *data = asset_get_by_name(ASSET_SOUND, sounds->files[i].name->text, NULL);
		if (!data)
			continue;
		bool waveform = data->size >= 12
			&& ((!memcmp(data->data, "RIFF", 4) && !memcmp(data->data + 8, "WAVE", 4))
				|| !memcmp(data->data, "OggS", 4) || !memcmp(data->data, "fLaC", 4));
		archive_free_data(data);
		if (waveform) {
			known = sounds->files[i].name;
			break;
		}
	}
	assert(known);
	int number;
	assert(asset_exists_by_name(ASSET_SOUND, known->text, &number));
	assert(asset_exists(ASSET_SOUND, number));
	printf("real_wave_asset_number=%d\n", number);
	unsigned ref = known->ref;
	click(true, 0);
	set_name(known);
	assert(known->ref == ref);
	click(true, 1);
	assert(last_sound == number);
	click(false, 0);
	struct string *empty = cstr_to_string("");
	set_name(empty);
	click(true, 0);
	assert(set_number(number));
	struct string *missing = cstr_to_string("__codex_missing_click_miss_sound__");
	set_name(missing);
	set_name(NULL);
	assert(!set_number(INT_MAX));
	click(true, 1);
	assert(last_sound == number);
	assert(set_number(-1));
	click(true, 0);
	set_name(known);
	int hit = PE_GetFreeNumber();
	PE_AddCreatePixelOnlyToPartsConstructionProcess(hit, 20, 20, 1);
	assert(PE_BuildPartsConstructionProcess(hit, 1));
	PE_SetPos(hit, 0, 0);
	PE_SetClickable(hit, true);
	PE_UpdateComponent(0);
	pointer_x = pointer_y = 5;
	click(true, 0);
	set_name(empty);
	free_string(empty);
	free_string(missing);
	PE_Reset();
	puts("PASS real AIN ABI; real-wave miss once; hit/inactive silent; empty/-1 clear; invalid preserves sound");
	return 0;
}
