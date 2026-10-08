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
 * InstallInfo
 * -----------
 * Queries the Windows registry for other installed AliceSoft games (used
 * by the "product information" screens). On Linux the answer is always
 * "not installed", which just hides the corresponding menu entries.
 */

#include <stdbool.h>

#include "system4/string.h"

#include "hll.h"

static bool InstallInfo_IsInstalledGame(struct string *pIGameName)
{
	return false;
}

static int InstallInfo_GetInstallMediaType(void)
{
	// 0 = unknown/not installed
	return 0;
}

HLL_LIBRARY(InstallInfo,
	    HLL_EXPORT(IsInstalledGame, InstallInfo_IsInstalledGame),
	    HLL_EXPORT(GetInstallMediaType, InstallInfo_GetInstallMediaType));
