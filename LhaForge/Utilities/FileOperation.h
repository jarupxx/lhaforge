﻿/*
* MIT License

* Copyright (c) 2005- Claybird

* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

std::wstring UtilGetDesktopPath();
std::wstring UtilGetSendToPath();

//returns a temp dir exclusive use of lhaforge
std::wstring UtilGetTempPath();
std::wstring UtilGetTemporaryFileName();
bool UtilDeletePath(const std::wstring& path);

//bDeleteParent=true: delete Path itself
//bDeleteParent=false: delete only children of Path
bool UtilDeleteDir(const std::wstring& path, bool bDeleteParent);


//delete temporary directory automatically
class CTemporaryDirectoryManager
{
	enum { NUM_DIR_LIMIT = 10000 };
protected:
	std::filesystem::path m_path;
public:
	CTemporaryDirectoryManager(){
		//%TEMP%/tmp%05d/filename...
		std::filesystem::path base = UtilGetTempPath();
		for (int count = 0; count < NUM_DIR_LIMIT; count++) {
			auto name = Format(L"tmp%05d", count);
			if(!std::filesystem::exists(base / name)){
				try {
					std::filesystem::create_directories(base / name);
					m_path = base / name;
					return;
				} catch (std::filesystem::filesystem_error) {
					RAISE_EXCEPTION(L"Failed to create directory");
				}
			}
		}
		RAISE_EXCEPTION(L"Failed to create directory");
	}
	virtual ~CTemporaryDirectoryManager() {
		UtilDeleteDir(m_path, true);
	}

	std::filesystem::path path()const {
		return m_path;
	}
};


bool UtilMoveFileToRecycleBin(const std::vector<std::wstring>& fileList);

//recursively enumerates files (no directories) in specified directory
std::vector<std::wstring> UtilRecursiveEnumFile(const std::wstring& root);

//recursively enumerates files and directories in specified directory
std::vector<std::wstring> UtilRecursiveEnumFileAndDirectory(const std::wstring& root);

std::vector<std::wstring> UtilEnumSubFileAndDirectory(const std::wstring& root);


bool UtilPathIsRoot(const std::wstring& path);
std::wstring UtilPathAddLastSeparator(const std::wstring& path);
std::wstring UtilPathRemoveLastSeparator(const std::wstring& path);

//get full & absolute path
std::wstring UtilGetCompletePathName(const std::wstring& filePath);

//returns filenames that matches to the given pattern
std::vector<std::wstring> UtilPathExpandWild(const std::wstring& pattern);

//executable name
std::wstring UtilGetModulePath();
std::wstring UtilGetModuleDirectoryPath();

//read whole file
std::vector<BYTE> UtilReadFile(const std::wstring& filePath);


class CAutoFile {
protected:
	FILE *_fp;
	CAutoFile(const CAutoFile&) = delete;
	const CAutoFile& operator=(const CAutoFile&) = delete;
public:
	CAutoFile() :_fp(NULL){}
	virtual ~CAutoFile() {
		close();
	}
	operator FILE*() { return _fp; }
	bool is_opened() const { return _fp != NULL; }
	void close() {
		if (_fp) {
			fclose(_fp);
			_fp = NULL;
		}
	}
	void open(const std::wstring& fname, const std::wstring& mode = L"r") {
		close();
		auto err = _wfopen_s(&_fp, fname.c_str(), mode.c_str());
		if (err==0 && _fp) {
			//set buffer size
			setvbuf(_fp, NULL, _IOFBF, 1024 * 1024);
		}
	}
};


void touchFile(const std::wstring& path);
