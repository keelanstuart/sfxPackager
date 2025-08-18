/*
Copyright © 2016-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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
#include <wininet.h>

/*
WARNING: ASYNC DOWNLOADS AND WRITES APPEAR TO HAVE SOME ISSUES. USE AT YOUR OWN RISK.
*/


class CHttpDownloader
{
public:

	enum
	{
		EDLFLAG_ASYNC_DOWNLOAD = 0,
		EDLFLAG_ASYNC_WRITE
	};

#define DLFLAG_ASYNC_DOWNLOAD	(1 << EDLFLAG_ASYNC_DOWNLOAD)
#define DLFLAG_ASYNC_WRITE		(1 << EDLFLAG_ASYNC_WRITE)

	CHttpDownloader(int dlflags = 0);
	~CHttpDownloader();

	enum { UNKNOWN_EXPECTED_SIZE = -1 };
	typedef void (__cdecl DOWNLOAD_STATUS_CALLBACK)(uint64_t bytes_received, uint64_t bytes_expected, void *userdata);

	bool DownloadHttpFile(const TCHAR *szUrl, const TCHAR *szDestFile = nullptr, HANDLE hEvtAbort = INVALID_HANDLE_VALUE, DOWNLOAD_STATUS_CALLBACK *usercb = nullptr, void *usercb_userdata = nullptr);

private:
	static void CALLBACK wininetStatusCallback(HINTERNET hinet, DWORD_PTR context, DWORD inetstat, LPVOID statinfo, DWORD statinfolen);

protected:
	HINTERNET m_hInet;
	HINTERNET m_hUrl;
	bool m_bTxStarted;
	int m_DLFlags;
	HANDLE m_UrlReady, m_InetReady, m_RxChunk;

};