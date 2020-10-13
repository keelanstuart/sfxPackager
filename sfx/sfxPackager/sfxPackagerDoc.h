/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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

#include <vector>
#include <set>

#include "../../Archiver/Include/Archiver.h"
#include "OutputWnd.h"

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

	UINT AddFile(const TCHAR *filename = nullptr, const TCHAR *srcpath = nullptr, const TCHAR *dstpath = nullptr, const TCHAR *exclude = nullptr, const TCHAR *prefile_scriptsnippet = nullptr, const TCHAR *postfile_scriptsnippet = nullptr);
	void RemoveFile(UINT handle);

	enum EFileDataType
	{
		FDT_NAME = 0,
		FDT_SRCPATH,
		FDT_DSTPATH,			// the destination where files will be installed
		FDT_EXCLUDE,			// a semi-colon-delimited set of file specifications (wildcards ok) of things to be excluded from the build
		FDT_PREFILE_SNIPPET,	// a script snippet that is appended to the pre-file global script and executed before a file installed
		FDT_POSTFILE_SNIPPET,	// a script snippet that is appended to the post-file global script and executed after a file is installed

		FDT_NUMTYPES
	};

	enum EScriptType
	{
		INITIALIZE = 0,
		PREINSTALL,
		PREFILE,
		POSTFILE,
		POSTINSTALL,

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
	props::IPropertySet *GetFileProperties(UINT handle);

	bool AdjustFileOrder(UINT key, EMoveType mt, UINT *swap_key);

	UINT GetNumFiles();

	const TCHAR *LastBuiltInstallerFilename() { return m_LastBuiltInstallerFilename.c_str(); }

	static CSfxPackagerDoc *GetDoc();

protected:

	typedef enum eFileProp
	{
		FILENAME = 'NAME',				// the name of the source file(s) (wildcard ok)
		SOURCE_PATH = 'SRCP',			// the path to the source file(s) 
		DESTINATION_PATH = 'DSTP',		// the path where the file will be extracted to. If relative, is relative to the target directory, but can be absolute
		EXCLUDING = 'EXCL',				// a comma-delimited wildcard match of files to exclude
		PREFILE_SNIPPET = 'PRES',		// a script that is appended to the per-file global, executed BEFORE the given file has been decompressed
		POSTFILE_SNIPPET = 'SNPT',		// a script that is appended to the per-file global, executed AFTER the given file has been decompressed
	} EFILEPROP;

	typedef props::IPropertySet * SFileData;

	typedef std::map<UINT, SFileData> TFileDataMap;

	TFileDataMap m_FileData;
	UINT m_Key;

	tstring m_LastBuiltInstallerFilename;

	bool InitializeArchive(CSfxPackagerView *pview, TStringArray &created_archives, TSizeArray &created_archive_filecounts, const TCHAR *basename, UINT span = 0);
	bool AddFileToArchive(CSfxPackagerView *pview, IArchiver *parc, TStringArray &created_archives, TSizeArray &created_archive_filecounts, const TCHAR *srcspec, const TCHAR *excludespec, const TCHAR *prefile_scriptsnippet, const TCHAR *postfile_scriptsnippet, const TCHAR *dstpath, const TCHAR *dstfilename = NULL, uint64_t *sz_uncomp = NULL, uint64_t *sz_comp = NULL, UINT recursion = 0);
	bool FixupPackage(const TCHAR *filename, const TCHAR *launchcmd, bool span, UINT32 filecount);
	bool SetupSfxExecutable(const TCHAR *filename, UINT span = 0);

	bool CopyFileToTemp(CSfxPackagerView *pview, const TCHAR *srcspec, const TCHAR *dstpath, const TCHAR *dstfilename, const TCHAR *excludespec, UINT recursion = 0);

public:
	typedef const TCHAR *TResType;
	typedef std::set<const TCHAR *> TResNameSet;
	typedef std::map<TResType, TResNameSet> TResMap;

	TResMap m_ResMap, m_PackageResMap;

	typedef enum eDocProp
	{
		OUTPUT_FILE = 'OUTP',
		OUTPUT_FILE_SUFFIX_MODE = 'OFSF',
		OUTPUT_MODE = 'OPMD',
		MAXIMUM_SIZE_MB = 'MXSZ',
		ICON_FILE = 'ICON',
		IMAGE_FILE = 'IMAG',
		CAPTION = 'CPTN',
		WELCOME_MESSAGE = 'WLCM',
		LICENSE_MESSAGE = 'LICM',
		VERSION = 'VERS',
		DEFAULT_DESTINATION = 'DDST',
		ALLOW_DESTINATION_CHANGE = 'ADCH',
		REQUIRE_ADMIN = 'ADMN',
		REQUIRE_REBOOT = 'BOOT',
		ENABLE_EXPLORE_CHECKBOX = 'ENEX',
		LAUNCH_COMMAND = 'LNCH',
		VERSION_PRODUCTNAME = 'VPRD',
		VERSION_DESCRIPTION = 'VDSC',
		VERSION_COPYRIGHT = 'VCPY',
		COMPRESSION_BLOCKSIZE = 'CBSZ',

	} EDOCPROP;

	props::IPropertySet *m_Props;

	LARGE_INTEGER m_UncompressedSize;

	CString m_Script[EScriptType::NUMTYPES];

	HANDLE m_hThread;
	HANDLE m_hCancelEvent;

// Operations
public:
	bool CreateSFXPackage(const TCHAR *filename = nullptr, CSfxPackagerView *pview = nullptr);

	static DWORD WINAPI RunCreateSFXPackage(LPVOID param);

	static BOOL CALLBACK EnumTypesFunc(HMODULE hModule, LPCTSTR lpType, LONG_PTR lParam);
	static BOOL CALLBACK EnumNamesFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, LONG_PTR lParam);
	static BOOL CALLBACK EnumLangsFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLang, LONG_PTR lParam);

// Overrides
public:
	virtual BOOL OnNewDocument();

	void ReadSettings(genio::IParserT *gp);
	void ReadScripts(genio::IParserT *gp);
	void ReadFiles(genio::IParserT *gp);
	void ReadProject(genio::IParserT *gp);

	void LogMessage(COutputWnd::EOutputType t, const TCHAR *msg);

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

	static const TCHAR *GetFileBrowseFilter(props::FOURCHARCODE property_id);
	static const TCHAR *GetPropertyDescription(props::FOURCHARCODE property_id);

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
