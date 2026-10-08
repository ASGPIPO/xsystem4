
#include <assert.h>
#include <stdio.h>
#include "../src/hll/iarray.c"

static void check(const char *text, size_t size, const int *expected, unsigned count)
{
	struct string *s = make_string(text, size);
	struct iarray_writer writer = {0};
	iarray_write_string(&writer, s);
	assert(writer.size == count);
	for (unsigned i = 0; i < count; i++)
		assert(writer.data[i] == expected[i]);
	free_string(s);
	iarray_free_writer(&writer);
}

int main(void)
{
	sjis_set_encoding(SJIS_ENCODING_CP936);
	const int truncated[] = { (char)0xc0, 0 };
	check("\xc0", 1, truncated, 2);
	const int bad_tail[] = { (char)0xc0, ' ', 0 };
	check("\xc0 ", 2, bad_tail, 3);
	const int chinese[] = { 0xbcc0, 0xb9cb, 'X', 0 };
	check("\xc0\xbc\xcb\xb9X", 5, chinese, 4);
	sjis_set_encoding(SJIS_ENCODING_CP932);
	const int japanese[] = { 0xa082, (char)0xb1, 0 };
	check("\x82\xa0\xb1", 3, japanese, 3);
	puts("PASS actual iarray writer preserves packed character protocol and truncated/illegal-tail boundaries");
	return 0;
}

