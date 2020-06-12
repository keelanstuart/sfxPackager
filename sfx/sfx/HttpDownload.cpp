/*
Copyright © 2016-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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

#if defined(_DEBUG) || defined(DEBUG)

#if !defined(DOWNLOADER_USES_WININET)

#if defined(_M_X64)
#pragma comment(lib, "libcurl_64D.lib")
#else
#pragma comment(lib, "libcurl_32D.lib")
#endif

#endif

#else

#if !defined(DOWNLOADER_USES_WININET)

#if defined(_M_X64)
#pragma comment(lib, "libcurl_64.lib")
#else
#pragma comment(lib, "libcurl_32.lib")
#endif

#endif

#endif

#if defined(DOWNLOADER_USES_WININET)
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "Crypt32.lib")
#endif


#define BUFFER_SIZE		8192


UINT CHttpDownloader::sCommFailures = 0;


CHttpDownloader::CHttpDownloader()
{
#if defined(DOWNLOADER_USES_WININET)
	m_hInet = NULL;
	m_hUrl = NULL;
	m_SemReqComplete = CreateSemaphore(NULL, 1, 1, NULL);

	// Create our internet handle
	m_hInet = InternetOpen(_T("File Download"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);

	InternetSetStatusCallback(m_hInet, (INTERNET_STATUS_CALLBACK)DownloadStatusCallback);
#else
	m_pCURL = curl_easy_init();
#endif
}


CHttpDownloader::~CHttpDownloader()
{
#if defined(DOWNLOADER_USES_WININET)
	if (m_hInet)
	{
		// Critical! Must reset the status callback!
		InternetSetStatusCallback(m_hInet, (INTERNET_STATUS_CALLBACK)NULL);

		// Release the inet handle
		InternetCloseHandle(m_hInet);
		m_hInet = NULL;
	}

	if (m_SemReqComplete)
	{
		CloseHandle(m_SemReqComplete);
		m_SemReqComplete = NULL;
	}
#else
	if (m_pCURL)
	{
		curl_easy_cleanup(m_pCURL);
	}
#endif
}

extern bool FLZACreateDirectories(const TCHAR *dir);

BOOL CHttpDownloader::DownloadHttpFile(const TCHAR *szUrl, const TCHAR *szDestFile, const TCHAR *szDestDir, float *ppct, BOOL *pabortque, UINT expected_size)
{
	if (!szUrl)
		return false;

	if (sCommFailures > 10)
		return false;

	BOOL retval = false;

	tstring url = szUrl;
	std::replace(url.begin(), url.end(), _T('\\'), _T('/'));

    FLZACreateDirectories(szDestDir);

#if defined(DOWNLOADER_USES_WININET)

	TCHAR filepath[MAX_PATH];
	filepath[0] = '\0';

	// If a destination file was specified, then create the file handle
	if (szDestFile)
	{
		// If we are writing to a particular directory, then prepend it here...
		if (szDestDir)
		{
			_tcscpy_s(filepath, MAX_PATH, szDestDir);
		}

		// append the file name
		PathAppend(filepath, szDestFile);
	}

	if (!PathIsURL(url.c_str()))
	{
		if (PathFileExists(url.c_str()))
		{
			return CopyFile(url.c_str(), filepath, false);
		}
	}

	UINT8 buffer[2][BUFFER_SIZE];

	if (m_hInet)
	{
		if (WaitForSingleObject(m_SemReqComplete, 2000) == WAIT_TIMEOUT)
		{
			return false;
		}

        DWORD flags = INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI;
        
        if (_tcsnicmp(url.c_str(), _T("https"), 5) == 0)
            flags |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_SECURE;

        // Open the url specified
        m_hUrl = InternetOpenUrl(m_hInet, url.c_str(), NULL, 0, flags, (DWORD_PTR)this);

		// if we didn't get the hurl back immediately (which we won't, because it's an async op)
		// then wait until the semaphore clears
		if (!m_hUrl)
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				if (WaitForSingleObject(m_SemReqComplete, 10000) == WAIT_TIMEOUT)
				{
					return false;
				}
			}
		}

		// if we succeeded, continue...
		if (m_hUrl)
		{
			TCHAR buf_query[1024];
			buf_query[0] = '\0';

			DWORD errcode = 0;
			DWORD buflen = sizeof(errcode);

			// See what the server has to say about this resource...
			if (HttpQueryInfo(m_hUrl, HTTP_QUERY_STATUS_CODE, buf_query, &buflen, NULL))
			{
				errcode = _ttoi(buf_query);
			}

            if (errcode < 400)
            {
                buflen = sizeof(buf_query);
                buf_query[0] = '\0';

                BOOL chunked = false;

                // Determine if the data is encoded in a chunked format
                if (HttpQueryInfo(m_hUrl, HTTP_QUERY_TRANSFER_ENCODING, buf_query, &buflen, NULL))
                {
                    if (!_tcsicmp(buf_query, _T("chunked")))
                    {
                        chunked = true;
                    }
                }

                buflen = sizeof(buf_query);

                // query the length of the file we're grabbing
                if (HttpQueryInfo(m_hUrl, HTTP_QUERY_CONTENT_LENGTH, buf_query, &buflen, NULL) || chunked)
                {
                    HANDLE hfile = NULL;
                    BOOL empty_file = true;

                    // If a destination file was specified, then create the file handle
                    if (szDestFile)
                    {
                        // create the file
                        hfile = CreateFile(filepath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hfile == (HANDLE)-1)
                            hfile = 0;
                    }

                    uint64_t amount_to_download = chunked ? BUFFER_SIZE : _ttoi64(buf_query);

                    if (amount_to_download && (errcode < 400))
                    {
                        empty_file = false;

                        uint64_t amount_downloaded = 0;
						uint64_t amount_remaining = amount_to_download;
                        INTERNET_BUFFERS inbuf[2];

                        int bufidx;

                        // initialize our async inet buffers
                        for (bufidx = 0; bufidx < 2; bufidx++)
                        {
                            ZeroMemory(&inbuf[bufidx], sizeof(INTERNET_BUFFERS));

                            inbuf[bufidx].dwStructSize = sizeof(INTERNET_BUFFERS);
                            inbuf[bufidx].lpvBuffer = (LPVOID)buffer[bufidx];
                            inbuf[bufidx].dwBufferLength = BUFFER_SIZE;
                            inbuf[bufidx].Next = bufidx ? &inbuf[0] : &inbuf[1];
                        }

                        bufidx = 1;

                        do
                        {
                            bufidx ^= 1;

                            // Read data from the internet into our current buffer
                            if (!InternetReadFileEx(m_hUrl, &inbuf[bufidx], IRF_NO_WAIT, (DWORD_PTR)this))
                            {
                                DWORD lasterr = GetLastError();
                                if (lasterr == ERROR_IO_PENDING)
                                {
                                    if (WaitForSingleObject(m_SemReqComplete, 20000) == WAIT_TIMEOUT)
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }

                            // if the buffer length is non-zero...
                            if (inbuf[bufidx].dwBufferLength)
                            {
                                void *data_start = inbuf[bufidx].lpvBuffer;

                                uint64_t data_size = std::min<uint64_t>(amount_remaining, uint64_t(inbuf[bufidx].dwBufferLength));

                                // and if we have a valid file handle, write data
                                if (hfile)
                                {
                                    DWORD bwritten;
                                    WriteFile(hfile, data_start, (DWORD)data_size, &bwritten, NULL);
                                }

                                amount_downloaded += data_size;
								amount_remaining -= data_size;

                                if (ppct)
                                    *ppct = chunked ? 0 : (float)amount_downloaded / (float)amount_to_download;

                                inbuf[bufidx].dwBufferLength = BUFFER_SIZE;
                            }

                            // The abort que was set, so we're closing the file, deleting it, and getting out...
                            if (pabortque && *pabortque)
                            {
                                if (hfile)
                                {
                                    CloseHandle(hfile);
                                    hfile = NULL;
                                    DeleteFile(filepath);
                                }

                                // Exit the download loop
                                break;
                            }
                        }
                        while ((inbuf[bufidx].dwBufferLength > 0) && (amount_downloaded < amount_to_download));

                        // inbuf[bufidx].dwBufferLength will be 0 when all data is read

                        // we should have the file now
                        retval = (amount_downloaded == amount_to_download);
                    }

                    if (hfile)
                    {
                        CloseHandle(hfile);

                        // we have an empty file because it didn't exist on the server
                        if (empty_file)
                        {
                            DeleteFile(filepath);
                        }
                    }
                }
            }

            InternetCloseHandle(m_hUrl);
			m_hUrl = NULL;

			// Release the semaphore
			ReleaseSemaphore(m_SemReqComplete, 1, NULL);
		}
	}

#else

	if (m_pCURL)
	{
		struct _stat statbuf;
		BOOL newfile = true;

		tstring filename = szDestDir;
		if ((filename.back() != _T('\\')) && (filename.back() != _T('/')))
			filename += _T("\\");
		filename += szDestFile;

		int fd;
		if (!_tsopen_s(&fd, filename.c_str(), _O_RDONLY | _O_BINARY, _SH_DENYWR, _S_IWRITE | _S_IREAD) && (fd != -1))
		{
			_fstat(fd, &statbuf);
			_close(fd);
			newfile = false;
	
			if ((expected_size == DL_KNOWN_SIZE_UNAVAILABLE) || ((expected_size != statbuf.st_size) && (expected_size == DL_UNKNOWN_SIZE)))
			{
				newfile = true;
			}
		}

		TCHAR tmpfilename[MAX_PATH];
		_tcsncpy_s(tmpfilename, filename.c_str() , MAX_PATH - 1);
		_tcsncat_s(tmpfilename, _T("_"), MAX_PATH - 1);

		FILE *f;
		_tfopen_s(&f, tmpfilename, _T("wb"));

		if (f)
		{
			CURLcode curlret;

			TCHAR *canonUrl = (TCHAR *)_alloca(((_tcslen(url.c_str()) * 3) + 1) * sizeof(TCHAR));;
			DWORD cch = (UINT)_tcslen(url.c_str()) * 3;
			HRESULT hr = UrlCanonicalize(url.c_str(), canonUrl, &cch, URL_ESCAPE_UNSAFE);
			canonUrl[cch] = '\0';

			int len = WideCharToMultiByte(CP_UTF8, 0, canonUrl, -1, NULL, 0, NULL, NULL);
			char *tmpurl = NULL;
			if (len)
			{
				tmpurl = (char *)_alloca(len + 1);
				if (tmpurl)
				{
					WideCharToMultiByte(CP_UTF8, 0, canonUrl, -1, tmpurl, len, NULL, NULL);
					tmpurl[len] = '\0';
				}
			}

			BOOL download_error = true;
			UINT trycount = 1;
			while (trycount)
			{
				curlret = curl_easy_setopt(m_pCURL, CURLOPT_URL, tmpurl);

				if (curlret == CURLE_OK)
					curlret = curl_easy_setopt(m_pCURL, CURLOPT_TIMECONDITION, newfile ? CURL_TIMECOND_NONE : CURL_TIMECOND_IFMODSINCE);

				if (curlret == CURLE_OK)
					curlret = curl_easy_setopt(m_pCURL, CURLOPT_TIMEVALUE, newfile ? 0 : statbuf.st_mtime);

				if (curlret == CURLE_OK)
					curlret = curl_easy_setopt(m_pCURL, CURLOPT_WRITEDATA, f);

				if (curlret == CURLE_OK)
					curlret = curl_easy_setopt(m_pCURL, CURLOPT_CONNECTTIMEOUT, 5);

				if (curlret == CURLE_OK)
					curlret = curl_easy_setopt(m_pCURL, CURLOPT_TIMEOUT, 5);

				if (curlret == CURLE_OK)
					curlret = curl_easy_perform(m_pCURL);

				if (curlret == CURLE_OK)
				{
					DWORD retcode;
					curl_easy_getinfo(m_pCURL, CURLINFO_RESPONSE_CODE, &retcode);

					if (retcode == 404)
					{
						retval = false;
						download_error = true;
						break;
					}
					else if (retcode == 304)
					{
						retval = true;
						download_error = false;
						break;
					}
					else if ((retcode == 200) || (retcode == 302))
					{
						long newfiletime;
						curl_easy_getinfo(m_pCURL, CURLINFO_FILETIME, &newfiletime);
						statbuf.st_mtime = newfiletime;

						DeleteFile(filename.c_str());
						retval = true;
						newfile = true;
						download_error = false;
						break;
					}
					else
					{
						retval = false;
						newfile = true;
					}
				}
				else
				{
					sCommFailures++;
					break;
				}

				trycount--;
			}

			fclose(f);

			if (newfile && !download_error)
			{
				// move the temp file into the real destination
				MoveFile(tmpfilename, filename.c_str());

#if 0
				// fix up the file time
				_utimbuf tb;
				tb.modtime = statbuf.st_mtime;
				tb.actime = statbuf.st_mtime;
				_tutime(szDestFile, &tb);
#endif
			}
			else
			{
				DeleteFile(tmpfilename);
			}
		}
	}

#endif

	return retval;
}

#if defined(DOWNLOADER_USES_WININET)

// This is called by wininet during asynchronous operations
void CALLBACK CHttpDownloader::DownloadStatusCallback(HINTERNET hinet, DWORD_PTR context, DWORD inetstat, LPVOID statinfo, DWORD statinfolen)
{
	Sleep(0);

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
			// release the semaphore if this owns the internet handle
			ReleaseSemaphore(_this->m_SemReqComplete, 1, NULL);

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

#endif