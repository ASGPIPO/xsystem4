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
 * TextFile
 * --------
 * Simple sequential text file writer/reader used by the game logging
 * helpers (AFL_TextFileWriter_*).
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system4/file.h"
#include "system4/string.h"

#include "xsystem4.h"
#include "hll.h"

static FILE *text_file = NULL;

static bool TextFile_Close(void);

static bool TextFile_Create(struct string *FileName)
{
	TextFile_Close();
	char *path = unix_path(FileName->text);
	text_file = file_open_utf8(path, "w");
	if (!text_file)
		WARNING("TextFile.Create: failed to open \"%s\": %s", display_utf0(path), strerror(errno));
	free(path);
	return text_file != NULL;
}

static bool TextFile_Close(void)
{
	if (!text_file)
		return true;
	bool ok = fclose(text_file) == 0;
	if (!ok)
		WARNING("TextFile.Close: fclose failed");
	text_file = NULL;
	return ok;
}

static bool TextFile_Write(struct string *Text)
{
	if (!text_file) {
		WARNING("TextFile.Write: no file open");
		return false;
	}
	if (Text && Text->size > 0)
		fwrite(Text->text, 1, Text->size, text_file);
	return true;
}

static bool TextFile_ReadAll(struct string *FileName, struct string **Text)
{
	char *path = unix_path(FileName->text);
	FILE *f = file_open_utf8(path, "rb");
	free(path);
	if (!f) {
		WARNING("TextFile.ReadAll: failed to open \"%s\": %s", display_utf0(FileName->text), strerror(errno));
		return false;
	}
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0) {
		fclose(f);
		return false;
	}
	char *buf = xmalloc(len + 1);
	size_t rd = fread(buf, 1, len, f);
	fclose(f);

	if (*Text)
		free_string(*Text);
	*Text = make_string(buf, rd);
	free(buf);
	return true;
}

HLL_LIBRARY(TextFile,
	    HLL_EXPORT(Create, TextFile_Create),
	    HLL_EXPORT(Close, TextFile_Close),
	    HLL_EXPORT(Write, TextFile_Write),
	    HLL_EXPORT(ReadAll, TextFile_ReadAll));
