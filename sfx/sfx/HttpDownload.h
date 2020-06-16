/*
Copyright © 2016-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#pragma once

#include <windows.h>
#include <string>
#include <algorithm>
#include <tchar.h>

typedef std::basic_string<TCHAR> tstring;


#include <wininet.h>

class CHttpDownloader
{
public:
	CHttpDownloader(bool async = false);
	~CHttpDownloader();

	typedef void (__cdecl DOWNLOAD_STATUS_CALLBACK)(uint64_t bytes_received, uint64_t bytes_expected);

	bool DownloadHttpFile(const TCHAR *szUrl, const TCHAR *szDestFile, const TCHAR *szDestDir, HANDLE hEvtAbort = INVALID_HANDLE_VALUE, DOWNLOAD_STATUS_CALLBACK *usercb = nullptr);

private:
	static void CALLBACK wininetStatusCallback(HINTERNET hinet, DWORD_PTR context, DWORD inetstat, LPVOID statinfo, DWORD statinfolen);

protected:
	HINTERNET m_hInet;
	HINTERNET m_hUrl;
	bool m_bTxStarted;
	bool m_bAsync;
	HANDLE m_UrlReady, m_InetReady, m_RxChunk;

};