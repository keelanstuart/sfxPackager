/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#ifdef WINVER
#if WINVER < 0x0600
#undef WINVER
#endif
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions


#include <afxdisp.h>        // MFC Automation classes



#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // MFC support for ribbons and control bars
#include <afxcview.h>           // MFC support for Internet Explorer 4 Common Controls


#include <inttypes.h>

#include <string>
#include <deque>
#include <map>
#include <vector>
#include <algorithm>
#include <ios>

#include <windows.h>
#include <tchar.h>

#include <afxwin.h>
#include <afxwin.h>

typedef std::basic_string<TCHAR> tstring;





#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif


// macros to convert to and from wide- and multi-byte- character
// strings from the other type... all on the stack.
#define LOCAL_WCS2MBCS(wcs, mbcs) {               \
  size_t origsize = _tcslen(wcs) + 1;             \
  size_t newsize = (origsize * 2) * sizeof(char); \
  mbcs = (char *)_alloca(newsize);                \
  size_t retval = 0;                              \
  wcstombs_s(&retval, mbcs, newsize, wcs, newsize); }

#define LOCAL_MBCS2WCS(mbcs, wcs) {               \
  size_t origsize = strlen(mbcs) + 1;             \
  size_t newsize = origsize * sizeof(wchar_t);    \
  wcs = (wchar_t *)_alloca(newsize);              \
  size_t retval = 0;                              \
  mbstowcs_s(&retval, wcs, origsize, mbcs, newsize); }

#if defined(_UNICODE) || defined(UNICODE)

#define LOCAL_TCS2MBCS(tcs, mbcs) LOCAL_WCS2MBCS(tcs, mbcs)

#define LOCAL_TCS2WCS(tcs, wcs) wcs = (wchar_t *)tcs;

#define LOCAL_MBCS2TCS(mbcs, tcs) LOCAL_MBCS2WCS(mbcs, tcs)

#define LOCAL_WCS2TCS(wcs, tcs) tcs = (TCHAR *)wcs;

#else

#define LOCAL_TCS2MBCS(tcs, mbcs) mbcs = (char *)tcs;

#define LOCAL_TCS2WCS(tcs, wcs) LOCAL_MBCS2WCS(tcs, wcs)

#define LOCAL_MBCS2TCS(mbcs, tcs) tcs = (TCHAR *)mbcs;

#define LOCAL_WCS2TCS(tcs, wcs) LOCAL_WCS2MBCS(wcs, tcs)

#endif


typedef std::basic_ios<TCHAR, std::char_traits<TCHAR>> tios;
typedef std::basic_streambuf<TCHAR, std::char_traits<TCHAR>> tstreambuf;
typedef std::basic_istream<TCHAR, std::char_traits<TCHAR>> tistream;
typedef std::basic_ostream<TCHAR, std::char_traits<TCHAR>> tostream;
typedef std::basic_iostream<TCHAR, std::char_traits<TCHAR>> tiostream;
typedef std::basic_stringbuf<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR>> tstringbuf;
typedef std::basic_istringstream<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR>> tistringstream;
typedef std::basic_ostringstream<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR>> tostringstream;
typedef std::basic_stringstream<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR>> tstringstream;
typedef std::basic_filebuf<TCHAR, std::char_traits<TCHAR>> tfilebuf;
typedef std::basic_ifstream<TCHAR, std::char_traits<TCHAR>> tifstream;
typedef std::basic_ofstream<TCHAR, std::char_traits<TCHAR>> tofstream;
typedef std::basic_fstream<TCHAR, std::char_traits<TCHAR>> tfstream;

#include <GenIO.h>
#include <PowerProps.h>
