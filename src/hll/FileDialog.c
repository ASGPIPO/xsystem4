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
 * FileDialog
 * ----------
 * Native (Win32) open/save file dialogs. There is no portable equivalent,
 * so the functions report "canceled" (return false). The game treats a
 * canceled dialog as a no-op.
 */

#include <stdbool.h>

#include "system4/string.h"

#include "vm/page.h"
#include "hll.h"

static bool FileDialog_OpenLoadDialog(struct string **LoadFileName, struct string *TitleName,
		struct string *FolderName, struct page *FilterTitleList, struct page *FilterExtendList)
{
	WARNING("FileDialog.OpenLoadDialog: native dialogs unsupported; reporting cancel");
	if (*LoadFileName)
		free_string(*LoadFileName);
	*LoadFileName = make_string("", 0);
	return false;
}

static bool FileDialog_OpenSaveDialog(struct string **SaveFileName, struct string *TitleName,
		struct string *FolderName, struct page *FilterTitleList, struct page *FilterExtendList)
{
	WARNING("FileDialog.OpenSaveDialog: native dialogs unsupported; reporting cancel");
	if (*SaveFileName)
		free_string(*SaveFileName);
	*SaveFileName = make_string("", 0);
	return false;
}

HLL_LIBRARY(FileDialog,
	    HLL_EXPORT(OpenLoadDialog, FileDialog_OpenLoadDialog),
	    HLL_EXPORT(OpenSaveDialog, FileDialog_OpenSaveDialog));
