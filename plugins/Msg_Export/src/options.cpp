/////////////////////////////////////////////////////////////////////////////////////////
// This file is part of Msg_Export a Miranda IM plugin
// Copyright (C)2002 Kennet Nielsen ( http://sourceforge.net/projects/msg-export/ )
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#define STRINGIZE(x) #x
#define EVAL_STRINGIZE(x) STRINGIZE(x)
#define __LOC__ __FILE__ "("EVAL_STRINGIZE(__LINE__)") : "

// width in pixels of the UIN column in the List Ctrl
const int nUINColWitdh = 80;
// width in pixels of the UIN column in the List Ctrl
const int nProtoColWitdh = 100;


// Used to controle the sending of the PSM_CHANGED to miranda
// and to se if the user has unapplyed changes when he presses the
// Export All button
BOOL bUnaplyedChanges = FALSE;


/////////////////////////////////////////////////////////////////////
// Class           :  CLDBEvent
// Superclass      :  
// Project         :  Mes_export
// Designer        :  Kennet Nielsen
// Version         :  1.0.0
// Date            :  020422, 22 April 2002
//
//
// Description: This class is used to store one DB event dyring the export 
//              All history function
//
// Version History:
//   Ver:     Initials:    Date:     Text:
//   1.0.0    KN           020422    First edition
//
/////////////////////////////////////////////////////////////////////
//
class CLDBEvent
{
	DWORD time;
public:
	MCONTACT hUser;
	MEVENT   hDbEvent;

	CLDBEvent(MCONTACT hU, MEVENT hDBE)
	{
		hUser = hU;
		hDbEvent = hDBE;

		DBEVENTINFO dbei = {};
		db_event_get(hDbEvent, &dbei);
		time = dbei.timestamp;
	}

	bool operator <(const CLDBEvent& rOther) const
	{	return time < rOther.time;
	}
};

/////////////////////////////////////////////////////////////////////
// Member Function : CompareFunc
// Type            : Global
// Parameters      : lParam1    - ?
//                   lParam2    - ?
//                   lParamSort - ?
// Returns         : int CALLBACK
// Description     : Used to sort list view by Nick
//                   
// References      : -
// Remarks         : -
// Created         : 020422, 22 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	if (lParamSort == 1)
		return mir_wstrcmpi(pcli->pfnGetContactDisplayName(lParam1, 0), pcli->pfnGetContactDisplayName(lParam2, 0));

	if (lParamSort == 2)
		return _DBGetString((MCONTACT)lParam1, "Protocol", "p", L"").compare(_DBGetString((MCONTACT)lParam2, "Protocol", "p", L""));

	if (lParamSort == 3) {
		DWORD dwUin1 = db_get_dw((MCONTACT)lParam1, _DBGetStringA((MCONTACT)lParam1, "Protocol", "p", "").c_str(), "UIN", 0);
		DWORD dwUin2 = db_get_dw((MCONTACT)lParam2, _DBGetStringA((MCONTACT)lParam2, "Protocol", "p", "").c_str(), "UIN", 0);
		if (dwUin1 == dwUin2)
			return 0;
		if (dwUin1 > dwUin2)
			return -1;
		return 1;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////
// Member Function : DialogProc
// Type            : Global
// Parameters      : hwndDlg   - handle to dialog box
//                   uMsg      - message
//                   wParam    - first message parameter
//                   parameter - second message parameter
// Returns         : INT_PTR CALLBACK
// Description     : Progress bar window function
//                   
// References      : -
// Remarks         : -
// Created         : 020422, 22 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
INT_PTR CALLBACK __stdcall DialogProc(HWND hwndDlg, UINT uMsg, WPARAM, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		return TRUE;
	case WM_CLOSE:
		if (lParam > 0)
			DestroyWindow(hwndDlg);
		return TRUE;
	}
	return FALSE;
}

struct ExportDialogData {
	list<MCONTACT> contacts;
	HWND hDialog;
};

void exportContactsMessages(void *p)
{
	ExportDialogData *data = (ExportDialogData *)p;
	
	HWND hDlg = data->hDialog;
	HWND hProg = GetDlgItem(hDlg, IDC_EXPORT_PROGRESS);
	HWND hStatus = GetDlgItem(hDlg, IDC_EXP_ALL_STATUS);

	SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, data->contacts.size() - 1));
	SetWindowText(hStatus, TranslateT("Reading database information (Phase 1 of 2)"));

	// map with list to stored all DB history before it is exported 
	map<tstring, list< CLDBEvent >, less<tstring> > AllEvents;
	{
		// reading from the database !!! 
		int nCur = 0;
		list< MCONTACT >::const_iterator iterator;
		for (iterator = data->contacts.begin(); iterator != data->contacts.end(); ++iterator) {
			MCONTACT hContact = (*iterator);

			// Check if we should ignore this contact/protocol
			if (!bIsExportEnabled(hContact))
				continue;

			list<CLDBEvent> &rclCurList = AllEvents[GetFilePathFromUser(hContact)];

			MEVENT hDbEvent = db_event_first(hContact);
			while (hDbEvent) {
				rclCurList.push_back(CLDBEvent(hContact, hDbEvent));
				hDbEvent = db_event_next(hContact, hDbEvent);
			}

			SendMessage(hProg, PBM_SETPOS, nCur, 0);
			RedrawWindow(hDlg, NULL, NULL, RDW_ALLCHILDREN | RDW_UPDATENOW);

			nCur++;
		}
		// Free the list of contacts
		data->contacts.clear();
		delete data;
	}

	// window text update 
	SetWindowText(hStatus, TranslateT("Sorting and writing database information (Phase 2 of 2)"));
	SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, AllEvents.size() - 1));
	SendMessage(hProg, PBM_SETPOS, 0, 0);

	// time to write to files !!!
	map<tstring, list< CLDBEvent >, less<tstring> >::iterator FileIterator;

	int nCur = 0;
	for (FileIterator = AllEvents.begin(); FileIterator != AllEvents.end(); ++FileIterator) {
		(FileIterator->second).sort(); // Sort is preformed here !!
		// events with same time will not be swaped, they will 
		// remain in there original order

		// Open/create file for writing
		wstring sFilePath = FileIterator->first;
		HANDLE hFile = openCreateFile(sFilePath);
		if (hFile == INVALID_HANDLE_VALUE) {
			DisplayErrorDialog(LPGENW("Failed to open or create file :\n"), sFilePath, NULL);
			continue;
		}

		// At first write we need to have this false (to write file header, etc.), for each next write to same file use true
		bool bAppendOnly = false;

		list< CLDBEvent >::const_iterator iterator;
		for (iterator = FileIterator->second.begin(); iterator != FileIterator->second.end(); ++iterator) {
			MEVENT hDbEvent = (*iterator).hDbEvent;
			MCONTACT hContact = (*iterator).hUser;
			if (!bExportEvent(hContact, hDbEvent, hFile, sFilePath, bAppendOnly))
				break; // serious error, we should close the file and don't continue with it

			// Set this flag, because we're appending to same file now
			bAppendOnly = true;
		}

		// Close the file
		CloseHandle(hFile);

		SendMessage(hProg, PBM_SETPOS, ++nCur, 0);
		RedrawWindow(hDlg, NULL, NULL, RDW_ALLCHILDREN | RDW_UPDATENOW);
	}

	SendMessage(hDlg, WM_CLOSE, 0, 1);
}

/////////////////////////////////////////////////////////////////////
// Member Function : nExportCompleatList
// Type            : Global
// Parameters      : hParent       - handle to the parrent, ( Options Dlg )
//                   bOnlySelected - Only Export the userges that hase been selected in the list view
// Returns         : int not used currently 
// Description     : 
//                   
// References      : -
// Remarks         : -
// Created         : 020422, 22 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
int nExportCompleatList(HWND hParent, bool bOnlySelected)
{
	HWND hMapUser = GetDlgItem(hParent, IDC_MAP_USER_LIST);

	int nTotalContacts = ListView_GetItemCount(hMapUser);

	int nContacts;
	if (bOnlySelected)
		nContacts = ListView_GetSelectedCount(hMapUser);
	else
		nContacts = nTotalContacts;

	if (!hMapUser || nContacts <= 0) {
		MessageBox(hParent, TranslateT("No contacts found to export"), MSG_BOX_TITEL, MB_OK);
		return 0;
	}

	ExportDialogData *data = new ExportDialogData();

	// List all contacts to export
	{
		LVITEM sItem = { 0 };
		sItem.mask = LVIF_PARAM;

		for (int nCur = 0; nCur < nTotalContacts; nCur++) {
			if (bOnlySelected)
			if (!(ListView_GetItemState(hMapUser, nCur, LVIS_SELECTED) & LVIS_SELECTED))
				continue;

			sItem.iItem = nCur;
			if (!ListView_GetItem(hMapUser, &sItem)) {
				MessageBox(hParent, TranslateT("Failed to export at least one contact"), MSG_BOX_TITEL, MB_OK);
				continue;
			}

			MCONTACT hContact = (MCONTACT)sItem.lParam;
			data->contacts.push_back(hContact);
		}
	}

	// Create progress dialog
	HWND hDlg = data->hDialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_EXPORT_ALL_DLG), NULL, DialogProc);
	ShowWindow(hDlg, SW_SHOWNORMAL);
	
	// Process the export in other thread
	mir_forkthread(&exportContactsMessages, data);
	return 0;
}

/////////////////////////////////////////////////////////////////////
// Member Function : SetToDefault
// Type            : Global
// Parameters      : hwndDlg - ?
// Returns         : void
// Description     : 
//                   
// References      : -
// Remarks         : -
// Created         : 021228, 28 December 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
void SetToDefault(HWND hParent)
{
	HWND hMapUser = GetDlgItem(hParent, IDC_MAP_USER_LIST);
	int nContacts = ListView_GetItemCount(hMapUser);

	if (!hMapUser || nContacts <= 0)
		return;

	wchar_t szTemp[500];
	if (!GetDlgItemText(hParent, IDC_DEFAULT_FILE, szTemp, _countof(szTemp)))
		return;

	LVITEM sItem = { 0 };

	for (int nCur = 0; nCur < nContacts; nCur++) {
		if (!(ListView_GetItemState(hMapUser, nCur, LVIS_SELECTED) & LVIS_SELECTED))
			continue;

		sItem.iItem = nCur;
		sItem.mask = LVIF_PARAM;
		if (!ListView_GetItem(hMapUser, &sItem))
			continue;

		tstring sFileName = szTemp;
		ReplaceDefines((MCONTACT)sItem.lParam, sFileName);
		ReplaceTimeVariables(sFileName);

		sItem.mask = LVIF_TEXT;
		sItem.pszText = (wchar_t*)sFileName.c_str();
		ListView_SetItem(hMapUser, &sItem);

		if (!bUnaplyedChanges) {
			bUnaplyedChanges = TRUE;
			SendMessage(GetParent(hParent), PSM_CHANGED, 0, 0);
		}
	}
}

/////////////////////////////////////////////////////////////////////
// Member Function : bApplyChanges
// Type            : Global
// Parameters      : hwndDlg - handle to the parrent, ( Options Dlg )
// Returns         : Returns true if the changes was applyed
// Description     : but since we cant abort an apply opperation, 
//                   this can not currently be used 
// References      : -
// Remarks         : -
// Created         : 020422, 22 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
BOOL bApplyChanges(HWND hwndDlg)
{
	BOOL bTrans;
	BOOL bRet = true;
	wchar_t szTemp[500];

	int nTmp = GetDlgItemInt(hwndDlg, IDC_MAX_CLOUMN_WIDTH, &bTrans, TRUE);
	if (!bTrans || (nTmp > 0 && nTmp < 5)) {
		mir_snwprintf(szTemp, TranslateT("Max line width must be at least %d. Or use 0 for no limit."), 5);
		MessageBox(hwndDlg, szTemp, MSG_BOX_TITEL, MB_OK);
		bRet = false;
	}
	else nMaxLineWidth = nTmp;

	GetDlgItemText(hwndDlg, IDC_EXPORT_TIMEFORMAT, szTemp, _countof(szTemp));
	sTimeFormat = szTemp;

	GetDlgItemText(hwndDlg, IDC_EXPORT_DIR, szTemp, _countof(szTemp));
	sExportDir = szTemp;

	GetDlgItemText(hwndDlg, IDC_DEFAULT_FILE, szTemp, _countof(szTemp));
	sDefaultFile = szTemp;

	GetDlgItemText(hwndDlg, IDC_FILE_VIEWER, szTemp, _countof(szTemp));
	sFileViewerPrg = szTemp;

	bUseInternalViewer(IsDlgButtonChecked(hwndDlg, IDC_USE_INTERNAL_VIEWER) == BST_CHECKED);

	bool bNewRp = IsDlgButtonChecked(hwndDlg, IDC_REPLACE_MIRANDA_HISTORY) == BST_CHECKED;
	if (bReplaceHistory != bNewRp) {
		bReplaceHistory = bNewRp;
		MessageBox(hwndDlg, TranslateT("You need to restart Miranda to change the history function"), MSG_BOX_TITEL, MB_OK);
	}

	bAppendNewLine = IsDlgButtonChecked(hwndDlg, IDC_APPEND_NEWLINE) == BST_CHECKED;
	bUseUtf8InNewFiles = IsDlgButtonChecked(hwndDlg, IDC_USE_UTF8_IN_NEW_FILES) == BST_CHECKED;

	bUseLessAndGreaterInExport = IsDlgButtonChecked(hwndDlg, IDC_USE_LESS_AND_GREATER_IN_EXPORT) == BST_CHECKED;


	HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);
	int nCount = ListView_GetItemCount(hMapUser);
	for (int nCur = 0; nCur < nCount; nCur++) {
		LVITEM sItem = { 0 };
		sItem.iItem = nCur;
		sItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		sItem.pszText = szTemp;
		sItem.cchTextMax = _countof(szTemp);

		if (ListView_GetItem(hMapUser, &sItem)) {
			MCONTACT hUser = (MCONTACT)sItem.lParam;
			if (mir_wstrlen(szTemp) > 0)
				db_set_ws(hUser, MODULE, "FileName", szTemp);
			else
				db_unset(hUser, MODULE, "FileName");

			if (sItem.iImage)
				db_unset(hUser, MODULE, "EnableLog"); // default is Enabled !!
			else
				db_set_b(hUser, MODULE, "EnableLog", 0);
		}
	}
	UpdateFileToColWidth();

	SaveSettings();


	bUnaplyedChanges = FALSE;
	return bRet;
}

/////////////////////////////////////////////////////////////////////
// Member Function : ClearAllFileNames
// Type            : Global
// Parameters      : hwndDlg - handle to the parrent, ( Options Dlg )
// Returns         : void
// Description     : Just clear all file name's entered 
//                   
// References      : -
// Remarks         : -
// Created         : 020422, 23 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
void ClearAllFileNames(HWND hwndDlg)
{
	LVITEM sItem = { 0 };
	sItem.mask = LVIF_TEXT;
	sItem.pszText = L"";

	HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);
	int nCount = ListView_GetItemCount(hMapUser);
	for (int nCur = 0; nCur < nCount; nCur++) {
		sItem.iItem = nCur;
		ListView_SetItem(hMapUser, &sItem);
	}
	if (!bUnaplyedChanges) {
		bUnaplyedChanges = TRUE;
		SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
	}
}

/////////////////////////////////////////////////////////////////////
// Member Function : AutoFindeFileNames
// Type            : Global
// Parameters      : hwndDlg - handle to the parrent, ( Options Dlg )
// Returns         : void
// Description     : Try to finde new file names for user's with 2or more UIN's
//                   
// References      : -
// Remarks         : -
// Created         : 020422, 23 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////
//
void AutoFindeFileNames(HWND hwndDlg)
{
	wchar_t szDefaultFile[500];
	GetDlgItemText(hwndDlg, IDC_DEFAULT_FILE, szDefaultFile, _countof(szDefaultFile));

	LVITEM sItem = { 0 };

	HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);
	int nCount = ListView_GetItemCount(hMapUser);
	for (int nCur = 0; nCur < nCount; nCur++) {
		wchar_t szSearch[100];
		sItem.pszText = szSearch;
		sItem.mask = LVIF_TEXT;
		sItem.iItem = nCur;
		sItem.iSubItem = 1;
		sItem.cchTextMax = _countof(szSearch);
		if (!ListView_GetItem(hMapUser, &sItem))
			continue;

		int nShortestMatch = 0xFFFF;
		MCONTACT hStortest = 0;
		int nStortestIndex = -1;
		for (int nSubCur = 0; nSubCur < nCount; nSubCur++) {
			if (nSubCur == nCur)
				continue;
			wchar_t szSubCur[100];
			sItem.mask = LVIF_TEXT | LVIF_PARAM;
			sItem.iItem = nSubCur;
			sItem.iSubItem = 1;
			sItem.pszText = szSubCur;
			sItem.cchTextMax = _countof(szSubCur);
			if (ListView_GetItem(hMapUser, &sItem)) {
				size_t nLen = mir_wstrlen(szSubCur);
				if (wcsnicmp(szSubCur, szSearch, nLen) == 0) {
					if (nLen < (size_t)nShortestMatch) {
						nShortestMatch = (int)nLen;
						nStortestIndex = nSubCur;
						hStortest = (MCONTACT)sItem.lParam;
					}
				}
			}
		}
		
		if (nShortestMatch != 0xFFFF) {
			tstring sFileName;
			szSearch[0] = 0;
			ListView_GetItemText(hMapUser, nCur, 0, szSearch, _countof(szSearch));
			bool bPriHasFileName = szSearch[0] != 0;
			if (bPriHasFileName)
				sFileName = szSearch;

			szSearch[0] = 0;
			ListView_GetItemText(hMapUser, nStortestIndex, 0, szSearch, _countof(szSearch));
			bool bSubHasFileName = szSearch[0] != 0;
			if (bSubHasFileName)
				sFileName = szSearch;

			if (sFileName.empty()) {
				sFileName = szDefaultFile;
				ReplaceDefines(hStortest, sFileName);
				ReplaceTimeVariables(sFileName);
			}

			if (!bPriHasFileName)
				ListView_SetItemText(hMapUser, nCur, 0, (wchar_t*)sFileName.c_str());

			if (!bSubHasFileName)
				ListView_SetItemText(hMapUser, nStortestIndex, 0, (wchar_t*)sFileName.c_str());

			if (!bUnaplyedChanges) {
				bUnaplyedChanges = TRUE;
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////
// Member Function : DlgProcMsgExportOpts
// Type            : Global
// Parameters      : hwndDlg - handle to this dialog
//                   msg     - ?
//                   wParam  - ?
//                   lParam  - ?
// Returns         : static BOOL CALLBACK
// Description     : Main message prossing fore my options dialog
//                   
// References      : -
// Remarks         : -
// Created         : 020422, 22 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK DlgProcMsgExportOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Used to prevent sending the PSM_CHANGED to miranda
	// when initilizing 
	static BOOL bWindowTextSet = FALSE;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);

		bWindowTextSet = FALSE;
		{
			HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);

			{ // init adv. win styles 
				DWORD dw = ListView_GetExtendedListViewStyle(hMapUser);
				dw |= LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT;
				ListView_SetExtendedListViewStyle(hMapUser, dw /*| LVS_EX_LABELTIP*/);
			}

			int nColumnWidth = 100;
			RECT rListSize;
			if (GetWindowRect(hMapUser, &rListSize)) {
				nColumnWidth = (rListSize.right - rListSize.left - GetSystemMetrics(SM_CXVSCROLL) - 5 - nUINColWitdh - nProtoColWitdh) / 2;
				if (nColumnWidth < 10)
					nColumnWidth = 10;
			}

			// header setup !!
			{
				LVCOLUMN cCol = { 0 };
				cCol.mask = LVCF_TEXT | LVCF_WIDTH;
				cCol.cx = nColumnWidth;
				cCol.pszText = TranslateT("File");
				ListView_InsertColumn(hMapUser, 0, &cCol);

				cCol.pszText = TranslateT("Nick");
				ListView_InsertColumn(hMapUser, 1, &cCol);

				cCol.cx = nProtoColWitdh;
				cCol.pszText = TranslateT("Proto");
				ListView_InsertColumn(hMapUser, 2, &cCol);

				cCol.cx = nUINColWitdh;
				cCol.mask |= LVCF_FMT;
				cCol.fmt = LVCFMT_RIGHT;
				cCol.pszText = TranslateT("UIN");
				ListView_InsertColumn(hMapUser, 3, &cCol);
			}
			{
				HIMAGELIST hIml = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR4 | ILC_MASK, 2, 2);
				ImageList_AddIcon(hIml, Skin_LoadIcon(SKINICON_OTHER_NOTICK));
				ImageList_AddIcon(hIml, Skin_LoadIcon(SKINICON_OTHER_TICK));
				ListView_SetImageList(hMapUser, hIml, LVSIL_SMALL);
			}
			{
				int nUser = 0;
				tstring sTmp;
				LVITEM sItem = { 0 };
				for (MCONTACT hContact = db_find_first(); hContact; hContact = db_find_next(hContact)) {
					PROTOACCOUNT *pa = Proto_GetAccount(GetContactProto(hContact));
					if (pa == NULL)
						continue;

					sItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
					sItem.iItem = nUser++;
					sItem.iSubItem = 0;
					sItem.iImage = db_get_b(hContact, MODULE, "EnableLog", 1);
					sItem.lParam = hContact;

					sTmp = _DBGetString(hContact, MODULE, "FileName", L"");
					sItem.pszText = (wchar_t*)sTmp.c_str();
					ListView_InsertItem(hMapUser, &sItem);

					sItem.mask = LVIF_TEXT;
					sItem.iSubItem = 1;
					sItem.pszText = pcli->pfnGetContactDisplayName(hContact, 0);
					ListView_SetItem(hMapUser, &sItem);

					sItem.iSubItem = 2;
					sItem.pszText = pa->tszAccountName;
					ListView_SetItem(hMapUser, &sItem);

					DWORD dwUIN = db_get_dw(hContact, pa->szModuleName, "UIN", 0);
					wchar_t szTmp[50];
					mir_snwprintf(szTmp, L"%d", dwUIN);
					sItem.iSubItem = 3;
					sItem.pszText = szTmp;
					ListView_SetItem(hMapUser, &sItem);
				}
				ListView_SortItems(hMapUser, CompareFunc, 1);

				sItem.mask = LVIF_STATE;
				sItem.iItem = 0;
				sItem.iSubItem = 0;
				sItem.state = LVIS_FOCUSED;
				sItem.stateMask = LVIS_FOCUSED;
				ListView_SetItem(hMapUser, &sItem);
			}
			HWND hComboBox;

			SetDlgItemInt(hwndDlg, IDC_MAX_CLOUMN_WIDTH, nMaxLineWidth, TRUE);

			// Export dir
			SetDlgItemText(hwndDlg, IDC_EXPORT_DIR, sExportDir.c_str());
			hComboBox = GetDlgItem(hwndDlg, IDC_EXPORT_DIR);
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%dbpath%\\MsgExport\\");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"C:\\Backup\\MsgExport\\");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%dbpath%\\MsgExport\\%group% - ");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%dbpath%\\MsgExport\\%group%\\");

			// default file
			SetDlgItemText(hwndDlg, IDC_DEFAULT_FILE, sDefaultFile.c_str());
			hComboBox = GetDlgItem(hwndDlg, IDC_DEFAULT_FILE);
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%nick%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%UIN%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%group%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%e-mail%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%identifier%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%year%-%month%-%day%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%group%\\%nick%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%group%\\%UIN%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%group%\\%identifier%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"%protocol%\\%nick%.txt");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"History.txt");
			
			// time format
			SetDlgItemText(hwndDlg, IDC_EXPORT_TIMEFORMAT, sTimeFormat.c_str());
			hComboBox = GetDlgItem(hwndDlg, IDC_EXPORT_TIMEFORMAT);
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"d t");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"d s");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"d m");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"D s");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"D m :");

			// File viewer
			SetDlgItemText(hwndDlg, IDC_FILE_VIEWER, sFileViewerPrg.c_str());
			hComboBox = GetDlgItem(hwndDlg, IDC_FILE_VIEWER);
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"C:\\Windows\\Notepad.exe");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"C:\\WinNT\\Notepad.exe");
			SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"C:\\Program Files\\Notepad++\\notepad++.exe");

			CheckDlgButton(hwndDlg, IDC_USE_INTERNAL_VIEWER, bUseInternalViewer() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_REPLACE_MIRANDA_HISTORY, bReplaceHistory ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_APPEND_NEWLINE, bAppendNewLine ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_USE_UTF8_IN_NEW_FILES, bUseUtf8InNewFiles ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_USE_LESS_AND_GREATER_IN_EXPORT, bUseLessAndGreaterInExport ? BST_CHECKED : BST_UNCHECKED);
		}

		bWindowTextSet = TRUE;
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_EXPORTSELECTED:
		case IDC_EXPORTALL:
			if (bUnaplyedChanges) {
				DWORD res = MessageBox(hwndDlg, TranslateT("You have not applied the changes, do you wish to apply them first?"), MSG_BOX_TITEL, MB_YESNOCANCEL);
				if (res == IDCANCEL)
					return TRUE;
				if (res == IDYES) {
					if (!bApplyChanges(hwndDlg)) {
						return TRUE;
					}
				}
			}
			nExportCompleatList(hwndDlg, LOWORD(wParam) == ID_EXPORTSELECTED);
			return TRUE;

		case IDC_EXPORT_DIR:
		case IDC_EXPORT_TIMEFORMAT:
		case IDC_DEFAULT_FILE:
		case IDC_FILE_VIEWER:
			if (!bWindowTextSet)
				return TRUE;

			if (HIWORD(wParam) == CBN_EDITUPDATE || HIWORD(wParam) == CBN_SELCHANGE) {
				bUnaplyedChanges = TRUE;
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			return TRUE;

		case IDC_MAX_CLOUMN_WIDTH:
			if (!bWindowTextSet)
				return TRUE;

			if (HIWORD(wParam) == EN_CHANGE) {
				bUnaplyedChanges = TRUE;
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			return TRUE;

		case IDC_USE_INTERNAL_VIEWER: // fall thru here
		case IDC_REPLACE_MIRANDA_HISTORY:
		case IDC_APPEND_NEWLINE:
		case IDC_USE_UTF8_IN_NEW_FILES:
		case IDC_USE_LESS_AND_GREATER_IN_EXPORT:
			if (HIWORD(wParam) == BN_CLICKED) {
				bUnaplyedChanges = TRUE;
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			return TRUE;

		case ID_USERLIST_USERDETAILS:
			{
				LVITEM sItem = { 0 };
				sItem.mask = LVIF_PARAM;
				HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);
				sItem.iItem = ListView_GetNextItem(hMapUser, -1, LVIS_SELECTED);
				if (sItem.iItem >= 0 && ListView_GetItem(hMapUser, &sItem))
					CallService(MS_USERINFO_SHOWDIALOG, (WPARAM)sItem.lParam, 0);
			}
			return TRUE;

		case IDC_AUTO_FILENAME:
			AutoFindeFileNames(hwndDlg);
			return TRUE;

		case IDC_CLEAR_ALL:
			ClearAllFileNames(hwndDlg);
			return TRUE;

		case IDC_OPEN_HELP:
			Utils_OpenUrl("https://miranda-ng.org/p/Msg_Export/");
			return TRUE;

		case ID_SET_TO_DEFAULT:
			SetToDefault(hwndDlg);
			return TRUE;

		case IDC_FILE_VIEWER_BROWSE:
			wchar_t szFile[260];       // buffer for file name
			GetDlgItemText(hwndDlg, IDC_FILE_VIEWER, szFile, _countof(szFile));

			wchar_t buf[MAX_PATH];
			mir_snwprintf(buf, L"%s (*.exe;*.com;*.bat;*.cmd)%c*.exe;*.com;*.bat;*.cmd%c%s (*.*)%c*.*%c%c", TranslateT("Executable files"), 0, 0, TranslateT("All files"), 0, 0, 0);
			{
				OPENFILENAME ofn = {};       // common dialog box structure
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwndDlg;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = _countof(szFile);
				ofn.lpstrFilter = buf;
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

				// Display the Open dialog box. 
				if (GetOpenFileName(&ofn)) {
					SetDlgItemText(hwndDlg, IDC_FILE_VIEWER, szFile);
					bUnaplyedChanges = TRUE;
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				}
			}
			return TRUE;

		case IDC_EXPORT_DIR_BROWSE:
			// CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
			// Get the shells allocator
			LPMALLOC pMalloc;
			if (FAILED(SHGetMalloc(&pMalloc))) { // we need to use this to support old Windows versions 
				MessageBox(hwndDlg, TranslateT("Failed to get the shells allocator!"), MSG_BOX_TITEL, MB_OK);
				return TRUE; // TRUE because we have handled the message, sort of *S*
			}

			// Allocate the Dest Dir buffer to receive browse info
			wchar_t *lpDestDir = (wchar_t *)pMalloc->Alloc(MAX_PATH + 100);
			if (!lpDestDir) {
				pMalloc->Release();
				MessageBox(hwndDlg, TranslateT("Failed to Allocate buffer space"), MSG_BOX_TITEL, MB_OK);
				return TRUE;
			}

			BROWSEINFO sBrowseInfo;
			sBrowseInfo.hwndOwner = hwndDlg;
			sBrowseInfo.pidlRoot = NULL;
			sBrowseInfo.pszDisplayName = lpDestDir;
			sBrowseInfo.lpszTitle = TranslateT("Select Destination Directory");
			sBrowseInfo.ulFlags = BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
			sBrowseInfo.lpfn = NULL;
			sBrowseInfo.lParam = 0;
			sBrowseInfo.iImage = 0;

			LPITEMIDLIST psItemIDList = SHBrowseForFolder(&sBrowseInfo);
			if (psItemIDList) {
				SHGetPathFromIDList(psItemIDList, lpDestDir);
				size_t n = mir_wstrlen(lpDestDir);
				if (n > 0 && lpDestDir[n] != '\\') {
					lpDestDir[n] = '\\';
					lpDestDir[n + 1] = 0;
				}
				SetDlgItemText(hwndDlg, IDC_EXPORT_DIR, lpDestDir);
				bUnaplyedChanges = TRUE;
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				// Clean up
				pMalloc->Free(psItemIDList);
			}
			pMalloc->Free(lpDestDir);
			pMalloc->Release();
			return TRUE;
		}
		break;

	case WM_CONTEXTMENU:
		if (wParam == (WPARAM)GetDlgItem(hwndDlg, IDC_MAP_USER_LIST)) {
			HMENU hMainMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MSG_EXPORT));
			if (hMainMenu) {
				HMENU hMenu = GetSubMenu(hMainMenu, 0);

				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				if (pt.x == -1 && pt.y == -1) {
					HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);
					int nFirst = ListView_GetNextItem(hMapUser, -1, LVNI_FOCUSED);
					if (nFirst >= 0) {
						ListView_GetItemPosition(hMapUser, nFirst, &pt);
					}

					if (pt.y < 16)
						pt.y = 16;
					else {
						RECT rUserList;
						GetClientRect(hMapUser, &rUserList);
						if (pt.y > rUserList.bottom - 16)
							pt.y = rUserList.bottom - 16;
						else
							pt.y += 8;
					}
					pt.x = 8;
					ClientToScreen(hMapUser, &pt);
				}

				TranslateMenu(hMenu);
				TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);

				DestroyMenu(hMainMenu);
			}
			return TRUE;
		}
		return FALSE;

	case WM_NOTIFY:
		NMHDR *p = ((LPNMHDR)lParam);
		if (p->idFrom == IDC_MAP_USER_LIST) {
			LVHITTESTINFO hti;
			LVITEM lvi;

			switch (p->code) {
			case NM_CLICK:
				hti.pt = ((NMLISTVIEW*)lParam)->ptAction;
				ListView_SubItemHitTest(p->hwndFrom, &hti);

				if (hti.flags != LVHT_ONITEMICON)
					break;

				lvi.mask = LVIF_IMAGE;
				lvi.iItem = hti.iItem;
				lvi.iSubItem = 0;
				ListView_GetItem(p->hwndFrom, &lvi);
				lvi.iImage ^= 1;
				ListView_SetItem(p->hwndFrom, &lvi);
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;

			case LVN_ENDLABELEDIT:
				{
					NMLVDISPINFO *pdi = (NMLVDISPINFO *)lParam;
					if (pdi->item.mask & LVIF_TEXT) {
						pdi->item.mask &= LVIF_TEXT;
						ListView_SetItem(p->hwndFrom, &pdi->item);

						bUnaplyedChanges = TRUE;
						SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
					}
				}
				return TRUE;

			case LVN_KEYDOWN:
				{
					NMLVKEYDOWN *lpnmk = (NMLVKEYDOWN *)lParam;
					if (lpnmk->wVKey == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
						// select all 
						int nCount = ListView_GetItemCount(p->hwndFrom);
						for (int nCur = 0; nCur < nCount; nCur++)
							ListView_SetItemState(p->hwndFrom, nCur, LVIS_SELECTED, LVIS_SELECTED);

						return TRUE;
					}

					if (lpnmk->wVKey == VK_F2 || (lpnmk->wVKey >= 'A' &&  lpnmk->wVKey <= 'Z') || (lpnmk->wVKey >= '1' &&  lpnmk->wVKey <= '9') || lpnmk->wVKey == VK_BACK) {
						HWND hEdit = ListView_EditLabel(p->hwndFrom, ListView_GetSelectionMark(p->hwndFrom));
						if (hEdit && lpnmk->wVKey != VK_F2) {
							if (isupper(lpnmk->wVKey))
								SendMessage(hEdit, WM_CHAR, tolower(lpnmk->wVKey), 0);
							else
								SendMessage(hEdit, WM_CHAR, lpnmk->wVKey, 0);
						}
					}
				}
				return TRUE;

			case NM_DBLCLK:
				{
					NMITEMACTIVATE *pdi = (NMITEMACTIVATE *)lParam;
					if (pdi->iItem >= 0)
						ListView_EditLabel(p->hwndFrom, pdi->iItem);
				}
				return TRUE;

			case NM_CUSTOMDRAW:
				LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
				switch (lplvcd->nmcd.dwDrawStage) {
				case CDDS_PREPAINT:
					SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
					return true;

				case CDDS_ITEMPREPAINT:
					if (lplvcd->nmcd.lItemlParam == 0)
						lplvcd->clrText = RGB(0, 0, 255);

					SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_NEWFONT);
					return true;
				}
				return FALSE;
			}
		}
		else {
			switch (p->code) {
			case PSN_APPLY:
				bApplyChanges(hwndDlg);
				return TRUE;

			case HDN_ITEMCLICK:
				NMHEADER *phdr = (LPNMHEADER)p;
				if (phdr->iButton == 0) { // 0 => Left button 
					HWND hMapUser = GetDlgItem(hwndDlg, IDC_MAP_USER_LIST);
					ListView_SortItems(hMapUser, CompareFunc, phdr->iItem);
					return TRUE;
				}
				return FALSE;
			}
		}
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////
// Member Function : bApplyChanges2
// Type            : Global
// Parameters      : hwndDlg - ?
// Returns         : Returns true if 
// Description     : 
//                   
// References      : -
// Remarks         : -
// Created         : 050429, 29 april 2005
// Developer       : KN   
/////////////////////////////////////////////////////////////////////

BOOL bApplyChanges2(HWND hwndDlg)
{
	if (IsDlgButtonChecked(hwndDlg, IDC_FC_PROMPT) == BST_CHECKED)
		enRenameAction = eDAPromptUser;
	else if (IsDlgButtonChecked(hwndDlg, IDC_FC_RENAME) == BST_CHECKED)
		enRenameAction = eDAAutomatic;
	else if (IsDlgButtonChecked(hwndDlg, IDC_FC_NOTHING) == BST_CHECKED)
		enRenameAction = eDANothing;

	if (IsDlgButtonChecked(hwndDlg, IDC_FD_PROMPT) == BST_CHECKED)
		enDeleteAction = eDAPromptUser;
	else if (IsDlgButtonChecked(hwndDlg, IDC_FD_DELETE) == BST_CHECKED)
		enDeleteAction = eDAAutomatic;
	else if (IsDlgButtonChecked(hwndDlg, IDC_FD_NOTHING) == BST_CHECKED)
		enDeleteAction = eDANothing;

	char szTemp[500];
	mir_strcpy(szTemp, "DisableProt_");

	HWND hMapUser = GetDlgItem(hwndDlg, IDC_EXPORT_PROTOS);
	int nCount = ListView_GetItemCount(hMapUser);
	for (int nCur = 0; nCur < nCount; nCur++) {
		LVITEMA sItem = { 0 };
		sItem.iItem = nCur;
		sItem.mask = LVIF_TEXT | LVIF_IMAGE;
		sItem.pszText = &szTemp[12];
		sItem.cchTextMax = (_countof(szTemp) - 15);
		if (::SendMessage(hMapUser, LVM_GETITEMA, 0, (LPARAM)&sItem)) {
			if (sItem.iImage)
				db_unset(NULL, MODULE, szTemp); // default is Enabled !!
			else
				db_set_b(NULL, MODULE, szTemp, 0);
		}
	}
	SaveSettings();
	return TRUE;
}

/////////////////////////////////////////////////////////////////////
// Member Function : DlgProcMsgExportOpts2
// Type            : Global
// Parameters      : hwndDlg - ?
//                   msg     - ?
//                   wParam  - ?
//                   lParam  - ?
// Returns         : static BOOL CALLBACK
// Description     : 
//                   
// References      : -
// Remarks         : -
// Created         : 040205, 05 februar 2004
// Developer       : KN   
/////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK DlgProcMsgExportOpts2(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static BOOL bWindowTextSet = FALSE;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		bWindowTextSet = FALSE;

		switch (enRenameAction) {
		case eDAPromptUser:
			CheckDlgButton(hwndDlg, IDC_FC_PROMPT, BST_CHECKED);
			break;
		case eDAAutomatic:
			CheckDlgButton(hwndDlg, IDC_FC_RENAME, BST_CHECKED);
			break;
		case eDANothing:
			CheckDlgButton(hwndDlg, IDC_FC_NOTHING, BST_CHECKED);
			break;
		}
		switch (enDeleteAction) {
		case eDAPromptUser:
			CheckDlgButton(hwndDlg, IDC_FD_PROMPT, BST_CHECKED);
			break;
		case eDAAutomatic:
			CheckDlgButton(hwndDlg, IDC_FD_DELETE, BST_CHECKED);
			break;
		case eDANothing:
			CheckDlgButton(hwndDlg, IDC_FD_NOTHING, BST_CHECKED);
			break;
		}

		RECT rListSize;
		{
			HWND hMapUser = GetDlgItem(hwndDlg, IDC_EXPORT_PROTOS);
			int nColumnWidth = 100;
			if (GetWindowRect(hMapUser, &rListSize)) {
				nColumnWidth = (rListSize.right - rListSize.left - GetSystemMetrics(SM_CXVSCROLL) - 5);
				if (nColumnWidth < 10)
					nColumnWidth = 10;
			}

			// header setup !!
			LVCOLUMN cCol = { 0 };
			cCol.mask = LVCF_TEXT | LVCF_WIDTH;
			cCol.cx = nColumnWidth;
			cCol.pszText = TranslateT("Export Protocols");
			ListView_InsertColumn(hMapUser, 0, &cCol);

			HIMAGELIST hIml = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR4 | ILC_MASK, 2, 2);
			ImageList_AddIcon(hIml, Skin_LoadIcon(SKINICON_OTHER_NOTICK));
			ImageList_AddIcon(hIml, Skin_LoadIcon(SKINICON_OTHER_TICK));
			ListView_SetImageList(hMapUser, hIml, LVSIL_SMALL);

			PROTOACCOUNT **proto;
			int nCount;
			Proto_EnumAccounts(&nCount, &proto);

			LVITEMA sItem = { 0 };
			sItem.mask = LVIF_TEXT | LVIF_IMAGE;
			char szTemp[500];

			for (int i = 0; i < nCount; i++) {
				mir_snprintf(szTemp, "DisableProt_%s", proto[i]->szModuleName);
				sItem.pszText = proto[i]->szModuleName;
				sItem.iImage = db_get_b(NULL, MODULE, szTemp, 1);
				::SendMessage(hMapUser, LVM_INSERTITEMA, 0, (LPARAM)&sItem);
				sItem.iItem++;
			}
		}
		bWindowTextSet = TRUE;
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_FC_PROMPT:
		case IDC_FC_RENAME:
		case IDC_FC_NOTHING:
		case IDC_FD_PROMPT:
		case IDC_FD_DELETE:
		case IDC_FD_NOTHING:
			if (!bWindowTextSet)
				return TRUE;

			if (HIWORD(wParam) == BN_CLICKED) {
				bUnaplyedChanges = TRUE;
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
			return TRUE;
	
		case IDC_DEBUG_INFO:
			ShowDebugInfo();
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		NMHDR *p = ((LPNMHDR)lParam);
		if (p->idFrom == IDC_EXPORT_PROTOS) {
			if (p->code == NM_CLICK) {
				LVHITTESTINFO hti;
				hti.pt = ((NMLISTVIEW*)lParam)->ptAction;
				ListView_SubItemHitTest(p->hwndFrom, &hti);
				if (hti.flags != LVHT_ONITEMICON)
					break;

				LVITEM lvi;
				lvi.mask = LVIF_IMAGE;
				lvi.iItem = hti.iItem;
				lvi.iSubItem = 0;
				ListView_GetItem(p->hwndFrom, &lvi);
				lvi.iImage ^= 1;
				ListView_SetItem(p->hwndFrom, &lvi);
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			}
			break;
		}

		switch (p->code) {
		case PSN_APPLY:
			bApplyChanges2(hwndDlg);
			return TRUE;

		case HDN_ITEMCLICK:
			return FALSE;
		}
		break;
	}

	return FALSE;
}


/////////////////////////////////////////////////////////////////////
// Member Function : OptionsInitialize
// Type            : Global
// Parameters      : wParam - ?
//                   lParam - ?
// Returns         : int
// Description     : Called when the user openes the options dialog
//                   I need to add my options page.
// References      : -
// Remarks         : -
// Created         : 020422, 22 April 2002
// Developer       : KN   
/////////////////////////////////////////////////////////////////////

int OptionsInitialize(WPARAM wParam, LPARAM /*lParam*/)
{
	bUnaplyedChanges = FALSE;

	OPTIONSDIALOGPAGE odp = { 0 };
	odp.position = 100000000;
	odp.hInstance = hInstance;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_MSGEXPORT);
	odp.flags = ODPF_BOLDGROUPS | ODPF_UNICODE;
	odp.szTitle.w = LPGENW("Message export");
	odp.szGroup.w = LPGENW("History");
	odp.szTab.w = LPGENW("General");
	odp.pfnDlgProc = DlgProcMsgExportOpts;
	Options_AddPage(wParam, &odp);

	odp.position = 100000001;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_MSGEXPORT2);
	odp.szTab.w = LPGENW("Additional");
	odp.pfnDlgProc = DlgProcMsgExportOpts2;
	Options_AddPage(wParam, &odp);
	return 0;
}
