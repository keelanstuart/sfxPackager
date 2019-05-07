/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once

enum ESfxFlagShift
{
	FS_EXPLORE = 0,
	FS_SPAN,
	FS_ADMINONLY,
	FS_ALLOWDESTCHG,
};

#define SFX_FLAG_EXPLORE			(1 << FS_EXPLORE)				// show a check box on the installer that will allow the user to explore the destination folder
#define SFX_FLAG_SPAN				(1 << FS_SPAN)					// internal use only: indicates that the archive spans multiple files
#define SFX_FLAG_ADMINONLY			(1 << FS_ADMINONLY)				// the install requires admin rights and will try to elevate privileges
#define SFX_FLAG_ALLOWDESTCHG		(1 << FS_ALLOWDESTCHG)			// indicates that the installer allows the destination directory to be changed

struct sFixupResourceData
{
	char m_Ident[16];

	TCHAR m_LaunchCmd[MAX_PATH];
	TCHAR m_VersionID[MAX_PATH];
	UINT32 m_Flags;
	LARGE_INTEGER m_SpaceRequired;
	UINT32 m_CompressedFileCount;
};

typedef struct sFixupResourceData SFixupResourceData;



#define SFX_FIXUP_SEARCH_STRING		"CADBURY EGGS!!!"


#define KB		* (1 << 10)
#define MB		* (1 << 20)
#define GB		* (1 << 30)