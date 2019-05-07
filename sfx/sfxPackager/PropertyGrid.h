/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once


// CPropertyGrid

class CPropertyGrid : public CMFCPropertyGridCtrl
{
	DECLARE_DYNAMIC(CPropertyGrid)

public:
	CPropertyGrid();
	virtual ~CPropertyGrid();

	virtual void OnClickButton(CPoint point);

	bool IsLocked() { return m_bLocked; }

protected:
	bool m_bLocked;

	DECLARE_MESSAGE_MAP()
public:
	virtual void OnPropertyChanged(CMFCPropertyGridProperty* pProp) const;
};


