/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once


// CShellTree

class CShellTree : public CMFCShellTreeCtrl
{
	DECLARE_DYNAMIC(CShellTree)

public:
	CShellTree();
	virtual ~CShellTree();

	BOOL SelectPath(LPCTSTR lpszPath);
	BOOL SelectPath(LPCITEMIDLIST lpidl);

protected:
	DECLARE_MESSAGE_MAP()
};


