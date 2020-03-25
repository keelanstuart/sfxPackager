/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

// ShellTree.cpp : implementation file
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "ShellTree.h"


// CShellTree

IMPLEMENT_DYNAMIC(CShellTree, CMFCShellTreeCtrl)

CShellTree::CShellTree()
{

}

CShellTree::~CShellTree()
{
}


BEGIN_MESSAGE_MAP(CShellTree, CMFCShellTreeCtrl)
END_MESSAGE_MAP()

BOOL CShellTree::SelectPath(LPCTSTR lpszPath)
{
	ASSERT_VALID(this);
	ASSERT_VALID(afxShellManager);
	ENSURE(lpszPath != NULL);

	LPITEMIDLIST pidl;
	if (FAILED(afxShellManager->ItemFromPath(lpszPath, pidl)))
	{
		return FALSE;
	}

	BOOL bRes = SelectPath(pidl);
	afxShellManager->FreeItem(pidl);

	return bRes;
}

BOOL CShellTree::SelectPath(LPCITEMIDLIST lpidl)
{
	BOOL bRes = FALSE;

	ASSERT_VALID(this);
	ASSERT_VALID(afxShellManager);

	if (lpidl == NULL)
	{
		ASSERT(FALSE);
		return FALSE;
	}

	HTREEITEM htreeItem = GetRootItem();

	SetRedraw(FALSE);

	if (afxShellManager->GetItemCount(lpidl) == 0)
	{
		// Desktop
	}
	else
	{
		LPCITEMIDLIST lpidlCurr = lpidl;
		LPITEMIDLIST lpidlParent;

		CList<LPITEMIDLIST,LPITEMIDLIST> lstItems;
		lstItems.AddHead(afxShellManager->CopyItem(lpidl));

		while (afxShellManager->GetParentItem(lpidlCurr, lpidlParent) > 0)
		{
			lstItems.AddHead(lpidlParent);
			lpidlCurr = lpidlParent;
		}

		for (POSITION pos = lstItems.GetHeadPosition(); pos != NULL;)
		{
			LPITEMIDLIST lpidlList = lstItems.GetNext(pos);

			if (htreeItem != NULL)
			{
				if (GetChildItem(htreeItem) == NULL)
				{
					Expand(htreeItem, TVE_EXPAND);
				}

				BOOL bFound = FALSE;

				for (HTREEITEM hTreeChild = GetChildItem(htreeItem); !bFound && hTreeChild != NULL; hTreeChild = GetNextSiblingItem(hTreeChild))
				{
					LPAFX_SHELLITEMINFO pItem = (LPAFX_SHELLITEMINFO) GetItemData(hTreeChild);
					if (pItem == NULL)
					{
						continue;
					}

					SHFILEINFO sfi1;
					SHFILEINFO sfi2;

					if (SHGetFileInfo((LPCTSTR) pItem->pidlFQ, 0, &sfi1, sizeof(sfi1), SHGFI_PIDL | SHGFI_DISPLAYNAME) &&
						SHGetFileInfo((LPCTSTR) lpidlList, 0, &sfi2, sizeof(sfi2), SHGFI_PIDL | SHGFI_DISPLAYNAME))
					{
						if (lstrcmpi(sfi1.szDisplayName, sfi2.szDisplayName) == 0)
						{
							bFound = TRUE;
							htreeItem = hTreeChild;
						}
					}
				}

				if (!bFound)
				{
					htreeItem = NULL;
				}
			}

			afxShellManager->FreeItem(lpidlList);
		}
	}

	if (htreeItem != NULL)
	{
		m_bNoNotify = TRUE;

		SelectItem(htreeItem);

		if (GetChildItem(htreeItem) == NULL)
		{
			Expand(htreeItem, TVE_EXPAND);
		}

		EnsureVisible(htreeItem);

		m_bNoNotify = FALSE;
		bRes = TRUE;
	}

	SetRedraw();
	RedrawWindow();

	return bRes;
}


// CShellTree message handlers


