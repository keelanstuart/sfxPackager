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

class CGenParser
{
public:
	CGenParser();

	~CGenParser();

	enum TOKEN_TYPE
	{
		TT_NONE = 0,

		TT_IDENT,		// [a-z,A-Z,_]+[0-9,a-z,A-Z,_]*
		TT_STRING,		// ["'][...]*["']
		TT_NUMBER,		// [-][0-9]+[.][0-9]*
		TT_SYMBOL,		// [!@#$%^&*()`~-=+[]{}<>/?;':",.|\]
		TT_HEXNUMBER,	// [0][xX][0-9,a-f,A-F]+

		TT_NUMTOKENTYPES
	};

	void SetSourceData(const TCHAR *data, UINT datalen);

	bool NextToken();
	bool NextLine();

	TOKEN_TYPE GetCurrentTokenType();
	TCHAR *GetCurrentTokenString();

	bool IsToken(TCHAR *s, bool case_sensitive = false);

protected:
	TCHAR *m_data;
	UINT m_datalen;
	UINT m_pos;

	TOKEN_TYPE m_curType;
	tstring m_curStr;
};

