/* Exercise the Rance03 SystemService mouse cursor configuration ABI. */
#include <assert.h>
#include <ffi.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "system4/ain.h"
#include "../src/hll/hll.h"

extern struct static_library lib_SystemService;

static void *binding(const char *name)
{
	for (int i = 0; lib_SystemService.functions[i].name; i++)
		if (!strcmp(lib_SystemService.functions[i].name, name))
			return lib_SystemService.functions[i].fun;
	return NULL;
}

static bool set_config(void *function, int type, int value)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32, &ffi_type_sint32};
	void *args[] = {&type, &value};
	ffi_arg result = 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_uint8, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(function), &result, args);
	return result != 0;
}

static bool get_config(void *function, int type, int *value)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32, &ffi_type_pointer};
	int *value_pointer = value;
	void *args[] = {&type, &value_pointer};
	ffi_arg result = 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_uint8, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(function), &result, args);
	return result != 0;
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	int error;
	struct ain *program = ain_open("/root/rance03/Rance03.ain", &error);
	assert(program && error == AIN_SUCCESS);
	int library = ain_get_library(program, "SystemService");
	assert(library == 17);
	int setter = ain_get_library_function(program, library, "SetMouseCursorConfig");
	int getter = ain_get_library_function(program, library, "GetMouseCursorConfig");
	assert(setter >= 0 && getter >= 0);
	struct ain_hll_function *set_signature = &program->libraries[library].functions[setter];
	struct ain_hll_function *get_signature = &program->libraries[library].functions[getter];
	assert(set_signature->return_type.data == AIN_BOOL && set_signature->nr_arguments == 2);
	assert(set_signature->arguments[0].type.data == AIN_INT
			&& set_signature->arguments[1].type.data == AIN_INT);
	assert(get_signature->return_type.data == AIN_BOOL && get_signature->nr_arguments == 2);
	assert(get_signature->arguments[0].type.data == AIN_INT
			&& get_signature->arguments[1].type.data == AIN_REF_INT);
	void *set_function = binding(set_signature->name);
	void *get_function = binding(get_signature->name);
	assert(set_function && get_function);

	int value = -1;
	assert(set_config(set_function, 0, 0));
	assert(get_config(get_function, 0, &value) && value == 0);
	assert(set_config(set_function, 0, 1));
	assert(get_config(get_function, 0, &value) && value == 1);

	const int invalid_types[] = {-1, 1, INT_MAX};
	for (int repeat = 0; repeat < 20; repeat++) {
		for (unsigned i = 0; i < sizeof(invalid_types) / sizeof(*invalid_types); i++) {
			value = 0x13579bdf;
			assert(!set_config(set_function, invalid_types[i], repeat));
			assert(!get_config(get_function, invalid_types[i], &value));
			assert(value == 0x13579bdf);
		}
	}

	printf("PASS actual AIN SystemService functions %d/%d preserve supported and invalid semantics\n",
			setter, getter);
	ain_free(program);
	return 0;
}
