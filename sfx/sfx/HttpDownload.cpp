/*
Copyright © 2016-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#include "stdafx.h"
#include "HttpDownload.h"
#include <shlwapi.h>

#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <asyncinfo.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "shlwapi.lib")


#define BUFFER_SIZE		8192



CHttpDownloader::CHttpDownloader(int dlflags) : m_DLFlags(dlflags)
{
	m_hInet = NULL;
	m_hUrl = NULL;
	m_bTxStarted = false;
	m_UrlReady = CreateEvent(nullptr, true, false, nullptr);
	m_RxChunk = CreateEvent(nullptr, true, true, nullptr);		// received a chunk

	// Create our internet handle
	m_hInet = InternetOpen(_T("FILE_DOWNLOAD"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, (m_DLFlags & DLFLAG_ASYNC_DOWNLOAD) ? INTERNET_FLAG_ASYNC : 0);

	InternetSetStatusCallback(m_hInet, (INTERNET_STATUS_CALLBACK)wininetStatusCallback);
}


CHttpDownloader::~CHttpDownloader()
{
	if (m_hInet)
	{
		// Critical! Must reset the status callback!
		InternetSetStatusCallback(m_hInet, nullptr);

		// Release the inet handle
		InternetCloseHandle(m_hInet);
	}

	CloseHandle(m_UrlReady);
	CloseHandle(m_RxChunk);
}

extern bool FLZACreateDirectories(const TCHAR *dir);

bool CHttpDownloader::DownloadHttpFile(const TCHAR *szUrl, const TCHAR *szDestFile, HANDLE hEvtAbort, DOWNLOAD_STATUS_CALLBACK *usercb, void *usercb_userdata)
{
	if (!szUrl)
		return false;

	bool retval = false;
	bool aborted = false;

	tstring url = szUrl;
	std::replace(url.begin(), url.end(), _T('\\'), _T('/'));

	TCHAR fname[MAX_PATH] = {0};
	if (szDestFile)
	{
		TCHAR fpath[MAX_PATH];
		_tcscpy_s(fpath, szDestFile);

		PathRemoveFileSpec(fpath);

		if (fpath[0])
			FLZACreateDirectories(fpath);
	}
	else
	{
		_tcscpy_s(fname, PathFindFileName(szUrl));
		szDestFile = fname;
	}

	m_hUrl = NULL;
	m_bTxStarted = false;
	ResetEvent(m_UrlReady);
	SetEvent(m_RxChunk);

	if (!PathIsURL(url.c_str()))
	{
		if (PathFileExists(url.c_str()))
		{
			return CopyFile(url.c_str(), szDestFile, false);
		}
	}

	UINT8 buffer[2][BUFFER_SIZE];

	HANDLE hfile = INVALID_HANDLE_VALUE;

	if (m_hInet)
	{
		DWORD flags = INTERNET_FLAG_RELOAD | 
			INTERNET_FLAG_NO_COOKIES | 
			INTERNET_FLAG_NO_UI | 
			INTERNET_FLAG_PRAGMA_NOCACHE | 
			INTERNET_FLAG_NO_CACHE_WRITE |
			INTERNET_FLAG_KEEP_CONNECTION;

		if (_tcsnicmp(url.c_str(), _T("https"), 5) == 0)
			flags |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_SECURE;

		// Open the url specified
		m_hUrl = InternetOpenUrl(m_hInet, url.c_str(), NULL, 0, flags, (DWORD_PTR)this);

		// if we didn't get the hurl back immediately (which we won't, because it's an async op)
		// then wait until the semaphore clears
		if (!m_hUrl)
		{
			if (GetLastError() != ERROR_IO_PENDING)
				return false;

			if (WaitForSingleObject(m_UrlReady, 10000) == WAIT_TIMEOUT)
				return false;
		}

		// if we succeeded, continue...
		if (m_hUrl)
		{
			TCHAR buf_query[1024];

			DWORD errcode = 0;

			DWORD buflen = sizeof(errcode);
			buf_query[0] = _T('\0');

			// See what the server has to say about this resource...
			if (HttpQueryInfo(m_hUrl, HTTP_QUERY_STATUS_CODE, buf_query, &buflen, NULL))
			{
				errcode = _ttoi(buf_query);
			}

			if (errcode < 400)
			{
				buflen = sizeof(buf_query);
				buf_query[0] = '\0';

				bool chunked = false;
				bool async_write = (m_DLFlags & DLFLAG_ASYNC_WRITE) ? true : false;

				// Determine if the data is encoded in a chunked format
				if (HttpQueryInfo(m_hUrl, HTTP_QUERY_TRANSFER_ENCODING, buf_query, &buflen, NULL))
				{
					if (!_tcsicmp(buf_query, _T("chunked")))
					{
						chunked = true;
						async_write = false;
					}
				}

				buflen = sizeof(buf_query);
				buf_query[0] = '\0';

				// query the length of the file we're grabbing
				if (HttpQueryInfo(m_hUrl, HTTP_QUERY_CONTENT_LENGTH, buf_query, &buflen, NULL) || chunked)
				{
					uint64_t amount_to_download = chunked ? UNKNOWN_EXPECTED_SIZE : _ttoi64(buf_query);

					// If a destination file was specified, then create the file handle
					if (szDestFile)
						hfile = CreateFile(szDestFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | ((m_DLFlags & DLFLAG_ASYNC_WRITE) ? (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED) : 0), NULL);

					if (hfile != INVALID_HANDLE_VALUE)
					{
						if (async_write)
						{
							LARGE_INTEGER atd;
							atd.QuadPart = amount_to_download;
							SetFilePointerEx(hfile, atd, nullptr, FILE_BEGIN);
							SetEndOfFile(hfile);
							SetFileValidData(hfile, atd.QuadPart);
						}

						OVERLAPPED ovr = {0};
						HANDLE data_written_event =  CreateEvent(nullptr, true, true, nullptr);

						uint64_t amount_received = 0, amount_written = 0;
						INTERNET_BUFFERS inbuf[2];

						int bufidx, last_bufidx = -1;

						// initialize our async inet buffers
						for (bufidx = 0; bufidx < 2; bufidx++)
						{
							ZeroMemory(&inbuf[bufidx], sizeof(INTERNET_BUFFERS));

							inbuf[bufidx].dwStructSize = sizeof(INTERNET_BUFFERS);
							inbuf[bufidx].lpvBuffer = (LPVOID)buffer[bufidx];
						}

						bufidx = 1;

						// returns from GetLastError when our async functions return immediately
						DWORD inet_err, file_err;

						HANDLE events[] = {m_RxChunk, ovr.hEvent};

						// if you're reading this and find the logic hard to follow...
						// the internet read operation and the file write operation -- they're both asynchronous
						// the first time through we have not written any data, so the write event must be initially set
						// the last time through, the rx event must not be reset

						m_bTxStarted = true;
						bool received_all = false;

						do
						{
							// if the user aborted, then quit now
							if ((hEvtAbort != INVALID_HANDLE_VALUE) && (WaitForSingleObject(hEvtAbort, 0) == WAIT_OBJECT_0))
							{
								aborted = true;
								break;
							}

							// wait until the data has been read and written before swapping buffers 
							if (WaitForMultipleObjects(_countof(events), events, TRUE, (last_bufidx >= 0) ? 1000 : 0) == WAIT_TIMEOUT)
								continue;

							if (async_write)
							{
								// see how much was written to our file last time
								DWORD last_amount_written = 0;
								if (GetOverlappedResult(hfile, &ovr, &last_amount_written, FALSE))
									amount_written += last_amount_written;
							}

							// see how much was read from our connection last time
							amount_received += inbuf[bufidx].dwBufferLength;
							DWORD amount_to_write = received_all ? 0 : inbuf[bufidx].dwBufferLength;

							// report to the user how much we've received and how much we expect to get overall
							if (usercb)
								usercb(amount_written, amount_to_download, usercb_userdata);

							if (!chunked)
							{
								received_all = (amount_received >= amount_to_download);
							}
							else
							{
								DWORD da = 0;
								if (InternetQueryDataAvailable(m_hUrl, &da, 0, NULL))
								{
									received_all = (da == 0);
									amount_to_download = da;
								}
							}

							// store the last buffer that we used before swapping
							last_bufidx = bufidx;
							bufidx ^= 1;

							// set up our receive buffer size
							inbuf[bufidx].dwBufferLength = std::min<DWORD>(BUFFER_SIZE, (DWORD)std::min<uint64_t>(MAXDWORD, (chunked ? amount_to_download : (amount_to_download - amount_received))));

							// if we have more left to read, then do that...
							if (!received_all && inbuf[bufidx].dwBufferLength)
							{
								ResetEvent(m_RxChunk);

								// Read data from the internet into our current buffer
								if (!InternetReadFileEx(m_hUrl, &inbuf[bufidx], (m_DLFlags & DLFLAG_ASYNC_DOWNLOAD) ? IRF_ASYNC : IRF_SYNC, (DWORD_PTR)this))
								{
									inet_err = GetLastError();

									// if the return wasn't that things are pending, then break out...
									if (inet_err != ERROR_IO_PENDING)
										break;
								}
								else
								{
									SetEvent(m_RxChunk);
								}
							}

							// if the buffer length is non-zero...
							if (amount_to_write)
							{
								if (async_write)
								{
									// set up our async file write stuff
									ZeroMemory(&ovr, sizeof(OVERLAPPED));

									LARGE_INTEGER tmp;
									tmp.QuadPart = amount_written;
									ovr.OffsetHigh = tmp.HighPart;
									ovr.Offset = tmp.LowPart;
									ovr.hEvent = data_written_event;
								}

								// and write whatever data we have
								DWORD bw = 0;
								if (!WriteFile(hfile, buffer[last_bufidx], amount_to_write, async_write ? nullptr : &bw, async_write ? &ovr : nullptr))
								{
									file_err = GetLastError();

									// if the return wasn't that things are pending, then break out...
									if (file_err != ERROR_IO_PENDING)
										break;
								}
								else
								{
									amount_written += bw;
								}
							}
						}
						while (!received_all);

						CloseHandle(data_written_event);

						// we should have the file now
						retval = received_all;

					}
				}
			}

			InternetCloseHandle(m_hUrl);
			m_hUrl = NULL;
		}
	}

	if (hfile != INVALID_HANDLE_VALUE)
	{
		// Our last async write might not be done here, so flush the file before we close...
		FlushFileBuffers(hfile);
		CloseHandle(hfile);
	}

	if (aborted)
		DeleteFile(fname);

	return retval;
}


// This is called by wininet during asynchronous operations
void CALLBACK CHttpDownloader::wininetStatusCallback(HINTERNET hinet, DWORD_PTR context, DWORD inetstat, LPVOID statinfo, DWORD statinfolen)
{
	//Sleep(1);

	// The context should be this
	CHttpDownloader *_this = (CHttpDownloader *)context;
	if (!_this)
		return;

	switch (inetstat)
	{
		// The URL handle was successfully acquired
		case INTERNET_STATUS_HANDLE_CREATED:
		{
			// make sure that the hinet handle is the one this owns
			INTERNET_ASYNC_RESULT *res = (INTERNET_ASYNC_RESULT *)statinfo;
			_this->m_hUrl = (HINTERNET)(res->dwResult);

			break;
		}

		// The request was completed
		case INTERNET_STATUS_REQUEST_COMPLETE:
		{
			if (_this->m_hUrl)
				SetEvent(_this->m_UrlReady);

			if (_this->m_bTxStarted)
				SetEvent(_this->m_RxChunk);

			break;
		}

		case INTERNET_STATUS_RESPONSE_RECEIVED:
		{
			break;
		}

		default:
			break;
	}
}

