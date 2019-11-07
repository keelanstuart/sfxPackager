/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfxPackagerDoc.h : interface of the CSfxPackagerDoc class
//


#pragma once

#include "GenParser.h"

#include <vector>

#include "../../Archiver/Include/Archiver.h"

typedef std::vector<tstring> TStringArray;
typedef std::vector<UINT64> TSizeArray;
typedef std::vector<TSizeArray> TZipOffsetArray;

class CSfxPackagerView;

class CSfxHandle;

class CSfxPackagerDoc : public CDocument
{
protected: // create from serialization only
	CSfxPackagerDoc();
	DECLARE_DYNCREATE(CSfxPackagerDoc)

// Attributes
public:
	friend class CSfxHandle;

	UINT AddFile(const TCHAR *filename, const TCHAR *srcpath, const TCHAR *dstpath, const TCHAR *exclude, const TCHAR *scriptsnippet);
	void RemoveFile(UINT handle);

	enum EFileDataType
	{
		FDT_NAME = 0,
		FDT_SRCPATH,
		FDT_DSTPATH,		// the destination where files will be installed
		FDT_EXCLUDE,		// a semi-colon-delimited set of file specifications (wildcards ok) of things to be excluded from the build
		FDT_SNIPPET,		// a script snippet that is appended to the per-file global script

		FDT_NUMTYPES
	};

	enum EScriptType
	{
		INIT = 0,
		PERFILE,
		FINISH,

		NUMTYPES
	};

	enum EMoveType
	{
		UP = 0,
		DOWN,
		TOP,
		BOTTOM
	};

	const TCHAR *GetFileData(UINT handle, EFileDataType fdt);
	void SetFileData(UINT handle, EFileDataType fdt, const TCHAR *data);

	bool AdjustFileOrder(UINT key, EMoveType mt, UINT *swap_key);

	UINT GetNumFiles();

	static CSfxPackagerDoc *GetDoc();

protected:
	struct SFileData
	{
		tstring name;
		tstring srcpath;
		tstring dstpath;
		tstring exclude;	// wildcard exclusion
		tstring snippet;	// script snippet appended to per-file global
	};

	typedef std::map<UINT, SFileData> TFileDataMap;
	typedef std::pair<UINT, SFileData> TFileDataPair;

	TFileDataMap m_FileData;
	UINT m_Key;
	LARGE_INTEGER m_UncompressedSize;

	bool InitializeArchive(CSfxPackagerView *pview, TStringArray &created_archives, TSizeArray &created_archive_filecounts, const TCHAR *basename, UINT span = 0);
	bool AddFileToArchive(CSfxPackagerView *pview, IArchiver *parc, TStringArray &created_archives, TSizeArray &created_archive_filecounts, const TCHAR *srcspec, const TCHAR *excludespec, const TCHAR *scriptsnippet, const TCHAR *dstpath, const TCHAR *dstfilename = NULL, uint64_t *sz_uncomp = NULL, uint64_t *sz_comp = NULL, UINT recursion = 0);
	bool FixupPackage(const TCHAR *filename, const TCHAR *launchcmd, bool span, UINT32 filecount);
	bool SetupSfxExecutable(const TCHAR *filename, UINT span = 0);

	bool CopyFileToTemp(CSfxPackagerView *pview, const TCHAR *srcspec, const TCHAR *dstpath, const TCHAR *dstfilename, const TCHAR *excludespec, UINT recursion = 0);

public:
	CString m_SfxOutputFile;
	CString m_IconFile;
	CString m_ImageFile;
	CString m_Caption;
	CString m_Description;
	CString m_DefaultPath;
	CString m_VersionID;
	bool m_bExploreOnComplete;
	bool m_bRequireAdmin;
	bool m_bRequireReboot;
	bool m_bAllowDestChg;
	CString m_LaunchCmd;
	long m_MaxSize;

	CString m_Script[EScriptType::NUMTYPES];

	LPTSTR m_IconName;

	HANDLE m_hThread;
	HANDLE m_hCancelEvent;

// Operations
public:
	bool CreateSFXPackage(const TCHAR *filename = NULL, CSfxPackagerView *pview = NULL);
	bool CreateTarGzipPackage(const TCHAR *filename = NULL, CSfxPackagerView *pview = NULL);

	static DWORD WINAPI RunCreateSFXPackage(LPVOID param);

	static BOOL CALLBACK EnumTypesFunc(HMODULE hModule, LPTSTR lpType, LONG_PTR lParam);
	static BOOL CALLBACK EnumNamesFunc(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam);
	static BOOL CALLBACK EnumLangsFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLang, LONG_PTR lParam);

// Overrides
public:
	virtual BOOL OnNewDocument();

	void ReadSettings(CGenParser &gp);
	void ReadScripts(CGenParser &gp);
	void ReadFiles(CGenParser &gp);
	void ReadProject(CGenParser &gp);

	virtual void Serialize(CArchive& ar);
#ifdef SHARED_HANDLERS
	virtual void InitializeSearchContent();
	virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS

// Implementation
public:
	virtual ~CSfxPackagerDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
	// Helper function that sets search content for a Search Handler
	void SetSearchContent(const CString& value);
#endif // SHARED_HANDLERS
public:
	virtual void OnCloseDocument();
};
