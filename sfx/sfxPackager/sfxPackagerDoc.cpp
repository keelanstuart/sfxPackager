/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfxPackagerDoc.cpp : implementation of the CSfxPackagerDoc class
//

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "sfxPackager.h"
#endif

#include "sfxPackagerDoc.h"
#include "sfxPackagerView.h"
#include "ChildFrm.h"
#include "MainFrm.h"
#include "PropertiesWnd.h"

#include <propkey.h>

#include "../sfxFlags.h"

#include <vector>
#include <chrono>
#include <ctime>
#include <map>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);
extern bool FLZACreateDirectories(const TCHAR *dir);

#define SFXRESID_ICON		130


bool GetFileVersion(const TCHAR *filename, int &major, int &minor, int &release, int &build, VS_FIXEDFILEINFO *retffi = nullptr);

bool GetFileVersion(const TCHAR *filename, int &major, int &minor, int &release, int &build, VS_FIXEDFILEINFO *retffi)
{
	bool ret = false;

	DWORD  verHandle = 0;
	UINT   size = 0;
	LPBYTE lpBuffer = NULL;
	DWORD  verSize = GetFileVersionInfoSize(filename, &verHandle);

	if (verSize != NULL)
	{
		LPBYTE verData = (LPBYTE)malloc(verSize);

		if (verData)
		{
			if (GetFileVersionInfo(filename, verHandle, verSize, verData))
			{
				if (VerQueryValue(verData, _T("\\"), (LPVOID *)&lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							major = (verInfo->dwFileVersionMS >> 16) & 0xffff;
							minor = (verInfo->dwFileVersionMS >> 0) & 0xffff;
							release = (verInfo->dwFileVersionLS >> 16) & 0xffff;
							build = (verInfo->dwFileVersionLS >> 0) & 0xffff;

							if (retffi)
								memcpy(retffi, verInfo, sizeof(VS_FIXEDFILEINFO));

							ret = true;
						}
					}
				}
			}

			free(verData);
		}
	}

	return ret;
}

wchar_t *FindWStringInMemory(const BYTE *mem, const wchar_t *str, size_t memlen)
{
	if (!mem || !str || !memlen)
		return nullptr;

	size_t ss = wcslen(str);
	if (!ss)
		return nullptr;

	for (size_t i = 0; i < memlen; i++, mem++)
	{
		if (!memcmp(mem, str, ss))
			return (wchar_t *)mem;
	}

	return nullptr;
}

#pragma pack(push, 1)
typedef struct
{
	WORD Reserved1;       // reserved, must be 0
	WORD ResourceType;    // type is 1 for icons
	WORD ImageCount;      // number of icons in structure (1)
} GROUPICON;

typedef struct
{
	BYTE Width;           // icon width
	BYTE Height;          // icon height
	BYTE Colors;          // colors (0 means more than 8 bits per pixel)
	BYTE Reserved2;       // reserved, must be 0
	WORD Planes;          // color planes
	WORD BitsPerPixel;    // bit depth
	DWORD ImageSize;      // size of structure
	WORD ResourceID;      // resource ID
} ICONDATA_RES;

typedef struct
{
	BYTE Width;           // icon width
	BYTE Height;          // icon height
	BYTE Colors;          // colors (0 means more than 8 bits per pixel)
	BYTE Reserved2;       // reserved, must be 0
	WORD Planes;          // color planes
	WORD BitsPerPixel;    // bit depth
	DWORD ImageSize;      // size of structure
	DWORD ImageOffset;      // resource ID
} ICONDATA_FILE;
#pragma pack(pop)

BOOL EnumResLangProc(HMODULE hMod, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIDLanguage, LONG_PTR lParam)
{
	if ((lpszType == RT_VERSION) && (lpszName == MAKEINTRESOURCE(VS_VERSION_INFO)))
	{
		*(WORD *)lParam = wIDLanguage;
		return FALSE;
	}

	return TRUE;
}

bool SetupSfxExecutable(const TCHAR *filename, CSfxPackagerDoc *pDoc, HANDLE &hFile, size_t spanIdx)
{
	bool ret = false;

	TCHAR docpath[MAX_PATH];
	_tcscpy_s(docpath, pDoc->GetPathName());
	PathRemoveFileSpec(docpath);

	HRSRC hsfxres = FindResource(NULL, MAKEINTRESOURCE(IDR_EXE_SFX), _T("EXE"));
	if (hsfxres)
	{
		HGLOBAL hsfxload = LoadResource(NULL, hsfxres);
		if (hsfxload)
		{
			BYTE *pbuf = (BYTE *)LockResource(hsfxload);
			if (pbuf)
			{
				DWORD sfxsize = SizeofResource(NULL, hsfxres);

				TCHAR dir[MAX_PATH], *d = dir;
				_tcscpy_s(dir, MAX_PATH, filename);
				while (d && *(d++)) { if (*d == _T('/')) *d = _T('\\'); }
				PathRemoveFileSpec(dir);
				FLZACreateDirectories(dir);

				hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile != INVALID_HANDLE_VALUE)
				{
					DWORD cw;
					WriteFile(hFile, pbuf, sfxsize, &cw, NULL);

					CloseHandle(hFile);

					ret = (sfxsize == cw);
				}

				FreeResource(hsfxload);
			}
		}
	}

	EnumResourceTypes(NULL, (ENUMRESTYPEPROC)CSfxPackagerDoc::EnumTypesFunc, (LONG_PTR)pDoc);

	HMODULE hmod = LoadLibraryEx(filename, NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
	EnumResourceTypes(hmod, (ENUMRESTYPEPROC)CSfxPackagerDoc::EnumTypesFunc, (LONG_PTR)pDoc);

	if (hmod)
	{
		// Load the existing version info from the sfx .exe and replace it with the metadata our user has given
		BYTE *pverinfo = nullptr;
		DWORD pverinfosz = 0;
		WORD verlang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
		HRSRC hir = FindResource(hmod, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
		if (hir)
		{
			EnumResourceLanguages(hmod, RT_VERSION, MAKEINTRESOURCE(VS_VERSION_INFO), EnumResLangProc, (LONG_PTR)&verlang);

			HGLOBAL hgv = LoadResource(hmod, hir);
			BYTE *pev = (BYTE *)LockResource(hgv);
			pverinfosz = SizeofResource(hmod, hir);

			if (pev && pverinfosz)
			{
				pverinfo = (BYTE *)malloc(pverinfosz);
				if (pverinfo)
				{
					memcpy(pverinfo, pev, pverinfosz);

					// 3 WORDS preceed the string id
					BYTE *pid = (BYTE *)wcsstr((wchar_t *)((BYTE *)pverinfo + (sizeof(WORD) * 3)), L"VS_VERSION_INFO");
					if (pid)
					{
						pid += (sizeof(wchar_t) * 17);

						VS_FIXEDFILEINFO *pffi = (VS_FIXEDFILEINFO *)pid;

						if (pffi->dwSignature == 0xFEEF04BD)
						{
							// get the packager's version...
							TCHAR exename[MAX_PATH];
							_tcscpy_s(exename, theApp.m_pszHelpFilePath);
							PathRemoveExtension(exename);
							PathAddExtension(exename, _T(".exe"));

							// ...and make the installer's FileVersion that version, so it can be tracked back in case of issues
							int major, minor, release, build;

							major = 1; minor = 0; release = 0; build = 0;
							GetFileVersion(exename, major, minor, release, build);
							pffi->dwFileVersionMS = (major << 16) | minor;
							pffi->dwFileVersionLS = (release << 16) | build;

							// Then get the version number from the file the user has said contains it... or use 1.0.0.0 if none was specified
							major = 1; minor = 0; release = 0; build = 0;
							CString vers;
							auto pversion = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::VERSION];
							if (pversion)
								vers = pversion->AsString();
							if (PathFileExists(vers))
							{
								GetFileVersion(vers, major, minor, release, build);
							}
							else
							{
								_stscanf(vers, _T("%d.%d.%d.%d"), &major, &minor, &release, &build);
							}
							pffi->dwProductVersionMS = (major << 16) | minor;
							pffi->dwProductVersionLS = (release << 16) | build;

							wchar_t *pcopyright = FindWStringInMemory(pverinfo, L"<LegalCopyright>", pverinfosz);
							if (pcopyright)
							{
								auto _pcopyright = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::VERSION_COPYRIGHT];
								if (_pcopyright)
								{
									wcsncpy_s(pcopyright, 128, _pcopyright->AsString(), 127);
								}
								else
									*pcopyright = L'\0';
							}

							wchar_t *pfiledesc = FindWStringInMemory(pverinfo, L"<FileDescription>", pverinfosz);
							if (pfiledesc)
							{
								auto _pfiledesc = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::VERSION_DESCRIPTION];
								if (_pfiledesc)
								{
									wcsncpy_s(pfiledesc, 128, _pfiledesc->AsString(), 127);
								}
								else
									*pfiledesc = L'\0';
							}

							wchar_t *pproductname = FindWStringInMemory(pverinfo, L"<ProductName>", pverinfosz);
							if (pproductname)
							{
								auto _pproductname = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::VERSION_PRODUCTNAME];
								if (_pproductname)
								{
									wcsncpy_s(pproductname, 128, _pproductname->AsString(), 127);
								}
								else
									*pproductname = L'\0';
							}
						}
					}
				}
			}

			UnlockResource(hgv);
		}

		HRSRC hii = FindResource(hmod, _T("ICON"), RT_GROUP_ICON);
		HGLOBAL hgi = LoadResource(hmod, hii);
		BYTE *pei = (BYTE *)LockResource(hgi); // existing icon

		GROUPICON *pei_hdr = (GROUPICON *)pei;
		ICONDATA_RES *pei_icon = (ICONDATA_RES *)(pei + sizeof(GROUPICON));

		std::vector<ICONDATA_RES> oid; // original icons data
		oid.reserve(pei_hdr->ImageCount);
		for (WORD k = 0; k < pei_hdr->ImageCount; k++)
			oid.push_back(pei_icon[k]);

		UnlockResource(hgi);

		FreeLibrary(hmod);

		HANDLE hbur = BeginUpdateResource(filename, FALSE);
		BOOL bresult = FALSE;

		if (hbur)
		{
			// if we found some version info, we modified it above -- so update the resource now
			if (pverinfo)
			{
				bresult = UpdateResource(hbur, RT_VERSION, MAKEINTRESOURCE(VS_VERSION_INFO), verlang, pverinfo, pverinfosz);
				free(pverinfo);
				pverinfo = nullptr;
			}

			tstring iconpathraw;
			auto picon = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::ICON_FILE];
			if (picon)
				iconpathraw = picon->AsString();
			if (!iconpathraw.empty())
			{
				TCHAR iconpath[MAX_PATH];

				if (PathIsRelative(iconpathraw.c_str()))
				{
					PathCombine(iconpath, docpath, iconpathraw.c_str());
				}
				else
				{
					_tcscpy_s(iconpath, MAX_PATH, iconpathraw.c_str());
				}

				// Replace the icon if desired
				if (PathFileExists(iconpath))
				{
					HANDLE hicobin = CreateFile(iconpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hicobin)
					{
						DWORD fsz = GetFileSize(hicobin, NULL);
						BYTE *pbin = (BYTE *)malloc(fsz);
						if (pbin)
						{
							DWORD rb;
							if (ReadFile(hicobin, pbin, fsz, &rb, NULL))
							{
								WORD last_id = oid.back().ResourceID;

								GROUPICON *hdr = (GROUPICON *)pbin;

								BYTE *pbin_r = (BYTE *)malloc(sizeof(GROUPICON) + (sizeof(ICONDATA_RES) * hdr->ImageCount));
								if (pbin_r)
								{
									memcpy(pbin_r, pbin, sizeof(GROUPICON));

									WORD hdrsz = sizeof(GROUPICON) + (sizeof(ICONDATA_RES) * hdr->ImageCount);

									ICONDATA_FILE *icon = (ICONDATA_FILE *)(pbin + sizeof(GROUPICON));
									ICONDATA_RES *icon_r = (ICONDATA_RES *)(pbin_r + sizeof(GROUPICON));
									for (WORD i = 0; i < hdr->ImageCount; i++)
									{
										memcpy(&icon_r[i], &icon[i], sizeof(ICONDATA_RES));
										icon_r[i].ResourceID = (i < oid.size()) ? oid[i].ResourceID : ++last_id;
									}

									bresult = UpdateResource(hbur, RT_GROUP_ICON, _T("ICON"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), pbin_r, hdrsz);

									for (size_t i = 0, maxi = hdr->ImageCount; i < maxi; i++)
									{
										bresult = UpdateResource(hbur, RT_ICON, MAKEINTRESOURCE(icon_r[i].ResourceID), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), &pbin[icon[i].ImageOffset], icon_r[i].ImageSize);
									}

									free(pbin_r);
								}
							}

							free(pbin);
						}

						CloseHandle(hicobin);
					}
				}
			}

			tstring imagepathraw;
			auto pimage = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::IMAGE_FILE];
			if (pimage)
				imagepathraw = pimage->AsString();
			if (!imagepathraw.empty())
			{
				TCHAR imgpath[MAX_PATH];

				if (PathIsRelative(imagepathraw.c_str()))
				{
					PathCombine(imgpath, docpath, imagepathraw.c_str());
				}
				else
				{
					_tcscpy_s(imgpath, imagepathraw.c_str());
				}

				if (PathFileExists(imgpath))
				{
					HANDLE himgbin = CreateFile(imgpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (himgbin)
					{
						DWORD fsz = GetFileSize(himgbin, NULL);
						BYTE *pbin = (BYTE *)malloc(fsz);
						BITMAPINFOHEADER *pbmphdr = (BITMAPINFOHEADER *)(pbin + sizeof(BITMAPFILEHEADER));
						if (pbin)
						{
							DWORD rb;
							ReadFile(himgbin, pbin, fsz, &rb, NULL);

							if (pbmphdr->biBitCount <= 24)
							{
								bresult = UpdateResource(hbur, RT_BITMAP, _T("PACKAGE"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), pbin + sizeof(BITMAPFILEHEADER), fsz - sizeof(BITMAPFILEHEADER));
							}
							else
							{
								CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);
								pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, _T("WARNING: Image file may not be more than 24bpp!\r\n"));
							}

							free(pbin);
						}

						CloseHandle(himgbin);
					}
				}
			}

			
			auto pcaption = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::CAPTION];
			CString spanstr;
			spanstr.Format(_T(" (part %d)"), spanIdx + 1);
			CString caption;
			caption.Format(_T("%s%s"), pcaption->AsString(), spanIdx ? spanstr : _T(""));
			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_CAPTION"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (void *)((LPCTSTR)caption), (caption.GetLength() + 1) * sizeof(TCHAR));

			auto pdefpath = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::DEFAULT_DESTINATION];
			tstring defpath;
			if (pdefpath)
				defpath = pdefpath->AsString();
			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_DEFAULTPATH"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (void *)((LPCTSTR)defpath.c_str()), DWORD(defpath.length() + 1) * sizeof(TCHAR));

			tstring descraw;
			auto pwelcome = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::WELCOME_MESSAGE];
			if (pwelcome)
				descraw = pwelcome->AsString();

			if (!descraw.empty())
			{
				char *shtml;
				bool created_shtml = false;

				TCHAR welcomepath[MAX_PATH];

				if (PathIsRelative(descraw.c_str()))
				{
					PathCombine(welcomepath, docpath, descraw.c_str());
				}
				else
				{
					_tcscpy_s(welcomepath, descraw.c_str());
				}

				if (PathFileExists(welcomepath))
				{
					HANDLE hhtmlfile = CreateFile(welcomepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hhtmlfile)
					{
						DWORD fsz = GetFileSize(hhtmlfile, NULL);
						shtml = (char *)malloc(fsz + 1);
						if (shtml)
						{
							created_shtml = true;
							DWORD rb;
							ReadFile(hhtmlfile, shtml, fsz, &rb, NULL);
							shtml[fsz] = 0;
						}

						CloseHandle(hhtmlfile);
					}
				}
				else
				{
					LOCAL_TCS2MBCS(descraw.c_str(), shtml);
				}

				bresult = UpdateResource(hbur, RT_HTML, _T("welcome"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (void *)shtml, DWORD(strlen(shtml) * sizeof(char)));

				if (created_shtml)
				{
					free(shtml);
				}
			}

			tstring licraw;
			auto plicense = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::LICENSE_MESSAGE];
			if (plicense)
				licraw = plicense->AsString();

			if (!licraw.empty())
			{
				char *shtml;
				bool created_shtml = false;

				TCHAR licensepath[MAX_PATH];

				if (PathIsRelative(licraw.c_str()))
				{
					PathCombine(licensepath, docpath, licraw.c_str());
				}
				else
				{
					_tcscpy_s(licensepath, licraw.c_str());
				}

				if (PathFileExists(licensepath))
				{
					HANDLE hhtmlfile = CreateFile(licensepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hhtmlfile)
					{
						DWORD fsz = GetFileSize(hhtmlfile, NULL);
						shtml = (char *)malloc(fsz + 1);
						if (shtml)
						{
							created_shtml = true;
							DWORD rb;
							ReadFile(hhtmlfile, shtml, fsz, &rb, NULL);
							shtml[fsz] = 0;
						}

						CloseHandle(hhtmlfile);
					}
				}
				else
				{
					LOCAL_TCS2MBCS(licraw.c_str(), shtml);
				}

				bresult = UpdateResource(hbur, RT_HTML, _T("license"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (void *)shtml, DWORD(strlen(shtml) * sizeof(char)));

				if (created_shtml)
				{
					free(shtml);
				}
			}

			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_SCRIPT_INITIALIZE"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
									 (void *)((LPCTSTR)pDoc->m_Script[CSfxPackagerDoc::EScriptType::INITIALIZE]),
									 (pDoc->m_Script[CSfxPackagerDoc::EScriptType::INITIALIZE].GetLength() + 1) * sizeof(TCHAR));

			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_SCRIPT_PREINSTALL"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
									 (void *)((LPCTSTR)pDoc->m_Script[CSfxPackagerDoc::EScriptType::PREINSTALL]),
									 (pDoc->m_Script[CSfxPackagerDoc::EScriptType::PREINSTALL].GetLength() + 1) * sizeof(TCHAR));

			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_SCRIPT_PREFILE"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
									 (void *)((LPCTSTR)pDoc->m_Script[CSfxPackagerDoc::EScriptType::PREFILE]),
									 (pDoc->m_Script[CSfxPackagerDoc::EScriptType::PREFILE].GetLength() + 1) * sizeof(TCHAR));

			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_SCRIPT_POSTFILE"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
									 (void *)((LPCTSTR)pDoc->m_Script[CSfxPackagerDoc::EScriptType::POSTFILE]),
									 (pDoc->m_Script[CSfxPackagerDoc::EScriptType::POSTFILE].GetLength() + 1) * sizeof(TCHAR));

			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_SCRIPT_POSTINSTALL"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
									 (void *)((LPCTSTR)pDoc->m_Script[CSfxPackagerDoc::EScriptType::POSTINSTALL]),
									 (pDoc->m_Script[CSfxPackagerDoc::EScriptType::POSTINSTALL].GetLength() + 1) * sizeof(TCHAR));

			SFixupResourceData furd;
			ZeroMemory(&furd, sizeof(SFixupResourceData));
			strcpy(furd.m_Ident, SFX_FIXUP_SEARCH_STRING);

			bresult = UpdateResource(hbur, _T("SFX"), _T("SFX_FIXUPDATA"), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), &furd, sizeof(SFixupResourceData));

			bresult = EndUpdateResource(hbur, FALSE);
		}

		if (pverinfo)
		{
			free(pverinfo);
			pverinfo = nullptr;
		}
	}

	hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	LARGE_INTEGER p = {0}, z = {0};
	SetFilePointerEx(hFile, z, &p, FILE_END);

	return (hFile != INVALID_HANDLE_VALUE);
}

bool FixupSfxExecutable(CSfxPackagerDoc *pDoc, const TCHAR *filename, const TCHAR *launchcmd, bool span, UINT32 filecount)
{
	bool bresult = true;

	HANDLE hf = CreateFile(filename, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hf == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER ofs;
	DWORD cb;
	int64_t archive_mode = 0;
	auto parcmode = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::OUTPUT_MODE];
	if (parcmode)
		archive_mode = parcmode->AsInt();

	switch (archive_mode)
	{
		// mode 0 is SFX
		default:
		case 0:
			SetFilePointer(hf, -(int)(sizeof(LONGLONG)), NULL, FILE_END);
			ReadFile(hf, &(ofs.QuadPart), sizeof(LONGLONG), &cb, NULL);
			break;

			// mode 1 is an external archove file
		case 1:
			ofs.LowPart = GetFileSize(hf, (LPDWORD)&ofs.HighPart);
			break;
	}

	BYTE *pdata = (BYTE *)malloc(ofs.QuadPart);
	if (pdata)
	{
		SetFilePointer(hf, 0, NULL, FILE_BEGIN);
		ReadFile(hf, pdata, (DWORD)ofs.QuadPart, &cb, NULL);

		char ss[16];
		strcpy(ss, SFX_FIXUP_SEARCH_STRING);

		// search for our identifier string in the exe file... when we find it, then replace the data ourselves instead of using the resource API
		BYTE *p = pdata;
		bool found = false;
		while (p && ((uint64_t)(p - pdata) < (uint64_t)(ofs.QuadPart - sizeof(SFixupResourceData))))
		{
			p = (BYTE *)memchr((char *)p, ss[0], ofs.QuadPart - (p - pdata));

			if (!p)
			{
				break;
			}
			else if (!strcmp((const char *)p, SFX_FIXUP_SEARCH_STRING))
			{
				found = true;
				break;
			}

			p++;
		}

		if (found)
		{
			SetFilePointer(hf, (LONG)(p - pdata), NULL, FILE_BEGIN);

			SFixupResourceData *furd = (SFixupResourceData *)p;
			_tcscpy_s(furd->m_LaunchCmd, MAX_PATH, launchcmd);

			CString vers;
			auto pversion = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::VERSION];
			if (pversion)
				vers = pversion->AsString();
			if (PathFileExists(vers))
			{
				int major, minor, release, build;
				if (GetFileVersion(vers, major, minor, release, build))
				{
					vers.Format(_T("Version %d.%d.%d.%d"), major, minor, release, build);
				}
			}

			_tcscpy_s(furd->m_VersionID, MAX_PATH, vers);

			UINT32 flags = 0;

			auto pexp = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::ENABLE_EXPLORE_CHECKBOX];
			if (pexp && pexp->AsBool() && !span)
				flags |= SFX_FLAG_EXPLORE;

			if (span)
				flags |= SFX_FLAG_SPAN;

			auto pdstchg = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::ALLOW_DESTINATION_CHANGE];
			if (pdstchg && pdstchg->AsBool())
				flags |= SFX_FLAG_ALLOWDESTCHG;

			auto preqadmin = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::REQUIRE_ADMIN];
			if (preqadmin && preqadmin->AsBool())
				flags |= SFX_FLAG_ADMINONLY;

			auto preqreboot = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::REQUIRE_REBOOT];
			if (preqreboot && preqreboot->AsBool())
				flags |= SFX_FLAG_REBOOTNEEDED;

			auto parcmode = (*(pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::OUTPUT_MODE];
			if (parcmode && (parcmode->AsInt() == 1))
				flags |= SFX_FLAG_EXTERNALARCHIVE;

			furd->m_Flags = flags;
			furd->m_SpaceRequired = pDoc->m_UncompressedSize;

			furd->m_CompressedFileCount = filecount;

			WriteFile(hf, furd, sizeof(SFixupResourceData), &cb, NULL);
		}

		free(pdata);
	}

	CloseHandle(hf);

	return bresult;
}

class CPackagerArchiveHandle : public IArchiveHandle
{
protected:
	IArchiver *m_pArc;
	CSfxPackagerDoc *m_pDoc;

	TCHAR m_BaseFilename[MAX_PATH];
	TCHAR m_CurrentFilename[MAX_PATH];
	HANDLE m_hFile;

	UINT m_spanIdx;
	LARGE_INTEGER m_spanTotalSize;

public:
	CPackagerArchiveHandle(CSfxPackagerDoc *pdoc)
	{
		m_pArc = nullptr;
		m_hFile = INVALID_HANDLE_VALUE;
		m_spanIdx = 0;
		m_pDoc = pdoc;
		m_spanTotalSize.QuadPart = 0;
	}

	virtual ~CPackagerArchiveHandle()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
		{
			// we finalize by storing the file table and writing the starting offset of the archive in the stream
			size_t fc = m_pArc->GetFileCount(IArchiver::IM_WHOLE);

			m_pArc->Finalize();

			CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;

			auto plaunch = (*(m_pDoc->m_Props))[CSfxPackagerDoc::EDOCPROP::LAUNCH_COMMAND];
			// the last file is never spanned
			FixupSfxExecutable(m_pDoc, m_BaseFilename, plaunch ? plaunch->AsString() : _T(""), false, (UINT32)fc);
		}
	}

	void SetArchiver(IArchiver *parc)
	{
		m_pArc = parc;
	}

	virtual HANDLE GetHandle()
	{
		return m_hFile;
	}

	virtual uint64_t GetLength()
	{
		LARGE_INTEGER p;
		GetFileSizeEx(m_hFile, &p);
		return p.QuadPart;
	}

	virtual uint64_t GetOffset()
	{
		LARGE_INTEGER p, z;
		z.QuadPart = 0;
		SetFilePointerEx(m_hFile, z, &p, FILE_CURRENT);
		return p.QuadPart;
	}

	virtual UINT GetSpanCount()
	{
		return (m_spanIdx + 1);
	}

	virtual ULONGLONG GetSpanTotalSize()
	{
		return m_spanTotalSize.QuadPart;
	}

};

class CExtArcHandle : public CPackagerArchiveHandle
{

public:

	CExtArcHandle(const TCHAR *base_filename, CSfxPackagerDoc *pdoc) : CPackagerArchiveHandle(pdoc)
	{
		_tcscpy_s(m_BaseFilename, MAX_PATH, base_filename);
		_tcscpy_s(m_CurrentFilename, MAX_PATH, base_filename);
		PathRenameExtension(m_CurrentFilename, _T(".data"));

		m_hFile = INVALID_HANDLE_VALUE;
		if (!SetupSfxExecutable(m_BaseFilename, m_pDoc, m_hFile, 0))
		{
			CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);
			pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, _T("SFX setup failed; your output exe may be locked or the directory set to read-only.\r\n"));
		}

		ASSERT(m_hFile != INVALID_HANDLE_VALUE);

		CloseHandle(m_hFile);

		m_hFile = CreateFile(m_CurrentFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		ASSERT(m_hFile != INVALID_HANDLE_VALUE);
	}

	virtual ~CExtArcHandle() { }

	virtual void Release()
	{
		delete this;
	}

	virtual bool Span()
	{
		m_spanIdx++;

		TCHAR local_filename[MAX_PATH];
		_tcscpy_s(local_filename, MAX_PATH, m_BaseFilename);

		TCHAR *plext = PathFindExtension(local_filename);
		if (plext)
			*plext = _T('\0');

		_stprintf_s(m_CurrentFilename, MAX_PATH, _T("%s_part%d.data"), local_filename, m_spanIdx + 1);

		size_t fc = m_pArc->GetFileCount(IArchiver::IM_SPAN);

		// we finalize by storing the file table and writing the starting offset of the archive in the stream
		m_pArc->Finalize();

		LARGE_INTEGER sz;
		sz.LowPart = GetFileSize(m_hFile, (LPDWORD)&sz.HighPart);
		m_spanTotalSize.QuadPart += sz.QuadPart;

		// Finalize archive
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;

		m_hFile = CreateFile(m_CurrentFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		return (m_hFile != INVALID_HANDLE_VALUE);
	}

};

class CSfxHandle : public CPackagerArchiveHandle
{

public:
	CSfxHandle(const TCHAR *base_filename, CSfxPackagerDoc *pdoc) : CPackagerArchiveHandle(pdoc)
	{
		_tcscpy_s(m_BaseFilename, MAX_PATH, base_filename);
		_tcscpy_s(m_CurrentFilename, MAX_PATH, base_filename);

		m_hFile = INVALID_HANDLE_VALUE;
		if (!SetupSfxExecutable(m_BaseFilename, m_pDoc, m_hFile, 0))
		{
			CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);
			pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, _T("SFX setup failed; your output exe may be locked or the directory set to read-only.\r\n"));
		}
		ASSERT(m_hFile != INVALID_HANDLE_VALUE);
	}

	virtual ~CSfxHandle() { }

	virtual void Release()
	{
		delete this;
	}

	virtual bool Span()
	{
		m_spanIdx++;

		TCHAR local_filename[MAX_PATH];
		_tcscpy_s(local_filename, MAX_PATH, m_BaseFilename);

		TCHAR *pext = PathFindExtension(m_BaseFilename);
		TCHAR *plext = PathFindExtension(local_filename);
		_stprintf_s(plext, MAX_PATH, _T("_part%d"), m_spanIdx + 1);
		_tcscat_s(local_filename, MAX_PATH, pext);
		TCHAR *lcmd = PathFindFileName(local_filename);

		size_t fc = m_pArc->GetFileCount(IArchiver::IM_SPAN);

		// we finalize by storing the file table and writing the starting offset of the archive in the stream
		m_pArc->Finalize();

		LARGE_INTEGER sz;
		sz.LowPart = GetFileSize(m_hFile, (LPDWORD)&sz.HighPart);
		m_spanTotalSize.QuadPart += sz.QuadPart;

		// Finalize archive
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;

		FixupSfxExecutable(m_pDoc, m_CurrentFilename, lcmd, m_spanIdx, (UINT32)fc);

		_tcscpy_s(m_CurrentFilename, MAX_PATH, local_filename);

		return SetupSfxExecutable(m_CurrentFilename, m_pDoc, m_hFile, m_spanIdx);
	}

};


// CSfxPackagerDoc

IMPLEMENT_DYNCREATE(CSfxPackagerDoc, CDocument)

BEGIN_MESSAGE_MAP(CSfxPackagerDoc, CDocument)
END_MESSAGE_MAP()


// CSfxPackagerDoc construction/destruction

CSfxPackagerDoc::CSfxPackagerDoc()
{
	m_Key = 0;

	m_Props = props::IPropertySet::CreatePropertySet();
	if (m_Props)
	{
		props::IProperty *p;

		if ((p = m_Props->CreateProperty(_T("Settings\\Output File"), EDOCPROP::OUTPUT_FILE)) != nullptr)
		{
			p->SetString(_T("mySfx.exe"));
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Maximum Size (MB)"), EDOCPROP::MAXIMUM_SIZE_MB)) != nullptr)
		{
			p->SetInt(-1);
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Output File Suffix"), EDOCPROP::OUTPUT_FILE_SUFFIX_MODE)) != nullptr)
		{
			p->SetEnumStrings(_T("None,Build Date,Version"));
			p->SetEnumVal(0);
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Installer Type"), EDOCPROP::OUTPUT_MODE)) != nullptr)
		{
			p->SetEnumStrings(_T("Self-Extracting Executable,Executable With External Data"));
			p->SetEnumVal(0);
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Default Path"), EDOCPROP::DEFAULT_DESTINATION)) != nullptr)
		{
			p->SetString(_T(""));
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Allow Destination Change"), EDOCPROP::ALLOW_DESTINATION_CHANGE)) != nullptr)
		{
			p->SetBool(true);
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_YESNO);
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Require Administrator"), EDOCPROP::REQUIRE_ADMIN)) != nullptr)
		{
			p->SetBool(false);
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_YESNO);
		}

		if ((p = m_Props->CreateProperty(_T("Settings\\Compresion Block Size (advanced)"), EDOCPROP::COMPRESSION_BLOCKSIZE)) != nullptr)
		{
			p->SetEnumStrings(_T("Default,Smallest,Smaller,Small,Normal,Large,Larger,Largest"));
			p->SetEnumVal(0);
		}

		if ((p = m_Props->CreateProperty(_T("Appearance\\Caption"), EDOCPROP::CAPTION)) != nullptr)
		{
			p->SetString(_T("My Setup"));
		}

		if ((p = m_Props->CreateProperty(_T("Appearance\\Welcome Message"), EDOCPROP::WELCOME_MESSAGE)) != nullptr)
		{
			p->SetString(_T("Provide a description for the files being installed"));
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		}

		if ((p = m_Props->CreateProperty(_T("Appearance\\Icon File"), EDOCPROP::ICON_FILE)) != nullptr)
		{
			p->SetString(_T(""));
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		}

		if ((p = m_Props->CreateProperty(_T("Appearance\\Image File"), EDOCPROP::IMAGE_FILE)) != nullptr)
		{
			p->SetString(_T(""));
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		}

		if ((p = m_Props->CreateProperty(_T("Appearance\\Version"), EDOCPROP::VERSION)) != nullptr)
		{
			p->SetString(_T(""));
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		}

		if ((p = m_Props->CreateProperty(_T("Appearance\\License Message"), EDOCPROP::LICENSE_MESSAGE)) != nullptr)
		{
			p->SetString(_T(""));
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		}

		if ((p = m_Props->CreateProperty(_T("Post-Install\\Enable Explore Checkbox"), EDOCPROP::ENABLE_EXPLORE_CHECKBOX)) != nullptr)
		{
			p->SetBool(false);
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_YESNO);
		}

		if ((p = m_Props->CreateProperty(_T("Post-Install\\Launch Command"), EDOCPROP::LAUNCH_COMMAND)) != nullptr)
		{
			p->SetString(_T(""));
		}

		if ((p = m_Props->CreateProperty(_T("Post-Install\\Require Reboot"), EDOCPROP::REQUIRE_REBOOT)) != nullptr)
		{
			p->SetBool(false);
			p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_YESNO);
		}

		if ((p = m_Props->CreateProperty(_T("Metadata\\Copyright"), EDOCPROP::VERSION_COPYRIGHT)) != nullptr)
		{
			p->SetString(_T(""));
		}

		if ((p = m_Props->CreateProperty(_T("Metadata\\File Description"), EDOCPROP::VERSION_DESCRIPTION)) != nullptr)
		{
			p->SetString(_T(""));
		}

		if ((p = m_Props->CreateProperty(_T("Metadata\\Product Name"), EDOCPROP::VERSION_PRODUCTNAME)) != nullptr)
		{
			p->SetString(_T(""));
		}
	}

	m_hCancelEvent = CreateEvent(NULL, true, false, NULL);
	m_hThread = NULL;
}

const TCHAR *CSfxPackagerDoc::GetFileBrowseFilter(props::FOURCHARCODE property_id)
{
	static const TCHAR szIcoFilter[] = _T("Icon Files(*.ico)|*.ico|All Files(*.*)|*.*||");
	static const TCHAR szBmpFilter[] = _T("Bitmap Files(*.bmp)|*.bmp|All Files(*.*)|*.*||");
	static const TCHAR szExeFilter[] = _T("Executable Files(*.exe)|*.exe|All Files(*.*)|*.*||");
	static const TCHAR szAllFilter[] = _T("All Files(*.*)|*.*||");

	switch (property_id)
	{
		case EDOCPROP::ICON_FILE:
			return szIcoFilter;

		case EDOCPROP::IMAGE_FILE:
			return szBmpFilter;

		case EDOCPROP::OUTPUT_FILE:
			return szExeFilter;
	}

	return szAllFilter;
}

props::IPropertySet *CSfxPackagerDoc::GetFileProperties(UINT handle)
{
	TFileDataMap::iterator it = m_FileData.find(handle);
	if (it != m_FileData.end())
		return it->second;

	return nullptr;
}

const TCHAR *CSfxPackagerDoc::GetPropertyDescription(props::FOURCHARCODE property_id)
{
	switch (property_id)
	{
		case EDOCPROP::OUTPUT_FILE:
			return _T("Specifies the output executable file that will be created when this project is built | *.exe");

		case EDOCPROP::MAXIMUM_SIZE_MB:
			return _T("The maximum size (in MB) constraint for generated sfx archives, beyond which, files will be split (-1 is no constraint)");

		case EDOCPROP::OUTPUT_FILE_SUFFIX_MODE:
			return _T("Allows suffix to be automatically added immediately before the output file names extension. You can choose no suffix, the version (maj.min.rel.bld format) or the current date (YYYYMMDD format)");

		case EDOCPROP::OUTPUT_MODE:
			return _T("Determines whether the package data is stored inside the exe or in another data file; if your archive exceeds 4GB, use an external data file");

		case EDOCPROP::DEFAULT_DESTINATION:
			return _T("The default root path where the install data will go");

		case EDOCPROP::ALLOW_DESTINATION_CHANGE:
			return _T("Allow the user to change the install destination folder");

		case EDOCPROP::REQUIRE_ADMIN:
			return _T("If set, requires the user to have administrative privileges and will prompt the user to elevate if they do not");

		case EDOCPROP::COMPRESSION_BLOCKSIZE:
			return _T("Specifies the size of pre-compression blocks, ranging from 8KB to 256KB. Some data may compress better in larger or smaller blocks. WARNING: ADJUST AT YOUR OWN RISK.");

		case EDOCPROP::ICON_FILE:
			return _T("Specifies the ICO-format window icon");

		case EDOCPROP::IMAGE_FILE:
			return _T("Specifies a BMP-format image that will be displayed on the window (the default size is 200 x 400)");

		case EDOCPROP::CAPTION:
			return _T("Specifies the text that will be displayed in the window's title bar");

		case EDOCPROP::WELCOME_MESSAGE:
			return _T("Specifies the text that will be displayed in the window's main area to tell the end-user what the package is. May contain HTML in-line or reference a filename that contains HTML content");

		case EDOCPROP::LICENSE_MESSAGE:
			return _T("OPTIONAL: Specifies the text that will be displayed on the license dialog when the Javascript GetLicenseKey function is called. May contain HTML in-line or reference a filename that contains HTML content. If the JS function is never called, this goes unused");

		case EDOCPROP::VERSION:
			return _T("Specifies the version number that will be displayed by the installer. This is EITHER a string literal or the path to an .EXE, from which a version number will be extracted");

		case EDOCPROP::REQUIRE_REBOOT:
			return _T("If set, will inform that user that a reboot of the system is recommended and prompt to do that");

		case EDOCPROP::ENABLE_EXPLORE_CHECKBOX:
			return _T("If set, will open a windows explorer window to the install directory upon completion");

		case EDOCPROP::LAUNCH_COMMAND:
			return _T("A command that will be issued when installation is complete (see full docs for parameter options)");

		case EDOCPROP::VERSION_COPYRIGHT:
			return _T("Sets the Copyright field in the installer executable version info, as visible from the Properties->Details tab in Windows Explorer");

		case EDOCPROP::VERSION_DESCRIPTION:
			return _T("Sets the File Description field in the installer executable version info, as visible from the Properties->Details tab in Windows Explorer");

		case EDOCPROP::VERSION_PRODUCTNAME:
			return _T("Sets the Product Name field in the installer executable version info, as visible from the Properties->Details tab in Windows Explorer");


		case EFILEPROP::FILENAME:
			return _T("The name of the file that will be installed (note: this can be different than the name of the source file)");

		case EFILEPROP::SOURCE_PATH:
			return _T("The source file");

		case EFILEPROP::DESTINATION_PATH:
			return _T("The destination directory, relative to the install folder chosen by the user");

		case EFILEPROP::EXCLUDING:
			return _T("A semi-colon-delimited list of wildcard file descriptions of things that should be excluded (only applies to wildcard Filenames to begin with)");

		case EFILEPROP::PREFILE_SNIPPET:
			return _T("This Javascript code snippet will be appended to the PRE-FILE script that is executed before file is installed. As an example, it could be used to call a function embedded within the global PRE-FILE script. This is where one might call Skip() if the file should not be installed.");

		case EFILEPROP::POSTFILE_SNIPPET:
			return _T("This Javascript code snippet will be appended to the POST-FILE script that is executed after the file is installed. As an example, it could be used to call a function embedded within the global POST-FILE script");
	}

	return _T("TODO: add property description");
}

CSfxPackagerDoc::~CSfxPackagerDoc()
{
	if (m_hThread)
	{
		SignalObjectAndWait(m_hCancelEvent, m_hThread, INFINITE, FALSE);
	}

	CloseHandle(m_hCancelEvent);

	EndWaitCursor();

	// release all properties in the file data map
	std::for_each(m_FileData.begin(), m_FileData.end(), [](TFileDataMap::value_type &f) { if (f.second) f.second->Release(); });

	// release all document properties
	m_Props->Release();
}

CSfxPackagerDoc *CSfxPackagerDoc::GetDoc()
{
	CMDIFrameWnd *pframe = (CMDIFrameWnd *)(AfxGetApp()->m_pMainWnd);
	CMDIChildWnd *pchild = pframe ? pframe->MDIGetActive() : NULL;
	if (pchild)
	{
		CDocument *pdoc = pchild->GetActiveDocument();

		if (pdoc && pdoc->IsKindOf(RUNTIME_CLASS(CSfxPackagerDoc)))
			return (CSfxPackagerDoc *)pdoc;
	}

	return NULL;
}

BOOL CSfxPackagerDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	return TRUE;
}


BOOL CSfxPackagerDoc::EnumNamesFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, LONG_PTR lParam)
{
	CSfxPackagerDoc *_this = (CSfxPackagerDoc *)lParam;

	TResMap *rm = (hModule == NULL) ? &(_this->m_ResMap) : &(_this->m_PackageResMap);

	TResMap::iterator it = rm->find(lpType);

	if (it == rm->end())
	{
		std::pair<TResMap::iterator, bool> insret = rm->insert(TResMap::value_type(lpType, TResNameSet()));
		if (insret.second)
			it = insret.first;
	}

	if (it != rm->end())
		it->second.insert(lpName);

	return TRUE;
}

BOOL CSfxPackagerDoc::EnumTypesFunc(HMODULE hModule, LPCTSTR lpType, LONG_PTR lParam)
{
	CSfxPackagerDoc *_this = (CSfxPackagerDoc *)lParam;

	EnumResourceNames(hModule, lpType, (ENUMRESNAMEPROC)EnumNamesFunc, lParam);

	return TRUE;
}

BOOL CSfxPackagerDoc::EnumLangsFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLang, LONG_PTR lParam)
{
	CSfxPackagerDoc *_this = (CSfxPackagerDoc *)lParam;

	HANDLE hResInfo = FindResourceEx(hModule, lpType, lpName, wLang); 
	
	return TRUE; 
}

DWORD CSfxPackagerDoc::RunCreateSFXPackage(LPVOID param)
{
	CSfxPackagerView *pv = (CSfxPackagerView *)param;

	CSfxPackagerDoc *pd = pv->GetDocument(); // should be this

	pd->m_hThread = GetCurrentThread();

	tstring output;
	auto poutput = (*pd->m_Props)[EDOCPROP::OUTPUT_FILE];
	if (poutput)
		output = poutput->AsString();

	TCHAR *ext = PathFindExtension(output.c_str());
	if (ext)
	{
		if (!_tcsicmp(ext, _T(".exe")))
		{
			pd->CreateSFXPackage(NULL, pv);
		}
	}

	pv->DonePackaging();

	pd->m_hThread = NULL;

	ResetEvent(pd->m_hCancelEvent);

	return 0;
}


DWORD GetFileSizeByName(const TCHAR *filename)
{
	HANDLE h = CreateFile(filename, FILE_READ_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (!h)
		return 0;

	DWORD ret = GetFileSize(h, NULL);

	CloseHandle(h);

	return ret;
}

bool ShouldExclude(const TCHAR *filename, const TCHAR *excludespec)
{
	// this SHOULD never happen...?
	if (!filename)
		return false;

	if (!excludespec || !*excludespec)
		return false;

	TCHAR *buf = (TCHAR *)_malloca((_tcslen(excludespec) + 1) * sizeof(TCHAR));
	if (!buf)
		return false;

	_tcscpy(buf, excludespec);

	bool ret = false;

	TCHAR *c = buf, *d;
	while (c && *c)
	{
		d = _tcschr(c, _T(';'));
		if (d)
			*d = _T('\0');

		if (PathMatchSpec(filename, c))
		{
			ret = true;
			break;
		}

		c = d;
		if (d)
			c++;
	}

	_freea(buf);

	return ret;
}

bool CSfxPackagerDoc::AddFileToArchive(CSfxPackagerView *pview, IArchiver *parc, TStringArray &created_archives, TSizeArray &created_archive_filecounts, const TCHAR *srcspec, const TCHAR *excludespec, const TCHAR *prefile_scriptsnippet, const TCHAR *postfile_scriptsnippet, const TCHAR *dstpath, const TCHAR *dstfilename, uint64_t *sz_uncomp, uint64_t *sz_comp, UINT recursion)
{
	DWORD wr = WaitForSingleObject(m_hCancelEvent, 0);
	if ((wr == WAIT_OBJECT_0) || (wr == WAIT_ABANDONED))
	{
		return false;
	}

	if (!srcspec)
		return false;

	CString msg;

	CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);

	PARSEDURL urlinf = {0};
	urlinf.cbSize = sizeof(PARSEDURL);
	if (ParseURL(srcspec, &urlinf) == S_OK)
	{
		TCHAR local_dstpath[MAX_PATH], *dp = (TCHAR *)dstpath;
		while (dp && *dp && ((*dp == _T('\\')) || (*dp == _T('/')))) dp++;

		_tcscpy_s(local_dstpath, dp);
		if (_tcslen(local_dstpath) > 0)
			PathAddBackslash(local_dstpath);
		_tcscat(local_dstpath, dstfilename);

		dp = local_dstpath;
		while (dp && *(dp++)) { if (*dp == _T('/')) *dp = _T('\\'); }

		parc->AddFile(srcspec, local_dstpath, nullptr, nullptr, prefile_scriptsnippet, postfile_scriptsnippet);

		msg.Format(_T("    Adding download reference to \"%s\" from (%s) ...\r\n"), local_dstpath, srcspec);
		pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

		return true;
	}

	TCHAR docpath[MAX_PATH];
	_tcscpy_s(docpath, GetPathName());
	PathRemoveFileSpec(docpath);

	TCHAR fullfilename[MAX_PATH];
	if (PathIsRelative(srcspec))
	{
		PathCombine(fullfilename, docpath, srcspec);
	}
	else
	{
		_tcscpy_s(fullfilename, srcspec);
	}

	TCHAR *filespec = PathFindFileName(srcspec);
	TCHAR *filename = PathFindFileName(fullfilename);

	bool wildcard = (_tcschr(filespec, _T('*')) != NULL);

	if (wildcard)
	{
		_tcscpy(filename, _T("*"));
		dstfilename = NULL;
	}

	// set marquee mode on the progress bar while in recursion
	if (!recursion && wildcard)
	{
		pmf->GetStatusBarWnd().PostMessage(CProgressStatusBar::WM_UPDATE_STATUS, 0xff, 1);
		Sleep(0);
	}

	bool ret = false;

	WIN32_FIND_DATA fd;
	HANDLE hfind = FindFirstFile(fullfilename, &fd);
	if (hfind != INVALID_HANDLE_VALUE)
	{
		ret = true;

		do
		{
			wr = WaitForSingleObject(m_hCancelEvent, 0);
			if ((wr == WAIT_OBJECT_0) || (wr == WAIT_ABANDONED))
			{
				return false;
			}

			if (_tcscmp(fd.cFileName, _T(".")) && _tcscmp(fd.cFileName, _T("..")))
			{
				_tcscpy(filename, fd.cFileName);

				uint64_t comp = 0, uncomp = 0;

				if (PathIsDirectory(fullfilename))
				{
					if (!PathIsDirectoryEmpty(fullfilename))
					{
						PathAddBackslash(filename);
						_tcscat(filename, filespec);

						TCHAR local_dstpath[MAX_PATH];
						_tcscpy_s(local_dstpath, dstpath);
						if (_tcslen(local_dstpath) > 0)
							PathAddBackslash(local_dstpath);
						_tcscat(local_dstpath, fd.cFileName);

						uint64_t rcomp = 0, runcomp = 0;
						ret &= AddFileToArchive(pview, parc, created_archives, created_archive_filecounts, fullfilename, excludespec, prefile_scriptsnippet, postfile_scriptsnippet, local_dstpath, NULL, &runcomp, &rcomp, recursion + 1);
						if (ret)
						{
							uncomp = runcomp;
							comp = rcomp;
						}
					}
				}
				else if (PathMatchSpec(fd.cFileName, filespec))
				{
					if (!ShouldExclude(fd.cFileName, excludespec))
					{
						if (!PathIsNetworkPath(dstpath) && *dstpath == _T('\\'))
							dstpath++;

						TCHAR dst[MAX_PATH];
						_tcscpy_s(dst, dstpath ? dstpath : _T(""));

						if (dst[0] != _T('\0'))
							PathAddBackslash(dst);

						_tcscat(dst, (dstfilename && !wildcard) ? dstfilename : fd.cFileName);

						if (PathFileExists(fullfilename))
						{
							msg.Format(_T("    Adding \"%s\" from \"%s\" ...\r\n"), dst, fullfilename);
							pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

							parc->AddFile(fullfilename, dst, &uncomp, &comp, prefile_scriptsnippet, postfile_scriptsnippet);
						}
						else
						{
							msg.Format(_T("    WARNING: \"%s\" NOT FOUND!\r\n"), fullfilename);
							pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

							ret = false;
							break;
						}
					}
				}

				if (sz_uncomp)
					*sz_uncomp += uncomp;

				if (sz_comp)
					*sz_comp += comp;
			}
		}
		while (FindNextFile(hfind, &fd));

		FindClose(hfind);
	}

	// set marquee mode on the progress bar while in recursion
	if (!recursion && wildcard)
	{
		pmf->GetStatusBarWnd().PostMessage(CProgressStatusBar::WM_UPDATE_STATUS, 0xff, 0);
		Sleep(0);
	}

	return ret;
}


bool CSfxPackagerDoc::CreateSFXPackage(const TCHAR *filename, CSfxPackagerView *pview)
{
	time_t start_op, finish_op;
	time(&start_op);

	UINT c = 0, maxc = (UINT)m_FileData.size();
	if (!maxc)
		return true;

	CString msg;

	CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);

	TCHAR docpath[MAX_PATH];
	_tcscpy_s(docpath, GetPathName());
	PathRemoveFileSpec(docpath);

	if (!filename)
	{
		auto pfilename = (*m_Props)[CSfxPackagerDoc::EDOCPROP::OUTPUT_FILE];
		if (pfilename)
			filename = pfilename->AsString();
	}

	TCHAR fullfilename[MAX_PATH];

	if (PathIsRelative(filename))
	{
		PathCombine(fullfilename, docpath, filename);
	}
	else
	{
		_tcscpy(fullfilename, filename);
	}

	auto ptargetmod = (*m_Props)[CSfxPackagerDoc::EDOCPROP::OUTPUT_FILE_SUFFIX_MODE];
	// if we're suppose to, append the current date to the filename before the extension
	if (ptargetmod && (ptargetmod->AsInt() > 0))
	{
		TCHAR extcpy[MAX_PATH];
		TCHAR *ext = PathFindExtension(fullfilename);
		_tcscpy(extcpy, ext);
		PathRemoveExtension(fullfilename);

		switch (ptargetmod->AsInt())
		{
			case 2:
			{
				auto pvers = (*m_Props)[CSfxPackagerDoc::EDOCPROP::VERSION];
				if (pvers && PathFileExists(pvers->AsString()))
				{
					int major, minor, release, build;
					if (GetFileVersion(pvers->AsString(), major, minor, release, build))
					{
						_tcscat(fullfilename, _T("_"));

						TCHAR vb[16];
						_tcscat(fullfilename, _itot(major, vb, 10));
						_tcscat(fullfilename, _T("."));
						_tcscat(fullfilename, _itot(minor, vb, 10));
						if (release || build)
						{
							_tcscat(fullfilename, _T("."));
							_tcscat(fullfilename, _itot(release, vb, 10));

							if (build)
							{
								_tcscat(fullfilename, _T("."));
								_tcscat(fullfilename, _itot(build, vb, 10));
							}
						}
					}
				}
				break;
			}

			case 1:
				// if we're suppose to, append the current date to the filename before the extension
				auto now = std::chrono::system_clock::now();
				std::time_t now_c = std::chrono::system_clock::to_time_t(now);
				struct tm *parts = std::localtime(&now_c);

				TCHAR dates[MAX_PATH];
				_stprintf(dates, _T("_%04d%02d%02d"), 1900 + parts->tm_year, 1 + parts->tm_mon, parts->tm_mday);
				_tcscat(fullfilename, dates);
				break;
		}

		_tcscat(fullfilename, extcpy);
	}

	m_LastBuiltInstallerFilename = fullfilename;

	TCHAR timebuf[160];
	struct tm * timeinfo;

	timeinfo = localtime(&start_op);
	_tcsftime(timebuf, 160, _T("%R:%S   %A, %e %B %Y"), timeinfo);

	auto pcaption = (*m_Props)[CSfxPackagerDoc::EDOCPROP::CAPTION];
	msg.Format(_T("Beginning build of \"%s\"  ----  [ %s ]\r\n\r\n    Output File: %s\r\n\r\n"), pcaption ? pcaption->AsString() : fullfilename, timebuf, fullfilename);
	pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

	TStringArray created_archives;
	TSizeArray created_archive_filecounts;

	bool ret = true;

	HANDLE hf = NULL;

	IArchiver *parc = NULL;
	UINT spanct;
	uint64_t sz_uncomp = 0, sz_totalcomp = 0, sz_comp = 0;

	CPackagerArchiveHandle *pah = nullptr;

	DWORD wr = 0;
	{
		auto parcmode = (*m_Props)[CSfxPackagerDoc::EDOCPROP::OUTPUT_MODE];
		if (parcmode)
		{
			switch (parcmode->AsInt())
			{
				// mode 0 is SFX
				default:
				case 0:
					pah = new CSfxHandle(fullfilename, this);
					break;

					// mode 1 is an external archove file
				case 1:
					pah = new CExtArcHandle(fullfilename, this);
					break;
			}
		}

		ret = (IArchiver::CreateArchiver(&parc, pah, IArchiver::CT_FASTLZ) == IArchiver::CR_OK);

		if (pah)
			pah->SetArchiver(parc);

		auto pblocksize = (*m_Props)[CSfxPackagerDoc::EDOCPROP::COMPRESSION_BLOCKSIZE];
		parc->SetCompressionBlockSize((IArchiver::EBufferSize)pblocksize->AsInt());

		auto pmaxsize = (*m_Props)[CSfxPackagerDoc::EDOCPROP::MAXIMUM_SIZE_MB];
		int64_t maxsz = pmaxsize ? pmaxsize->AsInt() : -1;
		parc->SetMaximumSize((maxsz > 0) ? (maxsz MB) : UINT64_MAX);

		m_UncompressedSize.QuadPart = 0;

		TFileDataMap::iterator it = m_FileData.begin();
		TFileDataMap::iterator last_it = m_FileData.end();

		while (it != last_it)
		{
			wr = WaitForSingleObject(m_hCancelEvent, 0);
			if ((wr == WAIT_OBJECT_0) || (wr == WAIT_ABANDONED))
			{
				break;
			}

			tstring name;
			props::IProperty *pname = it->second->GetPropertyById(EFILEPROP::FILENAME);
			if (pname)
				name = pname->AsString();

			tstring srcpath;
			props::IProperty *psrcpath = it->second->GetPropertyById(EFILEPROP::SOURCE_PATH);
			if (psrcpath)
				srcpath = psrcpath->AsString();

			tstring exclude;
			props::IProperty *pexclude = it->second->GetPropertyById(EFILEPROP::EXCLUDING);
			if (pexclude)
				exclude = pexclude->AsString();

			tstring prefile_scriptsnippet;
			props::IProperty *ppresnippet = it->second->GetPropertyById(EFILEPROP::PREFILE_SNIPPET);
			if (ppresnippet)
				prefile_scriptsnippet = ppresnippet->AsString();

			tstring postfile_scriptsnippet;
			props::IProperty *ppostsnippet = it->second->GetPropertyById(EFILEPROP::POSTFILE_SNIPPET);
			if (ppostsnippet)
				postfile_scriptsnippet = ppostsnippet->AsString();

			tstring dstpath;
			props::IProperty *pdstpath = it->second->GetPropertyById(EFILEPROP::DESTINATION_PATH);
			if (pdstpath)
				dstpath = pdstpath->AsString();

			bool wildcard = ((_tcschr(name.c_str(), _T('*')) != NULL) || PathIsDirectory(srcpath.c_str()));

			c++;
			UINT pct = (c * 100) / maxc;
			pmf->GetStatusBarWnd().PostMessage(CProgressStatusBar::WM_UPDATE_STATUS, pct, 0);
			Sleep(0);

			if (wildcard)
			{
				// TODO: add include / exclude lists
				TCHAR wcsrcpath[MAX_PATH];
				_tcscpy_s(wcsrcpath, srcpath.c_str());
				PathAddBackslash(wcsrcpath);
				_tcscat(wcsrcpath, name.c_str());

				ret &= AddFileToArchive(pview, parc, created_archives, created_archive_filecounts, wcsrcpath, exclude.c_str(), prefile_scriptsnippet.c_str(), postfile_scriptsnippet.c_str(), dstpath.c_str(), nullptr, &sz_uncomp, &sz_comp);
			}
			else
			{
				ret &= AddFileToArchive(pview, parc, created_archives, created_archive_filecounts, srcpath.c_str(), nullptr, prefile_scriptsnippet.c_str(), postfile_scriptsnippet.c_str(), dstpath.c_str(), name.c_str(), &sz_uncomp, &sz_comp);
			}

			sz_totalcomp = sz_comp;
			m_UncompressedSize.QuadPart = sz_uncomp;

			it++;
		}

		if (pah)
		{
			spanct = pah->GetSpanCount();
			sz_totalcomp = pah->GetSpanTotalSize();
		}

		LARGE_INTEGER tsz = {0};
		tsz.LowPart = GetFileSize(pah->GetHandle(), (LPDWORD)&tsz.HighPart);
		sz_totalcomp += tsz.QuadPart;
	}

	time(&finish_op);
	int elapsed = (int)difftime(finish_op, start_op);

	int hours = elapsed / 3600;
	elapsed %= 3600;

	int minutes = elapsed / 60;
	int seconds = elapsed % 60;

	timeinfo = localtime (&finish_op);
	_tcsftime(timebuf, 160, _T("%R:%S   %A, %e %B %Y"), timeinfo);

	if ((wr == WAIT_OBJECT_0) || (wr == WAIT_ABANDONED))
	{
		msg.Format(_T("\r\nCancelled build of \"%s\"  ----  [ %s ]. (after: %02d:%02d:%02d)\r\n"), pcaption ? pcaption->AsString() : fullfilename, timebuf, hours, minutes, seconds);
	}
	else
	{
		msg.Format(_T("\r\nFinished build of \"%s\"  ----  [ %s ].\r\n\r\nAdded %d files, spanning %d archive(s).\r\n"), pcaption ? pcaption->AsString() : fullfilename, timebuf, parc->GetFileCount(IArchiver::IM_WHOLE), spanct);
		pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

		double comp_pct = 0.0;
		double uncomp_sz = (double)m_UncompressedSize.QuadPart;
		double comp_sz = (double)sz_totalcomp;
		if (comp_sz > 0)
		{
			comp_pct = 100.0 * std::max<double>(0.0, ((uncomp_sz / comp_sz) - 1.0));
		}
		msg.Format(_T("Uncompressed size: %1.02fMB\r\nCompressed size (including installer overhead): %1.02fMB\r\nCompression: %1.02f%%\r\n\r\n"), uncomp_sz / 1024.0f / 1024.0f, comp_sz / 1024.0f / 1024.0f, comp_pct);
		pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

		msg.Format(_T("Elapsed time: %02d:%02d:%02d\r\n\r\n\r\n"), hours, minutes, seconds);
	}

	pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

	pmf->GetStatusBarWnd().PostMessage(CProgressStatusBar::WM_UPDATE_STATUS, -1, 0);
	Sleep(0);

	if (pah)
	{
		pah->Release();
		pah = nullptr;
	}

	IArchiver::DestroyArchiver(&parc);

	return ret;
}

bool CSfxPackagerDoc::CopyFileToTemp(CSfxPackagerView *pview, const TCHAR *srcspec, const TCHAR *dstpath, const TCHAR *dstfilename, const TCHAR *excludespec, UINT recursion)
{
	DWORD wr = WaitForSingleObject(m_hCancelEvent, 0);
	if ((wr == WAIT_OBJECT_0) || (wr == WAIT_ABANDONED))
	{
		return false;
	}

	if (!srcspec)
		return false;

	CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);

	TCHAR docpath[MAX_PATH];
	_tcscpy_s(docpath, GetPathName());
	PathRemoveFileSpec(docpath);

	TCHAR fullfilename[MAX_PATH];
	if (PathIsRelative(srcspec))
	{
		PathCombine(fullfilename, docpath, srcspec);
	}
	else
	{
		_tcscpy(fullfilename, srcspec);
	}

	TCHAR *filespec = PathFindFileName(srcspec);
	TCHAR *filename = PathFindFileName(fullfilename);

	bool wildcard = (_tcschr(filespec, _T('*')) != NULL);

	if (wildcard)
	{
		_tcscpy(filename, _T("*"));
		dstfilename = NULL;
	}

	// set marquee mode on the progress bar while in recursion
	if (!recursion && wildcard)
	{
		pmf->GetStatusBarWnd().PostMessage(CProgressStatusBar::WM_UPDATE_STATUS, 0xff, 1);
		Sleep(0);
	}

	bool ret = false;

	WIN32_FIND_DATA fd;
	HANDLE hfind = FindFirstFile(fullfilename, &fd);
	if (hfind != INVALID_HANDLE_VALUE)
	{
		ret = true;

		CString msg;

		do
		{
			wr = WaitForSingleObject(m_hCancelEvent, 0);
			if ((wr == WAIT_OBJECT_0) || (wr == WAIT_ABANDONED))
			{
				return false;
			}

			if (_tcscmp(fd.cFileName, _T(".")) && _tcscmp(fd.cFileName, _T("..")))
			{
				_tcscpy(filename, fd.cFileName);

				if (PathIsDirectory(fullfilename))
				{
					if (!PathIsDirectoryEmpty(fullfilename))
					{
						PathAddBackslash(filename);
						_tcscat(filename, filespec);

						TCHAR local_dstpath[MAX_PATH];
						_tcscpy_s(local_dstpath, dstpath);
						if (_tcslen(local_dstpath) > 0)
							PathAddBackslash(local_dstpath);
						_tcscat(local_dstpath, fd.cFileName);

						ret &= CopyFileToTemp(pview, fullfilename, local_dstpath, NULL, excludespec, recursion + 1);
					}
				}
				else if (PathMatchSpec(fd.cFileName, filespec) && !ShouldExclude(fd.cFileName, excludespec))
				{
					if (*dstpath == _T('\\'))
						dstpath++;

					TCHAR dst[MAX_PATH];
					_tcscpy_s(dst, dstpath ? dstpath : _T(""));

					FLZACreateDirectories(dst);

					if (dst[0] != _T('\0'))
						PathAddBackslash(dst);

					_tcscat(dst, (dstfilename && !wildcard) ? dstfilename : fd.cFileName);

					msg.Format(_T("    Copying temp file \"%s\" from \"%s\"...\r\n"), dst, fullfilename);
					pmf->GetOutputWnd().AppendMessage(COutputWnd::OT_BUILD, msg);

					CopyFile(fullfilename, dst, FALSE);
				}
			}
		}
		while (FindNextFile(hfind, &fd));

		FindClose(hfind);
	}

	// set marquee mode on the progress bar while in recursion
	if (!recursion && wildcard)
	{
		pmf->GetStatusBarWnd().PostMessage(CProgressStatusBar::WM_UPDATE_STATUS, 0xff, 0);
		Sleep(0);
	}

	return ret;
}


UINT CSfxPackagerDoc::AddFile(const TCHAR *filename, const TCHAR *srcpath, const TCHAR *dstpath, const TCHAR *exclude, const TCHAR *prefile_scriptsnippet, const TCHAR *postfile_scriptsnippet)
{
	TFileDataMap::value_type fd(m_Key, props::IPropertySet::CreatePropertySet());

	props::IProperty *p;

	if ((p = fd.second->CreateProperty(_T("Filename"), CSfxPackagerDoc::EFILEPROP::FILENAME)) != nullptr)
	{
		p->SetString(filename ? filename : _T(""));
	}

	if ((p = fd.second->CreateProperty(_T("Source Path"), CSfxPackagerDoc::EFILEPROP::SOURCE_PATH)) != nullptr)
	{
		p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		p->SetString(srcpath ? srcpath : _T(""));
	}

	if ((p = fd.second->CreateProperty(_T("Destination Path"), CSfxPackagerDoc::EFILEPROP::DESTINATION_PATH)) != nullptr)
	{
		p->SetString(dstpath ? dstpath : _T(""));
	}

	if ((p = fd.second->CreateProperty(_T("Exclude"), CSfxPackagerDoc::EFILEPROP::EXCLUDING)) != nullptr)
	{
		p->SetString(exclude ? exclude : _T(""));
	}

	if ((p = fd.second->CreateProperty(_T("Pre-File Snippet"), CSfxPackagerDoc::EFILEPROP::PREFILE_SNIPPET)) != nullptr)
	{
		p->SetString(prefile_scriptsnippet ? prefile_scriptsnippet : _T(""));
	}

	if ((p = fd.second->CreateProperty(_T("Post-File Snippet"), CSfxPackagerDoc::EFILEPROP::POSTFILE_SNIPPET)) != nullptr)
	{
		p->SetString(postfile_scriptsnippet ? postfile_scriptsnippet : _T(""));
	}

	m_FileData.insert(fd);

	UINT ret = m_Key;
	m_Key++;

	return ret;
}

void CSfxPackagerDoc::RemoveFile(UINT handle)
{
	TFileDataMap::iterator it = m_FileData.find(handle);
	if (it != m_FileData.end())
	{
		props::IPropertySet *ps = it->second;
		if (ps)
			ps->Release();

		m_FileData.erase(it);
	}
}

bool CSfxPackagerDoc::AdjustFileOrder(UINT key, EMoveType mt, UINT *swap_key)
{
	TFileDataMap::iterator it = m_FileData.find(key);
	if (it == m_FileData.end())
		return false;

	TFileDataMap::iterator swap_it = it;
	TFileDataMap::iterator last_it = m_FileData.find(m_FileData.rbegin()->first);

	switch (mt)
	{
		case EMoveType::UP:
		{
			if (it == m_FileData.begin())
				return false;
			swap_it--;
			break;
		}

		case EMoveType::DOWN:
			swap_it++;
			break;

		case EMoveType::TOP:
			swap_it = m_FileData.begin();
			break;

		case EMoveType::BOTTOM:
			swap_it = last_it;
			break;
	}

	if (swap_it != m_FileData.end())
	{
		if (swap_key)
			*swap_key = swap_it->first;

		TFileDataMap::mapped_type tmp = swap_it->second;
		swap_it->second = it->second;
		it->second = tmp;

		return true;
	}

	return false;
}

UINT CSfxPackagerDoc::GetNumFiles()
{
	return (UINT)m_FileData.size();
}

const TCHAR *CSfxPackagerDoc::GetFileData(UINT handle, EFileDataType fdt)
{
	TFileDataMap::iterator it = m_FileData.find(handle);
	if (it == m_FileData.end())
		return NULL;

	switch (fdt)
	{
		case FDT_NAME:
			return (*it->second)[EFILEPROP::FILENAME]->AsString();
			break;

		case FDT_SRCPATH:
			return (*it->second)[EFILEPROP::SOURCE_PATH]->AsString();
			break;

		case FDT_DSTPATH:
			return (*it->second)[EFILEPROP::DESTINATION_PATH]->AsString();
			break;

		case FDT_EXCLUDE:
			return (*it->second)[EFILEPROP::EXCLUDING]->AsString();
			break;

		case FDT_PREFILE_SNIPPET:
			return (*it->second)[EFILEPROP::PREFILE_SNIPPET]->AsString();
			break;

		case FDT_POSTFILE_SNIPPET:
			return (*it->second)[EFILEPROP::POSTFILE_SNIPPET]->AsString();
			break;
	}

	return nullptr;
}

void CSfxPackagerDoc::SetFileData(UINT handle, EFileDataType fdt, const TCHAR *data)
{
	TFileDataMap::iterator it = m_FileData.find(handle);
	if (it == m_FileData.end())
		return;

	switch (fdt)
	{
		case FDT_NAME:
			(*it->second)[EFILEPROP::FILENAME]->SetString(data);
			break;

		case FDT_SRCPATH:
			(*it->second)[EFILEPROP::SOURCE_PATH]->SetString(data);
			break;

		case FDT_DSTPATH:
			(*it->second)[EFILEPROP::DESTINATION_PATH]->SetString(data);
			break;

		case FDT_EXCLUDE:
			(*it->second)[EFILEPROP::EXCLUDING]->SetString(data);
			break;

		case FDT_PREFILE_SNIPPET:
			(*it->second)[EFILEPROP::PREFILE_SNIPPET]->SetString(data);
			break;

		case FDT_POSTFILE_SNIPPET:
			(*it->second)[EFILEPROP::POSTFILE_SNIPPET]->SetString(data);
			break;
	}
}

// CSfxPackagerDoc serialization

void UnescapeString(const TCHAR *in, tstring &out)
{
	out.reserve(_tcslen(in));
	const TCHAR *c = in;
	while (c && *c)
	{
		if (*c == _T('&'))
		{
			if (!memcmp(c, _T("&lt;"), sizeof(TCHAR) * 4))
			{
				out += _T('<');
				c += 3;
			}
			else if (!memcmp(c, _T("&gt;"), sizeof(TCHAR) * 4))
			{
				out += _T('>');
				c += 3;
			}
			else if (!memcmp(c, _T("&amp;"), sizeof(TCHAR) * 5))
			{
				out += _T('&');
				c += 4;
			}
			else if (!memcmp(c, _T("&quot;"), sizeof(TCHAR) * 6))
			{
				out += _T('\"');
				c += 5;
			}
		}
		else
			out += *c;

		c++;
	}
}

void EscapeString(const TCHAR *in, tstring &out)
{
	out.clear();
	out.reserve(_tcslen(in) * 2);
	const TCHAR *c = in;
	while (c && *c)
	{
		switch (*c)
		{
			case _T('<'):
			{
				out += _T("&lt;");
				break;
			}
			case _T('>'):
			{
				out += _T("&gt;");
				break;
			}
			case _T('&'):
			{
				out += _T("&amp;");
				break;
			}
			case _T('\"'):
			{
				out += _T("&quot;");
				break;
			}
			default:
			{
				out += *c;
				break;
			}
		};

		c++;
	}
}

// Legacy settings!! New path is through props
void CSfxPackagerDoc::ReadSettings(genio::IParserT *gp)
{
	tstring name, value;

	while (gp->NextToken())
	{
		if (gp->IsToken(_T("<")))
		{
			gp->NextToken();

			if (!gp->IsToken(_T("/")))
			{
				name = gp->GetCurrentTokenString();
			}
			else
			{
				gp->NextToken();
				if (gp->IsToken(_T("settings")))
				{
					while (gp->NextToken() && !gp->IsToken(_T(">"))) { }
					break;
				}
			}
		}
		else if (gp->IsToken(_T("value")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			value = gp->GetCurrentTokenString();
			tstring tmp;
			UnescapeString(value.c_str(), tmp);
			value = tmp;
		}
		else if (gp->IsToken(_T(">")))
		{
			if (!_tcsicmp(name.c_str(), _T("output")))
				(*m_Props)[EDOCPROP::OUTPUT_FILE]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("caption")))
				(*m_Props)[EDOCPROP::CAPTION]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("description")))
				(*m_Props)[EDOCPROP::WELCOME_MESSAGE]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("licensemsg")))
				(*m_Props)[EDOCPROP::LICENSE_MESSAGE]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("icon")))
				(*m_Props)[EDOCPROP::ICON_FILE]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("image")))
				(*m_Props)[EDOCPROP::IMAGE_FILE]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("launchcmd")))
				(*m_Props)[EDOCPROP::LAUNCH_COMMAND]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("explore")))
				(*m_Props)[EDOCPROP::ENABLE_EXPLORE_CHECKBOX]->SetBool(!_tcsicmp(value.c_str(), _T("true")) ? true : false);
			else if (!_tcsicmp(name.c_str(), _T("maxsize")))
				(*m_Props)[EDOCPROP::MAXIMUM_SIZE_MB]->SetInt(_tstoi(value.c_str()));
			else if (!_tcsicmp(name.c_str(), _T("defaultpath")))
				(*m_Props)[EDOCPROP::DEFAULT_DESTINATION]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("versionid")))
				(*m_Props)[EDOCPROP::VERSION]->SetString(value.c_str());
			else if (!_tcsicmp(name.c_str(), _T("requireadmin")))
				(*m_Props)[EDOCPROP::REQUIRE_ADMIN]->SetBool(!_tcsicmp(value.c_str(), _T("true")) ? true : false);
			else if (!_tcsicmp(name.c_str(), _T("requirereboot")))
				(*m_Props)[EDOCPROP::REQUIRE_REBOOT]->SetBool(!_tcsicmp(value.c_str(), _T("true")) ? true : false);
			else if (!_tcsicmp(name.c_str(), _T("allowdestchg")))
				(*m_Props)[EDOCPROP::ALLOW_DESTINATION_CHANGE]->SetBool(!_tcsicmp(value.c_str(), _T("true")) ? true : false);
		}
	}
}

void CSfxPackagerDoc::ReadScripts(genio::IParserT *gp)
{
	tstring stype;

	while (gp->NextToken())
	{
		if (gp->IsToken(_T("<")))
		{
			gp->NextToken();

			if (!gp->IsToken(_T("/")))
			{
				if (gp->IsToken(_T("script")))
				{
					gp->NextToken();

					if (gp->IsToken(_T("type")))
					{
						gp->NextToken(); // skip '='

						gp->NextToken();
						tstring t = gp->GetCurrentTokenString();

						gp->NextToken(); // skip '>'

						gp->ReadUntil(_T("<"), false, true);
						tstring s = gp->GetCurrentTokenString();
						tstring ues;
						UnescapeString(s.c_str(), ues);

						if (!_tcsicmp(t.c_str(), _T("initialize")))
						{
							m_Script[EScriptType::INITIALIZE] = ues.c_str();
						}
						else if (!_tcsicmp(t.c_str(), _T("init")) || !_tcsicmp(t.c_str(), _T("preinstall")))
						{
							m_Script[EScriptType::PREINSTALL] = ues.c_str();
						}
						else if (!_tcsicmp(t.c_str(), _T("prefile")))
						{
							m_Script[EScriptType::PREFILE] = ues.c_str();
						}
						else if (!_tcsicmp(t.c_str(), _T("perfile")) || !_tcsicmp(t.c_str(), _T("postfile")))
						{
							m_Script[EScriptType::POSTFILE] = ues.c_str();
						}
						else if (!_tcsicmp(t.c_str(), _T("finish")) || !_tcsicmp(t.c_str(), _T("postinstall")))
						{
							m_Script[EScriptType::POSTINSTALL] = ues.c_str();
						}
					}
				}
			}
			else
			{
				gp->NextToken();
				if (gp->IsToken(_T("scripts")))
				{
					while (gp->NextToken() && !gp->IsToken(_T(">"))) { }
					break;
				}
			}
		}
	}
}

void CSfxPackagerDoc::ReadFiles(genio::IParserT *gp)
{
	tstring name, src, dst, exclude, prefile_scriptsnippet, postfile_scriptsnippet;

	while (gp->NextToken())
	{
		if (gp->IsToken(_T("<")))
		{
			name.clear();
			src.clear();
			dst.clear();
			exclude.clear();
			prefile_scriptsnippet.clear();
			postfile_scriptsnippet.clear();

			gp->NextToken();
			if (!gp->IsToken(_T("file")))
				break;
		}
		else if (gp->IsToken(_T("/")))
		{
			if (!name.empty() && !src.empty() && !dst.empty())
			{
				UINT handle = AddFile(name.c_str(), src.c_str(), dst.c_str(), exclude.c_str(), prefile_scriptsnippet.c_str(), postfile_scriptsnippet.c_str());
			}

			while (gp->NextToken() && !gp->IsToken(_T(">"))) { }
		}
		else if (gp->IsToken(_T("name")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			name = gp->GetCurrentTokenString();
		}
		else if (gp->IsToken(_T("src")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			src = gp->GetCurrentTokenString();
		}
		else if (gp->IsToken(_T("dst")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			dst = gp->GetCurrentTokenString();
		}
		else if (gp->IsToken(_T("exclude")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			exclude = gp->GetCurrentTokenString();
		}
		else if (gp->IsToken(_T("prefile_snippet")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			UnescapeString(gp->GetCurrentTokenString(), prefile_scriptsnippet);
		}
		else if (gp->IsToken(_T("snippet")) || gp->IsToken(_T("postfile_snippet")))
		{
			gp->NextToken(); // skip '='

			gp->NextToken();
			UnescapeString(gp->GetCurrentTokenString(), postfile_scriptsnippet);
		}
	}
}

void CSfxPackagerDoc::ReadProject(genio::IParserT *gp)
{
	while (gp->NextToken())
	{
		if (gp->IsToken(_T("<")))
		{
			if (gp->NextToken())
			{
				if (gp->IsToken(_T("settings")))
				{
					
					ReadSettings(gp);
				}
				else if (gp->IsToken(_T("scripts")))
				{
					ReadScripts(gp);
				}
				else if (gp->IsToken(_T("files")))
				{
					ReadFiles(gp);
				}
			}
		}
		else if (gp->IsToken(_T("/")))
		{
			gp->NextToken();
			if (gp->IsToken(_T("sfxpackager")))
				break;
		}

	}
}

void CSfxPackagerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		CString s;
		s += _T("<sfxpackager>\n\n");
		s += _T("<settings2>\n\n");

		tstring tmp;

		tstring props;
		m_Props->SerializeToXMLString(props::IProperty::SERIALIZE_MODE::SM_BIN_VALUESONLY, props);
		s += props.c_str();

		s += _T("\n\n</settings2>\n\n");

		s += _T("<files2>");

#if defined(UNICODE)
		int nLen;
		char *pbuf;

		nLen = WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)s, -1, NULL, NULL, NULL, NULL);
		pbuf = (char *)malloc(nLen);
		if (pbuf)
			WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)s, -1, pbuf, nLen, NULL, NULL);
#else
		pbuf = (LPCTSTR)s;
#endif

		if (pbuf)
			ar.Write(pbuf, nLen - 1);

		for (TFileDataMap::iterator it = m_FileData.begin(), last_it = m_FileData.end(); it != last_it; it++)
		{
			s = _T("\n\n<file2>\n\n");

			tstring props;
			it->second->SerializeToXMLString(props::IProperty::SERIALIZE_MODE::SM_BIN_VALUESONLY, props);
			s += props.c_str();

			s += _T("\n\n</file2>");

#if defined(UNICODE)
			nLen = WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)s, -1, NULL, NULL, NULL, NULL);
			pbuf = (char *)realloc(pbuf, nLen);
			if (pbuf)
				WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)s, -1, pbuf, nLen, NULL, NULL);
#else
		pbuf = (LPCTSTR)s;
#endif

			if (pbuf)
				ar.Write(pbuf, nLen - 1);
		}

		s = _T("\n\n</files2>\n\n");

		s += _T("<scripts>");

		POSITION vp = GetFirstViewPosition();
		CView *pv = nullptr;
		CSfxPackagerView *ppv = nullptr;
		while ((pv = GetNextView(vp)) != nullptr)
		{
			if ((ppv = dynamic_cast<CSfxPackagerView *>(pv)) != nullptr)
				break;
		}

		CScriptEditView *pe = nullptr;
		if (ppv)
		{
			pe = dynamic_cast<CScriptEditView *>(ppv->GetSplitterWnd()->GetPane(1, 0));
			if (pe)
				pe->UpdateDocWithActiveScript();
		}

		tstring scr;

		if (!m_Script[EScriptType::INITIALIZE].IsEmpty())
		{
			EscapeString(m_Script[EScriptType::INITIALIZE], scr);
			if (!scr.empty())
				s += _T("\n\n<script type=\"initialize\">"); s += scr.c_str(); s += _T("</script>");
		}

		if (!m_Script[EScriptType::PREINSTALL].IsEmpty())
		{
			EscapeString(m_Script[EScriptType::PREINSTALL], scr);
			if (!scr.empty())
				s += _T("\n\n<script type=\"preinstall\">"); s += scr.c_str(); s += _T("</script>");
		}

		if (!m_Script[EScriptType::PREFILE].IsEmpty())
		{
			EscapeString(m_Script[EScriptType::PREFILE], scr);
			if (!scr.empty())
				s += _T("\n\n<script type=\"prefile\">"); s += scr.c_str(); s += _T("</script>");
		}

		if (!m_Script[EScriptType::POSTFILE].IsEmpty())
		{
			EscapeString(m_Script[EScriptType::POSTFILE], scr);
			if (!scr.empty())
				s += _T("\n\n<script type=\"postfile\">"); s += scr.c_str(); s += _T("</script>");
		}

		if (!m_Script[EScriptType::POSTINSTALL].IsEmpty())
		{
			EscapeString(m_Script[EScriptType::POSTINSTALL], scr);
			if (!scr.empty())
				s += _T("\n\n<script type=\"postinstall\">"); s += scr.c_str(); s += _T("</script>");
		}

		s += _T("\n\n</scripts>\n\n");

		s += _T("</sfxpackager>\n");

#if defined(UNICODE)
		nLen = WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)s, -1, NULL, NULL, NULL, NULL);
		pbuf = (char *)realloc(pbuf, nLen);
		if (pbuf)
			WideCharToMultiByte(CP_UTF8, 0, (LPCTSTR)s, -1, pbuf, nLen, NULL, NULL);
#else
		pbuf = (LPCTSTR)s;
#endif

		if (pbuf)
		{
			ar.Write(pbuf, nLen - 1);

#if defined(UNICODE)
			free(pbuf);
#endif
		}
	}
	else
	{
		CFile *pf = ar.GetFile();

		ULONGLONG sz = pf->GetLength() * sizeof(char);
		char *pbuf = (char *)malloc(sz);
		if (pbuf)
		{
			if (sz == ar.Read(pbuf, (UINT)sz))
			{
				TCHAR *ptbuf;

#if defined(UNICODE)
				int nLen = MultiByteToWideChar(CP_UTF8, 0, pbuf, -1, NULL, NULL);
				ptbuf = (TCHAR *)malloc((nLen + 1) * sizeof(TCHAR));
				MultiByteToWideChar(CP_UTF8, 0, pbuf, -1, ptbuf, nLen);
#else
				ptbuf = pbuf;
#endif
				if (ptbuf)
				{
					TCHAR *settings_start = _tcsstr(ptbuf, _T("<settings2>"));

					if (settings_start)
					{
						TCHAR *files_start = _tcsstr(ptbuf, _T("<files2>"));
						TCHAR *scripts_start = _tcsstr(ptbuf, _T("<scripts>"));

						if (settings_start)
						{
							TCHAR *settings_end = _tcsstr(ptbuf, _T("</settings2>"));

							settings_start += _tcslen(_T("<settings2>"));

							if (settings_end)
								*settings_end = _T('\0');

							tstring s = settings_start;
							m_Props->DeserializeFromXMLString(s);
						}

						if (files_start)
						{
							TCHAR *files_end = _tcsstr(files_start, _T("</files2>"));

							TCHAR *file_start = files_start, *file_end = file_start;
							while (file_start)
							{
								file_start = _tcsstr(file_end, _T("<file2>"));

								if (file_start)
								{
									file_end = _tcsstr(file_start, _T("</file2>"));

									if (file_end)
										*file_end = _T('\0');

									file_start += _tcslen(_T("<file2>"));
									tstring s = file_start;

									UINT key = AddFile();
									TFileDataMap::iterator it = m_FileData.find(key);
									if (it != m_FileData.end())
										it->second->DeserializeFromXMLString(s);

									file_end++;
								}
							}
					
						}

						if (scripts_start)
						{
							TCHAR *scripts_end = _tcsstr(scripts_start, _T("</scripts>"));
							if (scripts_end)
							{
								scripts_end += _tcslen(_T("</scripts>"));
								*scripts_end = _T('\0');

								genio::IParserT *gp = (genio::IParserT *)genio::IParser::Create(genio::IParser::CHAR_MODE::CM_TCHAR);
								gp->SetSourceData(scripts_start, (UINT)(scripts_end - scripts_start));
								ReadScripts(gp);
								gp->Release();
							}
						}
					}
					else
					{
						genio::IParserT *gp = (genio::IParserT *)genio::IParser::Create(genio::IParser::CHAR_MODE::CM_TCHAR);
						gp->SetSourceData(ptbuf, (UINT)sz);
						ReadProject(gp);
						gp->Release();
					}

#if defined(UNICODE)
					free(ptbuf);
#endif
				}

				CMDIFrameWnd *pframe = (CMDIFrameWnd *)(AfxGetApp()->m_pMainWnd);
				CMDIChildWnd *pchild = pframe ? pframe->MDIGetActive() : NULL;
				if (pchild)
				{
					pchild->RedrawWindow();
				}
			}

			free(pbuf);
		}
	}
}

#ifdef SHARED_HANDLERS

// Support for thumbnails
void CSfxPackagerDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
	// Modify this code to draw the document's data
	dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

	CString strText = _T("TODO: implement thumbnail drawing here");
	LOGFONT lf;

	CFont* pDefaultGUIFont = CFont::FromHandle((HFONT) GetStockObject(DEFAULT_GUI_FONT));
	pDefaultGUIFont->GetLogFont(&lf);
	lf.lfHeight = 36;

	CFont fontDraw;
	fontDraw.CreateFontIndirect(&lf);

	CFont* pOldFont = dc.SelectObject(&fontDraw);
	dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
	dc.SelectObject(pOldFont);
}

// Support for Search Handlers
void CSfxPackagerDoc::InitializeSearchContent()
{
	CString strSearchContent;
	// Set search contents from document's data. 
	// The content parts should be separated by ";"

	// For example:  strSearchContent = _T("point;rectangle;circle;ole object;");
	SetSearchContent(strSearchContent);
}

void CSfxPackagerDoc::SetSearchContent(const CString& value)
{
	if (value.IsEmpty())
	{
		RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
	}
	else
	{
		CMFCFilterChunkValueImpl *pChunk = NULL;
		ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
		if (pChunk != NULL)
		{
			pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
			SetChunkValue(pChunk);
		}
	}
}

#endif // SHARED_HANDLERS

// CSfxPackagerDoc diagnostics

#ifdef _DEBUG
void CSfxPackagerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CSfxPackagerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CSfxPackagerDoc commands


void CSfxPackagerDoc::OnCloseDocument()
{
	BeginWaitCursor();

	CDocument::OnCloseDocument();

	CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);
	if (pmf && pmf->GetSafeHwnd())
	{
		CPropertiesWnd &propwnd = pmf->GetPropertiesWnd();
		propwnd.FillPropertyList(NULL);
	}
}
