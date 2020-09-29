#include "stdafx.h"
#include "sfx.h"
#include "HttpDownload.h"
#include <chrono>
#include <ctime>

extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);
extern bool FLZACreateDirectories(const TCHAR *dir);

// ******************************************************************************
// ******************************************************************************

HRESULT CreateShortcut(const TCHAR *targetFile, const TCHAR *targetArgs, const TCHAR *linkFile, const TCHAR *description,
					   int showMode, const TCHAR *curDir, const TCHAR *iconFile, int iconIndex)
{
	HRESULT hr = E_INVALIDARG;

	if ((targetFile && *targetFile) && (linkFile && linkFile))
	{
		CoInitialize(NULL);

		IShellLink *pl;
		hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&pl);

		if (SUCCEEDED(hr))
		{
			hr = pl->SetPath(targetFile);
			hr = pl->SetArguments(targetArgs ? targetArgs : _T(""));

			if (description && *description)
				hr = pl->SetDescription(description);

			if (showMode > 0)
				hr = pl->SetShowCmd(showMode);

			if (curDir && curDir)
				hr = pl->SetWorkingDirectory(curDir);

			if (iconFile && *iconFile && (iconIndex >= 0))
				hr = pl->SetIconLocation(iconFile, iconIndex);

			IPersistFile* pf;
			hr = pl->QueryInterface(IID_IPersistFile, (LPVOID*)&pf);
			if (SUCCEEDED(hr))
			{
				wchar_t *fn;
				LOCAL_TCS2WCS(linkFile, fn);

				hr = pf->Save(fn, TRUE);
				pf->Release();
			}
			pl->Release();
		}

		CoUninitialize();
	}

	return (hr);
}

// ******************************************************************************
// ******************************************************************************

void scSetGlobalInt(CScriptVar *c, void *userdata)
{
	tstring name = c->getParameter(_T("name"))->getString();
	int64_t val = c->getParameter(_T("val"))->getInt();

	std::pair<CSfxApp::TIntMap::iterator, bool> insret = theApp.m_jsGlobalIntMap.insert(CSfxApp::TIntMap::value_type(name, val));
	if (!insret.second)
		insret.first->second = val;
}


void scGetGlobalInt(CScriptVar *c, void *userdata)
{
	tstring name = c->getParameter(_T("name"))->getString();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
	{
		CSfxApp::TIntMap::iterator it = theApp.m_jsGlobalIntMap.find(name);

		ret->setInt((it != theApp.m_jsGlobalIntMap.end()) ? it->second : 0);
	}
}


void scMessageBox(CScriptVar *c, void *userdata)
{
	tstring title = c->getParameter(_T("title"))->getString();
	tstring msg = c->getParameter(_T("msg"))->getString();

	::MessageBox(NULL, msg.c_str(), title.c_str(), MB_OK);
}


void scMessageBoxYesNo(CScriptVar *c, void *userdata)
{
	tstring title = c->getParameter(_T("title"))->getString();
	tstring msg = c->getParameter(_T("msg"))->getString();

	bool bret = (::MessageBox(NULL, msg.c_str(), title.c_str(), MB_YESNO) == IDYES) ? 1 : 0;
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(bret);
}


void scCreateDirectoryTree(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	bool create_result = TRUE;
	if (!theApp.m_TestOnlyMode)
	{
		create_result = FLZACreateDirectories(path.c_str());
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(create_result ? 1 : 0);
}


void scCopyFile(CScriptVar *c, void *userdata)
{
	tstring src = c->getParameter(_T("src"))->getString(), _src;
	ReplaceEnvironmentVariables(src, _src);
	ReplaceRegistryKeys(_src, src);

	tstring dst = c->getParameter(_T("dst"))->getString(), _dst;
	ReplaceEnvironmentVariables(dst, _dst);
	ReplaceRegistryKeys(_dst, dst);

	BOOL copy_result;
	if (!theApp.m_TestOnlyMode)
	{
		copy_result = CopyFile(src.c_str(), dst.c_str(), false);
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(copy_result ? 1 : 0);
}


void scRenameFile(CScriptVar *c, void *userdata)
{
	tstring filename = c->getParameter(_T("filename"))->getString(), _filename;
	ReplaceEnvironmentVariables(filename, _filename);
	ReplaceRegistryKeys(_filename, filename);

	tstring newname = c->getParameter(_T("newname"))->getString(), _newname;
	ReplaceEnvironmentVariables(newname, _newname);
	ReplaceRegistryKeys(_newname, newname);

	int rename_result = TRUE;
	if (!theApp.m_TestOnlyMode)
	{
		rename_result = _trename(filename.c_str(), newname.c_str());
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt((rename_result == 0) ? 1 : 0);
}


void scDeleteFile(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);


	BOOL delete_result = TRUE;
	if (!theApp.m_TestOnlyMode)
	{
		delete_result = DeleteFile(path.c_str());
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(delete_result ? 1 : 0);
}


void scFileExists(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	BOOL result = PathFileExists(path.c_str());
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scFilenameHasExtension(CScriptVar *c, void *userdata)
{
	tstring filename = c->getParameter(_T("filename"))->getString(), _filename;
	ReplaceEnvironmentVariables(filename, _filename);
	ReplaceRegistryKeys(_filename, filename);

	tstring ext = c->getParameter(_T("ext"))->getString();

	TCHAR *p = PathFindExtension(filename.c_str());
	if (p && (*p == _T('.')))
		p++;

	bool result = false;
	if (p)
		result = (_tcsicmp(p, ext.c_str()) == 0) ? true : false;

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scIsDirectory(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	BOOL result = PathIsDirectory(path.c_str());
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}


void scIsDirectoryEmpty(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	BOOL result = PathIsDirectoryEmpty(path.c_str());
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scCreateSymbolicLink(CScriptVar *c, void *userdata)
{
	tstring targetname = c->getParameter(_T("targetname"))->getString(), _targetname;
	ReplaceEnvironmentVariables(targetname, _targetname);
	ReplaceRegistryKeys(_targetname, targetname);

	tstring linkname = c->getParameter(_T("linkname"))->getString(), _linkname;
	ReplaceEnvironmentVariables(linkname, _linkname);
	ReplaceRegistryKeys(_linkname, linkname);

	DWORD flags = 0;
	if (PathIsDirectory(targetname.c_str()))
		flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

	BOOL result = CreateSymbolicLink(linkname.c_str(), targetname.c_str(), flags);
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scCreateShortcut(CScriptVar *c, void *userdata)
{
	tstring file = c->getParameter(_T("file"))->getString(), _file;
	ReplaceEnvironmentVariables(file, _file);
	ReplaceRegistryKeys(_file, file);

	tstring targ = c->getParameter(_T("target"))->getString(), _targ;
	ReplaceEnvironmentVariables(targ, _targ);
	ReplaceRegistryKeys(_targ, targ);

	tstring args = c->getParameter(_T("args"))->getString(), _args;
	ReplaceEnvironmentVariables(args, _args);
	ReplaceRegistryKeys(_args, args);

	tstring rundir = c->getParameter(_T("rundir"))->getString(), _rundir;
	ReplaceEnvironmentVariables(rundir, _rundir);
	ReplaceRegistryKeys(_rundir, rundir);

	tstring desc = c->getParameter(_T("desc"))->getString(), _desc;
	ReplaceEnvironmentVariables(desc, _desc);
	ReplaceRegistryKeys(_desc, desc);

	int64_t showmode = c->getParameter(_T("showmode"))->getInt();

	tstring icon = c->getParameter(_T("icon"))->getString(), _icon;
	ReplaceEnvironmentVariables(icon, _icon);
	ReplaceRegistryKeys(_icon, icon);

	int64_t iconidx = c->getParameter(_T("iconidx"))->getInt();

	if (!theApp.m_TestOnlyMode)
	{
		CreateShortcut(targ.c_str(), args.c_str(), file.c_str(), desc.c_str(),
					   (int)showmode, rundir.c_str(), icon.c_str(), (int)iconidx);
	}
}


void scGetGlobalEnvironmentVariable(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	tstring var = c->getParameter(_T("varname"))->getString(), _var;
	ReplaceEnvironmentVariables(var, _var);
	ReplaceRegistryKeys(_var, var);

	tstring rs;
	DWORD sz = GetEnvironmentVariable(var.c_str(), nullptr, 0);
	if (sz > 0)
	{
		rs.resize(sz, _T('#'));
		GetEnvironmentVariable(var.c_str(), (TCHAR *)(rs.data()), sz);
	}

	ret->setString(rs);
}


void scSetGlobalEnvironmentVariable(CScriptVar *c, void *userdata)
{
	tstring var = c->getParameter(_T("varname"))->getString(), _var;
	ReplaceEnvironmentVariables(var, _var);
	ReplaceRegistryKeys(_var, var);

	tstring val = c->getParameter(_T("val"))->getString(), _val;
	ReplaceEnvironmentVariables(val, _val);
	ReplaceRegistryKeys(_val, val);

	if (!theApp.m_TestOnlyMode)
	{
		CRegKey cKey;
		if (SUCCEEDED(cKey.Create(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"))))
		{
			cKey.SetStringValue(var.c_str(), val.c_str());
		}
	}
}


void scRegistryKeyValueExists(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	ret->setInt(0);

	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	for (tstring::iterator it = key.begin(), last_it = key.end(); it != last_it; it++)
	{
		if (*it == _T('/'))
			*it = _T('\\');
	}

	tstring name = c->getParameter(_T("name"))->getString(), _name;
	ReplaceEnvironmentVariables(name, _name);
	ReplaceRegistryKeys(_name, name);

	CRegKey cKey;
	if (SUCCEEDED(cKey.Open(hr, key.c_str())) && cKey.m_hKey)
	{
		if (!name.empty())
		{
			TCHAR tmp;
			ULONG tmps = 1;
			if (cKey.QueryStringValue(name.c_str(), &tmp, &tmps) != ERROR_FILE_NOT_FOUND)
				ret->setInt(1);
		}

		cKey.Close();
	}
}


void scGetRegistryKeyValue(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	std::replace(key.begin(), key.end(), _T('/'), _T('\\'));

	tstring valname = c->getParameter(_T("name"))->getString(), _valname;
	ReplaceEnvironmentVariables(valname, _valname);
	ReplaceRegistryKeys(_valname, valname);

	if (!theApp.m_TestOnlyMode)
	{
		HKEY hkey;
		if (RegOpenKeyEx(hr, key.c_str(), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			DWORD cb;
			DWORD type;
			BYTE val[(MAX_PATH + 1) * 2 * sizeof(TCHAR)];
			if (RegGetValue(hkey, nullptr, valname.c_str(), RRF_RT_DWORD | RRF_RT_QWORD | RRF_RT_REG_SZ, &type, val, &cb) == ERROR_SUCCESS)
			{
				switch (type)
				{
					case REG_SZ:
						ret->setString((const TCHAR *)val);
						break;

					case REG_DWORD:
						ret->setInt((int64_t)(DWORD((*(const DWORD *)val))));
						break;

					case REG_QWORD:
						ret->setInt((int64_t)(QWORD((*(const QWORD *)val))));
						break;

					default:
						break;
				}

			}

			RegCloseKey(hkey);
		}
	}
}


void scSetRegistryKeyValue(CScriptVar *c, void *userdata)
{
	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	std::replace(key.begin(), key.end(), _T('/'), _T('\\'));

	tstring name = c->getParameter(_T("name"))->getString(), _name;
	ReplaceEnvironmentVariables(name, _name);
	ReplaceRegistryKeys(_name, name);

	tstring val = c->getParameter(_T("val"))->getString(), _val;
	ReplaceEnvironmentVariables(val, _val);
	ReplaceRegistryKeys(_val, val);

	if (!theApp.m_TestOnlyMode)
	{
		CRegKey cKey;
		if (SUCCEEDED(cKey.Create(hr, key.c_str())))
		{
			cKey.SetStringValue(name.c_str(), val.c_str());
		}
	}
}


void scDeleteRegistryKey(CScriptVar *c, void *userdata)
{
	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	std::replace(key.begin(), key.end(), _T('/'), _T('\\'));

	tstring subkey = c->getParameter(_T("subkey"))->getString(), _subkey;
	ReplaceEnvironmentVariables(subkey, _subkey);
	ReplaceRegistryKeys(_subkey, subkey);
	std::replace(subkey.begin(), subkey.end(), _T('/'), _T('\\'));

	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		ret->setInt(0);

	if (!theApp.m_TestOnlyMode)
	{
		HKEY hkey;
		if (RegOpenKeyEx(hr, key.c_str(), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			if (SUCCEEDED(RegDeleteKey(hkey, subkey.c_str())))
			{
				if (!ret)
					ret->setInt(1);
			}

			RegCloseKey(hkey);
		}
	}
}


void scSpawnProcess(CScriptVar *c, void *userdata)
{
	CSfxApp *_this = (CSfxApp *)userdata;

	tstring cmd = c->getParameter(_T("cmd"))->getString(), _cmd;
	ReplaceEnvironmentVariables(cmd, _cmd);
	ReplaceRegistryKeys(_cmd, cmd);

	tstring params = c->getParameter(_T("params"))->getString(), _params;
	ReplaceEnvironmentVariables(params, _params);
	ReplaceRegistryKeys(_params, params);

	tstring rundir = c->getParameter(_T("rundir"))->getString(), _rundir;
	ReplaceEnvironmentVariables(rundir, _rundir);
	ReplaceRegistryKeys(_rundir, rundir);

	bool block = c->getParameter(_T("block"))->getBool();

	tstring arg;
	arg.reserve((cmd.length() + params.length()) * 2);

	if (!params.empty())
		arg += _T("\"");

	arg += cmd;

	if (!params.empty())
	{
		arg += _T("\" ");
		arg += params;
	}

	tstring msg;
	if (!rundir.empty())
	{
		msg += rundir;
		msg += _T("> ");
	}
	msg += arg.c_str();

	_this->Echo(_T("Spawning Process:\t"));
	_this->Echo(msg.c_str());
	_this->Echo(_T("\r\n"));

	BOOL created = true;
	if (!theApp.m_TestOnlyMode)
	{
		STARTUPINFO si = { 0 };
		si.cb = sizeof(si);

		PROCESS_INFORMATION pi;

#if 0
		SECURITY_ATTRIBUTES saAttr; 

		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		saAttr.bInheritHandle = TRUE; 
		saAttr.lpSecurityDescriptor = NULL; 

		HANDLE hChildStdOutR, hChildStdOutW;
		HANDLE hChildStdInR, hChildStdInW;

		// Create a pipe for the child process's STDOUT. 
		CreatePipe(&hChildStdOutR, &hChildStdOutW, &saAttr, 0);
		SetHandleInformation(hChildStdOutR, HANDLE_FLAG_INHERIT, 0);

		// Create a pipe for the child process's STDIN. 
		CreatePipe(&hChildStdInR, &hChildStdInW, &saAttr, 0);
		SetHandleInformation(hChildStdInW, HANDLE_FLAG_INHERIT, 0);

		si.hStdError = hChildStdOutW;
		si.hStdOutput = hChildStdOutW;
		si.hStdInput = hChildStdInR;
		si.dwFlags |= STARTF_USESTDHANDLES;
#endif

		created = CreateProcess(nullptr, (TCHAR *)(arg.c_str()), NULL, NULL, FALSE, NULL, NULL, rundir.empty() ? NULL : rundir.c_str(), &si, &pi);
		if (created)
		{
			if (block)
				WaitForSingleObject(pi.hProcess, INFINITE);

#if 0
			CloseHandle(hChildStdOutW);
			CloseHandle(hChildStdInR);
#endif
		}
		else
		{
			LPVOID lpMsgBuf;
			DWORD dw = GetLastError();
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						  NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

			// Display the error
			_this->Echo(_T("SpawnProcess Failed:\t"));
			_this->Echo((const TCHAR *)lpMsgBuf);
			_this->Echo(_T("\r\n"));

			// Free resources created by the system
			LocalFree(lpMsgBuf);
		}
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(created ? 1 : 0);
}

void scGetExeVersion(CScriptVar* c, void* userdata)
{
	tstring fn = c->getParameter(_T("file"))->getString(), _fn;
	ReplaceEnvironmentVariables(fn, _fn);
	ReplaceRegistryKeys(_fn, fn);

	TCHAR v[128];
	_stprintf_s(v, 127, _T("%d.%d.%d.%d"), 0, 0, 0, 0);

	if (PathFileExists(fn.c_str()))
	{
		DWORD  verHandle = 0;
		UINT   size = 0;
		LPBYTE lpBuffer = NULL;
		DWORD  verSize = GetFileVersionInfoSize(fn.c_str(), &verHandle);

		if (verSize != NULL)
		{
			LPSTR verData = new char[verSize];

			if (GetFileVersionInfo(fn.c_str(), verHandle, verSize, verData))
			{
				if (VerQueryValue(verData, _T("\\"), (VOID FAR * FAR*) & lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							_stprintf_s(v, 127, _T("%d.%d.%d.%d"),
										(verInfo->dwFileVersionMS >> 16) & 0xffff,
										(verInfo->dwFileVersionMS >> 0) & 0xffff,
										(verInfo->dwFileVersionLS >> 16) & 0xffff,
										(verInfo->dwFileVersionLS >> 0) & 0xffff);
						}
					}
				}
			}

			delete[] verData;
		}
	}

	CScriptVar* ret = c->getReturnVar();
	if (ret)
		ret->setString(tstring(v));
}


void scCompareStrings(CScriptVar* c, void* userdata)
{
	tstring str1 = c->getParameter(_T("str1"))->getString();
	tstring str2 = c->getParameter(_T("str2"))->getString();

	int cmp = _tcscmp(str1.c_str(), str2.c_str());

	CScriptVar* ret = c->getReturnVar();
	if (ret)
		ret->setInt(cmp);
}


void scToUpper(CScriptVar* c, void* userdata)
{
	tstring str = c->getParameter(_T("str"))->getString();

	std::transform(str.begin(), str.end(), str.begin(), _toupper);

	CScriptVar* ret = c->getReturnVar();
	if (ret)
		ret->setString(str);
}


void scToLower(CScriptVar* c, void* userdata)
{
	tstring str = c->getParameter(_T("str"))->getString();

	std::transform(str.begin(), str.end(), str.begin(), _tolower);

	CScriptVar* ret = c->getReturnVar();
	if (ret)
		ret->setString(str);
}


void scAbortInstall(CScriptVar* c, void* userdata)
{
	exit(-1);
}


void scShowLicenseAcceptanceDlg(CScriptVar *c, void *userdata)
{
	if (!theApp.m_LicenseAcceptanceDlg)
	{
		theApp.m_LicenseAcceptanceDlg = new CLicenseAcceptanceDlg(_T(""));
	}

	INT_PTR dlg_ret = theApp.m_LicenseAcceptanceDlg->DoModal();

	if (dlg_ret == IDCANCEL)
	{
		exit(-1);
	}
}


void scShowLicenseDlg(CScriptVar *c, void *userdata)
{
	if (!theApp.m_LicenseDlg)
	{
		theApp.m_LicenseDlg = new CLicenseKeyEntryDlg(_T(""));
	}

	INT_PTR dlg_ret = theApp.m_LicenseDlg->DoModal();

	if (dlg_ret == IDCANCEL)
	{
		exit(-1);
	}
}


void scGetLicenseKey(CScriptVar *c, void *userdata)
{
	tstring key;
	if (theApp.m_LicenseDlg)
		key = theApp.m_LicenseDlg->GetKey();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(key);
}


void scGetLicenseUser(CScriptVar *c, void *userdata)
{
	tstring user;
	if (theApp.m_LicenseDlg)
		user = theApp.m_LicenseDlg->GetUser();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(user);
}


void scGetLicenseOrg(CScriptVar *c, void *userdata)
{
	tstring org;
	if (theApp.m_LicenseDlg)
		org = theApp.m_LicenseDlg->GetOrg();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(org);
}


void scDownloadFile(CScriptVar *c, void *userdata)
{
	CScriptVar *purl = c->getParameter(_T("url"));
	CScriptVar *pfile = c->getParameter(_T("file"));

	BOOL result = FALSE;

	if (purl && pfile)
	{
		CHttpDownloader dl;
		result = dl.DownloadHttpFile(purl->getString().c_str(), pfile->getString().c_str(), _T(""));
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}


void scTextFileOpen(CScriptVar *c, void *userdata)
{
	CScriptVar *pfile = c->getParameter(_T("filename"));
	CScriptVar *pmode = c->getParameter(_T("mode"));

	FILE *f = nullptr;
	if (pfile)
		f = _tfopen(pfile->getString().c_str(), pmode ? pmode->getString().c_str() : _T("r, ccs=UTF-8"));

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt((int64_t)f);
}


void scTextFileClose(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));

	if (phandle)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
			fclose(f);
	}
}


void scTextFileReadLn(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));

	tstring s;

	if (phandle)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
		{
			TCHAR _s[4096];
			if (_fgetts(_s, 4096, f))
			{
				size_t n = _tcslen(_s);

				if ((n > 0) && (n <= 4096))
					_s[n - 1] = _T('\0');

				s = _s;
			}
		}
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(s);
}


void scTextFileWrite(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));
	CScriptVar *ptext = c->getParameter(_T("text"));

	if (phandle && ptext)
	{
		tstring t = ptext->getString(), _t;
		ReplaceEnvironmentVariables(t, _t);
		ReplaceRegistryKeys(_t, t);

		FILE *f = (FILE *)phandle->getInt();
		if (f)
			_fputts(t.c_str(), f);
	}
}


void scTextFileReachedEOF(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));

	int64_t b = 1;
	if (phandle)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
			b = feof(f);
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(b);
}


void scGetCurrentDateStr(CScriptVar *c, void *userdata)
{
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	struct tm *parts = std::localtime(&now_c);

	TCHAR dates[MAX_PATH];
	_stprintf(dates, _T("%04d/%02d/%02d"), 1900 + parts->tm_year, 1 + parts->tm_mon, parts->tm_mday);

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(tstring(dates));
}


void scGetFileNameFromPath(CScriptVar *c, void *userdata)
{
	CScriptVar *pfilename = c->getParameter(_T("filepath"));

	tstring n;
	if (pfilename)
	{
		tstring _n = pfilename->getString();
		size_t o = _n.find_last_of(_T("\\/"));
		tstring::const_iterator it = _n.cbegin();
		if (o <= _n.length())
		{
			it += (o + 1);
			while (it != _n.cend()) { n += *it; it++; }
		}
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(n);
}

void scEcho(CScriptVar *c, void *userdata)
{
	tstring msg = c->getParameter(_T("msg"))->getString(), _msg;
	ReplaceEnvironmentVariables(msg, _msg);
	ReplaceRegistryKeys(_msg, msg);

	CSfxApp *_this = (CSfxApp *)userdata;
	_this->Echo(msg.c_str());
}


void scSetProperty(CScriptVar *c, void *userdata)
{
	tstring name = c->getParameter(_T("name"))->getString();
	tstring stype = c->getParameter(_T("type"))->getString();
	tstring saspect = c->getParameter(_T("aspect"))->getString();
	tstring val = c->getParameter(_T("value"))->getString();

	props::IProperty *p = theApp.m_Props->GetPropertyByName(name.c_str());
	if (!p)
		p = theApp.m_Props->CreateProperty(name.c_str(), props::FOURCHARCODE(theApp.m_Props->GetPropertyCount()));

	if (p)
	{
		p->SetString(val.c_str());

		if (!_tcsicmp(stype.c_str(), _T("int")))
			p->ConvertTo(props::IProperty::PROPERTY_TYPE::PT_INT);
		else if (!_tcsicmp(stype.c_str(), _T("float")))
			 p->ConvertTo(props::IProperty::PROPERTY_TYPE::PT_FLOAT);
		else if (!_tcsicmp(stype.c_str(), _T("guid")))
			 p->ConvertTo(props::IProperty::PROPERTY_TYPE::PT_GUID);
		else if (!_tcsicmp(stype.c_str(), _T("enum")))
			 p->ConvertTo(props::IProperty::PROPERTY_TYPE::PT_ENUM);
		else if (!_tcsicmp(stype.c_str(), _T("bool")))
			 p->ConvertTo(props::IProperty::PROPERTY_TYPE::PT_BOOLEAN);

		 if (!_tcsicmp(saspect.c_str(), _T("filename")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FILENAME);
		 else if (!_tcsicmp(saspect.c_str(), _T("directory")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_DIRECTORY);
		 else if (!_tcsicmp(saspect.c_str(), _T("rgb")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_COLOR_RGB);
		 else if (!_tcsicmp(saspect.c_str(), _T("rgba")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_COLOR_RGBA);
		 else if (!_tcsicmp(saspect.c_str(), _T("onoff")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_ONOFF);
		 else if (!_tcsicmp(saspect.c_str(), _T("yesno")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_YESNO);
		 else if (!_tcsicmp(saspect.c_str(), _T("truefalse")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_TRUEFALSE);
		 else if (!_tcsicmp(saspect.c_str(), _T("abled")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_BOOL_ABLED);
		 else if (!_tcsicmp(saspect.c_str(), _T("font")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_FONT_DESC);
		 else if (!_tcsicmp(saspect.c_str(), _T("date")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_DATE);
		 else if (!_tcsicmp(saspect.c_str(), _T("time")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_TIME);
		 else if (!_tcsicmp(saspect.c_str(), _T("ipaddr")))
			 p->SetAspect(props::IProperty::PROPERTY_ASPECT::PA_IPADDRESS);
	}
}


void scGetProperty(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	tstring name = c->getParameter(_T("name"))->getString();

	props::IProperty *p = theApp.m_Props->GetPropertyByName(name.c_str());
	if (p)
	{
		switch (p->GetType())
		{
			case props::IProperty::PROPERTY_TYPE::PT_INT:
			case props::IProperty::PROPERTY_TYPE::PT_BOOLEAN:
			case props::IProperty::PROPERTY_TYPE::PT_ENUM:
				ret->setInt(p->AsInt());
				break;

			case props::IProperty::PROPERTY_TYPE::PT_STRING:
				ret->setString(p->AsString());
				break;

			case props::IProperty::PROPERTY_TYPE::PT_GUID:
			{
				TCHAR gs[128];
				ret->setString(p->AsString(gs));
				break;
			}

			case props::IProperty::PROPERTY_TYPE::PT_FLOAT:
				ret->setDouble(p->AsFloat());
				break;
		}
	}
}
