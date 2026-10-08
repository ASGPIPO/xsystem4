/* Focused startup HLL contract regression tests.
 * Build/run instructions: test/hll_startup_runtime.sh
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "system4/string.h"
#include "../src/hll/HTTPDownloader.c"

static void *exported(const char *name)
{
	for (int i = 0; lib_HTTPDownloader.functions[i].name; i++) {
		if (!strcmp(name, lib_HTTPDownloader.functions[i].name)) {
			assert(lib_HTTPDownloader.functions[i].fun);
			return lib_HTTPDownloader.functions[i].fun;
		}
	}
	fprintf(stderr, "Missing HTTPDownloader export: %s\n", name);
	assert(0);
}

static void transform(const char *name, const char *input, const char *output, bool success)
{
	struct string *s = cstr_to_string(input);
	struct string *shared = string_ref(s);
	bool (*fn)(struct string **) = exported(name);
	assert(fn(&s) == success);
	assert(!strcmp(s->text, output));
	assert(!strcmp(shared->text, input));
	free_string(shared);
	free_string(s);
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	if (!strcmp(argv[1], "offline")) {
		bool (*get)(struct string *) = exported("Get");
		bool (*post)(struct string *, struct string *) = exported("Post");
		bool (*is_run)(void) = exported("IsRun");
		void (*stop)(void) = exported("Stop");
		int (*size)(void) = exported("GetReadSize");
		bool (*read)(struct string **) = exported("ReadString");
		bool (*read_sjis)(struct string **) = exported("ReadStringUTF8ToSJIS");
		struct string *url = cstr_to_string("https://invalid.example/");
		struct string *param = cstr_to_string("test=value");
		assert(!get(url));
		assert(!post(url, param));
		assert(!is_run());
		assert(size() == 0);
		stop();
		stop();
		assert(!is_run());
		assert(!read(&param));
		assert(param->size == 0);
		free_string(param);
		param = cstr_to_string("stale");
		assert(!read_sjis(&param));
		assert(param->size == 0);
		free_string(param);
		free_string(url);
	} else if (!strcmp(argv[1], "encode")) {
		transform("EncodeString", "a b+c&d/=?%~_-.", "a%20b%2Bc%26d%2F%3D%3F%25~_-.", true);
		transform("EncodeString", "\xe3\x81\x82", "%E3%81%82", true);
		transform("EncodeString", "", "", true);
	} else if (!strcmp(argv[1], "decode")) {
		transform("DecodeString", "a%20b%2bc%26d+e%7e", "a b+c&d e~", true);
		transform("DecodeString", "%E3%81%82", "\xe3\x81\x82", true);
		transform("DecodeString", "", "", true);
	} else if (!strcmp(argv[1], "invalid_url")) {
		transform("DecodeString", "%", "%", false);
		transform("DecodeString", "a%2", "a%2", false);
		transform("DecodeString", "%GG", "%GG", false);
		transform("DecodeString", "a%00b", "a%00b", false);
	} else if (!strcmp(argv[1], "charset")) {
		transform("SJISToUTF8", "\x82\xa0\xb1", "\xe3\x81\x82\xef\xbd\xb1", true);
		transform("UTF8ToSJIS", "\xe3\x81\x82\xef\xbd\xb1", "\x82\xa0\xb1", true);
		transform("SJISToUTF8", "", "", true);
		transform("UTF8ToSJIS", "", "", true);
	} else if (!strcmp(argv[1], "invalid_charset")) {
		transform("SJISToUTF8", "\x82", "\x82", false);
		transform("SJISToUTF8", "\x82\x20", "\x82\x20", false);
		transform("SJISToUTF8", "\xff", "\xff", false);
		transform("UTF8ToSJIS", "\xe3\x81", "\xe3\x81", false);
		transform("UTF8ToSJIS", "\xc0\xaf", "\xc0\xaf", false);
		transform("UTF8ToSJIS", "\xed\xa0\x80", "\xed\xa0\x80", false);
		transform("UTF8ToSJIS", "\xf4\x90\x80\x80", "\xf4\x90\x80\x80", false);
	} else {
		assert(0);
	}
	printf("PASS %s\n", argv[1]);
	return 0;
}
