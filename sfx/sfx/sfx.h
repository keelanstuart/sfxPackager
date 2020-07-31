/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfx.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include "TinyJS.h"
#include "LicenseEntryDlg.h"


// CSfxApp:
// See sfx.cpp for the implementation of this class
//

class CSfxApp : public CWinApp
{
public:
	CSfxApp();

	CString m_Caption;
	CString m_InstallPath;
	CString m_RunCommand;
	CString m_VersionID;
	UINT32 m_Flags;
	LARGE_INTEGER m_SpaceRequired;
	UINT32 m_CompressedFileCount;
	UINT32 m_ZipParts;

	bool m_TestOnlyMode;

	props::IPropertySet *m_Props;

	CTinyJS m_js;

	CLicenseKeyEntryDlg *m_LicenseDlg;

	enum EScriptType
	{
		INITIALIZE = 0,
		PREINSTALL,
		PREFILE,
		POSTFILE,
		POSTINSTALL,

		NUMTYPES
	};

	tstring m_Script[EScriptType::NUMTYPES];

	typedef std::map<tstring, int64_t> TIntMap;
	TIntMap m_jsGlobalIntMap;

// Overrides
public:
	virtual BOOL InitInstance();
	virtual BOOL ExitInstance();
	virtual BOOL ProcessMessageFilter(int code, LPMSG lpMsg);

// Implementation
	void Echo(const TCHAR *msg);

	DECLARE_MESSAGE_MAP()
};

extern CSfxApp theApp;

extern bool IsScriptEmpty(const tstring &scr);
