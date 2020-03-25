/*
Copyright © 2016-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#pragma once

#define DOWNLOADER_USES_WININET

#if defined(DOWNLOADER_USES_WININET)
#include <wininet.h>
#else
#include <curl/curl.h>
#include <curl/easy.h>
#endif

class CHttpDownloader
{
public:
	CHttpDownloader();
	~CHttpDownloader();

#define DL_KNOWN_SIZE_UNAVAILABLE	-1
#define DL_UNKNOWN_SIZE				-2
	BOOL DownloadHttpFile(const TCHAR *szUrl, const TCHAR *szDestFile, const TCHAR *szDestDir, float *ppct = NULL, BOOL *pabortque = NULL, UINT expected_size = DL_UNKNOWN_SIZE);

private:
#if defined(DOWNLOADER_USES_WININET)
	static void CALLBACK DownloadStatusCallback(HINTERNET hinet, DWORD_PTR context, DWORD inetstat, LPVOID statinfo, DWORD statinfolen);
#endif

protected:
#if defined(DOWNLOADER_USES_WININET)
	HINTERNET m_hInet;
	HINTERNET m_hUrl;
	HANDLE m_SemReqComplete;
#else
	CURL *m_pCURL;
#endif

	static UINT sCommFailures;
};