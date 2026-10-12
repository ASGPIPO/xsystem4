/* Exercise the Rance03 SystemService mipmap debug ABI through real exports. */
#include <assert.h>
#include <ffi.h>
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

static int get(void *function)
{
	ffi_cif cif;
	ffi_arg value = -1;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, &ffi_type_sint32, NULL) == FFI_OK);
	ffi_call(&cif, FFI_FN(function), &value, NULL);
	return (int)value;
}

static void set(void *function, int value)
{
	ffi_cif cif;
	ffi_type *types[] = {&ffi_type_sint32};
	void *args[] = {&value};
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(function), NULL, args);
}

int main(void)
{
	setbuf(stdout, NULL);
	int error;
	struct ain *program = ain_open("/root/rance03/Rance03.ain", &error);
	assert(program && error == AIN_SUCCESS);
	int lib = ain_get_library(program, "SystemService");
	assert(lib == 17);
	int setter = ain_get_library_function(program, lib, "Debug_SetShowMipmapLevel");
	int getter = ain_get_library_function(program, lib, "Debug_IsShowMipmapLevel");
	assert(setter == 49 && getter == 50);
	struct ain_hll_function *s = &program->libraries[lib].functions[setter];
	struct ain_hll_function *g = &program->libraries[lib].functions[getter];
	assert(s->return_type.data == AIN_VOID && s->nr_arguments == 1
			&& s->arguments[0].type.data == AIN_BOOL);
	assert(g->return_type.data == AIN_BOOL && g->nr_arguments == 0);
	void *set_fn = binding(s->name);
	void *get_fn = binding(g->name);
	printf("Rance03 lib%d setter%d void(bool) %s; getter%d bool() %s\n",
			lib, setter, set_fn ? "bound" : "MISSING",
			getter, get_fn ? "bound" : "MISSING");
	if (!set_fn || !get_fn)
		return 1;
	assert(get(get_fn) == 0);
	set(set_fn, 1);
	assert(get(get_fn) == 0);
	set(set_fn, 1);
	assert(get(get_fn) == 0);
	set(set_fn, 0);
	assert(get(get_fn) == 0);
	puts("PASS unavailable overlay stays off after repeated enable and disable via actual HLL ABI");
	ain_free(program);
	return 0;
}
