/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#include "..\Include\Archiver.h"
#include "FastLZArchiver.h"


IArchiver::CREATE_RESULT IArchiver::CreateArchiver(IArchiver **ppia, IArchiveHandle *pah, COMPRESSOR_TYPE ct)
{

	if (ppia)
	{
		DWORD bw;
		UINT32 comp_magic;

		switch (ct)
		{
			case CT_FASTLZ:
				comp_magic = CFastLZArchiver::MAGIC_FASTLZ;
				break;

			default:
				break;
		}

		*ppia = NULL;

		UINT32 magic = IArchiver::MAGIC;
		WriteFile(pah->GetHandle(), &magic, sizeof(UINT32), &bw, NULL);

		WriteFile(pah->GetHandle(), &comp_magic, sizeof(UINT32), &bw, NULL);

		UINT64 flags = 0;
		WriteFile(pah->GetHandle(), &flags, sizeof(UINT64), &bw, NULL);

		switch (ct)
		{
			case CT_FASTLZ:
				*ppia = new CFastLZArchiver(pah);
				break;

			case CT_STOREONLY:
			default:
				break;
		}

		if (*ppia)
			return CR_OK;
	}

	return CR_UNKNOWN_ERROR;
}

void IArchiver::DestroyArchiver(IArchiver **ppia)
{
	if (ppia && *ppia)
	{
		delete *ppia;
		*ppia = NULL;
	}
}


IExtractor::CREATE_RESULT IExtractor::CreateExtractor(IExtractor **ppie, IArchiveHandle *pah)
{
	if (ppie)
	{
		DWORD br;
		UINT32 magic;
		ReadFile(pah->GetHandle(), &magic, sizeof(UINT32), &br, NULL);

		if (magic != IArchiver::MAGIC)
			return CR_BADMAGIC;

		*ppie = NULL;

		ReadFile(pah->GetHandle(), &magic, sizeof(UINT32), &br, NULL);

		UINT64 flags;
		ReadFile(pah->GetHandle(), &flags, sizeof(UINT64), &br, NULL);

		switch (magic)
		{
			case CFastLZArchiver::MAGIC_FASTLZ:
				*ppie = new CFastLZExtractor(pah, flags);
				break;

			default:
				return CR_COMPRESSORUNK;
				break;
		}

		if (*ppie)
			return CR_OK;
	}

	return CR_UNKNOWN_ERROR;
}

void IExtractor::DestroyExtractor(IExtractor **ppie)
{
	if (ppie && *ppie)
	{
		delete *ppie;
		*ppie = NULL;
	}
}
