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
#include "ArchiverCode/archive.h"
#include "CommonUtil.h"

//reconstructed archive content structure
struct ARCHIVE_ENTRY_INFO {
	LF_ENTRY_STAT _entry;
	std::vector<std::shared_ptr<ARCHIVE_ENTRY_INFO> > _children;
	ARCHIVE_ENTRY_INFO*	_parent;

	std::wstring _entryName;

	//for file, original file size. for directory, sum of children file size
	UINT64		_originalSize;

	//---
	ARCHIVE_ENTRY_INFO() { clear(); }
	virtual ~ARCHIVE_ENTRY_INFO() {}

	static const wchar_t* dirDummyExt() {
		const wchar_t* FOLDER_EXTENSION_STRING = L"***directory/dummy/extension***";
		return FOLDER_EXTENSION_STRING;
	}
	bool is_directory()const{
		//in case the directory is just a ghost entry that is not stored in the archive
		//ex. in zip, dirA/fileA.txt, does not include dirA as an entry
		return _entry.is_directory() || !_children.empty();
	}
	double compress_ratio()const {
		return double(_entry.compressed_size) / std::max((__int64)1, _entry.stat.st_size);
	}

	std::wstring getExt()const {
		if (is_directory()) {
			return dirDummyExt();
		} else {
			auto fname = _entry.path.filename();
			if (!fname.empty() && fname.wstring()[0]==L'.') {
				//ex. ".gitignore"
				return fname;
			} else {
				auto ext = fname.extension();
				if (ext.empty()) {
					return L"file_without_extension";
				} else {
					return ext;
				}
			}
		}
	}

	size_t getNumChildren()const { return _children.size(); }
	ARCHIVE_ENTRY_INFO* getChild(size_t idx)const {
		if (0 <= idx && idx < getNumChildren()) {
			return _children[idx].get();
		} else {
			return nullptr;
		}
	}
	ARCHIVE_ENTRY_INFO* getChild(const std::wstring &name)const {
		for(const auto& entry: _children){
			if (0 == _wcsicmp(entry->_entryName.c_str(), name.c_str())) {
				return entry.get();
			}
		}
		return nullptr;
	}
	ARCHIVE_ENTRY_INFO* makeSureItemExists(const std::wstring& entryName) {
		auto p = getChild(entryName);
		if (p)return p;
		auto s = std::make_shared<ARCHIVE_ENTRY_INFO>();
		s->_entryName = entryName;
		s->_parent = this;
		_children.push_back(s);
		return s.get();
	}
	ARCHIVE_ENTRY_INFO& addEntry(const std::vector<std::wstring> &pathElements) {
		if (pathElements.empty()) {
			return *this;
		} else {
			const auto entryName = pathElements.front();
			auto p = makeSureItemExists(entryName);
			auto sub = std::vector<std::wstring>(std::next(pathElements.begin()), pathElements.end());
			if (!sub.empty()) {
				p->_entry.stat.st_mode &= S_IFDIR;
			}
			return p->addEntry(sub);
		}
	}
	void clear() {
		_children.clear();
		_parent = nullptr;
		_entryName.clear();

		_originalSize = -1;
	}

	std::wstring calcFullpath()const {
		if (_parent) {
			auto parent_name = _parent->calcFullpath();
			if (parent_name.empty()) {
				return _entryName;
			} else {
				return parent_name + L"/" + _entryName;
			}
		} else {
			return L"";
		}
	}
	//returns pathname from base to this
	std::wstring getRelativePath(const ARCHIVE_ENTRY_INFO* base)const {
		if (_parent && base != this) {
			auto parent_name = _parent->getRelativePath(base);
			if (parent_name.empty()) {
				return _entryName;
			} else {
				return parent_name + L"/" + _entryName;
			}
		} else {
			return L"";
		}
	}
	//enumerate myself and children
	std::vector<const ARCHIVE_ENTRY_INFO*> enumChildren()const {
		std::vector<const ARCHIVE_ENTRY_INFO*> children;
		if (!_parent || is_directory()) {
			for (const auto &entry: _children) {
				auto subFiles = entry->enumChildren();
				children.insert(children.end(), subFiles.begin(), subFiles.end());
			}
		}
		if (_parent) {
			//when _parent is nullptr, it is the virtual root
			children.push_back(this);
		}
		return children;
	}
};


struct ARCHIVE_FIND_CONDITION {
	enum class KEY :int {
		filename,
		fullpath,
		originalSize,
		mdate,
		mode,	//directory or file
		//compressedSize,
		//ratio,
		ENUM_COUNT_AND_LASTITEM
	};

	enum class COMPARE :int {	//valid on size and date
		equal,
		equalOrGreater,
		equalOrLess,
		ENUM_COUNT_AND_LASTITEM
	};

	KEY key;
	std::wstring patternStr;	//pattern = replace(name_or_pattern, L"\\", L"/");
	int64_t st_size;
	SYSTEMTIME mdate;
	unsigned short st_mode_mask;

	COMPARE compare;
	void setFindByFilename(const std::wstring& pattern) {
		key = KEY::filename;
		patternStr = pattern;
	}
	void setFindByFullpath(const std::wstring& pattern) {
		key = KEY::fullpath;
		patternStr = pattern;
	}
	void setFindByOriginalSize(uint64_t size, COMPARE compMode) {
		key = KEY::originalSize;
		st_size = size;
		compare = compMode;
	}
	void setFindByMDate(SYSTEMTIME m, COMPARE compMode) {
		key = KEY::mdate;
		mdate = m;
		compare = compMode;
	}
	void setFindByMode(unsigned short mode) {
		key = KEY::mode;
		st_mode_mask = mode;
	}
	bool matchItem(const ARCHIVE_ENTRY_INFO& p)const;	//true if item matches condition
	std::wstring toString()const;
};

class CArchiveFileContent{
protected:
	std::shared_ptr<ARCHIVE_ENTRY_INFO> m_pRoot;
	std::shared_ptr<ILFPassphrase> m_passphrase;

	std::filesystem::path m_pathArchive;
	int64_t m_numFiles;
	bool m_bModifySupported;
	bool m_bEncrypted;	//true if at least one entry is encrypted
protected:
	//---internal functions
	void postScanArchive(ARCHIVE_ENTRY_INFO*);

	std::tuple<std::filesystem::path, std::unique_ptr<ILFArchiveFile>, std::vector<std::filesystem::path>>
	subDeleteEntries(
		const LF_COMPRESS_ARGS& args,
		const std::map<std::filesystem::path/*path in archive*/, std::filesystem::path/*path on disk*/> &items_to_delete,
		ILFProgressHandler& progressHandler,
		ILFOverwriteInArchiveConfirm& confirmHandler,
		ARCLOG &arcLog);
public:
	CArchiveFileContent(std::shared_ptr<ILFPassphrase> pp) :
		m_passphrase(pp),
		m_bModifySupported(false),
		m_bEncrypted(false)
	{
		clear();
	}
	virtual ~CArchiveFileContent() {}
	void clear() {
		m_pRoot.reset();
		m_bModifySupported = false;
		m_pathArchive.clear();
		m_bEncrypted = false;
		m_numFiles = 0;
	}
	std::filesystem::path getArchivePath()const { return m_pathArchive; }
	bool isArchiveEncrypted()const { return m_bEncrypted; }
	bool isModifySupported()const { return m_bModifySupported; }
	bool isMultipleContentAllowed()const;
	bool checkArchiveExists()const { return std::filesystem::exists(m_pathArchive); }
	bool isOK()const { return m_pRoot.get() != nullptr; }

	const ARCHIVE_ENTRY_INFO* getRootNode()const { return m_pRoot.get(); }
	ARCHIVE_ENTRY_INFO* getRootNode() { return m_pRoot.get(); }
	std::vector<std::shared_ptr<ARCHIVE_ENTRY_INFO> > findItem(
		const ARCHIVE_FIND_CONDITION& condition,
		const ARCHIVE_ENTRY_INFO* parent = nullptr
	)const;

	void scanArchiveStruct(const std::filesystem::path& archiveName, ILFScanProgressHandler& progressHandler);

	std::vector<std::filesystem::path> extractEntries(
		const std::vector<const ARCHIVE_ENTRY_INFO*> &items,
		const std::filesystem::path& outputDir,
		const ARCHIVE_ENTRY_INFO* lpBase,
		ILFProgressHandler& progressHandler,
		ARCLOG &arcLog);
	void addEntries(
		const LF_COMPRESS_ARGS& args,
		const std::vector<std::filesystem::path> &files,
		const ARCHIVE_ENTRY_INFO* lpParent,
		ILFProgressHandler& progressHandler,
		ILFOverwriteInArchiveConfirm& confirmHandler,
		ARCLOG &arcLog);
	void deleteEntries(
		const LF_COMPRESS_ARGS& args,
		const std::vector<const ARCHIVE_ENTRY_INFO*> &items,
		ILFProgressHandler& progressHandler,
		ARCLOG &arcLog);

	std::vector<std::filesystem::path> makeSureItemsExtracted(	//returns list of extracted files
		const std::vector<const ARCHIVE_ENTRY_INFO*> &items,
		const std::filesystem::path &outputDir,
		const ARCHIVE_ENTRY_INFO* lpBase,
		ILFProgressHandler& progressHandler,
		enum class overwrite_options options,
		ARCLOG &arcLog);
};

