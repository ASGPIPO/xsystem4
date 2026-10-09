/* Verify back-scene snapshots through the real Rance03 HLL signature. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ffi.h>
#include "system4/instructions.h"
#include "../src/parts/parts.c"
#include "../src/hll/iarray.h"
#include "vm/heap.h"
#include "../src/hll/hll.h"

extern struct static_library lib_PartsEngine;

static void *exported(const char *name)
{
	for (int i = 0; lib_PartsEngine.functions[i].name; i++)
		if (!strcmp(lib_PartsEngine.functions[i].name, name))
			return lib_PartsEngine.functions[i].fun;
	return NULL;
}

static struct page *snapshot(const char *name)
{
	int libno = ain_get_library(ain, "PartsEngine");
	assert(libno >= 0);
	int fno = ain_get_library_function(ain, libno, name);
	assert(fno >= 0);
	struct ain_hll_function *f = &ain->libraries[libno].functions[fno];
	assert(f->nr_arguments == 1 && f->arguments[0].type.data == AIN_REF_ARRAY_INT
			&& f->return_type.data == AIN_BOOL);
	struct page *buffer = NULL;
	struct page **ref = &buffer;
	void *args[] = { &ref };
	ffi_type *types[] = { &ffi_type_pointer };
	ffi_cif cif;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint32, types) == FFI_OK);
	void *fn = exported(name);
	assert(fn);
	ffi_arg result = 0;
	ffi_call(&cif, FFI_FN(fn), &result, args);
	assert(result && buffer && buffer->nr_vars > 8);
	return buffer;
}

int main(void)
{
	setbuf(stdout, NULL);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	initialize_instructions(ain->version);
	heap_init();
	parts_table = ht_create(1024);
	ctrl_stack_init();
	PE_SetPos(100, 37, 48);
	PE_SetAlpha(100, 173);
	PE_SetShow(101, false);
	PE_SetWantSaveBackScene(102, false); /* system/FPS parts */
	PE_parts_set_want_save(103, false);
	puts("REAL Rance03 SaveBackScene(bool, ref int[]) binding");
	struct page *back = snapshot("SaveBackScene");
	struct page *normal = snapshot("Save");
	assert(PE_Load(&back));
	assert(PE_IsExist(100) && PE_IsExist(101));
	assert(!PE_IsExist(102) && !PE_IsExist(103));
	assert(PE_GetPartsX(100) == 37 && PE_GetPartsY(100) == 48);
	assert(parts_get(100)->local.alpha == 173 && !parts_get(101)->local.show);
	puts("PASS back-scene XPE decoded with selected state and hidden parts; system/FPS excluded");
	assert(PE_Load(&normal));
	assert(PE_IsExist(100) && PE_IsExist(101) && PE_IsExist(102));
	assert(!PE_IsExist(103));
	puts("PASS ordinary Save still includes back-scene-excluded parts");
	delete_page_vars(back);
	free_page(back);
	delete_page_vars(normal);
	free_page(normal);
	parts_release_all();
	ain_free(ain);
	ain = NULL;
	return 0;
}
