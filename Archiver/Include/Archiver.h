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

#include <Windows.h>
#include <tchar.h>
#include <string>

#if !defined(tstring)
typedef std::basic_string<TCHAR> tstring;
#endif 

// Implement a class that exposes this interface in your application...
// It will, in a stereotypical use case, need to open and maintain a HANDLE to a file on disk and
// handle spanning to another file if the archiver requires it.
class IArchiveHandle
{
public:

	// Returns a handle that IArchiver will write to.  This is assumed to be windows handle that could be used by WriteFile, etc.
	virtual HANDLE GetHandle() = NULL;

	// This is used as a callback by IArchiver-derived classes when more data has been added to the file than is allowed.
	// For example, if streaming to a file handle and the size of that file may not exceed a given length,
	// this function could close the current handle and open a new file for writing
	// if Span returns true, the Archiver should continue adding data to whatever GetHandle returns
	// if Span returns false, the Archiver should return AR_SPANFAIL without adding any more data
	virtual bool Span() = NULL;

	// Returns the length of the object at the handle
	virtual uint64_t GetLength() = NULL;

	// Returns the offset position for the handle from the beginning
	virtual uint64_t GetOffset() = NULL;

	// Releases any resources allocated by the archive handle
	virtual void Release() = NULL;
};

class IArchiver
{

public:

	enum CREATE_RESULT
	{
		CR_OK = 0,

		CR_UNKNOWN_ERROR
	};

	enum COMPRESSOR_TYPE
	{
		CT_STOREONLY = 0,

		CT_FASTLZ,

		CT_NUMTYPES
	};

	enum ADD_RESULT
	{
		AR_OK = 0,
		AR_OK_UNCOMPRESSED,

		AR_SPANFAIL,

		AR_UNKNOWN_ERROR
	};

	enum FINALIZE_RESULT
	{
		FR_OK = 0,

		FR_UNKNOWN_ERROR
	};

	enum INFO_MODE
	{
		IM_WHOLE = 0,		// information about the whole archive

		IM_SPAN				// information about only the current span
	};

	enum { MAGIC = 'MAGI' };

	// Creates and destroys the archiver
	static CREATE_RESULT CreateArchiver(IArchiver **ppia, IArchiveHandle *pah, COMPRESSOR_TYPE ct);
	static void DestroyArchiver(IArchiver **ppia);

	// This is the maximum number of bytes that will be written to the stream before the Span method is called
	virtual void SetMaximumSize(uint64_t maxsize) = NULL;

	// Returns the number of files that are in the archive (either the whole thing or just the current span)
	virtual size_t GetFileCount(INFO_MODE mode) = NULL;

	// Adds a file to the archive
	// src_filename can be either absolute or relative
	// dst_filename should always be relative
	virtual ADD_RESULT AddFile(const TCHAR *src_filename, const TCHAR *dst_filename, uint64_t *sz_uncomp = nullptr, uint64_t *sz_comp = nullptr, const TCHAR *prefile_scriptsnippet = nullptr, const TCHAR *postfile_scriptsnippet = nullptr) = NULL;

	// Finalizes the output, performing any operations that may be necessary to later extract and decompress the data (writing file tables, etc)
	virtual FINALIZE_RESULT Finalize() = NULL;
};


class IExtractor
{

public:

	enum CREATE_RESULT
	{
		CR_OK = 0,

		CR_BADMAGIC,

		CR_COMPRESSORUNK,

		CR_UNKNOWN_ERROR
	};

	enum COMPRESSOR_TYPE
	{
		CT_STOREONLY = 0,

		CT_FASTLZ,

		CT_NUMTYPES
	};

	enum EXTRACT_RESULT
	{
		ER_OK = 0,

		ER_SPAN,

		ER_DONE,

		ER_MUSTDOWNLOAD,

		ER_SKIP,

		ER_UNKNOWN_ERROR
	};

	// Creates and destroys the extractor
	static CREATE_RESULT CreateExtractor(IExtractor **ppie, IArchiveHandle *pah);
	static void DestroyExtractor(IExtractor **ppie);

	// Returns the number of files that are in the archive
	virtual size_t GetFileCount() = NULL;

	virtual bool GetFileInfo(size_t file_idx, tstring *filename = NULL, tstring *filepath = NULL, uint64_t *csize = NULL, uint64_t *usize = NULL, FILETIME *ctime = NULL, FILETIME *mtime = NULL, tstring *prefile_scriptsnippet = nullptr, tstring *postfile_scriptsnippet = nullptr) = NULL;

	// Extracts the next file from the archive - this is assumed to be a serial process where the whole
	// archive will be extracted at once, so no choice as to which file to extract is provided
	// filename_buf will be filled with the absolute path that the file was extracted to, which
	// is the relative path stored in the archive combined with the base output path provided
	virtual EXTRACT_RESULT ExtractFile(size_t file_idx, tstring *output_filename = NULL, const TCHAR *override_filename = NULL, bool test_only = false) = NULL;

	// Sets the base output path of the extractor
	virtual void SetBaseOutputPath(const TCHAR *path) = NULL;
};
