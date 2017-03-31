/*

Copyright 2000-12 Miranda IM, 2012-17 Miranda NG project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

void CChatRoomDlg::StreamInEvents(LOGINFO *lin, bool bRedraw)
{
	if (m_hwnd == nullptr || lin == nullptr || m_si == nullptr)
		return;

	if (!bRedraw && m_si->iType == GCW_CHATROOM && m_bFilterEnabled && (m_iLogFilterFlags & lin->iType) == 0)
		return;

	LOGSTREAMDATA streamData;
	memset(&streamData, 0, sizeof(streamData));
	streamData.hwnd = m_log.GetHwnd();
	streamData.si = m_si;
	streamData.lin = lin;
	streamData.bStripFormat = FALSE;

	BOOL bFlag = FALSE;

	EDITSTREAM stream = {};
	stream.pfnCallback = Srmm_LogStreamCallback;
	stream.dwCookie = (DWORD_PTR)& streamData;

	SCROLLINFO scroll;
	scroll.cbSize = sizeof(SCROLLINFO);
	scroll.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
	GetScrollInfo(m_log.GetHwnd(), SB_VERT, &scroll);

	POINT point = {};
	m_log.SendMsg(EM_GETSCROLLPOS, 0, (LPARAM)&point);

	// do not scroll to bottom if there is a selection
	CHARRANGE oldsel, sel;
	m_log.SendMsg(EM_EXGETSEL, 0, (LPARAM)&oldsel);
	if (oldsel.cpMax != oldsel.cpMin)
		m_log.SendMsg(WM_SETREDRAW, FALSE, 0);

	//set the insertion point at the bottom
	sel.cpMin = sel.cpMax = GetRichTextLength(m_log.GetHwnd());
	m_log.SendMsg(EM_EXSETSEL, 0, (LPARAM)&sel);

	// fix for the indent... must be a M$ bug
	if (sel.cpMax == 0)
		bRedraw = TRUE;

	// should the event(s) be appended to the current log
	WPARAM wp = bRedraw ? SF_RTF : SFF_SELECTION | SF_RTF;

	//get the number of pixels per logical inch
	if (bRedraw) {
		HDC hdc = GetDC(nullptr);
		pci->logPixelSY = GetDeviceCaps(hdc, LOGPIXELSY);
		pci->logPixelSX = GetDeviceCaps(hdc, LOGPIXELSX);
		ReleaseDC(nullptr, hdc);
		m_log.SendMsg(WM_SETREDRAW, FALSE, 0);
		bFlag = TRUE;
	}

	// stream in the event(s)
	streamData.lin = lin;
	streamData.bRedraw = bRedraw;
	m_log.SendMsg(EM_STREAMIN, wp, (LPARAM)&stream);

	// do smileys
	if (SmileyAddInstalled && (bRedraw || (lin->ptszText && lin->iType != GC_EVENT_JOIN && lin->iType != GC_EVENT_NICK && lin->iType != GC_EVENT_ADDSTATUS && lin->iType != GC_EVENT_REMOVESTATUS))) {
		CHARRANGE newsel;
		newsel.cpMax = -1;
		newsel.cpMin = sel.cpMin;
		if (newsel.cpMin < 0)
			newsel.cpMin = 0;

		SMADD_RICHEDIT3 sm = {};
		sm.cbSize = sizeof(sm);
		sm.hwndRichEditControl = m_log.GetHwnd();
		sm.Protocolname = m_si->pszModule;
		sm.rangeToReplace = bRedraw ? nullptr : &newsel;
		sm.disableRedraw = TRUE;
		sm.hContact = m_si->hContact;
		CallService(MS_SMILEYADD_REPLACESMILEYS, 0, (LPARAM)&sm);
	}

	// scroll log to bottom if the log was previously scrolled to bottom, else restore old position
	if (bRedraw || (UINT)scroll.nPos >= (UINT)scroll.nMax - scroll.nPage - 5 || scroll.nMax - scroll.nMin - scroll.nPage < 50)
		ScrollToBottom();
	else
		m_log.SendMsg(EM_SETSCROLLPOS, 0, (LPARAM)&point);

	// do we need to restore the selection
	if (oldsel.cpMax != oldsel.cpMin) {
		m_log.SendMsg(EM_EXSETSEL, 0, (LPARAM)&oldsel);
		m_log.SendMsg(WM_SETREDRAW, TRUE, 0);
		InvalidateRect(m_log.GetHwnd(), nullptr, TRUE);
	}

	// need to invalidate the window
	if (bFlag) {
		sel.cpMin = sel.cpMax = GetRichTextLength(m_log.GetHwnd());
		m_log.SendMsg(EM_EXSETSEL, 0, (LPARAM)&sel);
		m_log.SendMsg(WM_SETREDRAW, TRUE, 0);
		InvalidateRect(m_log.GetHwnd(), nullptr, TRUE);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

char* Message_GetFromStream(HWND hwndDlg, SESSION_INFO *si)
{
	if (hwndDlg == 0 || si == 0)
		return nullptr;

	char* pszText = nullptr;
	EDITSTREAM stream;
	memset(&stream, 0, sizeof(stream));
	stream.pfnCallback = Srmm_MessageStreamCallback;
	stream.dwCookie = (DWORD_PTR)&pszText; // pass pointer to pointer

	DWORD dwFlags = SF_RTFNOOBJS | SFF_PLAINRTF | SF_USECODEPAGE | (CP_UTF8 << 16);
	SendDlgItemMessage(hwndDlg, IDC_SRMM_MESSAGE, EM_STREAMOUT, dwFlags, (LPARAM)&stream);
	return pszText; // pszText contains the text
}

/////////////////////////////////////////////////////////////////////////////////////////

int GetRichTextLength(HWND hwnd)
{
	GETTEXTLENGTHEX gtl;
	gtl.flags = GTL_PRECISE;
	gtl.codepage = CP_ACP;
	return (int)SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
}

int GetColorIndex(const char* pszModule, COLORREF cr)
{
	MODULEINFO *pMod = pci->MM_FindModule(pszModule);
	int i = 0;

	if (!pMod || pMod->nColorCount == 0)
		return -1;

	for (i = 0; i < pMod->nColorCount; i++)
		if (pMod->crColors[i] == cr)
			return i;

	return -1;
}

// obscure function that is used to make sure that any of the colors
// passed by the protocol is used as fore- or background color
// in the messagebox. THis is to vvercome limitations in the richedit
// that I do not know currently how to fix

void CheckColorsInModule(const char* pszModule)
{
	MODULEINFO *pMod = pci->MM_FindModule(pszModule);
	if (!pMod)
		return;

	COLORREF crBG = (COLORREF)db_get_dw(0, CHAT_MODULE, "ColorMessageBG", GetSysColor(COLOR_WINDOW));
	for (int i = 0; i < pMod->nColorCount; i++) {
		if (pMod->crColors[i] == g_Settings.MessageAreaColor || pMod->crColors[i] == crBG) {
			if (pMod->crColors[i] == RGB(255, 255, 255))
				pMod->crColors[i]--;
			else
				pMod->crColors[i]++;
		}
	}
}

void ValidateFilename(wchar_t *filename)
{
	wchar_t *p1 = filename;
	wchar_t szForbidden[] = L"\\/:*?\"<>|";
	while (*p1 != '\0') {
		if (wcschr(szForbidden, *p1))
			*p1 = '_';
		p1 += 1;
	}
}

int RestoreWindowPosition(HWND hwnd, MCONTACT hContact, bool bHide)
{
	int x = db_get_dw(hContact, CHAT_MODULE, "roomx", -1);
	if (x == -1)
		return 0;

	int y = (int)db_get_dw(hContact, CHAT_MODULE, "roomy", -1);
	int width = db_get_dw(hContact, CHAT_MODULE, "roomwidth", -1);
	int height = db_get_dw(hContact, CHAT_MODULE, "roomheight", -1);

	DWORD dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
	if (bHide)
		dwFlags |= SWP_HIDEWINDOW;
	SetWindowPos(hwnd, nullptr, x, y, width, height, dwFlags);
	return 1;
}
