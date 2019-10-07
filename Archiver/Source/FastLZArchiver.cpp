/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#include <Windows.h>
#include "FastLZArchiver.h"
#include <Shlwapi.h>
#include <direct.h>

#include "fastlz.h"


CFastLZArchiver::CFastLZArchiver(IArchiveHandle *pah)
{
	m_LastFileTableItemCount = 0;
	m_LastFileTableSize = 0;
	m_OverallFileCount = 0;

	m_MaxSize = -1;

	m_pah = pah;
	m_InitialOffset = m_pah->GetOffset();

	DWORD bw;

	// temporary file table offset
	uint64_t fto_place_holder = 0;
	WriteFile(m_pah->GetHandle(), &fto_place_holder, sizeof(uint64_t), &bw, NULL);
}


CFastLZArchiver::~CFastLZArchiver()
{
}


void CFastLZArchiver::SetMaximumSize(uint64_t maxsize)
{
	m_MaxSize = maxsize;
}

size_t CFastLZArchiver::GetFileCount(IArchiver::INFO_MODE mode)
{
	switch (mode)
	{
		default:
		case IArchiver::IM_WHOLE:
			return m_OverallFileCount;

		case IArchiver::IM_SPAN:
			return m_FileTable.size();
	}

	return 0;
}

// Adds a file to the archive
CFastLZArchiver::ADD_RESULT CFastLZArchiver::AddFile(const TCHAR *src_filename, const TCHAR *dst_filename, uint64_t *sz_uncomp, uint64_t *sz_comp, const TCHAR *scriptsnippet)
{
	CFastLZArchiver::ADD_RESULT ret = AR_OK;

	HANDLE hin = CreateFile(src_filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hin != INVALID_HANDLE_VALUE)
	{
		TCHAR dst_path[MAX_PATH];
		_tcscpy_s(dst_path, MAX_PATH, dst_filename);
		PathRemoveFileSpec(dst_path);

		TCHAR *fn = PathFindFileName(dst_filename);

		SFileTableEntry fte;

		fte.m_Filename = fn;
		fte.m_Path = dst_path;

		// store the size of the uncompressed file
		LARGE_INTEGER fsz;
		GetFileSizeEx(hin, &fsz);
		fte.m_UncompressedSize = fsz.QuadPart;
		if (sz_uncomp)
			*sz_uncomp = fte.m_UncompressedSize;

		// store the file time
		GetFileTime(hin, &(fte.m_FTCreated), NULL, &(fte.m_FTModified));

		SFileBlock b;

		fte.m_Offset = m_pah->GetOffset();

		fte.m_ScriptSnippet = scriptsnippet;

		// read uncompressed blocks of data from the file
		while (b.ReadUncompressedData(hin))
		{
			fte.m_BlockCount++;

			// compress and write the data to the archive
			if (b.CompressData())
			{
				// update the compressed size of the file
				fte.m_CompressedSize += b.m_Header.m_SizeC;
			}
			else
			{
				fte.m_CompressedSize += b.m_Header.m_SizeU;
			}

			b.WriteCompressedData(m_pah->GetHandle());

			// spanning logic
			if ((m_MaxSize != UINT64_MAX) && ((m_pah->GetLength() + (uint64_t)ComputeFileTableSize()) >= m_MaxSize))
			{
				// if we're spanning, then we need to add this entry to the file table now and do some cleanup
				m_FileTable.push_back( fte );

				// reset the block count and compressed size (because this should technically be a new data stream)
				fte.m_BlockCount = 0;
				fte.m_CompressedSize = 0;

				// have the stream handle spanning behind the scenes
				m_pah->Span();

				DWORD bw;
				uint32_t magic = IArchiver::MAGIC;
				WriteFile(m_pah->GetHandle(), &magic, sizeof(uint32_t), &bw, NULL);

				uint32_t comp_magic = CFastLZArchiver::MAGIC_FASTLZ;
				WriteFile(m_pah->GetHandle(), &comp_magic, sizeof(uint32_t), &bw, NULL);

				uint64_t flags = 0;
				WriteFile(m_pah->GetHandle(), &flags, sizeof(uint64_t), &bw, NULL);

				// after the span, we should expect that offset will be different
				m_InitialOffset = m_pah->GetOffset();

				fte.m_Offset = m_pah->GetOffset();

				// mark it as spanned so that the next archive can append to, rather than create, the file
				fte.m_Flags |= SFileTableEntry::FTEFLAG_SPANNED;
			}
		}

		if (fte.m_CompressedSize == fte.m_UncompressedSize)
			ret = AR_OK_UNCOMPRESSED;

		if (sz_comp)
			*sz_comp = (fte.m_CompressedSize != (uint64_t)-1) ? fte.m_CompressedSize : fte.m_UncompressedSize;

		// add the file to the file table
		m_FileTable.push_back( fte );

		m_OverallFileCount++;

		CloseHandle(hin);
	}

	return ret;
}


CFastLZArchiver::FINALIZE_RESULT CFastLZArchiver::Finalize()
{
	// store the file position before writing the file table
	uint64_t file_table_ofs = m_pah->GetOffset();

	// write and clear the file table
	WriteFileTable();
	ClearFileTable();

	DWORD bw;

	LARGE_INTEGER iofs;
	iofs.QuadPart = m_InitialOffset;
	SetFilePointerEx(m_pah->GetHandle(), iofs, NULL, FILE_BEGIN);

	// write the offset of the file table from the beginning of the stream
	WriteFile(m_pah->GetHandle(), &file_table_ofs, sizeof(file_table_ofs), &bw, NULL);

	// store the initial offset in the stream (file header)
	m_InitialOffset -= (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t));
	SetFilePointer(m_pah->GetHandle(), 0, NULL, FILE_END);
	WriteFile(m_pah->GetHandle(), &m_InitialOffset, sizeof(m_InitialOffset), &bw, NULL);

	return FR_OK;
}


size_t CFastLZArchiver::ComputeFileTableSize()
{
	size_t ret;

	// set the initial return value based on whether we had anything in the table before
	if (!m_LastFileTableItemCount)
		ret = sizeof(size_t);			// the overall size of the table includes the the number of entries in the table
	else
		ret = m_LastFileTableSize;		// use the last computed size as the starting point

	for (TFileTable::const_iterator it = m_FileTable.begin() + m_LastFileTableItemCount, last_it = m_FileTable.end(); it != last_it; it++)
	{
		// increment the return by the computed size of the entry
		ret += it->Size();
	}

	// store the last values
	m_LastFileTableItemCount = m_FileTable.size();
	m_LastFileTableSize = ret;

	return ret;
}


bool CFastLZArchiver::WriteFileTable()
{
	bool ret = true;
	DWORD bw;

	// store the number of entries in the file table
	size_t ftec = m_FileTable.size();
	ret &= (bool)WriteFile(m_pah->GetHandle(), &ftec, sizeof(size_t), &bw, NULL);

	for (TFileTable::const_iterator it = m_FileTable.begin(), last_it = m_FileTable.end(); it != last_it; it++)
	{
		// write each entry in the file table
		ret &= it->Write(m_pah->GetHandle());
	}

	return ret;
}



void CFastLZArchiver::ClearFileTable()
{
	m_FileTable.clear();
	m_LastFileTableItemCount = 0;
	m_LastFileTableSize = 0;
}


bool sFileTableEntry::Write(HANDLE hOut) const
{
	bool ret = true;

	uint32_t sz;
	DWORD wb;

	ret &= (bool)WriteFile(hOut, &m_Flags, sizeof(m_Flags), &wb, NULL);

	sz = (uint32_t)m_Filename.size();
	ret &= (bool)WriteFile(hOut, &sz, sizeof(sz), &wb, NULL);
	if (sz)
	{
		ret &= (bool)WriteFile(hOut, m_Filename.c_str(), sizeof(TCHAR) * sz, &wb, NULL);
	}

	sz = (uint32_t)m_Path.size();
	ret &= (bool)WriteFile(hOut, &sz, sizeof(sz), &wb, NULL);
	if (sz)
	{
		ret &= (bool)WriteFile(hOut, m_Path.c_str(), sizeof(TCHAR) * sz, &wb, NULL);
	}

	ret &= (bool)WriteFile(hOut, &m_UncompressedSize, sizeof(m_UncompressedSize), &wb, NULL);
	ret &= (bool)WriteFile(hOut, &m_CompressedSize, sizeof(m_CompressedSize), &wb, NULL);
	ret &= (bool)WriteFile(hOut, &m_Crc, sizeof(m_Crc), &wb, NULL);

	ret &= (bool)WriteFile(hOut, &m_FTCreated, sizeof(m_FTCreated), &wb, NULL);
	ret &= (bool)WriteFile(hOut, &m_FTModified, sizeof(m_FTModified), &wb, NULL);

	ret &= (bool)WriteFile(hOut, &m_BlockCount, sizeof(m_BlockCount), &wb, NULL);
	ret &= (bool)WriteFile(hOut, &m_Offset, sizeof(m_Offset), &wb, NULL);

	sz = (uint32_t)m_ScriptSnippet.size();
	ret &= (bool)WriteFile(hOut, &sz, sizeof(sz), &wb, NULL);
	if (sz)
	{
		ret &= (bool)WriteFile(hOut, m_ScriptSnippet.c_str(), sizeof(TCHAR) * sz, &wb, NULL);
	}

	return ret;
}

bool sFileTableEntry::Read(HANDLE hIn)
{
	bool ret = true;

	uint32_t sz;
	DWORD rb;

	ret &= (bool)ReadFile(hIn, &m_Flags, sizeof(m_Flags), &rb, NULL);

	ret &= (bool)ReadFile(hIn, &sz, sizeof(sz), &rb, NULL);
	if (sz)
	{
		m_Filename.resize(sz, _T('#'));
		ret &= (bool)ReadFile(hIn, (TCHAR *)(m_Filename.data()), sizeof(TCHAR) * sz, &rb, NULL);
	}
	else
	{
		m_Filename.clear();
	}

	ret &= (bool)ReadFile(hIn, &sz, sizeof(sz), &rb, NULL);
	if (sz)
	{
		m_Path.resize(sz, _T('#'));
		ret &= (bool)ReadFile(hIn, (TCHAR *)(m_Path.data()), sizeof(TCHAR) * sz, &rb, NULL);
	}
	else
	{
		m_Path.clear();
	}

	ret &= (bool)ReadFile(hIn, &m_UncompressedSize, sizeof(m_UncompressedSize), &rb, NULL);
	ret &= (bool)ReadFile(hIn, &m_CompressedSize, sizeof(m_CompressedSize), &rb, NULL);
	ret &= (bool)ReadFile(hIn, &m_Crc, sizeof(m_Crc), &rb, NULL);

	ret &= (bool)ReadFile(hIn, &m_FTCreated, sizeof(m_FTCreated), &rb, NULL);
	ret &= (bool)ReadFile(hIn, &m_FTModified, sizeof(m_FTModified), &rb, NULL);

	ret &= (bool)ReadFile(hIn, &m_BlockCount, sizeof(m_BlockCount), &rb, NULL);
	ret &= (bool)ReadFile(hIn, &m_Offset, sizeof(m_Offset), &rb, NULL);

	ret &= (bool)ReadFile(hIn, &sz, sizeof(sz), &rb, NULL);
	if (sz)
	{
		m_ScriptSnippet.resize(sz, _T('#'));
		ret &= (bool)ReadFile(hIn, (TCHAR *)(m_ScriptSnippet.data()), sizeof(TCHAR) * sz, &rb, NULL);
	}
	else
	{
		m_ScriptSnippet.clear();
	}

	return ret;
}


size_t sFileTableEntry::Size() const
{
	size_t ret = (m_Filename.size() + m_Path.size() + m_ScriptSnippet.size()) * sizeof(TCHAR);

	ret +=
		sizeof(uint64_t) + /* m_Flags */ 
		sizeof(uint64_t) + /* m_UncompressedSize */ 
		sizeof(uint64_t) + /* m_CompressedSize */ 
		sizeof(uint32_t) + /* m_Crc */ 
		sizeof(uint32_t) + /* m_BlockCount */ 
		sizeof(uint64_t) + /* m_Offset */ 
		sizeof(FILETIME) + /* m_FTCreated */ 
		sizeof(FILETIME) + /* m_FTModified */ 
		sizeof(uint32_t) + /* m_Filename LENGTH */ 
		sizeof(uint32_t) + /* m_Path LENGTH */
		sizeof(uint32_t);  /* m_ScriptSnippet LENGTH */

	return ret;
}


bool sFileBlock::ReadUncompressedData(HANDLE hIn)
{
	m_Header.m_SizeC = -1;		// invalidate our compressed data once we read new uncompressed data

	if (ReadFile(hIn, m_BufU, FB_UNCOMPRESSED_BUFSIZE, (LPDWORD)&(m_Header.m_SizeU), NULL) && (m_Header.m_SizeU > 0))
	{
		return true;
	}

	return false;
}


bool sFileBlock::CompressData()
{
	if (m_Header.m_SizeU > 0)
	{
		m_Header.m_SizeC = fastlz_compress(m_BufU, m_Header.m_SizeU, m_BufC);

		if (m_Header.m_SizeC >= m_Header.m_SizeU)
		{
			m_Header.m_SizeC = -1;
			return false;
		}

		return true;
	}

	return false;
}


bool sFileBlock::ReadCompressedData(HANDLE hIn)
{
	DWORD br;
	if (ReadFile(hIn, &m_Header, sizeof(sFileBlock::sFileBlockHeader), &br, NULL))
	{
		if (m_Header.m_SizeC == (uint32_t)-1)
		{
			if (ReadFile(hIn, m_BufU, m_Header.m_SizeU, &br, NULL))
			{
				return true;
			}
		}
		else
		{
			if (ReadFile(hIn, m_BufC, m_Header.m_SizeC, &br, NULL))
			{
				return true;
			}
		}
	}

	return false;
}


bool sFileBlock::DecompressData()
{
	if (m_Header.m_SizeC != (uint32_t)-1)
	{
		m_Header.m_SizeU = fastlz_decompress(m_BufC, m_Header.m_SizeC, m_BufU, FB_UNCOMPRESSED_BUFSIZE);

		return true;
	}

	return false;
}


bool sFileBlock::WriteCompressedData(HANDLE hOut)
{
	DWORD bw;
	if (WriteFile(hOut, &m_Header, sizeof(sFileBlock::sFileBlockHeader), &bw, NULL))
	{
		if (m_Header.m_SizeC == (uint32_t)-1)
		{
			if (WriteFile(hOut, m_BufU, m_Header.m_SizeU, &bw, NULL))
			{
				return true;
			}
		}
		else
		{
			if (WriteFile(hOut, m_BufC, m_Header.m_SizeC, &bw, NULL))
			{
				return true;
			}
		}
	}

	return false;
}


bool sFileBlock::WriteUncompressedData(HANDLE hOut)
{
	DWORD bw;

	if (WriteFile(hOut, m_BufU, m_Header.m_SizeU, &bw, NULL))
	{
		return true;
	}

	return false;
}


CFastLZExtractor::CFastLZExtractor(IArchiveHandle *pah, UINT64 flags)
{
	m_pah = pah;

	DWORD br;
	LARGE_INTEGER p;

	uint64_t ftofs;
	ReadFile(m_pah->GetHandle(), &ftofs, sizeof(uint64_t), &br, NULL);

	uint64_t dataofs = m_pah->GetOffset();

	p.QuadPart = ftofs;
	SetFilePointerEx(m_pah->GetHandle(), p, NULL, FILE_BEGIN);

	ReadFileTable();

	p.QuadPart = dataofs;
	SetFilePointerEx(m_pah->GetHandle(), p, NULL, FILE_BEGIN);

	m_CachedFilePosition = m_pah->GetOffset();

	_tgetcwd(m_BasePath, MAX_PATH);
}


CFastLZExtractor::~CFastLZExtractor()
{
}


size_t CFastLZExtractor::GetFileCount()
{
	return m_FileTable.size();
}

bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst)
{
	dst.clear();

	tstring::const_iterator it = src.cbegin(), next_it = std::find(src.cbegin(), src.cend(), _T('%'));

	do
	{
		dst += tstring(it, next_it);

		if (next_it == src.cend())
			break;

		tstring::const_iterator env_start = next_it + 1;

		it = next_it;
		next_it = std::find(env_start, src.cend(), _T('%'));

		if (next_it != src.cend())
		{
			tstring env(env_start, next_it);

			if (!env.empty())
			{
				DWORD sz = GetEnvironmentVariable(env.c_str(), nullptr, 0);
				TCHAR *val = (TCHAR *)_alloca(sizeof(TCHAR) * (sz + 1));
				GetEnvironmentVariable(env.c_str(), val, sz + 1);
				dst += val;
			}
			else
			{
				dst += _T("%");
			}

			it = next_it + 1;
			if (it == src.end())
				next_it = it;
			else
				next_it = std::find(it + 1, src.cend(), _T('%'));
		}
	}
	while (it != src.cend());

	return true;
}

bool ReplaceRegistryKeys(const tstring &src, tstring &dst)
{
	dst.clear();

	tstring::const_iterator it = src.cbegin(), next_it = std::find(src.cbegin(), src.cend(), _T('@'));

	do
	{
		dst += tstring(it, next_it);

		if (next_it == src.cend())
			break;

		tstring::const_iterator reg_start = next_it + 1;

		it = next_it;
		next_it = std::find(reg_start, src.cend(), _T('@'));

		if (next_it != src.cend())
		{
			tstring reg(reg_start, next_it);

			if (!reg.empty())
			{
				HKEY root = HKEY_LOCAL_MACHINE;;
				if (_tcsstr(reg.c_str(), _T("HKEY_CURRENT_USER")) == reg.c_str())
					root = HKEY_CURRENT_USER;
				else if (_tcsstr(reg.c_str(), _T("HKEY_CLASSES_ROOT")) == reg.c_str())
					root = HKEY_CLASSES_ROOT;
				else if (_tcsstr(reg.c_str(), _T("HKEY_CURRENT_CONFIG")) == reg.c_str())
					root = HKEY_CURRENT_CONFIG;
				else if (_tcsstr(reg.c_str(), _T("HKEY_USERS")) == reg.c_str())
					root = HKEY_USERS;

				reg = reg.substr(reg.find(_T('\\')) + 1);
				tstring key = reg;
				size_t sofs = reg.rfind(_T('\\'));
				if (sofs != tstring::npos)
				{
					reg = reg.substr(0, sofs);
					key = key.substr(sofs + 1);

					HKEY hkey;
					if (RegOpenKeyEx(root, reg.c_str(), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
					{
						DWORD cb;
						DWORD type;
						BYTE val[(MAX_PATH + 1) * 2 * sizeof(TCHAR)];
						if (RegGetValue(hkey, nullptr, key.c_str(), RRF_RT_DWORD | RRF_RT_QWORD | RRF_RT_REG_SZ, &type, val, &cb) == ERROR_SUCCESS)
						{
							switch (type)
							{
								case REG_SZ:
								dst += (TCHAR *)val;
								break;

								default:
								break;
							}

						}

						RegCloseKey(hkey);
					}
				}
			}
			else
			{
				dst += _T("@");
			}

			it = next_it + 1;
			if (it == src.end())
				next_it = it;
			else
				next_it = std::find(it + 1, src.cend(), _T('@'));
		}
	}
	while (it != src.cend());

	return true;
}

bool CFastLZExtractor::GetFileInfo(size_t file_idx, tstring *filename, tstring *filepath, uint64_t *csize, uint64_t *usize, FILETIME *ctime, FILETIME *mtime, tstring *snippet)
{
	if (file_idx >= m_FileTable.size())
		return false;

	SFileTableEntry &fte = m_FileTable.at(file_idx);

	if (filename)
	{
		tstring tmp, _tmp;
		ReplaceEnvironmentVariables(fte.m_Filename, _tmp);
		ReplaceRegistryKeys(_tmp, tmp);
		*filename = tmp;
	}

	if (filepath)
	{
		tstring tmp, _tmp;
		ReplaceEnvironmentVariables(fte.m_Path, _tmp);
		ReplaceRegistryKeys(_tmp, tmp);
		*filepath = tmp;
	}

	if (csize)
		*csize = fte.m_CompressedSize;

	if (usize)
		*usize = fte.m_UncompressedSize;

	if (ctime)
		*ctime = fte.m_FTCreated;

	if (mtime)
		*mtime = fte.m_FTModified;

	if (snippet)
		*snippet = fte.m_ScriptSnippet;

	return true;
}

bool FLZACreateDirectories(const TCHAR *dir)
{
	if (PathIsRoot(dir) || PathFileExists(dir))
		return false;

	bool ret = true;

	TCHAR _dir[MAX_PATH];
	_tcscpy_s(_dir, dir);
	PathRemoveFileSpec(_dir);
	ret &= FLZACreateDirectories(_dir);

	ret &= (CreateDirectory(dir, NULL) ? true : false);

	return ret;
}


IExtractor::EXTRACT_RESULT CFastLZExtractor::ExtractFile(size_t file_idx, tstring *output_filename, const TCHAR *override_filename)
{
	if (file_idx >= m_FileTable.size())
		return IExtractor::ER_DONE;

	IExtractor::EXTRACT_RESULT ret = IExtractor::ER_OK;

	SFileTableEntry &fte = m_FileTable.at(file_idx);

	// if we're not where we're supposed to be for the file indicated, then we need to move the file pointer
	if ((m_CachedFilePosition != m_pah->GetOffset()) || (m_CachedFilePosition != fte.m_Offset))
	{
		LARGE_INTEGER p;
		p.QuadPart = fte.m_Offset;
		SetFilePointerEx(m_pah->GetHandle(), p, NULL, FILE_BEGIN);
	}

	TCHAR path[MAX_PATH];

	tstring _cvtpath, cvtpath;
	ReplaceEnvironmentVariables(fte.m_Path, _cvtpath);
	ReplaceRegistryKeys(_cvtpath, cvtpath);

	tstring _cvtfile, cvtfile;
	ReplaceEnvironmentVariables(fte.m_Filename, _cvtfile);
	ReplaceRegistryKeys(_cvtfile, cvtfile);

	if (!override_filename)
	{
		if (PathIsRelative(cvtpath.c_str()))
		{
			_tcscpy_s(path, MAX_PATH, m_BasePath);
			PathAddBackslash(path);

			_tcscat_s(path, MAX_PATH, cvtpath.c_str());
			PathRemoveBackslash(path);
		}
		else
			_tcscpy_s(path, MAX_PATH, cvtpath.c_str());


		FLZACreateDirectories(path);

		PathAddBackslash(path);
		_tcscat_s(path, MAX_PATH, cvtfile.c_str());
	}
	else
	{
		_tcscat_s(path, MAX_PATH, override_filename);
	}

	bool append = false;
	// if we're spanned and the file index we want is 0, then we need to append to the file rather than creating a new one
	if ((fte.m_Flags & SFileTableEntry::FTEFLAG_SPANNED) && (file_idx == 0))
		append = true;

	HANDLE hf = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, append ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf != INVALID_HANDLE_VALUE)
	{
		if (output_filename)
			*output_filename = path;

		if (append)
			SetFilePointer(hf, 0, NULL, FILE_END);

		SFileBlock b;
		for (UINT32 i = 0; i < fte.m_BlockCount; i++)
		{
			b.ReadCompressedData(m_pah->GetHandle());
			b.DecompressData();
			b.WriteUncompressedData(hf);
		}

		SetFileTime(hf, &(fte.m_FTCreated), NULL, &(fte.m_FTModified));

		CloseHandle(hf);
	}
	else
	{
		ret = IExtractor::ER_UNKNOWN_ERROR;
	}

	m_CachedFilePosition = m_pah->GetOffset();

	return ret;
}


void CFastLZExtractor::SetBaseOutputPath(const TCHAR *path)
{
	_tcscpy_s(m_BasePath, MAX_PATH, path);
}


bool CFastLZExtractor::ReadFileTable()
{
	bool ret = true;
	DWORD br;

	// store the number of entries in the file table
	size_t ftec = 0;
	ret &= (bool)ReadFile(m_pah->GetHandle(), &ftec, sizeof(size_t), &br, NULL);

	SFileTableEntry fte;
	for (size_t i = 0; i < ftec; i++)
	{
		// read each entry from the file table
		ret &= fte.Read(m_pah->GetHandle());

		m_FileTable.push_back(fte);
	}

	return ret;
}
