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
#include "Utilities/Utility.h"
#include "Utilities/FileOperation.h"
#include "ArchiverCode/archive.h"

#include "ConfigCode/ConfigManager.h"
#include "ConfigCode/ConfigGeneral.h"
#include "ConfigCode/ConfigCompress.h"

struct LF_COMPRESS_ARGS {
	CConfigManager mngr;
	CConfigGeneral general;
	CConfigCompress compress;
};

struct RAW_FILE_READER {
	CAutoFile fp;
	LF_BUFFER_INFO ibi;
	std::vector<unsigned char> buffer;
	RAW_FILE_READER() {
		ibi.make_eof();
		buffer.resize(1024 * 1024 * 32);	//32MB cache
	}
	virtual ~RAW_FILE_READER() {}
	const LF_BUFFER_INFO& operator()() {
		if (!fp || feof(fp)) {
			ibi.make_eof();
		} else {
			ibi.size = fread(&buffer[0], 1, buffer.size(), fp);
			ibi.buffer = &buffer[0];
			ibi.offset = _ftelli64(fp);
		}
		return ibi;
	}
	void open(const std::wstring& path) {
		close();
		fp.open(path, L"rb");
	}
	void close() {
		fp.close();
	}
};


struct CMDLINEINFO;

bool GUI_compress_multiple_files(
	const std::vector<std::wstring> &givenFiles,
	LF_ARCHIVE_FORMAT format,
	LF_WRITE_OPTIONS options,
	ILFProgressHandler &progressHandler,
	CMDLINEINFO& CmdLineInfo);

//command line parameter table
struct COMPRESS_COMMANDLINE_PARAMETER{
	const wchar_t* name;
	LF_ARCHIVE_FORMAT Type;
	int Options;
	WORD FormatName;
};
extern const std::vector<COMPRESS_COMMANDLINE_PARAMETER> g_CompressionCmdParams;
const COMPRESS_COMMANDLINE_PARAMETER& get_archive_format_args(LF_ARCHIVE_FORMAT fmt, int opts);
struct COMPRESS_SOURCES {
	virtual ~COMPRESS_SOURCES() {}
	struct PATH_PAIR {
		std::wstring originalFullPath;
		std::wstring entryPath;
	};
	std::wstring basePath;
	std::vector<PATH_PAIR> pathPair;
};
