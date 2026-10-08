/* Copyright (C) 2026 Rance03 compatibility contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

/*
 * Sys43VM
 * -------
 * Introspection/profiling library for the System 4.3 VM.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "system4/ain.h"
#include "system4/string.h"

#include "vm/heap.h"
#include "vm/page.h"
#include "xsystem4.h"
#include "hll.h"

static bool Sys43VM_GetFunctionNameList(struct page **FunctionNameList)
{
	union vm_value dim = { .i = ain->nr_functions };
	struct page *page = alloc_array(1, &dim, AIN_ARRAY_STRING, 0, false);
	for (int i = 0; i < ain->nr_functions; i++) {
		page->values[i].i = heap_alloc_string(cstr_to_string(ain->functions[i].name));
	}
	if (*FunctionNameList) {
		delete_page_vars(*FunctionNameList);
		free_page(*FunctionNameList);
	}
	*FunctionNameList = page;
	return true;
}

static bool Sys43VM_BeginProfiler(void)
{
	// XXX: the engine has no instruction-level profiler; this is a no-op.
	return true;
}

static bool Sys43VM_EndProfiler(void)
{
	// XXX: the engine has no instruction-level profiler; this is a no-op.
	return true;
}

static int Sys43VM_GetActualNumofPage(void)
{
	int n = 0;
	for (size_t i = 0; i < heap_size; i++) {
		if (heap[i].ref > 0 && heap[i].type == VM_PAGE)
			n++;
	}
	return n;
}

static int Sys43VM_GetActualMemorySize(void)
{
	// Approximate the total size of the VM heap in bytes.
	size_t size = 0;
	for (size_t i = 0; i < heap_size; i++) {
		if (heap[i].ref <= 0)
			continue;
		if (heap[i].type == VM_PAGE) {
			struct page *page = heap[i].page;
			if (page)
				size += sizeof(struct page) + page->nr_vars * sizeof(union vm_value);
		} else if (heap[i].type == VM_STRING) {
			struct string *s = heap[i].s;
			if (s)
				size += sizeof(struct string) + s->size + 1;
		}
	}
	return (int)size;
}

HLL_LIBRARY(Sys43VM,
	    HLL_EXPORT(GetFunctionNameList, Sys43VM_GetFunctionNameList),
	    HLL_EXPORT(BeginProfiler, Sys43VM_BeginProfiler),
	    HLL_EXPORT(EndProfiler, Sys43VM_EndProfiler),
	    HLL_EXPORT(GetActualNumofPage, Sys43VM_GetActualNumofPage),
	    HLL_EXPORT(GetActualMemorySize, Sys43VM_GetActualMemorySize));
