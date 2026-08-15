/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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

using SFileTableEntry = struct sFileTableEntry
{
	enum
	{
		FTEFLAG_SPANNED			= 0x00000001,		// a spanned file will be partially in multiple files
		FTEFLAG_DOWNLOAD		= 0x00000002,		// an empty file that is just a download reference
		FTEFLAG_BLOCKSIZEMASK	= 0x0000001C,		// a mask of 3 bits (0-7) that indicates the size of the block
		FTEFLAG_CONTINUED		= 0x00000020,		// continued in the next span
	};

	#define FTEFLAG_BLOCKSIZESHIFT		2			// mask off the bytes with BLOCKSIZEMASK, then shift right by this amount

	uint32_t m_Flags;
	uint64_t m_UncompressedSize;
	uint64_t m_CompressedSize;
	uint32_t m_Crc;
	uint32_t m_BlockCount;
	uint64_t m_Offset;
	FILETIME m_FTCreated;
	FILETIME m_FTModified;
	tstring m_Filename;
	tstring m_Path;
	tstring m_PreFileScriptSnippet;
	tstring m_PostFileScriptSnippet;

	sFileTableEntry()
	{
		m_Flags = 0;
		m_UncompressedSize = m_CompressedSize = 0;
		m_Crc = 0;
		m_BlockCount = 0;
		m_Offset = 0;
	}

	// Store the entry on disk
	bool Write(HANDLE hOut, const uint8_t *key) const;

	// Restore the entry from disk
	bool Read(HANDLE hIn, const uint8_t *key);

	// Return the size of the entry on disk
	size_t Size() const;
};

using TFileTable = std::deque<SFileTableEntry>;


#define ENCRYPTION_KEY_LENGTH			32

struct sFileBlock
{

#define MAX_UNCOMPRESSED_BUFSIZE		256

	enum
	{
		FB_UNCOMPRESSED_BUFSIZE = MAX_UNCOMPRESSED_BUFSIZE * (1 << 10),
		FB_COMPRESSED_BUFSIZE = (FB_UNCOMPRESSED_BUFSIZE * 2)
	};

	struct sFileBlockHeader
	{
		sFileBlockHeader() { m_Flags = 0; m_SizeC = m_SizeU = 0; }

		uint32_t m_Flags;							// flags
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

	bool CryptData(const uint8_t *key, uint64_t nonce);
};

typedef sFileBlock SFileBlock;


#define EF_ENCRYPTED			0x0001
#define EF_FAILED_DECRYPTION	0x0002

class CFastLZArchiver : public IArchiver
{
public:
	CFastLZArchiver(IArchiveHandle *pah, uint32_t flags, const TCHAR *password, uint64_t salt);

	virtual ~CFastLZArchiver();

	// This is the maximum number of bytes that will be written to the stream before the Span method is called
	virtual void SetMaximumSize(uint64_t maxsize);

	virtual size_t GetFileCount(INFO_MODE mode);

	// Adds a file to the archive
	virtual ADD_RESULT AddFile(const TCHAR *src_filename, const TCHAR *dst_filename, uint64_t *sz_uncomp = nullptr, uint64_t *sz_comp = nullptr, const TCHAR *prefile_scriptsnippet = nullptr, const TCHAR *postfile_scriptsnippet = nullptr);

	// Sets the size of the pre-compressed blocks after the time of this call
	virtual void SetCompressionBlockSize(IArchiver::EBufferSize sz) { m_BlockSize = sz; }

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

	uint32_t m_Flags;

	uint64_t m_MaxSize;

	IArchiver::EBufferSize m_BlockSize;

	uint8_t m_EncryptionKey[ENCRYPTION_KEY_LENGTH];
};

class CFastLZExtractor : public IExtractor
{

public:

	CFastLZExtractor(IArchiveHandle *pah, uint32_t flags, const TCHAR *password);

	virtual ~CFastLZExtractor();

	virtual size_t GetFileCount();

	virtual bool GetFileInfo(size_t file_idx, tstring *filename = NULL, tstring *filepath = NULL, uint64_t *csize = NULL, uint64_t *usize = NULL, FILETIME *ctime = NULL, FILETIME *mtime = NULL, tstring *prefile_snippet = NULL, tstring *postfile_snippet = NULL);

	virtual EXTRACT_RESULT ExtractFile(size_t file_idx, tstring *output_filename = NULL, const TCHAR *override_filename = NULL, bool test_only = false);

	virtual void SetBaseOutputPath(const TCHAR *path);

	bool DecryptionFailed() { return (m_Flags & EF_FAILED_DECRYPTION); }

protected:

	bool ReadFileTable();

	TFileTable m_FileTable;
	uint64_t m_CachedFilePosition;

	uint32_t m_Flags;

	// encryption items
	uint8_t m_EncryptionKey[ENCRYPTION_KEY_LENGTH];

	IArchiveHandle *m_pah;

	TCHAR m_BasePath[MAX_PATH];
};

