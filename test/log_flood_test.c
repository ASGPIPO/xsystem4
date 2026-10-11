/* Regression for BUG-007. Include the real sources; no game VM or assets run.
 * Build with -ffunction-sections -fdata-sections and link with --gc-sections.
 */
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/text.c"
#include "../src/hll/Sound2ex.c"
#include "../src/hll/PixelRestore.c"
#include "../src/hll/AliceLogo.c"

#define REPEATS 30000
static unsigned stub_warnings, hll_warnings, ordinary_warnings, error_calls;
static unsigned font_warnings[MAX_FNL_FONTS + 2], huge_font_warnings;
static unsigned failures, format_evaluations, return_evaluations;
static char first_stub[256];
static jmp_buf error_env;
static int pointer_value;

void sys_warning(const char *fmt, ...)
{
	char message[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	if (strstr(message, "unimplemented:")) {
		if (!stub_warnings)
			snprintf(first_stub, sizeof(first_stub), "%.255s", message);
		stub_warnings++;
	} else if (strstr(message, "Unimplemented HLL function:")) {
		hll_warnings++;
	} else if (strstr(message, "Invalid fnl face:")) {
		unsigned face;
		assert(sscanf(strstr(message, "Invalid fnl face:"), "Invalid fnl face: %u", &face) == 1);
		if (face < MAX_FNL_FONTS + 2)
			font_warnings[face]++;
		else
			huge_font_warnings++;
	} else {
		assert(strstr(message, "ordinary diagnostic"));
		ordinary_warnings++;
	}
}

_Noreturn void sys_error(const char *fmt, ...)
{
	assert(strstr(fmt, "save failure probe"));
	error_calls++;
	longjmp(error_env, 1);
}

static void expect_count(const char *name, unsigned actual, unsigned expected)
{
	printf("%s: actual=%u expected=%u\n", name, actual, expected);
	if (actual != expected)
		failures++;
}

static void macro_control_flow(bool enabled)
{
	if (enabled)
		UNIMPLEMENTED("(%u)", ++format_evaluations);
	else
		assert(!enabled);
}

static void macro_independent_sites(void)
{
	UNIMPLEMENTED("(site 1)");
	UNIMPLEMENTED("(site 2)");
}

static int *return_pointer(void)
{
	return_evaluations++;
	return &pointer_value;
}

HLL_WARN_UNIMPLEMENTED(return_pointer(), int *, Probe, Pointer, void);

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	struct font gothic = {0}, mincho = {0}, fnl = {0};
	font_ttf[0] = &gothic;
	font_ttf[1] = &mincho;

	for (unsigned i = 0; i < REPEATS; i++)
		Sound2ex_PreloadSound(i ? 9 : 7);
	expect_count("real Sound2ex UNIMPLEMENTED", stub_warnings, 1);
	assert(strstr(first_stub, "Sound2ex_PreloadSound(7)"));
	unsigned before = stub_warnings;
	for (unsigned i = 0; i < REPEATS; i++) {
		macro_control_flow(true);
		macro_control_flow(false);
		macro_independent_sites();
	}
	expect_count("macro independent call sites", stub_warnings - before, 3);
	expect_count("format argument evaluation", format_evaluations, 1);

	for (unsigned i = 0; i < REPEATS; i++) {
		assert(PixelRestore_Draw(17) == false);
		PixelRestore_Set(17);
		assert(AliceLogo_Init(NULL) == 1);
		assert(Probe_Pointer() == &pointer_value);
	}
	expect_count("real HLL bool/void/int and pointer warnings", hll_warnings, 4);
	expect_count("HLL return expression still runs every call", return_evaluations, REPEATS);

	for (unsigned i = 0; i < REPEATS; i++) {
		assert(get_font(256) == &gothic);
		assert(get_font(256 + MAX_FNL_FONTS - 1) == &gothic);
	}
	for (unsigned face = 1; face < MAX_FNL_FONTS - 1; face++) {
		assert(get_font(256 + face) == &gothic);
		assert(get_font(256 + face) == &gothic);
	}
	unsigned bounded_font_warnings = 0;
	for (unsigned face = 0; face < MAX_FNL_FONTS; face++) {
		bounded_font_warnings += font_warnings[face];
		expect_count("each legal missing FNL face", font_warnings[face], 1);
	}
	expect_count("all legal missing FNL faces", bounded_font_warnings, MAX_FNL_FONTS);

	/* A later available font must supersede the fallback immediately. */
	font_fnl[2] = &fnl;
	assert(get_font(258) == &fnl);
	assert(get_font(0) == &gothic);
	assert(get_font(1) == &mincho);
	assert(get_font(2) == &gothic);
	assert(get_font(255) == &gothic);
	for (unsigned i = 0; i < 3; i++) {
		assert(get_font(256 + MAX_FNL_FONTS) == &gothic);
		assert(get_font(257 + MAX_FNL_FONTS) == &gothic);
		assert(get_font(UINT_MAX) == &gothic);
	}
	expect_count("out-of-range FNL face 32 remains visible", font_warnings[MAX_FNL_FONTS], 3);
	expect_count("out-of-range FNL face 33 remains visible", font_warnings[MAX_FNL_FONTS + 1], 3);
	expect_count("extreme FNL face remains visible", huge_font_warnings, 3);

	for (unsigned i = 0; i < REPEATS; i++)
		WARNING("ordinary diagnostic");
	expect_count("ordinary WARNING remains repeated", ordinary_warnings, REPEATS);
	if (!setjmp(error_env))
		ERROR("save failure probe");
	expect_count("ERROR path remains visible", error_calls, 1);
	printf("failures=%u\n", failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
