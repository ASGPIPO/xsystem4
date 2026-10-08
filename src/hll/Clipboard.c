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
 * Clipboard
 * ---------
 * System clipboard access, implemented with SDL2. VM strings are Shift-JIS;
 * the system clipboard expects UTF-8.
 */

#include <SDL.h>
#include <stdbool.h>
#include <string.h>

#include "system4/string.h"
#include "system4/utfsjis.h"

#include "hll.h"

static int Clipboard_SetText(struct string *Text)
{
	if (!Text)
		return 0;
	char *utf8 = sjis2utf(Text->text, Text->size);
	int ok = SDL_SetClipboardText(utf8) == 0;
	free(utf8);
	return ok ? 1 : 0;
}

static int Clipboard_GetText(struct string **Text)
{
	char *utf8 = SDL_GetClipboardText();
	if (!utf8 || !utf8[0]) {
		if (utf8)
			SDL_free(utf8);
		return 0;
	}
	char *sjis = utf2sjis(utf8, strlen(utf8));
	if (*Text)
		free_string(*Text);
	*Text = cstr_to_string(sjis);
	free(sjis);
	SDL_free(utf8);
	return 1;
}

HLL_LIBRARY(Clipboard,
	    HLL_EXPORT(SetText, Clipboard_SetText),
	    HLL_EXPORT(GetText, Clipboard_GetText));
