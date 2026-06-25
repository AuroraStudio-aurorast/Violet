#include "pch.h"
#include "CAppLang.h"
int QKStrInStr(PCWSTR pszOrg, PCWSTR pszSubStr, int iStartPos)
{
	PCWSTR pszBase = pszOrg + iStartPos - 1;
	PCWSTR pszResult = StrStrIW(pszBase, pszSubStr);
	if (pszResult)
		return (int)(pszResult - pszOrg + 1);
	else
		return 0;
}

__forceinline BOOL ChrCmpW(WCHAR w1, WCHAR wMatch)
{
	return StrCmpNW(&w1, &wMatch, 1);
}


PWSTR StrRStrW(PCWSTR lpSource, PCWSTR lpLast, PCWSTR lpSrch)
{
	PCWSTR lpFound = NULL;

	if (!lpLast)
		lpLast = lpSource + lstrlenW(lpSource);

	if (lpSource && lpSrch && *lpSrch)
	{
		WCHAR   wMatch;
		UINT    uLen;
		PCWSTR  lpStart;

		wMatch = *lpSrch;
		uLen = lstrlenW(lpSrch);
		lpStart = lpSource;
		while (*lpStart && (lpStart < lpLast))
		{
			if (!ChrCmpW(*lpStart, wMatch))
			{
				if (StrCmpNW(lpStart, lpSrch, uLen) == 0)
					lpFound = lpStart;
			}
			lpStart++;
		}
	}
	return((PWSTR)lpFound);
}


HQKINI QKINIParse(PCWSTR pszFile)
{
	HANDLE hFile = CreateFileW(pszFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	DWORD dwErrCode = GetLastError();// 文件已存在==ERROR_ALREADY_EXISTS，文件不存在==0

	QKINIDESC* p = new QKINIDESC;
	ZeroMemory(p, sizeof(QKINIDESC));

	p->hFile = hFile;

	DWORD dwFileSize = GetFileSize(hFile, NULL);
	if (dwErrCode == 0 || (!dwFileSize))
	{
		p->iType = QKINI_NEWFILE;
		return p;
	}
	p->iType = QKINI_NORMAL;
	PWSTR pszBuf = (PWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize + sizeof(WCHAR));
	if (!pszBuf)
		goto Fail;
	p->pszContent = pszBuf;

	DWORD dw;
	if (!ReadFile(hFile, pszBuf, dwFileSize, &dw, NULL))
		goto Fail;
	p->dwSize = lstrlenW(pszBuf);

	return p;
Fail:
	CloseHandle(hFile);
	delete p;
	return NULL;
}
void QKINIClose(HQKINI hINI)
{
	if (hINI)
	{
		CloseHandle(hINI->hFile);
		HeapFree(GetProcessHeap(), 0, hINI->pszContent);
		delete hINI;
	}
}
int QKINIReadString(HQKINI hINI, PCWSTR pszSectionName, PCWSTR pszKeyName, PCWSTR pszDefStr, PWSTR pszRetStr, int iMaxBufSize)
{
	if (!hINI)
		goto CopyDefString;
	if (hINI->iType != QKINI_NORMAL)
		goto CopyDefString;

	int iPos;
	PWSTR psz;
	int iTempLen;
	int iLen;
	//////////////////找节名
	psz = new WCHAR[lstrlenW(pszSectionName) + 3];
	lstrcpyW(psz, L"[");
	lstrcatW(psz, pszSectionName);
	lstrcatW(psz, L"]");
	iPos = QKStrInStr(hINI->pszContent, psz, NULL);
	iTempLen = lstrlenW(psz);
	delete[] psz;
	if (iPos)
	{
		//////////////////找键名
		psz = new WCHAR[lstrlenW(pszKeyName) + 4];
		lstrcpyW(psz, L"\r\n");
		lstrcatW(psz, pszKeyName);
		lstrcatW(psz, L"=");
		iPos = QKStrInStr(hINI->pszContent, psz, iPos + iTempLen);
		iTempLen = lstrlenW(psz);
		delete[] psz;
		if (iPos)
		{
			//////////////////找换行
			int iStart = iPos + iTempLen - 1;
			iPos = QKStrInStr(hINI->pszContent, L"\r\n", iStart);
			int iEnd;
			if (iPos)
				iEnd = iPos;
			else
				iEnd = hINI->dwSize;
			iLen = iEnd - iStart;
			if (iMaxBufSize == 0)
				return iLen;
			if (iLen > 1)
			{
				if (iLen > iMaxBufSize)
				{
					lstrcpynW(pszRetStr, hINI->pszContent + iStart, iMaxBufSize);
					return -iLen;
				}
				else
				{
					lstrcpynW(pszRetStr, hINI->pszContent + iStart, iLen);
					return iLen;
				}
			}
		}
	}
CopyDefString:
	if (pszDefStr)
	{
		iLen = lstrlenW(pszDefStr) + 1;
		if (iLen > iMaxBufSize)
		{
			lstrcpynW(pszRetStr, pszDefStr, iMaxBufSize);
			return -iLen;
		}
		else
		{
			lstrcpynW(pszRetStr, pszDefStr, iLen);
			return iLen;
		}
	}
	else
		return 0;
}
void QKINIWriteString(HQKINI hINI, PCWSTR pszSectionName, PCWSTR pszKeyName, PCWSTR pszString)
{
	void* ptemp;
	if (hINI->iType == QKINI_NEWFILE)
	{
		ptemp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR));
		if (!ptemp)
			return;

		hINI->pszContent = (PWSTR)ptemp;
		hINI->dwSize = lstrlenW(hINI->pszContent);
		hINI->iType = QKINI_NORMAL;
	}

	int iPos, iSectionPos, iKeyPos;
	PWSTR pszSection, pszKey;
	int iSectionLen, iKeyLen;
	int iLen;
	//////////////////找节名
	pszSection = new WCHAR[lstrlenW(pszSectionName) + 3];
	lstrcpyW(pszSection, L"[");
	lstrcatW(pszSection, pszSectionName);
	lstrcatW(pszSection, L"]");
	iSectionPos = QKStrInStr(hINI->pszContent, pszSection, NULL);
	iSectionLen = lstrlenW(pszSection);

	if (iSectionPos)
	{
		//////////////////找键名
		pszKey = new WCHAR[lstrlenW(pszKeyName) + 4];
		lstrcpyW(pszKey, L"\r\n");
		lstrcatW(pszKey, pszKeyName);
		lstrcatW(pszKey, L"=");
		iKeyPos = QKStrInStr(hINI->pszContent, pszKey, iSectionPos + iSectionLen);
		iKeyLen = lstrlenW(pszKey);

		if (iKeyPos)
		{
			//////////////////找换行
			int iStart = iKeyPos + iKeyLen - 1;
			iPos = QKStrInStr(hINI->pszContent, L"\r\n", iStart);
			int iEnd;
			if (iPos)
				iEnd = iPos;
			else
				iEnd = hINI->dwSize;
			iLen = iEnd - iStart - 1;
			int iValueLen = lstrlenW(pszString);
			PWSTR pStart;
			if (iLen > 0)// 有值
			{
				if (iLen > iValueLen)
				{
					pStart = hINI->pszContent + iStart;
					memmove(pStart + iValueLen, pStart + iLen, (lstrlenW(pStart + iLen) + 1) * sizeof(WCHAR));// 向前移动

					memcpy(pStart, pszString, lstrlenW(pszString) * sizeof(WCHAR));// 写值
				}
				else if (iLen == iValueLen)
				{
					pStart = hINI->pszContent + iStart;
					memcpy(pStart, pszString, lstrlenW(pszString) * sizeof(WCHAR));// 写值
				}
				else
				{
					ptemp = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, hINI->pszContent,
						(hINI->dwSize + (iValueLen - iLen) + 1 /*结尾NULL*/) * sizeof(WCHAR));
					if (!ptemp)
						return;

					hINI->pszContent = (PWSTR)ptemp;
					pStart = hINI->pszContent + iStart;
					memmove(pStart + iValueLen, pStart + iLen, lstrlenW(pStart + iLen) * sizeof(WCHAR));// 向后移动

					memcpy(pStart, pszString, lstrlenW(pszString) * sizeof(WCHAR));// 写值
				}
			}
			else// 没有值
			{
				ptemp = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, hINI->pszContent,
					(hINI->dwSize + iValueLen + 1 /*结尾NULL*/) * sizeof(WCHAR));
				if (!ptemp)
					return;

				hINI->pszContent = (PWSTR)ptemp;
				pStart = hINI->pszContent + iStart;
				memmove(pStart + iValueLen, pStart, lstrlenW(pStart) * sizeof(WCHAR));// 向后移动

				memcpy(pStart, pszString, lstrlenW(pszString) * sizeof(WCHAR));// 写值
			}
		}
		else// 没有键，添加到节尾部
		{
			iPos = QKStrInStr(hINI->pszContent, L"\r\n[", iSectionPos + iSectionLen);// 找下一节
			int iAddtionLen =
				2 /*换行符*/ +
				lstrlenW(pszKeyName) + 1 /*等号*/ + lstrlenW(pszString);
			ptemp = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, hINI->pszContent,
				(hINI->dwSize + iAddtionLen + 1 /*结尾NULL*/) * sizeof(WCHAR));
			if (!ptemp)
				return;

			hINI->pszContent = (PWSTR)ptemp;
			PWSTR pNewStart;
			if (iPos)
			{
				pNewStart = hINI->pszContent + iPos - 1;
				memmove(pNewStart + iAddtionLen, pNewStart, lstrlenW(pNewStart) * sizeof(WCHAR));// 向后移动
			}
			else
			{
				pNewStart = hINI->pszContent + hINI->dwSize;
			}

			lstrcpyW(pNewStart, L"\r\n");
			lstrcatW(pNewStart, pszKeyName);// 写键名
			lstrcatW(pNewStart, L"=");
			memcpy(pNewStart + lstrlenW(pNewStart), pszString, lstrlenW(pszString) * sizeof(WCHAR));// 写值
		}
	}
	else// 没有节，添加到文件尾部
	{
		ptemp = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, hINI->pszContent, (
			hINI->dwSize +
			2 /*换行符*/ +
			iSectionLen +
			2 /*换行符*/ +
			lstrlenW(pszKeyName) + 1 /*等号*/ + lstrlenW(pszString) +
			1 /*结尾NULL*/
			) * sizeof(WCHAR));
		if (!ptemp)
			return;

		hINI->pszContent = (PWSTR)ptemp;
		lstrcatW(hINI->pszContent, L"\r\n");
		lstrcatW(hINI->pszContent, pszSection);// 写节名
		lstrcatW(hINI->pszContent, L"\r\n");
		lstrcatW(hINI->pszContent, pszKeyName);// 写键名
		lstrcatW(hINI->pszContent, L"=");
		lstrcatW(hINI->pszContent, pszString);// 写值
	}

	hINI->dwSize = lstrlenW(hINI->pszContent);
}
BOOL QKINISave(HQKINI hINI)
{
	DWORD dw;
	SetFilePointer(hINI->hFile, 0, NULL, FILE_BEGIN);
	BOOL b = WriteFile(hINI->hFile, hINI->pszContent, hINI->dwSize * sizeof(WCHAR), &dw, NULL);
	if (b)
		SetEndOfFile(hINI->hFile);
	return b;
}
int QKINIReadInt(HQKINI hINI, PCWSTR pszSectionName, PCWSTR pszKeyName, int iDefValue)
{
	WCHAR pszDef[20];
	wsprintfW(pszDef, L"%d", iDefValue);
	PWSTR pszBuf = QKINIReadString2(hINI, pszSectionName, pszKeyName, pszDef);

	int i;
	i = StrToIntW(pszBuf);
	delete[] pszBuf;
	return i;
}
void QKINIWriteInt(HQKINI hINI, PCWSTR pszSectionName, PCWSTR pszKeyName, int iValue)
{
	WCHAR pszBuf[20];
	wsprintfW(pszBuf, L"%d", iValue);
	QKINIWriteString(hINI, pszSectionName, pszKeyName, pszBuf);
}

PWSTR QKINIReadString2(HQKINI hINI, PCWSTR pszSectionName, PCWSTR pszKeyName, PCWSTR pszDefStr)
{
	if (!hINI)
		goto CopyDefString;
	if (hINI->iType != QKINI_NORMAL)
		goto CopyDefString;

	int iPos;
	PWSTR psz, pszRet;
	int iTempLen;
	int iLen;
	//////////////////找节名
	psz = new WCHAR[lstrlenW(pszSectionName) + 3];
	lstrcpyW(psz, L"[");
	lstrcatW(psz, pszSectionName);
	lstrcatW(psz, L"]");
	iPos = QKStrInStr(hINI->pszContent, psz, NULL);
	iTempLen = lstrlenW(psz);
	delete[] psz;
	if (iPos)
	{
		//////////////////找键名
		psz = new WCHAR[lstrlenW(pszKeyName) + 4];
		lstrcpyW(psz, L"\r\n");
		lstrcatW(psz, pszKeyName);
		lstrcatW(psz, L"=");
		iPos = QKStrInStr(hINI->pszContent, psz, iPos + iTempLen);
		iTempLen = lstrlenW(psz);
		delete[] psz;
		if (iPos)
		{
			//////////////////找换行
			int iStart = iPos + iTempLen - 1;
			iPos = QKStrInStr(hINI->pszContent, L"\r\n", iStart);
			int iEnd;
			if (iPos)
				iEnd = iPos;
			else
				iEnd = hINI->dwSize;
			iLen = iEnd - iStart;

			if (iLen > 1)
			{
				pszRet = new WCHAR[iLen];
				lstrcpynW(pszRet, hINI->pszContent + iStart, iLen);
				return pszRet;
			}
		}
	}
CopyDefString:
	if (pszDefStr)
	{
		iLen = lstrlenW(pszDefStr) + 1;
		pszRet = new WCHAR[iLen];
		lstrcpyW(pszRet, pszDefStr);
		return pszRet;
	}
	else
		return NULL;
}


CAppLang readLang(PCWSTR Path) {
	HQKINI hIni;
	CAppLang idioma;
	hIni = QKINIParse(Path);
	idioma.LANG_ID_INFO_ENGLISH = QKINIReadString2(hIni, L"info", L"english", L"[info_english]");
	idioma.LANG_ID_INFO_OWNLANG = QKINIReadString2(hIni, L"info", L"ownlang", L"[info_ownlang]");
	idioma.LANG_ID_INFO_MACHINE = QKINIReadString2(hIni, L"info", L"machine", L"[info_machine]");
	idioma.LANG_ID_INFO_BY = QKINIReadString2(hIni, L"info", L"by", L"[info_by]");
	idioma.LANG_ID_INFO_WARN = QKINIReadString2(hIni, L"info", L"warn", L"[info_warn]");
	idioma.LANG_ID_INFO_WARNOWN = QKINIReadString2(hIni, L"info", L"warnown", L"[info_warnown]");
	idioma.LANG_ID_GLOBAL_HAVENTDONE = QKINIReadString2(hIni, L"global", L"haventdone", L"[global_havntdone]");
	idioma.LANG_ID_STARTUP_TITLE = QKINIReadString2(hIni, L"startup", L"title", L"[startup_title]");
	idioma.LANG_ID_STARTUP_OPENFILE = QKINIReadString2(hIni, L"startup", L"openfile", L"[startup_openfile]");
	idioma.LANG_ID_STARTUP_OPENFOLDER = QKINIReadString2(hIni, L"startup", L"openfolder", L"[startup_openfolder]");
	idioma.LANG_ID_STARTUP_RECENTLY = QKINIReadString2(hIni, L"startup", L"recently", L"[startup_recently]");
	idioma.LANG_ID_STARTUP_ONLYFORTEST = QKINIReadString2(hIni, L"startup", L"onlyfortest", L"[startup_onlyfortest]");
	idioma.LANG_ID_STARTUP_EVALUATIONCOPY = QKINIReadString2(hIni, L"startup", L"evaluationcopy", L"[startup_evaluationcopy]");
	idioma.LANG_ID_OPENFILE_FILE = QKINIReadString2(hIni, L"openfile", L"file", L"[openfile_file]");
	idioma.LANG_ID_OPENFILE_FOLDER = QKINIReadString2(hIni, L"openfile", L"folder", L"[openfile_folder]");
	idioma.LANG_ID_MAIN_LIST = QKINIReadString2(hIni, L"main", L"list", L"[main_list]");
	idioma.LANG_ID_MAIN_ADD = QKINIReadString2(hIni, L"main", L"add", L"[main_add]");
	idioma.LANG_ID_MAIN_DEL = QKINIReadString2(hIni, L"main", L"del", L"[main_del]");
	idioma.LANG_ID_MAIN_OPENFILE = QKINIReadString2(hIni, L"main", L"openfile", L"[main_openfile]");
	idioma.LANG_ID_MAIN_OPENFOLDER = QKINIReadString2(hIni, L"main", L"openfolder", L"[main_openfolder]");
	idioma.LANG_ID_MAIN_OPENONLINE = QKINIReadString2(hIni, L"main", L"openonline", L"[main_openonline]");
	idioma.LANG_ID_MAIN_SAVECOVER = QKINIReadString2(hIni, L"main", L"savecover", L"[main_savecover]");
	idioma.LANG_ID_MAIN_EMPTYLIST = QKINIReadString2(hIni, L"main", L"emptylist", L"[main_emptylist]");
	idioma.LANG_ID_MAIN_EMPTYLRC = QKINIReadString2(hIni, L"main", L"emptylrc", L"[main_emptylrc]");
	idioma.LANG_ID_LISTMENU_PLAY = QKINIReadString2(hIni, L"listmenu", L"play", L"[listmenu_play]");
	idioma.LANG_ID_LISTMENU_REMOVE = QKINIReadString2(hIni, L"listmenu", L"remove", L"[listmenu_remove]");
	idioma.LANG_ID_LISTMENU_SPEED = QKINIReadString2(hIni, L"listmenu", L"speed", L"[listmenu_speed]");
	idioma.LANG_ID_SETTINGS_TITLE = QKINIReadString2(hIni, L"settings", L"title", L"[settings_title]");
	idioma.LANG_ID_SETTINGS_APPEARANCE = QKINIReadString2(hIni, L"settings", L"appearance", L"[settings_appearance]");
	idioma.LANG_ID_SETTINGS_THEME = QKINIReadString2(hIni, L"settings", L"theme", L"[settings_theme]");
	idioma.LANG_ID_SETTINGS_SYSNORMAL = QKINIReadString2(hIni, L"settings", L"sysnormal", L"[settings_sysnormal]");
	idioma.LANG_ID_SETTINGS_LIGHT = QKINIReadString2(hIni, L"settings", L"light", L"[settings_light]");
	idioma.LANG_ID_SETTINGS_DARK = QKINIReadString2(hIni, L"settings", L"dark", L"[settings_dark]");
	idioma.LANG_ID_SETTINGS_BACK = QKINIReadString2(hIni, L"settings", L"back", L"[settings_back]");
	idioma.LANG_ID_SETTINGS_BACKMATERIAL = QKINIReadString2(hIni, L"settings", L"backmaterial", L"[settings_backmaterial]");
	idioma.LANG_ID_SETTINGS_ACRYLIC = QKINIReadString2(hIni, L"settings", L"acrylic", L"[settings_acrylic]");
	idioma.LANG_ID_SETTINGS_MICA = QKINIReadString2(hIni, L"settings", L"mica", L"[settings_mica]");
	idioma.LANG_ID_SETTINGS_UPDATE = QKINIReadString2(hIni, L"settings", L"update", L"[settings_update]");
	idioma.LANG_ID_SETTINGS_CHECKING = QKINIReadString2(hIni, L"settings", L"checking", L"[settings_checking]");
	idioma.LANG_ID_SETTINGS_NONEW = QKINIReadString2(hIni, L"settings", L"nonew", L"[settings_nownew]");
	idioma.LANG_ID_SETTINGS_NEW = QKINIReadString2(hIni, L"settings", L"new", L"[settings_new]");
	idioma.LANG_ID_SETTINGS_RESTART = QKINIReadString2(hIni, L"settings", L"restart", L"[settings_restart]");
	idioma.LANG_ID_SETTINGS_DOWNLOAD = QKINIReadString2(hIni, L"settings", L"download", L"[settings_download]");
	idioma.LANG_ID_SETTINGS_DOWNLOADPROGRESS = QKINIReadString2(hIni, L"settings", L"downloadprogress", L"[settings_downloadprogress]");
	idioma.LANG_ID_SETTINGS_RESTARTBTN = QKINIReadString2(hIni, L"settings", L"restartbtn", L"[settings_restartbtn]");
	idioma.LANG_ID_SETTINGS_UPDATEBTN = QKINIReadString2(hIni, L"settings", L"updatebtn", L"[settings_updatebtn]");
	idioma.LANG_ID_SETTINGS_CURRENTVER = QKINIReadString2(hIni, L"settings", L"currentver", L"[settings_currentver]");
	idioma.LANG_ID_SETTINGS_NEWVER = QKINIReadString2(hIni, L"settings", L"newver", L"[settings_newver]");
	idioma.LANG_ID_SETTINGS_ABOUT = QKINIReadString2(hIni, L"settings", L"about", L"[settings_about]");
	idioma.LANG_ID_SETTINGS_DETAIL = QKINIReadString2(hIni, L"settings", L"detail", L"[settings_detail]");
	idioma.LANG_ID_SETTINGS_LANGUAGE = QKINIReadString2(hIni, L"settings", L"language", L"[settings_language]");
	idioma.LANG_ID_SETTINGS_APPLY = QKINIReadString2(hIni, L"settings", L"apply", L"[settings_apply]");
	idioma.LANG_ID_PLAYLIST_TITLE = QKINIReadString2(hIni, L"playlist", L"title", L"[playlist_title]");
	idioma.LANG_ID_PLAYLIST_ITEMS = QKINIReadString2(hIni, L"playlist", L"items", L"[playlist_items]");
	idioma.LANG_ID_PLAYLIST_PLAY = QKINIReadString2(hIni, L"playlist", L"play", L"[playlist_play]");
	idioma.LANG_ID_PLAYLIST_ADDFILE = QKINIReadString2(hIni, L"playlist", L"addfile", L"[playlist_addfile]");
	idioma.LANG_ID_PLAYLIST_LOCATE = QKINIReadString2(hIni, L"playlist", L"locate", L"[playlist_locate]");
	idioma.LANG_ID_PLAYLIST_PLAYLIST = QKINIReadString2(hIni, L"playlist", L"playlist", L"[playlist_playlist]");
	idioma.LANG_ID_PLAYLIST_UNKNOWNARTIST = QKINIReadString2(hIni, L"playlist", L"unknownartist", L"[playlist_unknownartist]");
	idioma.LANG_ID_PLAYLIST_UNKNOWNALBUM = QKINIReadString2(hIni, L"playlist", L"unknownalbum", L"[playlist_unknownalbum]");
	idioma.LANG_ID_PLAYLIST_SEARCHLIST = QKINIReadString2(hIni, L"playlist", L"searchlist", L"[playlist_searchlist]");
	idioma.LANG_ID_PLAYLIST_SEARCHSONG = QKINIReadString2(hIni, L"playlist", L"searchsong", L"[playlist_searchsong]");
	idioma.LANG_ID_EFFECT_TITLE = QKINIReadString2(hIni, L"effect", L"title", L"[effect_title]");
	idioma.LANG_ID_PLUGIN_TITLE = QKINIReadString2(hIni, L"plugin", L"title", L"[plugin_title]");
	idioma.LANG_ID_PLUGIN_LIST = QKINIReadString2(hIni, L"plugin", L"list", L"[plugin_list]");
	idioma.LANG_ID_OOBE_CONTINUE = QKINIReadString2(hIni, L"oobe", L"continue", L"[oobe_continue]");
	idioma.LANG_ID_OOBE_WELCOME = QKINIReadString2(hIni, L"oobe", L"welcome", L"[oobe_welcome]");
	idioma.LANG_ID_OOBE_LANG = QKINIReadString2(hIni, L"oobe", L"lang", L"[oobe_lang]");
	return idioma;
}