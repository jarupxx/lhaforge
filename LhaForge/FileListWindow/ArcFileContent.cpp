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

#include "stdafx.h"
#include "ArcFileContent.h"
#include "Utilities/StringUtil.h"
#include "Utilities/FileOperation.h"
#include "Utilities/Utility.h"
#include "ArchiverCode/arc_interface.h"
#include "CommonUtil.h"


void CArchiveFileContent::inspectArchiveStruct(
	const std::wstring& archiveName,
	IArchiveContentUpdateHandler* lpHandler)
{
	clear();
	m_pRoot = std::make_shared<ARCHIVE_ENTRY_INFO>();

	ARCHIVE_FILE_TO_READ arc;
	arc.read_open(archiveName, LF_passphrase_callback);

	bool bEncrypted = false;
	for (LF_ARCHIVE_ENTRY* entry = arc.begin(); entry; entry = arc.next()) {
		auto pathname = UtilPathRemoveLastSeparator(LF_sanitize_pathname(entry->get_pathname()));
		auto elements = UtilSplitString(pathname, L"/");

		if (elements.empty() || elements[0].empty())continue;

		auto &item = m_pRoot->addEntry(elements);
		item._entryName = elements.back();
		item._fullpath = pathname;
		item._nAttribute = entry->get_file_mode();
		item._originalSize = entry->get_original_filesize();
		item._st_mtime = entry->get_mtime();

		bEncrypted = bEncrypted || entry->is_encrypted();

		//notifier
		if (lpHandler) {
			while (UtilDoMessageLoop())continue;
			lpHandler->onUpdated(item);
			if (lpHandler->isAborted()) {
				CANCEL_EXCEPTION();
			}
		}
	}
	m_bEncrypted = bEncrypted;
	m_bReadOnly = GetFileAttributesW(archiveName.c_str()) & FILE_ATTRIBUTE_READONLY;
	m_pathArchive = archiveName;
	postInspectArchive(nullptr);
}


void CArchiveFileContent::postInspectArchive(ARCHIVE_ENTRY_INFO* pNode)
{
	if (!pNode)pNode = m_pRoot.get();

	if (pNode != m_pRoot.get()) {
		//in case of directories that does not have a fullpath
		pNode->_fullpath = pNode->getFullpath();
	}

	if (pNode->isDirectory()) {
		pNode->_originalSize = 0;
	}

	//children
	for (auto& child : pNode->_children) {
		postInspectArchive(child.get());

		if (pNode->_originalSize >= 0) {
			if (child->_originalSize >= 0) {
				pNode->_originalSize += child->_originalSize;
			} else {
				//file size unknown
				pNode->_originalSize = -1;
			}
		}
	}
}


std::vector<std::shared_ptr<ARCHIVE_ENTRY_INFO> > CArchiveFileContent::findSubItem(
	const std::wstring& pattern,
	const ARCHIVE_ENTRY_INFO* parent)const
{
	//---breadth first search

	std::vector<std::shared_ptr<ARCHIVE_ENTRY_INFO> > found;
	for (auto& child : parent->_children) {
		if (UtilPathMatchSpec(child->_entryName, pattern)) {
			found.push_back(child);
		}
	}
	for (auto& child : parent->_children) {
		if (child->isDirectory()) {
			auto subFound = findSubItem(pattern, child.get());
			found.insert(found.end(), subFound.begin(), subFound.end());
		}
	}
	return found;
}


bool CArchiveFileContent::ExtractItems(
	CConfigManager &Config,
	const std::vector<ARCHIVE_ENTRY_INFO*> &items,
	const std::wstring& outputDir,
	const ARCHIVE_ENTRY_INFO* lpBase,
	bool bCollapseDir,
	std::wstring &strLog)
{
	//TODO
	RAISE_EXCEPTION(L"NOT INMPELEMTED");
	return false;// return m_lpArchiver->ExtractItems(m_pathArchive, Config, lpBase, items, lpszDir, bCollapseDir, strLog);
}

void CArchiveFileContent::collectUnextractedFiles(const std::wstring& outputDir,const ARCHIVE_ENTRY_INFO* lpBase,const ARCHIVE_ENTRY_INFO* lpParent,std::map<const ARCHIVE_ENTRY_INFO*,std::vector<ARCHIVE_ENTRY_INFO*> > &toExtractList)
{
	size_t numChildren=lpParent->getNumChildren();
	for(size_t i=0;i<numChildren;i++){
		ARCHIVE_ENTRY_INFO* lpNode=lpParent->getChild(i);
		std::filesystem::path path=outputDir;

		auto subPath = lpNode->getRelativePath(lpBase);
		path /= subPath;

		if(std::filesystem::is_directory(path)){
			// フォルダが存在するが中身はそろっているか?
			collectUnextractedFiles(outputDir,lpBase,lpNode,toExtractList);
		}else if(!std::filesystem::is_regular_file(path)){
			// キャッシュが存在しないので、解凍要請リストに加える
			toExtractList[lpParent].push_back(lpNode);
		}
	}
}


//bOverwrite:trueなら存在するテンポラリファイルを削除してから解凍する
bool CArchiveFileContent::MakeSureItemsExtracted(
	CConfigManager& Config,
	const std::wstring &outputDir,
	bool bOverwrite,
	const ARCHIVE_ENTRY_INFO* lpBase,
	const std::vector<ARCHIVE_ENTRY_INFO*> &items,
	std::vector<std::wstring> &r_extractedFiles,
	std::wstring &strLog)
{
	//選択されたアイテムを列挙
	std::map<const ARCHIVE_ENTRY_INFO*,std::vector<ARCHIVE_ENTRY_INFO*> > toExtractList;

	for(auto &lpNode: items){
		// 存在をチェックし、もし解凍済みであればそれを開く
		std::filesystem::path path = outputDir;

		auto subPath = lpNode->getRelativePath(lpBase);
		path /= subPath;

		if(bOverwrite){
			// 上書き解凍するので、存在するファイルは削除
			if(lpNode->isDirectory()){
				if (std::filesystem::is_directory(path))UtilDeleteDir(path, true);
			}else{
				if (std::filesystem::is_regular_file(path))UtilDeletePath(path);
			}
			//解凍要請リストに加える
			toExtractList[lpBase].push_back(lpNode);
		}else{	//上書きはしない
			if(std::filesystem::is_directory(path)){
				// フォルダが存在するが中身はそろっているか?
				collectUnextractedFiles(outputDir,lpBase,lpNode,toExtractList);
			}else if(!std::filesystem::is_regular_file(path)){
				// キャッシュが存在しないので、解凍要請リストに加える
				toExtractList[lpBase].push_back(lpNode);
			}
		}
	}
	if(toExtractList.empty()){
		return true;
	}

	//未解凍の物のみ一時フォルダに解凍
	for(const auto &pair: toExtractList){
		const std::vector<ARCHIVE_ENTRY_INFO*> &filesList = pair.second;
		if(!ExtractItems(Config,filesList,outputDir,lpBase,false,strLog)){
			for(const auto &toDelete : filesList){
				//失敗したので削除
				std::filesystem::path path=outputDir;
				auto subPath = toDelete->getRelativePath(lpBase);
				path /= subPath;
				UtilDeletePath(path);
			}
			return false;
		}
	}
	return true;
}


HRESULT CArchiveFileContent::AddItem(const std::vector<std::wstring> &fileList,LPCTSTR lpDestDir,CConfigManager& rConfig,CString &strLog)
{
	//---ファイル名チェック
	for(const auto &file: fileList){
		if(0==_wcsicmp(m_pathArchive.c_str(), file.c_str())){
			//アーカイブ自身を追加しようとした
			return E_LF_SAME_INPUT_AND_OUTPUT;
		}
	}

	//TODO
	RAISE_EXCEPTION(L"NOT INMPELEMTED");
	return E_NOTIMPL;
/*	//---追加
	//基底ディレクトリ取得などはCArchiverDLL側に任せる
	if(m_lpArchiver->AddItemToArchive(m_pathArchive,m_bEncrypted,fileList,rConfig,lpDestDir,strLog)){
		return S_OK;
	}else{
		return S_FALSE;
	}*/
}

bool CArchiveFileContent::DeleteItems(CConfigManager &Config,const std::list<ARCHIVE_ENTRY_INFO*> &fileList,CString &strLog)
{
	//TODO
	RAISE_EXCEPTION(L"NOT INMPELEMTED");
	return false;
/*	//削除対象を列挙
	std::list<CString> filesToDel;
	for(std::list<ARCHIVE_ENTRY_INFO*>::const_iterator ite=fileList.begin();ite!=fileList.end();++ite){
		(*ite)->EnumFiles(filesToDel);
	}
	return m_lpArchiver->DeleteItemFromArchive(m_pathArchive,Config,filesToDel,strLog);*/
}

