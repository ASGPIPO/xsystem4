
#define ft_font_load test_ft_font_load
#include "../src/text.c"
#undef ft_font_load
#include <assert.h>
#include <string.h>
#include <stdio.h>

struct config config;
static struct font test_font;
static const char *loaded_path;

struct font *test_ft_font_load(const char *path)
{
	loaded_path = path;
	return &test_font;
}

int main(void)
{
	sjis_set_encoding(SJIS_ENCODING_CP932);
	assert(load_font(FONT_GOTHIC) == &test_font);
	assert(!strcmp(loaded_path, XSYS4_DATA_DIR "/" DEFAULT_FONT_GOTHIC));
	printf("PASS CP932 Gothic default=%s\n", loaded_path);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	assert(load_font(FONT_GOTHIC) == &test_font);
	assert(!strcmp(loaded_path, XSYS4_DATA_DIR "/" DEFAULT_FONT_MINCHO));
	printf("PASS CP936 Gothic default=%s\n", loaded_path);
	config.font_paths[FONT_GOTHIC] = "/explicit/user-font.ttf";
	assert(load_font(FONT_GOTHIC) == &test_font);
	assert(!strcmp(loaded_path, config.font_paths[FONT_GOTHIC]));
	printf("PASS explicit font-gothic takes precedence=%s\n", loaded_path);
	config.font_paths[FONT_GOTHIC] = NULL;
	assert(load_font(FONT_MINCHO) == &test_font);
	assert(!strcmp(loaded_path, XSYS4_DATA_DIR "/" DEFAULT_FONT_MINCHO));
	puts("PASS Mincho default preserved");
	return 0;
}

