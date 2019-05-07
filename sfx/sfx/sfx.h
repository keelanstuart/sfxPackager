/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CSfxApp theApp;