
#define main unused_system4_main
#include "../src/system4.c"
#undef main
#include <assert.h>

static void check_selection(char *name, char *ain_name, enum sjis_encoding expected, const char *save_name)
{
	config.game_name = name;
	config.ain_filename = ain_name;
	config.save_dir = xstrdup("SaveData");
	config_init();
	assert(sjis_get_encoding() == expected);
	if (save_name)
		assert(strstr(config.save_dir, save_name));
	printf("PASS charset=%d save=%s\n", expected, config.save_dir);
	free(config.save_dir);
	free(config.home_dir);
	config.save_dir = config.home_dir = NULL;
}

int main(void)
{
	check_selection("\xc0\xbc\xcb\xb9" "03", "Other.ain", SJIS_ENCODING_CP932, NULL);
	check_selection("\x93\xfa\x96\x7b\x8c\xea", "Rance03.ain", SJIS_ENCODING_CP932, "/日本語/SaveData");
	check_selection("\xc0\xbc\xcb\xb9" "03", "Rance03.ain", SJIS_ENCODING_CP936, "/兰斯03/SaveData");
	check_selection(NULL, "Rance03.ain", SJIS_ENCODING_CP932, "/Rance03.ain/SaveData");
	return 0;
}

