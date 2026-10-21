/* ADL load/save order, voice and ring-capacity regression with synthetic text. */
#define VM_PRIVATE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/string.h"
#include "vm.h"
#include "vm/heap.h"
#include "vm/page.h"
#include "../src/hll/iarray.h"
#include "../src/hll/AnteaterADVEngine.h"

static void drop(struct page *p)
{
	delete_page_vars(p);
	free_page(p);
}
static void label(char *s, int index)
{
	snprintf(s, 40, "page-%d", index);
}
static struct page *fixture(int count, bool leading_empty)
{
	struct iarray_writer w;
	iarray_init_writer(&w, "ADL");
	iarray_write(&w, 0); iarray_write(&w, count);
	for (int i = 0; i < count; i++) {
		char text[40]; label(text, i);
		bool empty = leading_empty && !i;
		struct string *s = cstr_to_string(empty ? "" : text);
		iarray_write(&w, empty ? 1 : 2);
		iarray_write_string(&w, s);
		free_string(s);
		if (!empty) {
			s = cstr_to_string("body");
			iarray_write_string(&w, s);
			free_string(s);
		}
		iarray_write(&w, empty ? 0 : 1);
		if (!empty) iarray_write(&w, 10000+i);
	}
	iarray_write(&w, 1);
	struct page *p = iarray_to_page(&w);
	iarray_free_writer(&w);
	return p;
}
static void line_is(int page, int line, const char *expected)
{
	struct string *s = NULL;
	ADVLogList_GetADVLogText(page, line, &s);
	assert(s && !strcmp(s->text, expected));
	free_string(s);
}
static void check_pages(int input_count, bool leading_empty)
{
	int count = input_count > 1000 ? 1000 : input_count ? input_count : 1;
	assert(ADVLogList_GetNumofADVLog() == count);
	int first = input_count > 1000 ? input_count-1000 : 0;
	for (int i = 0; i < count; i++) {
		int original = first+i;
		bool empty = !input_count || (leading_empty && !original);
		char text[40]; label(text, original);
		assert(ADVLogList_GetNumofADVLogText(i) == (empty ? 1 : 2));
		line_is(i, 0, empty ? "" : text);
		assert(ADVLogList_GetNumofADVLogVoice(i) == (empty ? 0 : 1));
		if (!empty) {
			line_is(i, 1, "body");
			assert(ADVLogList_GetADVLogVoice(i, 0) == 10000+original);
		}
	}
}
static void run_case(int count, bool leading_empty)
{
	struct page *input = fixture(count, leading_empty);
	assert(ADVLogList_Load(&input));
	drop(input);
	printf("input=%d leading_empty=%d restored=%d\n",
			count,leading_empty,ADVLogList_GetNumofADVLog());
	check_pages(count, leading_empty);
	for (int round = 0; round < 3; round++) {
		struct page *saved = NULL;
		assert(ADVLogList_Save(&saved));
		assert(ADVLogList_Load(&saved));
		drop(saved);
		check_pages(count, leading_empty);
	}
	if (count >= 3) {
		int n = ADVLogList_GetNumofADVLog();
		struct string *s = cstr_to_string("+tail");
		ADVLogList_AddText(&s); free_string(s);
		line_is(n-1, 1, "body+tail");
		ADVLogList_AddVoice(76543);
		assert(ADVLogList_GetADVLogVoiceLast(n-1) == 76543);
		ADVLogList_AddNewPage();
		s = cstr_to_string("new page");
		ADVLogList_AddText(&s); free_string(s);
		int expected = n == 1000 ? 1000 : n+1;
		assert(ADVLogList_GetNumofADVLog() == expected);
		line_is(expected-1, 0, "new page");
		line_is(expected-2, 1, "body+tail");
		if (n == 1000) {
			char text[40];
			label(text, count-999);
			line_is(0, 0, text);
		}
		struct page *saved = NULL;
		assert(ADVLogList_Save(&saved) && ADVLogList_Load(&saved));
		drop(saved);
		assert(ADVLogList_GetNumofADVLog() == expected);
		line_is(expected-1, 0, "new page");
		line_is(expected-2, 1, "body+tail");
		assert(ADVLogList_GetADVLogVoiceLast(expected-2) == 76543);
	}
	ADVLogList_Clear();
	puts("PASS exact pages/order/voices, three round trips and append");
}
int main(void)
{
	setbuf(stdout, NULL);
	heap_init();
	ADVLogList_SetEnable(true);
	run_case(3, false);
	run_case(0, false);
	run_case(1, true);
	run_case(4, true);
	run_case(1000, false);
	run_case(1003, false);
	return 0;
}
