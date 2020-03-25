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

#include "..\Include\Archiver.h"

#include <tchar.h>
#include <string>
#include <deque>


typedef std::basic_string<TCHAR> tstring;

struct sFileTableEntry
{
	enum
	{
		FTEFLAG_SPANNED = 0x0000000000000001,		// a spanned file will be partially in multiple files
	};

	uint64_t m_Flags;
	uint64_t m_UncompressedSize;
	uint64_t m_CompressedSize;
	uint32_t m_Crc;
	uint32_t m_BlockCount;
	uint64_t m_Offset;
	FILETIME m_FTCreated;
	FILETIME m_FTModified;
	tstring m_Filename;
	tstring m_Path;
	tstring m_ScriptSnippet;

	sFileTableEntry()
	{
		m_Flags = 0;
		m_UncompressedSize = m_CompressedSize = 0;
		m_Crc = 0;
		m_BlockCount = 0;
		m_Offset = 0;
	}

	// Store the entry on disk
	bool Write(HANDLE hOut) const;

	// Restore the entry from disk
	bool Read(HANDLE hIn);

	// Return the size of the entry on disk
	size_t Size() const;
};

typedef struct sFileTableEntry SFileTableEntry;

typedef std::deque<SFileTableEntry> TFileTable;


struct sFileBlock
{
	enum
	{
		FB_UNCOMPRESSED_BUFSIZE = 64 * (1 << 10),
		FB_COMPRESSED_BUFSIZE = (FB_UNCOMPRESSED_BUFSIZE * 2)
	};

	struct sFileBlockHeader
	{
		sFileBlockHeader() { m_Flags = 0; m_SizeC = m_SizeU = 0; }

		uint64_t m_Flags;							// flags
		uint32_t m_SizeC;							// compressed size
		uint32_t m_SizeU;							// uncompressed size
	} m_Header;

	BYTE m_BufU[FB_UNCOMPRESSED_BUFSIZE];		// the uncompressed data
	BYTE m_BufC[FB_COMPRESSED_BUFSIZE];			// the compressed data

	bool ReadUncompressedData(HANDLE hIn);
	bool CompressData();
	bool ReadCompressedData(HANDLE hIn);
	bool DecompressData();
	bool WriteCompressedData(HANDLE hOut);
	bool WriteUncompressedData(HANDLE hOut);
};

typedef sFileBlock SFileBlock;


class CFastLZArchiver : public IArchiver
{
public:
	CFastLZArchiver(IArchiveHandle *pah);

	virtual ~CFastLZArchiver();

	// This is the maximum number of bytes that will be written to the stream before the Span method is called
	virtual void SetMaximumSize(uint64_t maxsize);

	virtual size_t GetFileCount(INFO_MODE mode);

	// Adds a file to the archive
	virtual ADD_RESULT AddFile(const TCHAR *src_filename, const TCHAR *dst_filename, uint64_t *sz_uncomp = nullptr, uint64_t *sz_comp = nullptr, const TCHAR *scriptsnippet = nullptr);

	virtual FINALIZE_RESULT Finalize();

	enum { MAGIC_FASTLZ = 'FSTL' };

protected:

	size_t ComputeFileTableSize();
	bool WriteFileTable();
	void ClearFileTable();

	size_t m_LastFileTableItemCount;
	size_t m_LastFileTableSize;
	TFileTable m_FileTable;
	size_t m_OverallFileCount;

	IArchiveHandle *m_pah;
	uint64_t m_InitialOffset;

	uint64_t m_MaxSize;

};

class CFastLZExtractor : public IExtractor
{
public:
	CFastLZExtractor(IArchiveHandle *pah, UINT64 flags);

	virtual ~CFastLZExtractor();

	virtual size_t GetFileCount();

	virtual bool GetFileInfo(size_t file_idx, tstring *filename = NULL, tstring *filepath = NULL, uint64_t *csize = NULL, uint64_t *usize = NULL, FILETIME *ctime = NULL, FILETIME *mtime = NULL, tstring *snippet = NULL);

	virtual EXTRACT_RESULT ExtractFile(size_t file_idx, tstring *output_filename = NULL, const TCHAR *override_filename = NULL, bool test_only = false);

	virtual void SetBaseOutputPath(const TCHAR *path);

protected:

	bool ReadFileTable();

	TFileTable m_FileTable;
	uint64_t m_CachedFilePosition;

	IArchiveHandle *m_pah;

	TCHAR m_BasePath[MAX_PATH];
};

