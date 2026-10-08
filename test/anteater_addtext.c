/* Exercise AddText binding through libffi using the real Rance03 signature. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ffi.h>
#include "../src/hll/AnteaterADVEngine.c"

static struct static_hll_function *add_text_export(void)
{
	for (int i = 0; lib_AnteaterADVEngine.functions[i].name; i++)
		if (!strcmp(lib_AnteaterADVEngine.functions[i].name, "ADVLogList_AddText"))
			return &lib_AnteaterADVEngine.functions[i];
	assert(0);
	return NULL;
}

static void check_add_text(struct ain_hll_function *f)
{
	ADVLogList_Clear();
	ADVLogList_SetEnable(true);
	add_text_export()->fun = ADVLogList_AddText;
	AnteaterADVEngine_PreLink();
	struct string *text = cstr_to_string("real dialogue");
	struct string **ref = &text;
	int window = 1;
	void *args[2] = {
		f->arguments[0].type.data == AIN_STRING ? (void *)&text : (void *)&ref,
		&window
	};
	ffi_type *types[2] = { &ffi_type_pointer, &ffi_type_sint32 };
	ffi_cif cif;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, f->nr_arguments,
				&ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(add_text_export()->fun), NULL, args);
	assert(text->ref == 1);
	struct string *line = NULL;
	ADVLogList_GetADVLogText(0, 0, &line);
	assert(!strcmp(line->text, text->text));
	window = 2;
	ffi_call(&cif, FFI_FN(add_text_export()->fun), NULL, args);
	assert(!strcmp(line->text, "real dialogue"));
	free_string(line);
	line = NULL;
	ADVLogList_GetADVLogText(0, 0, &line);
	assert(!strcmp(line->text, "real dialoguereal dialogue"));
	free_string(line);
	ADVLogList_SetEnable(false);
	ffi_call(&cif, FFI_FN(add_text_export()->fun), NULL, args);
	line = NULL;
	ADVLogList_GetADVLogText(0, 0, &line);
	assert(!strcmp(line->text, "real dialoguereal dialogue"));
	free_string(line);
	free_string(text);
	ADVLogList_Clear();
}

int main(void)
{
	setbuf(stdout, NULL);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	int libno = ain_get_library(ain, "AnteaterADVEngine");
	int fno = ain_get_library_function(ain, libno, "ADVLogList_AddText");
	assert(libno >= 0 && fno >= 0);
	struct ain_hll_function *f = &ain->libraries[libno].functions[fno];
	assert(f->nr_arguments == 2 && f->arguments[0].type.data == AIN_STRING
			&& f->arguments[1].type.data == AIN_INT);
	puts("REAL Rance03 AddText(string value, int window)");
	check_add_text(f);
	puts("PASS real value-string signature, copy-on-write and disabled logging");
	f->arguments[0].type.data = AIN_REF_STRING;
	check_add_text(f);
	puts("PASS legacy ref-string + window signature");
	f->nr_arguments = 1;
	check_add_text(f);
	puts("PASS legacy ref-string signature");
	f->nr_arguments = 2;
	f->arguments[0].type.data = AIN_STRING;
	ain_free(ain);
	ain = NULL;
	return 0;
}
