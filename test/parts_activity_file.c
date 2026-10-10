/* Exercise the real Rance03 signature, HLL binding and PACT archive lookup. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "xsystem4.h"
#include "../src/hll/hll.h"

extern struct static_library lib_PartsEngine;
static void *lookup;

static bool exists(struct string *filename)
{
	ffi_cif cif;
	ffi_type *types[] = { &ffi_type_pointer };
	void *args[] = { &filename };
	ffi_arg result = 0;
	unsigned int ref = filename ? filename->ref : 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint32, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(lookup), &result, args);
	assert(!filename || filename->ref == ref);
	return result != 0;
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	int lib = ain_get_library(ain, "PartsEngine");
	int number = ain_get_library_function(ain, lib, "IsExistActivityFile");
	assert(lib >= 0 && number >= 0);
	struct ain_hll_function *signature = &ain->libraries[lib].functions[number];
	assert(signature->return_type.data == AIN_BOOL && signature->nr_arguments == 1);
	assert(signature->arguments[0].type.data == AIN_STRING);
	for (int i = 0; lib_PartsEngine.functions[i].name; i++) {
		if (!strcmp(lib_PartsEngine.functions[i].name, "IsExistActivityFile"))
			lookup = lib_PartsEngine.functions[i].fun;
	}
	if (!lookup) {
		fprintf(stderr, "FAIL real AIN requires unbound PartsEngine.IsExistActivityFile\n");
		return 1;
	}
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	asset_manager_init();
	struct afa_archive *pact = afa_open("/root/rance03/Rance03Pact.afa", 0, &error);
	assert(pact && pact->nr_files);
	struct string *known = pact->files[0].name;
	assert(asset_exists_by_name(ASSET_PACT, known->text, NULL));
	assert(exists(known));
	struct string *variant = make_string(known->text, known->size);
	for (int i = 0; i < variant->size; i++) {
		if (variant->text[i] >= 'a' && variant->text[i] <= 'z')
			variant->text[i] -= 'a' - 'A';
		if (variant->text[i] == '\\')
			variant->text[i] = '/';
	}
	assert(exists(variant));
	struct string *missing = cstr_to_string("__codex_missing_activity__.pact");
	struct string *empty = cstr_to_string("");
	assert(!exists(missing));
	assert(!exists(empty));
	assert(!exists(NULL));
	free_string(variant);
	free_string(missing);
	free_string(empty);
	printf("PASS real AIN ABI/binding, existing PACT, case/path, missing, empty, NULL, ref ownership\n");
	return 0;
}
