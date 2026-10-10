/* Real AIN SystemService PC-information contract regression.
 * Link against engine objects; no game VM or network request executes.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ffi.h>
#include <SDL.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

#include "system4/ain.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "../src/hll/hll.h"

extern struct static_library lib_SystemService;

static void *binding(struct ain *a, const char *name, int index,
                     unsigned nr_args, const enum ain_data_type *types)
{
	int lib = ain_get_library(a, "SystemService");
	assert(lib == 17);
	int fun = ain_get_library_function(a, lib, name);
	assert(fun == index);
	struct ain_hll_function *f = &a->libraries[lib].functions[fun];
	assert(f->return_type.data == AIN_VOID && f->nr_arguments == nr_args);
	for (unsigned i = 0; i < nr_args; i++)
		assert(f->arguments[i].type.data == types[i]);
	void *fn = NULL;
	for (int i = 0; lib_SystemService.functions[i].name; i++)
		if (!strcmp(lib_SystemService.functions[i].name, name))
			fn = lib_SystemService.functions[i].fun;
	printf("AIN lib%d fn%d void %s refs=%u binding=%s\n",
	       lib, fun, name, nr_args, fn ? "present" : "MISSING");
	fflush(stdout);
	return fn;
}

static void invoke(void *fn, unsigned nr, void **args)
{
	ffi_cif cif;
	ffi_type *types[4] = {&ffi_type_pointer, &ffi_type_pointer,
	                     &ffi_type_pointer, &ffi_type_pointer};
	assert(fn);
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, nr, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(fn), NULL, args);
}

static int memory_mb(unsigned long bytes, unsigned int unit)
{
	unsigned long long mb = (unsigned long long)bytes * unit / (1024 * 1024);
	return mb > INT_MAX ? INT_MAX : (int)mb;
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int err;
	struct ain *a = ain_open("/root/rance03/Rance03.ain", &err);
	assert(a && err == AIN_SUCCESS);
	enum ain_data_type memory_types[] = {AIN_REF_INT, AIN_REF_INT};
	enum ain_data_type cpu_types[] = {AIN_REF_STRING, AIN_REF_INT,
	                                 AIN_REF_INT, AIN_REF_STRING};
	enum ain_data_type string_types[] = {AIN_REF_STRING};
	void *memory = binding(a, "GetMemoryInfo", 43, 2, memory_types);
	void *cpu = binding(a, "GetCPUInfo", 42, 4, cpu_types);
	void *os = binding(a, "GetOSInfo", 44, 1, string_types);
	void *screen = binding(a, "GetScreenInfo", 45, 1, string_types);
	assert(memory && cpu && os && screen);

	assert(SDL_Init(SDL_INIT_VIDEO) == 0);
	int max = -1, use = -1;
	int *max_ptr = &max, *use_ptr = &use;
	void *memory_args[] = {&max_ptr, &use_ptr};
	struct sysinfo before, after;
	assert(sysinfo(&before) == 0);
	invoke(memory, 2, memory_args);
	assert(sysinfo(&after) == 0);
	int expected_max = memory_mb(before.totalram, before.mem_unit);
	int before_use = memory_mb(before.totalram - before.freeram, before.mem_unit);
	int after_use = memory_mb(after.totalram - after.freeram, after.mem_unit);
	assert(max == expected_max && max > 0 && use >= 0 && use <= max);
	/* Other VM processes can change free RAM during the query. */
	assert(use >= (before_use < after_use ? before_use : after_use) - 64);
	assert(use <= (before_use > after_use ? before_use : after_use) + 64);
	printf("PASS actual sysinfo total=%d MiB used=%d MiB\n", max, use);

	struct string *old = cstr_to_string("old output");
	struct string *vendor = string_ref(old), *brand = string_ref(old);
	struct string **vendor_ptr = &vendor, **brand_ptr = &brand;
	int signature = -1, flags = -1;
	int *signature_ptr = &signature, *flags_ptr = &flags;
	void *cpu_args[] = {&vendor_ptr, &signature_ptr, &flags_ptr, &brand_ptr};
	invoke(cpu, 4, cpu_args);
	assert(old->ref == 1 && !strcmp(old->text, "old output"));
	assert(brand->size > 0);
#if defined(__i386__) || defined(__x86_64__)
	unsigned eax, ebx, ecx, edx;
	assert(__get_cpuid(0, &eax, &ebx, &ecx, &edx));
	char expected_vendor[13] = {0};
	memcpy(expected_vendor, &ebx, 4);
	memcpy(expected_vendor + 4, &edx, 4);
	memcpy(expected_vendor + 8, &ecx, 4);
	assert(!strcmp(vendor->text, expected_vendor));
	assert(__get_cpuid(1, &eax, &ebx, &ecx, &edx));
	assert((unsigned)signature == eax && (unsigned)flags == edx);
#endif
	printf("PASS CPU vendor=%s brand=%s signature=%08x flags=%08x\n",
	       vendor->text, brand->text, (unsigned)signature, (unsigned)flags);
	invoke(cpu, 4, cpu_args);
	assert(vendor->ref == 1 && brand->ref == 1);
	free_string(vendor);
	free_string(brand);

	struct string *os_text = string_ref(old);
	struct string **os_ptr = &os_text;
	void *os_args[] = {&os_ptr};
	invoke(os, 1, os_args);
	struct utsname u;
	assert(uname(&u) == 0);
	assert(strstr(os_text->text, u.sysname));
	assert(strstr(os_text->text, u.release));
	assert(strstr(os_text->text, u.machine));
	assert(old->ref == 1);
	printf("PASS OS %s\n", os_text->text);
	invoke(os, 1, os_args);
	assert(os_text->ref == 1);
	free_string(os_text);

	struct string *screen_text = string_ref(old);
	struct string **screen_ptr = &screen_text;
	void *screen_args[] = {&screen_ptr};
	SDL_DisplayMode mode;
	assert(SDL_GetCurrentDisplayMode(0, &mode) == 0);
	char dimensions[64];
	snprintf(dimensions, sizeof(dimensions), "%dx%d", mode.w, mode.h);
	invoke(screen, 1, screen_args);
	assert(strstr(screen_text->text, dimensions));
	assert(old->ref == 1);
	printf("PASS SCREEN %s\n", screen_text->text);
	invoke(screen, 1, screen_args);
	assert(screen_text->ref == 1);
	free_string(screen_text);
	free_string(old);
	SDL_Quit();
	ain_free(a);
	puts("PASS all four actual AIN HLLs through libffi, real OS values, repeated calls, borrowed refs");
	return 0;
}
