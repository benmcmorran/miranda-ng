/*
Copyright (C) 2010 Mataes

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  
*/

#include "stdafx.h"

HINSTANCE hInst = NULL;
wchar_t g_tszRoot[MAX_PATH] = {0}, g_tszTempPath[MAX_PATH];
int hLangpack;
DWORD g_mirandaVersion;

PLUGININFOEX pluginInfoEx = {
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__AUTHOREMAIL,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
#if MIRANDA_VER < 0x0A00
	0,
#endif
	// {968DEF4A-BC60-4266-AC08-754CE721DB5F}
	{0x968def4a, 0xbc60, 0x4266, {0xac, 0x8, 0x75, 0x4c, 0xe7, 0x21, 0xdb, 0x5f}} 
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID)
{
	hInst = hinstDLL;
	return TRUE;
}

extern "C" __declspec(dllexport) PLUGININFOEX *MirandaPluginInfoEx(DWORD mirandaVersion)
{
	g_mirandaVersion = mirandaVersion;
	return &pluginInfoEx;
}


#if MIRANDA_VER < 0x0A00
extern "C" __declspec(dllexport) int Load(PLUGINLINK *link)
{
	pluginLink = link;
	mir_getMMI(&mmi);
	li.cbSize = LIST_INTERFACE_V2_SIZE;
	CallService(MS_SYSTEM_GET_LI, 0, (LPARAM)&li);
	mir_getMD5I(&md5i);
	mir_getUTFI(&utfi);
#else
extern "C" __declspec(dllexport) int Load(void)
{
	mir_getLP(&pluginInfoEx);

	InitServices();
#endif
	db_set_b(NULL, MODNAME, DB_SETTING_NEED_RESTART, 0);

	DWORD dwLen = GetTempPath(_countof(g_tszTempPath), g_tszTempPath);
	if (g_tszTempPath[dwLen-1] == '\\')
		g_tszTempPath[dwLen-1] = 0;

	LoadOptions();
	InitPopupList();
	InitNetlib();
	InitIcoLib();

	// Add cheking update menu item
	InitCheck();

#if MIRANDA_VER >= 0x0A00
	CMenuItem mi;

	SET_UID(mi, 0xfa2cbe01, 0x3b37, 0x4a4c, 0xa6, 0x97, 0xe4, 0x6f, 0x31, 0xa9, 0xfc, 0x33);
	mi.name.a = LPGEN("Check for updates");
	mi.hIcolibItem = iconList[0].hIcolib;
#else
	CLISTMENUITEM mi = { 0 };
	mi.cbSize = sizeof(mi);
	mi.icolibItem = IcoLib_GetIconHandle("check_update");
	mi.pszName = LPGEN("Check for updates");
#endif
	mi.position = 400010000;
	mi.pszService = MS_PU_CHECKUPDATES;
	Menu_AddMainMenuItem(&mi);

#if MIRANDA_VER >= 0x0A00
	InitListNew();

	SET_UID(mi, 0xafe94fad, 0xea83, 0x41aa, 0xa4, 0x26, 0xcb, 0x4a, 0x1c, 0x37, 0xc1, 0xd3);
	mi.position++;
	mi.hIcolibItem = iconList[2].hIcolib;
	mi.name.a = LPGEN("Available components list");
	mi.pszService = MS_PU_SHOWLIST;
	Menu_AddMainMenuItem(&mi);

	InitOptions();

	// Add hotkey
	HOTKEYDESC hkd = {};
	hkd.pszName = "Check for updates";
	hkd.szDescription.a = "Check for updates";
	hkd.szSection.a = "Plugin Updater";
	hkd.pszService = MS_PU_CHECKUPDATES;
	hkd.DefHotKey = HOTKEYCODE(HOTKEYF_CONTROL, VK_F10) | HKF_MIRANDA_LOCAL;
	hkd.lParam = FALSE;
	Hotkey_Register(&hkd);
#endif

	InitEvents();

#if MIRANDA_VER >= 0x0A00
	// add sounds
	Skin_AddSound("updatecompleted", LPGENW("Plugin Updater"), LPGENW("Update completed"));
	Skin_AddSound("updatefailed",    LPGENW("Plugin Updater"), LPGENW("Update failed"));

	// Upgrade old settings
	if (-1 == db_get_b(0, MODNAME, DB_SETTING_UPDATE_MODE, -1)) {
		ptrW dbvUpdateURL(db_get_wsa(0, MODNAME, DB_SETTING_UPDATE_URL));
		if (dbvUpdateURL) {
			if (!wcscmp(dbvUpdateURL, _A2W(DEFAULT_UPDATE_URL_OLD))) {
				db_set_b(0, MODNAME, DB_SETTING_UPDATE_MODE, UPDATE_MODE_STABLE);
				db_unset(0, MODNAME, DB_SETTING_UPDATE_URL);
			}
			else if (!wcscmp(dbvUpdateURL, _A2W(DEFAULT_UPDATE_URL_TRUNK_OLD))) {
				db_set_b(0, MODNAME, DB_SETTING_UPDATE_MODE, UPDATE_MODE_TRUNK);
				db_unset(0, MODNAME, DB_SETTING_UPDATE_URL);
			}
			else if (!wcscmp(dbvUpdateURL, _A2W(DEFAULT_UPDATE_URL_TRUNK_SYMBOLS_OLD) L"/")) {
				db_set_b(0, MODNAME, DB_SETTING_UPDATE_MODE, UPDATE_MODE_TRUNK_SYMBOLS);
				db_unset(0, MODNAME, DB_SETTING_UPDATE_URL);
			}
			else db_set_b(0, MODNAME, DB_SETTING_UPDATE_MODE, UPDATE_MODE_CUSTOM);
		}
	}
#endif
	return 0;
}

extern "C" __declspec(dllexport) int Unload(void)
{
	UnloadCheck();
	#if MIRANDA_VER >= 0x0A00
		UnloadListNew();
	#endif
	UnloadNetlib();
	return 0;
}
