/* Preserve integer mixer percentages through the actual SystemService ABI. */
#include <assert.h>
#include <ffi.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include "system4/ain.h"
#include "audio.h"
#include "mixer.h"
#include "xsystem4.h"
#include "../src/hll/hll.h"

extern struct static_library lib_SystemService;
extern struct static_library lib_KiwiSoundEngine;

static void *binding(struct static_library *library, const char *name)
{
	for (int i = 0; library->functions[i].name; i++)
		if (!strcmp(library->functions[i].name, name))
			return library->functions[i].fun;
	assert(!"missing HLL binding");
	return NULL;
}

static int invoke(void *function, bool getter, int mixer, int value, int *out)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32, getter ? &ffi_type_pointer : &ffi_type_sint32};
	int *pointer = out;
	void *args[] = {&mixer, getter ? (void *)&pointer : (void *)&value};
	ffi_arg result = 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint32, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(function), &result, args);
	return result != 0;
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain && error == AIN_SUCCESS);
	int lib = ain_get_library(ain, "SystemService");
	assert(lib >= 0);
	const char *names[] = {"SetMixerVolume", "GetMixerVolume"};
	for (int i = 0; i < 2; i++) {
		int no = ain_get_library_function(ain, lib, names[i]);
		assert(no >= 0);
		struct ain_hll_function *f = &ain->libraries[lib].functions[no];
		assert(f->return_type.data == AIN_BOOL && f->nr_arguments == 2);
		assert(f->arguments[0].type.data == AIN_INT);
		assert(f->arguments[1].type.data == (i ? AIN_REF_INT : AIN_INT));
	}
	void *set = binding(&lib_SystemService, names[0]);
	void *get = binding(&lib_SystemService, names[1]);
	config.default_volume = 100;
	((void (*)(void))binding(&lib_KiwiSoundEngine, "_PreLink"))();
	audio_init();
	int count = mixer_get_numof();
	assert(count == 8);
	int failures = 0;
	for (int mixer = 0; mixer < count; mixer++) {
		for (int expected = 0; expected <= 100; expected++) {
			int actual = -1;
			assert(invoke(set, false, mixer, expected, NULL));
			assert(invoke(get, true, mixer, 0, &actual));
			if (actual != expected) {
				printf("FAIL mixer=%d set=%d get=%d\n", mixer, expected, actual);
				failures++;
			}
		}
	}
	/* AIN settings repeatedly get and set a row during normal updates. */
	for (int expected = 53; expected <= 59; expected += 6) {
		assert(invoke(set, false, 6, expected, NULL));
		for (int i = 0; i < 100; i++) {
			int actual = -1;
			assert(invoke(get, true, 6, 0, &actual));
			assert(invoke(set, false, 6, actual, NULL));
		}
		int actual = -1;
		assert(invoke(get, true, 6, 0, &actual));
		if (actual != expected) {
			printf("FAIL repeated set=%d get=%d\n", expected, actual);
			failures++;
		}
	}
	const int invalid_mixers[] = {INT_MIN, -1, count, INT_MAX};
	assert(invoke(set, false, 0, 75, NULL));
	for (unsigned i = 0; i < sizeof(invalid_mixers) / sizeof(*invalid_mixers); i++) {
		int actual = 0x13579bdf;
		assert(!invoke(set, false, invalid_mixers[i], 53, NULL));
		assert(!invoke(get, true, invalid_mixers[i], 0, &actual));
		assert(actual == 0x13579bdf);
	}
	int actual = -1;
	assert(invoke(get, true, 0, 0, &actual) && actual == 75);
	const int outside[] = {INT_MIN, -1, 101, INT_MAX};
	for (unsigned i = 0; i < sizeof(outside) / sizeof(*outside); i++) {
		assert(invoke(set, false, 7, outside[i], NULL));
		assert(invoke(get, true, 7, 0, &actual));
		assert(actual == (outside[i] < 0 ? 0 : 100));
	}
	SDL_Quit();
	ain_free(ain);
	printf("Actual AIN ABI: %d mixers x101 integer percentages, repeated53/59, invalid-index output preservation and clamped volumes; failures=%d\n",
		count, failures);
	return failures ? 1 : 0;
}
