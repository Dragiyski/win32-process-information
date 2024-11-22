#include <windows.h>
#include <atlstr.h>
#include <commctrl.h>
#include <Psapi.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Psapi.lib")

#pragma warning(disable:4996)

typedef struct tagTypeInfo {
	DWORD Type;
} TYPEINFO;

typedef struct tagModuleInfo
{
	DWORD Type;
	CString BaseName;
	CString FullPath;
	HMODULE hm;
	MODULEINFO *minfo;
	DWORD Error;
} MODINFO;

typedef struct tagProcessInfo
{
	DWORD Type;
	CString BaseName;
	CString FullPath;
	DWORD pid;
	MODINFO *mods;
	UINT mods_Count;
	DWORD Error;
} PROCINFO;

typedef struct tagModProcResult {
	DWORD Type;
	CString BaseName;
	DWORD pid;
	DWORD Error;
} MODPROCRESULT;

typedef struct tagModResult
{
	DWORD Type;
	CString BaseName;
	CString FullPath;
	HMODULE hm;
	MODULEINFO *minfo;
	DWORD Error;
	MODPROCRESULT *loadedBy;
	UINT loadedByCount;
} MODRESULT;

PROCINFO *Processes;
MODRESULT *resultInfo;
DWORD ProcessCount;
DWORD ThreadCount;
DWORD HandleCount;
SYSTEMTIME SnapshotTime;

int Dll_Index;
int Exe_Index;

INT_PTR CALLBACK SearchDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

void DisplayError(DWORD ErrorCode, CString Title="Error", UINT Style=MB_OK|MB_ICONERROR)
{
	wchar_t ErrorBuffer[1024];
	int StoredChars = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, ErrorCode, 0, ErrorBuffer, 1024, NULL);
	if(StoredChars==0)
	{
		CString ErrorStr;
		ErrorStr.Format(TEXT("Error: %i"), ErrorCode);
		MessageBox(NULL, ErrorStr, Title, Style);
		return;
	}
	MessageBox(NULL, ErrorBuffer, Title, Style);
	return;
}

void enumProcInfo() {
	PERFORMANCE_INFORMATION pi; DWORD br;
	pi.cb = sizeof(pi);
	if(!GetPerformanceInfo(&pi, pi.cb)) {
		DisplayError(GetLastError(), "GetPerformanceInfo() Error");
		return;
	}
	DWORD *arrPID = new DWORD[pi.ProcessCount];
	if(!EnumProcesses(arrPID, (pi.ProcessCount)*sizeof(DWORD), &br)) {
		DisplayError(GetLastError(), "EnumProcesses() Error");
		return;
	}
	ProcessCount = pi.ProcessCount;
	ThreadCount = pi.ThreadCount;
	HandleCount = pi.HandleCount;
	Processes = new PROCINFO[pi.ProcessCount];
	GetSystemTime(&SnapshotTime);
	HANDLE hLookupProcess=NULL; TCHAR* NameBuffer;HMODULE* ModHandle;MODULEINFO *modInfo;
	for(DWORD i=0; i<pi.ProcessCount; i++) {
		Processes[i].Type = 1;
		Processes[i].pid = arrPID[i];
		hLookupProcess=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, arrPID[i]);
		if(hLookupProcess==NULL||hLookupProcess==INVALID_HANDLE_VALUE) {
			Processes[i].Error = GetLastError();
			Processes[i].BaseName = "";
			Processes[i].FullPath = "";
			Processes[i].mods = NULL;
			Processes[i].mods_Count = 0;
			continue;
		}
		NameBuffer=new TCHAR[MAX_PATH];
		if(GetModuleBaseName(hLookupProcess, NULL, NameBuffer, MAX_PATH)==0) {
			Processes[i].Error = GetLastError();
			Processes[i].BaseName = "";
			Processes[i].FullPath = "";
			Processes[i].mods = NULL;
			Processes[i].mods_Count = 0;
			delete[] NameBuffer;
			continue;
		}
		Processes[i].BaseName = NameBuffer;
		NameBuffer=new TCHAR[MAX_PATH];
		if(GetModuleFileNameEx(hLookupProcess, NULL, NameBuffer, MAX_PATH)==0) {
			Processes[i].Error = GetLastError();
			Processes[i].FullPath = "";
			Processes[i].mods = NULL;
			Processes[i].mods_Count = 0;
			delete[] NameBuffer;
			continue;
		}
		Processes[i].FullPath = NameBuffer;
		br = 0;
		ModHandle = new HMODULE;
		if(!EnumProcessModules(hLookupProcess, ModHandle, sizeof(HMODULE), &br)) {
			Processes[i].mods = NULL;
			Processes[i].mods_Count = 0;
			Processes[i].Error = GetLastError();
			delete ModHandle;
			continue;
		}
		Processes[i].mods_Count = br/sizeof(HMODULE);
		delete ModHandle;
		ModHandle = new HMODULE[Processes[i].mods_Count];
		if(!EnumProcessModules(hLookupProcess, ModHandle, br, &br)) {
			Processes[i].mods = NULL;
			Processes[i].mods_Count = 0;
			Processes[i].Error = GetLastError();
			delete[] ModHandle;
		}
		Processes[i].mods = new MODINFO[Processes[i].mods_Count];
		for(UINT j=0; j<Processes[i].mods_Count; j++) {
			Processes[i].mods[j].Type = 2;
			Processes[i].mods[j].hm = ModHandle[j];
			NameBuffer = new TCHAR[MAX_PATH];
			if(GetModuleBaseName(hLookupProcess, ModHandle[j], NameBuffer, MAX_PATH)==0) {
				Processes[i].mods[j].BaseName = "";
				Processes[i].mods[j].FullPath = "";
				Processes[i].mods[j].minfo = NULL;
				Processes[i].mods[j].Error = GetLastError();
				delete[] ModHandle;
				delete[] NameBuffer;
				continue;
			}
			Processes[i].mods[j].BaseName = NameBuffer;
			NameBuffer = new TCHAR[MAX_PATH];
			if(GetModuleFileNameEx(hLookupProcess, ModHandle[j], NameBuffer, MAX_PATH)==0) {
				Processes[i].mods[j].FullPath = "";
				Processes[i].mods[j].minfo = NULL;
				Processes[i].mods[j].Error = GetLastError();
				delete[] ModHandle;
				delete[] NameBuffer;
				continue;
			}
			Processes[i].mods[j].FullPath = NameBuffer;
			modInfo = new MODULEINFO;
			if(!GetModuleInformation(hLookupProcess, ModHandle[j], modInfo, sizeof(MODULEINFO))) {
				Processes[i].mods[j].minfo = NULL;
				Processes[i].mods[j].Error = GetLastError();
				delete modInfo;
				delete[] ModHandle;
				continue;
			}
			Processes[i].mods[j].minfo = modInfo;
		}
		delete[] ModHandle;
	}
}

void DisplayAllProcesses(HWND hTree) {
	if(Processes==NULL||ProcessCount==0) {
		return;
	}
	TVINSERTSTRUCT tvi; TCHAR *InputBuffer; CString FormatBuffer;HTREEITEM ProcessItem;
	memset(&tvi, 0, sizeof(TVINSERTSTRUCT));
	for(DWORD i=0; i<ProcessCount; i++) {
		tvi.hParent = NULL;
		tvi.hInsertAfter = TVI_SORT;
		tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvi.item.iImage = Exe_Index;
		tvi.item.iSelectedImage = Exe_Index;
		tvi.item.lParam = (LPARAM)&Processes[i];
		if(Processes[i].BaseName.IsEmpty()) {
			FormatBuffer.Format(TEXT(" *** PID(%I32i); Error getting process information, code: %I32i"), Processes[i].pid, Processes[i].Error);
			InputBuffer = new TCHAR[FormatBuffer.GetLength()+1];
			wcscpy(InputBuffer, FormatBuffer);
		} else {
			InputBuffer = new TCHAR[Processes[i].BaseName.GetLength()+1];
			wcscpy(InputBuffer, Processes[i].BaseName);
		}
		tvi.item.pszText = InputBuffer;
		ProcessItem = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tvi);
		delete[] InputBuffer;
		if(Processes[i].mods_Count>0&&Processes[i].mods!=NULL&&ProcessItem!=NULL) {
			for(UINT j=0; j<Processes[i].mods_Count; j++) {
				if(wcscmp(Processes[i].BaseName, Processes[i].mods[j].BaseName)==0) {
					continue;
				}
				memset(&tvi, 0, sizeof(TVINSERTSTRUCT));
				tvi.hParent = ProcessItem;
				tvi.hInsertAfter = TVI_SORT;
				tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
				tvi.item.iImage = Dll_Index;
				tvi.item.iSelectedImage = Dll_Index;
				tvi.item.lParam = (LPARAM)&Processes[i].mods[j];
				if(Processes[i].mods[j].BaseName.IsEmpty()) {
					FormatBuffer.Format(TEXT(" *** HMODULE(%I64i); Error getting module information, code %I32i"), Processes[i].mods[j].hm, Processes[i].mods[j].Error);
					InputBuffer = new TCHAR[FormatBuffer.GetLength()+1];
					wcscpy(InputBuffer, FormatBuffer);
				} else {
					InputBuffer = new TCHAR[Processes[i].mods[j].BaseName.GetLength()+1];
					wcscpy(InputBuffer, Processes[i].mods[j].BaseName);
				}
				tvi.item.pszText = InputBuffer;
				SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tvi);
				delete[] InputBuffer;
			}
		}
	}
}

void UpdateModuleInformation(HWND hWnd, MODINFO data) {
	CString ntos;
	if(data.minfo==NULL) {
		ntos.Format(TEXT(" *** Error getting information, code: %I32i"), data.Error);
	} else {
		ntos.Format(TEXT("0x%.016X"), (INT_PTR)data.minfo->lpBaseOfDll);
	}
	SetWindowText(GetDlgItem(hWnd, IDC_MOD_BASE), ntos);
	if(data.minfo==NULL) {
		ntos.Format(TEXT(" *** Error getting information, code: %I32i"), data.Error);
	} else {
		ntos.Format(TEXT("0x%.016X"), (INT_PTR)data.minfo->EntryPoint);
	}
	SetWindowText(GetDlgItem(hWnd, IDC_MOD_ENTRY), ntos);
	if(data.minfo==NULL) {
		ntos.Format(TEXT(" *** Error getting information, code: %I32i"), data.Error);
	} else {
		if(data.minfo->SizeOfImage<1024) {
			ntos.Format(TEXT("%I32i Bytes"), data.minfo->SizeOfImage);
		} else {
			if(data.minfo->SizeOfImage<1048576) {
				ntos.Format(TEXT("%.3g KB"), (float)data.minfo->SizeOfImage/1024.0f);
			} else {
				if(data.minfo->SizeOfImage<1073741824) {
					ntos.Format(TEXT("%.3g MB"), (float)data.minfo->SizeOfImage/1048576.0f);
				} else {
					ntos.Format(TEXT("%.3g GB"), (float)data.minfo->SizeOfImage/1073741824.0f);
				}
			}
		}
	}
	SetWindowText(GetDlgItem(hWnd, IDC_MOD_SIZE), ntos);
	SetWindowText(GetDlgItem(hWnd, IDC_MOD_PATH), data.FullPath);
}

void DisplayPerformanceStatistics(HWND hWnd) { //This function may be called without clearing
	CString ntos;
	ntos.Format(TEXT("%I32i"), ProcessCount);
	SetWindowText(GetDlgItem(hWnd, IDC_PI_PROCESSES), ntos);
	ntos.Format(TEXT("%I32i"), ThreadCount);
	SetWindowText(GetDlgItem(hWnd, IDC_PI_THREADS), ntos);
	ntos.Format(TEXT("%I32i"), HandleCount);
	SetWindowText(GetDlgItem(hWnd, IDC_PI_HANDLES), ntos);
	CString m,d,h,n,s,ms;
	if(SnapshotTime.wMonth<10)
		m.Format(TEXT("0%d"), SnapshotTime.wMonth);
	else
		m.Format(TEXT("%d"), SnapshotTime.wMonth);
	if(SnapshotTime.wDay<10)
		d.Format(TEXT("0%d"), SnapshotTime.wDay);
	else
		d.Format(TEXT("%d"), SnapshotTime.wDay);
	if(SnapshotTime.wHour<10)
		h.Format(TEXT("0%d"), SnapshotTime.wHour);
	else
		h.Format(TEXT("%d"), SnapshotTime.wHour);
	if(SnapshotTime.wMinute<10)
		n.Format(TEXT("0%d"), SnapshotTime.wMinute);
	else
		n.Format(TEXT("%d"), SnapshotTime.wMinute);
	if(SnapshotTime.wSecond<10)
		s.Format(TEXT("0%d"), SnapshotTime.wSecond);
	else
		s.Format(TEXT("%d"), SnapshotTime.wSecond);
	if(SnapshotTime.wMilliseconds<100) {
		if(SnapshotTime.wMilliseconds<10) {
			ms.Format(TEXT("00%d"), SnapshotTime.wMilliseconds);
		} else {
			ms.Format(TEXT("0%d"), SnapshotTime.wMilliseconds);
		}
	} else {
		ms.Format(TEXT("%d"), SnapshotTime.wMilliseconds);
	}
	ntos.Format(TEXT("%s/%s/%d %s:%s:%s.%s"), d, m, SnapshotTime.wYear, h, n, s, ms);
	SetWindowText(GetDlgItem(hWnd, IDC_PI_TIME), ntos);
}

BOOL FindProcessModule(MODINFO **outModule, PROCINFO procModule) {
	if(procModule.mods==NULL||procModule.mods_Count==0) {
		return FALSE;
	}
	for(UINT i=0; i<procModule.mods_Count; i++) {
		if(wcscmp(procModule.BaseName, procModule.mods[i].BaseName)==0) {
			*outModule = &procModule.mods[i];
			return TRUE;
		}
	}
	return FALSE;
}

void deleteListHandler(HWND hWnd, LPNMTREEVIEW lParam) {
	UINT i=0;
	if(lParam->itemOld.hItem = TVI_ROOT) {
		if(Processes!=NULL) {
			delete[] Processes;
			Processes=NULL;
			EnableMenuItem(GetMenu(hWnd), ID_SEARCH_ADDRESS, MF_BYCOMMAND|MF_GRAYED|MF_DISABLED);
		}
	}
	if(lParam->itemOld.lParam==NULL) {
		return;
	}
	switch(((TYPEINFO*)(lParam->itemOld.lParam))->Type) {
	case 1:
		if(((PROCINFO*)(lParam->itemOld.lParam))->mods!=NULL) {
			for(i=0; i<((PROCINFO*)(lParam->itemOld.lParam))->mods_Count; i++) {
				if(((PROCINFO*)(lParam->itemOld.lParam))->mods->minfo!=NULL) {
					delete ((PROCINFO*)(lParam->itemOld.lParam))->mods->minfo;
					((PROCINFO*)(lParam->itemOld.lParam))->mods->minfo = NULL;
				}
			}
			delete[] ((PROCINFO*)(lParam->itemOld.lParam))->mods;
			((PROCINFO*)(lParam->itemOld.lParam))->mods=NULL;
		}
		break;
	case 2:
		if(((MODINFO*)(lParam->itemOld.lParam))->minfo!=NULL) {
			delete ((MODINFO*)(lParam->itemOld.lParam))->minfo;
			((MODINFO*)(lParam->itemOld.lParam))->minfo=NULL;
		}
	case 3://No Dynamic Information
		break;
	case 4:
		if(((MODRESULT*)(lParam->itemOld.lParam))->loadedBy!=NULL) {
			delete[] ((MODRESULT*)(lParam->itemOld.lParam))->loadedBy;
			((MODRESULT*)(lParam->itemOld.lParam))->loadedBy = NULL;
		}
		break;
	}
}

void ModViewNotificationHandler(HWND hWnd, LPARAM lParam) {
	LPNMHDR msgHandler = (LPNMHDR)lParam;
	MODINFO *pModuleInformation=NULL;
	switch(msgHandler->code) {
	case TVN_SELCHANGED:
		switch(((TYPEINFO*)(((LPNMTREEVIEW)lParam)->itemNew.lParam))->Type) {
		case 1:
			if(!FindProcessModule(&pModuleInformation, *((PROCINFO*)(((LPNMTREEVIEW)lParam)->itemNew.lParam)))) {
				SetDlgItemText(hWnd, IDC_MOD_BASE, TEXT(""));
				SetDlgItemText(hWnd, IDC_MOD_SIZE, TEXT(""));
				SetDlgItemText(hWnd, IDC_MOD_ENTRY, TEXT(""));
				SetDlgItemText(hWnd, IDC_MOD_PATH, TEXT(""));
				return;
			}
			UpdateModuleInformation(hWnd, *pModuleInformation);
			break;
		case 2:
			UpdateModuleInformation(hWnd, *((MODINFO*)(((LPNMTREEVIEW)lParam)->itemNew.lParam)));
			break;
		default:
			SetDlgItemText(hWnd, IDC_MOD_BASE, TEXT(""));
			SetDlgItemText(hWnd, IDC_MOD_SIZE, TEXT(""));
			SetDlgItemText(hWnd, IDC_MOD_ENTRY, TEXT(""));
			SetDlgItemText(hWnd, IDC_MOD_PATH, TEXT(""));
			return;
		}
		break;
	case TVN_DELETEITEM:
		deleteListHandler(hWnd, (LPNMTREEVIEW)lParam);
		break;
	}
}

void NotificationHandler(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	switch(wParam) {
	case IDC_MODVIEW:
		ModViewNotificationHandler(hWnd, lParam);
		break;
	}
}



BOOL InitTreeViewImageLists(HWND hTree)
{
	HIMAGELIST hil;
	HBITMAP hbmp;
	if((hil = ImageList_Create(20, 20, ILC_COLOR24, 2, 0))==NULL)
	{
		return FALSE;
	}
	HINSTANCE h_Inst = GetModuleHandle(NULL);
	hbmp = LoadBitmap(h_Inst, MAKEINTRESOURCE(IDB_DLL));
	Dll_Index = ImageList_Add(hil, hbmp, (HBITMAP)NULL);
	DeleteObject(hbmp);
	hbmp = LoadBitmap(h_Inst, MAKEINTRESOURCE(IDB_EXE));
	Exe_Index = ImageList_Add(hil, hbmp, (HBITMAP)NULL);
	DeleteObject(hbmp);
	if(ImageList_GetImageCount(hil) < 2)
		return FALSE;
	SendMessage(hTree, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)hil);
	return TRUE;
}

void performAddressSearch(HWND hWnd) {
	UINT_PTR* addr = (UINT_PTR*)DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADDR_SEARCH), hWnd, SearchDlgProc);
	if(addr==NULL) {
		return;
	}
	MODRESULT *results; CString r1=""; UINT resCount=0;
	if(Processes==NULL||ProcessCount==0) {
		return;
	}
	UINT r,m;DWORD p;
	for(p=0; p<ProcessCount; p++) {
		for(m=0; m<Processes[p].mods_Count; m++) {
			if(Processes[p].mods[m].minfo!=NULL) {
				if((*addr>=(UINT_PTR)Processes[p].mods[m].minfo->lpBaseOfDll)&&(*addr<((UINT_PTR)Processes[p].mods[m].minfo->lpBaseOfDll+(UINT_PTR)Processes[p].mods[m].minfo->SizeOfImage))) {
					if(r1.IsEmpty()) {
						r1 = Processes[p].mods[m].BaseName;
						resCount++;
					} else {
						if(wcscmp(r1, Processes[p].mods[m].BaseName)!=0) {
							r1 = Processes[p].mods[m].BaseName;
							resCount++;
						}
					}
				}
			}
		}
	}
	TVINSERTSTRUCT tvs; TCHAR *NameBuffer;
	if(resCount>0) {
		results = new MODRESULT[resCount];
		resultInfo = results;
	} else {
		SendMessage(GetDlgItem(hWnd, IDC_MODVIEW), TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
		memset(&tvs, 0, sizeof(TVINSERTSTRUCT));
		tvs.hInsertAfter = TVI_SORT;
		tvs.hParent = NULL;
		tvs.item.mask = TVIF_TEXT;
		NameBuffer = new TCHAR[sizeof(TEXT("No results found for the specified address!"))+1];
		wcscpy(NameBuffer, TEXT("No results found for the specified address!"));
		tvs.item.pszText = NameBuffer;
		SendDlgItemMessage(hWnd, IDC_MODVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvs);
		return;
	}
	r=0;
	for(p=0; p<ProcessCount; p++) {
		for(m=0; m<Processes[p].mods_Count; m++) {
			if(Processes[p].mods[m].minfo!=NULL) {
				if((*addr>=(UINT_PTR)Processes[p].mods[m].minfo->lpBaseOfDll)&&(*addr<((UINT_PTR)Processes[p].mods[m].minfo->lpBaseOfDll+(UINT_PTR)Processes[p].mods[m].minfo->SizeOfImage))) {
					if(r1.IsEmpty()) {
						r1 = Processes[p].mods[m].BaseName;
						NameBuffer = new TCHAR[Processes[p].mods[m].BaseName.GetLength()+1];
						wcscpy(NameBuffer, Processes[p].mods[m].BaseName);
						results[r].BaseName = NameBuffer;
						NameBuffer = new TCHAR[Processes[p].mods[m].FullPath.GetLength()+1];
						wcscpy(NameBuffer, Processes[p].mods[m].FullPath);
						results[r].FullPath = NameBuffer;
						results[r].hm = Processes[p].mods[m].hm;
						results[r].minfo = new MODULEINFO;
						memcpy(results[r].minfo, Processes[p].mods[m].minfo, sizeof(MODULEINFO));
						results[r].Type = 4;
						results[r].loadedByCount=0;
						r++;
					} else {
						if(wcscmp(r1, Processes[p].mods[m].BaseName)!=0) {
							r1 = Processes[p].mods[m].BaseName;
							NameBuffer = new TCHAR[Processes[p].mods[m].BaseName.GetLength()+1];
							wcscpy(NameBuffer, Processes[p].mods[m].BaseName);
							results[r].BaseName = NameBuffer;
							NameBuffer = new TCHAR[Processes[p].mods[m].FullPath.GetLength()+1];
							wcscpy(NameBuffer, Processes[p].mods[m].FullPath);
							results[r].FullPath = NameBuffer;
							results[r].hm = Processes[p].mods[m].hm;
							results[r].minfo = new MODULEINFO;
							memcpy(results[r].minfo, Processes[p].mods[m].minfo, sizeof(MODULEINFO));
							results[r].Type = 4;
							results[r].loadedByCount=0;
							r++;
						}
					}
				}
			}
		}
	}
	for(r=0; r<resCount; r++) {
		for(p=0; p<ProcessCount; p++) {
			for(m=0; m<Processes[p].mods_Count; m++) {
				if(wcscmp(Processes[p].mods[m].BaseName, results[r].BaseName)==0) {
					results[r].loadedByCount++;
					break;
				}
			}
		}
		results[r].loadedBy = new MODPROCRESULT[results[r].loadedByCount];
	}
	UINT lb=0;
	for(r=0; r<resCount; r++) {
		lb=0;
		for(p=0; p<ProcessCount; p++) {
			for(m=0; m<Processes[p].mods_Count; m++) {
				if(wcscmp(Processes[p].mods[m].BaseName, results[r].BaseName)==0) {
					NameBuffer = new TCHAR[Processes[p].BaseName.GetLength()+1];
					wcscpy(NameBuffer, Processes[p].BaseName);
					results[r].loadedBy[lb].BaseName = NameBuffer;
					results[r].loadedBy[lb].pid = Processes[p].pid;
					results[r].loadedBy[lb].Type=3;
					results[r].loadedBy[lb].Error=0;
					lb++;
				}
			}
		}
	}
	HTREEITEM resItem;
	SendMessage(GetDlgItem(hWnd, IDC_MODVIEW), TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
	for(r=0; r<resCount; r++) {
		memset(&tvs, 0, sizeof(TVINSERTSTRUCT));
		tvs.hParent=NULL;
		tvs.hInsertAfter = TVI_SORT;
		tvs.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvs.item.iImage = Dll_Index;
		tvs.item.iSelectedImage = Dll_Index;
		tvs.item.lParam = (LPARAM)&results[r];
		NameBuffer = new TCHAR[results[r].BaseName.GetLength()+1];
		wcscpy(NameBuffer, results[r].BaseName);
		tvs.item.pszText = NameBuffer;
		resItem = (HTREEITEM)SendDlgItemMessage(hWnd, IDC_MODVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvs);
		for(m=0; m<results[r].loadedByCount; m++) {
			memset(&tvs, 0, sizeof(TVINSERTSTRUCT));
			tvs.hParent = resItem;
			tvs.hInsertAfter = TVI_SORT;
			tvs.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
			tvs.item.iImage = Exe_Index;
			tvs.item.iSelectedImage = Exe_Index;
			tvs.item.lParam = (LPARAM)&results[r].loadedBy[m];
			NameBuffer = new TCHAR[results[r].loadedBy[m].BaseName.GetLength()+1];
			wcscpy(NameBuffer, results[r].loadedBy[m].BaseName);
			tvs.item.pszText = NameBuffer;
			SendDlgItemMessage(hWnd, IDC_MODVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvs);
		}
	}
}

INT_PTR CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch(Msg) {
	case WM_DESTROY:
	case WM_CLOSE:
	case WM_QUIT:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		InitTreeViewImageLists(GetDlgItem(hWnd, IDC_MODVIEW));
		enumProcInfo();
		DisplayAllProcesses(GetDlgItem(hWnd, IDC_MODVIEW));
		DisplayPerformanceStatistics(hWnd);
		EnableMenuItem(GetMenu(hWnd), ID_SEARCH_ADDRESS, MF_BYCOMMAND|MF_ENABLED);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case ID_DISPLAY_REFRESH:
			SendMessage(GetDlgItem(hWnd, IDC_MODVIEW), TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
			enumProcInfo();
			DisplayAllProcesses(GetDlgItem(hWnd, IDC_MODVIEW));
			DisplayPerformanceStatistics(hWnd);
			EnableMenuItem(GetMenu(hWnd), ID_SEARCH_ADDRESS, MF_BYCOMMAND|MF_ENABLED);
			break;
		case ID_SEARCH_ADDRESS:
			performAddressSearch(hWnd);
			break;
		case IDC_EXIT:
			EndDialog(hWnd, 0);
			break;
		}
		break;
	case WM_NOTIFY:
		NotificationHandler(hWnd, wParam, lParam);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

const TCHAR chexStr[] = TEXT("Hexadecimal number");
const TCHAR cdecStr[] = TEXT("Decimal number");
void InitSearchDialog(HWND hWnd) {
	COMBOBOXEXITEM cbei;LRESULT dbg=0;
	TCHAR* hexStr = new TCHAR[wcslen(chexStr)+1];
	TCHAR* decStr = new TCHAR[wcslen(cdecStr)+1];
	wcscpy(hexStr, chexStr);wcscpy(decStr, cdecStr);
	memset(&cbei, 0, sizeof(COMBOBOXEXITEM));
	cbei.mask = CBEIF_TEXT;
	cbei.iItem = 0;
	cbei.pszText = decStr;
	dbg = SendDlgItemMessage(hWnd, IDC_NUMTYPE, CBEM_INSERTITEM, 0, (LPARAM)&cbei);
	memset(&cbei, 0, sizeof(COMBOBOXEXITEM));
	cbei.mask = CBEIF_TEXT;
	cbei.iItem = 1;
	cbei.pszText = hexStr;
	dbg = SendDlgItemMessage(hWnd, IDC_NUMTYPE, CBEM_INSERTITEM, 0, (LPARAM)&cbei);
	HWND cbox = (HWND)SendDlgItemMessage(hWnd, IDC_NUMTYPE, CBEM_GETCOMBOCONTROL, 0, 0);
	SendMessage(cbox, CB_SETCURSEL, 1, 0);
	SetDlgItemText(hWnd, IDC_PREFIX, TEXT("0x"));
}

void sDlgSelChangeHandler(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	if(LOWORD(wParam)==IDC_NUMTYPE) {
		HWND cbox = (HWND)SendDlgItemMessage(hWnd, IDC_NUMTYPE, CBEM_GETCOMBOCONTROL, 0, 0);
		LRESULT index=CB_ERR;
		index = SendMessage(cbox, CB_GETCURSEL, 0, 0);
		switch(index) {
		case 0:
			SetDlgItemText(hWnd, IDC_PREFIX, TEXT(""));
			break;
		case 1:
			SetDlgItemText(hWnd, IDC_PREFIX, TEXT("0x"));
			break;
		}
	}
}

bool aToHex(UINT_PTR* out, const TCHAR* data) {
	int sz = (int)wcslen(data); UINT_PTR retData=0; UINT_PTR ibuf;
	if(sz==0) {
		return false;
	}
	for(int i=sz-1; i>=0; i--) {
		if(data[i]<0x30) {
			return false;
		} else {
			if(data[i]<=0x39) {
				ibuf = ((UINT_PTR)data[i])-0x30;
			} else {
				if(data[i]<0x41) {
					return false;
				} else {
					if(data[i]<=0x46) {
						ibuf = ((UINT_PTR)data[i])-0x37;
					} else {
						if(data[i]<0x61) {
							return false;
						} else {
							if(data[i]<=0x66) {
								ibuf = ((UINT_PTR)data[i])-0x57;
							} else {
								return false;
							}
						}
					}
				}
			}
		}
		retData |= ibuf<<(((sz-1)-i)*4);
	}
	*out = retData;
	return true;
}

bool aToDec(UINT_PTR* out, const TCHAR* data) {
	if(wcslen(data)==0) {
		return false;
	}
	UINT_PTR value;
	value = _wtoi(data);
	if(errno!=0) {
		return false;
	}
	*out = value;
	return true;
}

void sendSearchToParent(HWND hWnd) {
	HWND ntcb = (HWND)SendDlgItemMessage(hWnd, IDC_NUMTYPE, CBEM_GETCOMBOCONTROL, 0, 0);
	LRESULT index = SendMessage(ntcb, CB_GETCURSEL, 0, 0);
	if(index==CB_ERR) {
		MessageBox(hWnd, TEXT("Bad number type!"), TEXT("User error"), MB_OK|MB_ICONERROR);
		return;
	}
	UINT_PTR* addr = new UINT_PTR;
	TCHAR* txt; int txtl = GetWindowTextLength(GetDlgItem(hWnd, IDC_ADDRESS));
	txt = new TCHAR[txtl+1]; GetDlgItemText(hWnd, IDC_ADDRESS, txt, txtl+1);
	switch(index) {
	case 0:
		if(!aToDec(addr, txt)) {
			MessageBox(hWnd, TEXT("Invalid address!"), TEXT("User error"), MB_OK|MB_ICONERROR);
			delete[] txt;
			delete[] addr;
			return;
		}
		delete[] txt;
		EndDialog(hWnd, (INT_PTR)addr);
		break;
	case 1:
		if(!aToHex(addr, txt)) {
			MessageBox(hWnd, TEXT("Invalid address!"), TEXT("User error"), MB_OK|MB_ICONERROR);
			delete[] txt;
			delete[] addr;
			return;
		}
		delete[] txt;
		EndDialog(hWnd, (INT_PTR)addr);
		break;
	default:
		delete[] txt;
		delete[] addr;
	}
}

INT_PTR CALLBACK SearchDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch(Msg) {
	case WM_DESTROY:
	case WM_CLOSE:
	case WM_QUIT:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		InitSearchDialog(hWnd);
		break;
	case WM_COMMAND:
		switch(HIWORD(wParam)) {
		case CBN_SELCHANGE:
			sDlgSelChangeHandler(hWnd, wParam, lParam);
			break;
		case BN_CLICKED:
			switch(LOWORD(wParam)) {
			case IDCANCEL:
				EndDialog(hWnd, 0);
				break;
			case IDOK:
				sendSearchToParent(hWnd);
				break;
			}
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	InitCommonControls();
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) 
	{
		DisplayError(GetLastError(), "Fatal Error");
		return 0;
	}
	LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)0, 0);
	if(GetLastError() != ERROR_SUCCESS)
	{
		DisplayError(GetLastError(), "Security Error");
		MessageBox(NULL, TEXT("\"Process Information\" was unable to get \'debug\' privilege.\nThis is because either your account is limited, or it is not in group allowed to debug operating system objects.\nWhile program would still run, it will be unable to get information about system processes due error code 5: Access Denied!"), TEXT("No privileges information"), MB_OK|MB_ICONINFORMATION);
	}
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, WndProc);
	return 0;
}