/* Copyright (C) 2025 kichikuou <KichikuouChrome@gmail.com>
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

#include <limits.h>
#include <string.h>

#include "system4/string.h"
#include "system4/utfsjis.h"

#include "hll.h"

/* Network access is unavailable. A failed request never has an active job or
 * response body; callers can still finish their offline/error path safely. */
static bool HTTPDownloader_Get(possibly_unused struct string *pIURL)
{
	return false;
}

static bool HTTPDownloader_Post(possibly_unused struct string *pIURL,
				possibly_unused struct string *pIParam)
{
	return false;
}

static bool HTTPDownloader_IsRun(void)
{
	return false;
}

static void HTTPDownloader_Stop(void)
{
}

static int HTTPDownloader_GetReadSize(void)
{
	return 0;
}

static void replace_string(struct string **s, struct string *out)
{
	free_string(*s);
	*s = out;
}

static bool HTTPDownloader_ReadString(struct string **pIString)
{
	replace_string(pIString, cstr_to_string(""));
	return false;
}

static bool valid_sjis(const struct string *s)
{
	const unsigned char *p = (const unsigned char *)s->text;
	for (int i = 0; i < s->size; i++) {
		unsigned char c = p[i];
		if (!c)
			return false;
		if (c <= 0x7f || (c >= 0xa1 && c <= 0xdf))
			continue;
		if (!((c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc))
				|| i + 1 >= s->size)
			return false;
		unsigned char trail = p[i + 1];
		if (trail < 0x40 || trail == 0x7f || trail > 0xfc)
			return false;
		int unicode;
		sjis_char2unicode((const char *)p + i, &unicode);
		if (!unicode)
			return false;
		i++;
	}
	return true;
}

static bool valid_utf8(const struct string *s)
{
	const unsigned char *p = (const unsigned char *)s->text;
	for (int i = 0; i < s->size; i++) {
		unsigned char c = p[i];
		if (!c)
			return false;
		if (c <= 0x7f)
			continue;
		int continuation;
		if (c >= 0xc2 && c <= 0xdf)
			continuation = 1;
		else if (c >= 0xe0 && c <= 0xef)
			continuation = 2;
		else if (c >= 0xf0 && c <= 0xf4)
			continuation = 3;
		else
			return false;
		if (i + continuation >= s->size)
			return false;
		for (int j = 1; j <= continuation; j++) {
			if ((p[i + j] & 0xc0) != 0x80)
				return false;
		}
		/* Reject overlong encodings, surrogates and codepoints > U+10FFFF. */
		if ((c == 0xe0 && p[i + 1] < 0xa0)
				|| (c == 0xed && p[i + 1] >= 0xa0)
				|| (c == 0xf0 && p[i + 1] < 0x90)
				|| (c == 0xf4 && p[i + 1] >= 0x90))
			return false;
		i += continuation;
	}
	return true;
}

static bool HTTPDownloader_UTF8ToSJIS(struct string **pIString)
{
	if (!valid_utf8(*pIString))
		return false;
	char *sjis = utf2sjis((*pIString)->text, (*pIString)->size);
	replace_string(pIString, cstr_to_string(sjis));
	free(sjis);
	return true;
}

static bool HTTPDownloader_SJISToUTF8(struct string **pIString)
{
	if (!valid_sjis(*pIString))
		return false;
	char *utf8 = sjis2utf((*pIString)->text, (*pIString)->size);
	replace_string(pIString, cstr_to_string(utf8));
	free(utf8);
	return true;
}

static bool HTTPDownloader_EncodeString(struct string **pIString)
{
	struct string *in = *pIString;
	if (in->size > INT_MAX / 3)
		return false;
	struct string *out = string_alloc(in->size * 3);
	char *p = out->text;
	const char hex[] = "0123456789ABCDEF";
	for (int i = 0; i < in->size; i++) {
		unsigned char c = in->text[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9') || c == '-' || c == '_'
				|| c == '.' || c == '~') {
			*p++ = c;
		} else {
			*p++ = '%';
			*p++ = hex[c >> 4];
			*p++ = hex[c & 0xf];
		}
	}
	*p = '\0';
	out->size = p - out->text;
	replace_string(pIString, out);
	return true;
}

static int hex_value(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static bool HTTPDownloader_DecodeString(struct string **pIString)
{
	struct string *in = *pIString;
	struct string *out = string_alloc(in->size);
	char *p = out->text;
	for (int i = 0; i < in->size; i++) {
		unsigned char c = in->text[i];
		if (c == '%') {
			if (i + 2 >= in->size)
				goto invalid;
			int hi = hex_value(in->text[i + 1]);
			int lo = hex_value(in->text[i + 2]);
			if (hi < 0 || lo < 0)
				goto invalid;
			c = (hi << 4) | lo;
			i += 2;
		} else if (c == '+') {
			c = ' ';
		}
		/* VM strings cannot expose an embedded NUL as a complete response. */
		if (!c)
			goto invalid;
		*p++ = c;
	}
	*p = '\0';
	out->size = p - out->text;
	replace_string(pIString, out);
	return true;
invalid:
	free_string(out);
	return false;
}

HLL_LIBRARY(HTTPDownloader,
	    HLL_EXPORT(Get, HTTPDownloader_Get),
	    HLL_EXPORT(Post, HTTPDownloader_Post),
	    HLL_EXPORT(IsRun, HTTPDownloader_IsRun),
	    HLL_EXPORT(Stop, HTTPDownloader_Stop),
	    HLL_EXPORT(GetReadSize, HTTPDownloader_GetReadSize),
	    HLL_EXPORT(ReadString, HTTPDownloader_ReadString),
	    HLL_EXPORT(ReadStringUTF8ToSJIS, HTTPDownloader_ReadString),
	    HLL_EXPORT(UTF8ToSJIS, HTTPDownloader_UTF8ToSJIS),
	    HLL_EXPORT(SJISToUTF8, HTTPDownloader_SJISToUTF8),
	    HLL_EXPORT(EncodeString, HTTPDownloader_EncodeString),
	    HLL_EXPORT(DecodeString, HTTPDownloader_DecodeString)
	    );
