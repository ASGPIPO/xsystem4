
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(TEST_PARTS)
#include "../src/parts/text.c"
#elif defined(TEST_CHARSPRITE)
#include "../src/hll/CharSpriteManager.c"
#elif defined(TEST_STOAT)
#include "../src/hll/hll.h"
// The helper probe does not register unrelated HLL functions.
#undef HLL_LIBRARY
#define HLL_LIBRARY(lname, ...) extern struct static_library lib_ ## lname
#include "../src/hll/StoatSpriteEngine.c"
#else
#error Choose an actual text consumer
#endif

static void consume(char *text, int expected)
{
	char *p = text;
	int count = 0;
	while (*p) {
		char character[3];
		int width = extract_sjis_char(p, character);
		assert(character[width] == 0);
		p += width;
		count++;
	}
	assert(count == expected);
}

int main(void)
{
	sjis_set_encoding(SJIS_ENCODING_CP936);
	struct string *s = make_string("", 0);
	string_push_back(&s, 0xc0);
	consume(s->text, 1);
	free_string(s);
	char malformed[] = { '\xc0', ' ', 0 };
	consume(malformed, 2);
	char valid[] = { '\xc0', '\xbc', '\xcb', '\xb9', 'X', 0 };
	consume(valid, 3);
	sjis_set_encoding(SJIS_ENCODING_CP932);
	char cp932[] = { '\x82', '\xa0', '\xb1', 0 };
	consume(cp932, 2);
	char truncated[] = { '\x82', 0 };
	consume(truncated, 1);
	puts("PASS actual extract helper consumes truncated/illegal tails and valid CP936/CP932 boundaries");
	return 0;
}

