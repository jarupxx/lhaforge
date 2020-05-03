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
#include "resource.h"
#include "Utilities/Utility.h"
#include "Utilities/StringUtil.h"
#include "Utilities/FileOperation.h"
#include "Dialogs/TextInputDlg.h"

int ErrorMessage(const wchar_t* message)
{
	TRACE(L"ErrorMessage:%s\n", message);
	return UtilMessageBox(NULL, message, MB_OK | MB_ICONSTOP);
}

int UtilMessageBox(HWND hWnd, const wchar_t* lpText, UINT uType)
{
	const CString strCaption(MAKEINTRESOURCE(IDS_MESSAGE_CAPTION));
	return MessageBoxW(
		hWnd,
		lpText,
		strCaption,
		uType);
}

std::wstring UtilGetLastErrorMessage(DWORD langID, DWORD errorCode)
{
	LPVOID lpMsgBuf;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorCode,
		langID,
		(wchar_t*)&lpMsgBuf, 0, NULL);

	std::wstring out=(const wchar_t*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return out;
}

//ファイル名が指定したパターンに当てはまればtrue
bool UtilExtMatchSpec(LPCTSTR lpszPath,LPCTSTR lpPattern)
{
	const CString strBuf=lpPattern;
	int Index=0,CopyFrom=0;

	while(true){
		Index=strBuf.Find(_T(';'),Index);
		CString strMatchSpec;
		if(-1==Index){
			strMatchSpec=strBuf.Mid(CopyFrom);
		}else{
			strMatchSpec=strBuf.Mid(CopyFrom,Index-CopyFrom);
		}
		if(!strMatchSpec.IsEmpty()){	//拡張子確認
			if(strMatchSpec[0]!=L'.')strMatchSpec=L'.'+strMatchSpec;	//.から始まるように補う
			strMatchSpec.Insert(0,_T("*"));	//.ext->*.ext
			if(PathMatchSpec(lpszPath,strMatchSpec)){
				return true;
			}
		}
		if(-1==Index){	//検索終了
			break;
		}else{
			Index++;
			CopyFrom=Index;
		}
	}
	return false;
}

//ファイル名が指定した2つの条件で[許可]されるかどうか;拒否が優先
bool UtilPathAcceptSpec(LPCTSTR lpszPath,LPCTSTR lpDeny,LPCTSTR lpAccept,bool bDenyOnly)
{
	if(UtilExtMatchSpec(lpszPath,lpDeny)){
		return false;
	}
	if(bDenyOnly){
		return true;
	}else{
		if(UtilExtMatchSpec(lpszPath,lpAccept)){
			return true;
		}
	}
	return false;
}

//レスポンスファイルを読み取る
bool UtilReadFromResponceFile(LPCTSTR lpszRespFile,UTIL_CODEPAGE uSrcCodePage,std::list<CString> &FileList)
{
	ASSERT(lpszRespFile);
	if(!lpszRespFile)return false;
	if(!PathFileExists(lpszRespFile))return false;

	HANDLE hFile=CreateFile(lpszRespFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(INVALID_HANDLE_VALUE==hFile)return false;

	//4GB越えファイルは扱わない
	const DWORD dwSize=GetFileSize(hFile,NULL);
	std::vector<BYTE> cReadBuffer;
	cReadBuffer.resize(dwSize+2);
	DWORD dwRead;
	//---読み込み
	if(!ReadFile(hFile,&cReadBuffer[0],dwSize,&dwRead,NULL)||dwSize!=dwRead){
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);

	//---文字コード変換
	//終端文字追加
	switch(uSrcCodePage){
	case UTIL_CODEPAGE::CP932:
	case UTIL_CODEPAGE::UTF8:	//FALLTHROUGH
		cReadBuffer[dwSize]='\0';
		break;
	case UTIL_CODEPAGE::UTF16:
		*((LPWSTR)&cReadBuffer[dwSize])=L'\0';
		break;
	default:
		ASSERT(!"This code canno be run");
		return false;
	}
	//文字コード変換
	CString strBuffer = UtilToUNICODE((const char*)&cReadBuffer[0], cReadBuffer.size(), uSrcCodePage).c_str();

	LPCTSTR p=strBuffer;
	const LPCTSTR end=p+strBuffer.GetLength()+1;
	//解釈
	CString strLine;
	for(;p!=end;p++){
		if(*p==_T('\n')||*p==_T('\r')||*p==_T('\0')){
			if(!strLine.IsEmpty()){
				CPath tmpPath(strLine);
				tmpPath.UnquoteSpaces();	//""を外す
				FileList.push_back(tmpPath);
			}
			strLine.Empty();
		}
		else 
			strLine+=*p;
	}
	return true;
}

//INIに数字を文字列として書き込む
BOOL UtilWritePrivateProfileInt(LPCTSTR lpAppName,LPCTSTR lpKeyName,LONG nData,LPCTSTR lpFileName)
{
	TCHAR Buffer[32]={0};
	wsprintf(Buffer,_T("%ld"),nData);
	return ::WritePrivateProfileString(lpAppName,lpKeyName,Buffer,lpFileName);
}


//INIに指定されたセクションがあるならtrueを返す
bool UtilCheckINISectionExists(LPCTSTR lpAppName,LPCTSTR lpFileName)
{
	TCHAR szBuffer[10];
	DWORD dwRead=GetPrivateProfileSection(lpAppName,szBuffer,9,lpFileName);
	return dwRead>0;
}

//文字列を入力させる
bool UtilInputText(LPCTSTR lpszMessage,CString &strInput)
{
	CInputDialog dlg(lpszMessage,strInput);
	return IDOK==dlg.DoModal();
}


//与えられたファイル名がマルチボリューム書庫と見なせるならtrueを返す
bool UtilIsMultiVolume(LPCTSTR lpszPath,CString &r_strFindParam)
{
	//初期化
	r_strFindParam.Empty();

	CPath strPath(lpszPath);
	if(strPath.IsDirectory())return false;	//ディレクトリなら無条件に返る

	strPath.StripPath();	//ファイル名のみに
	int nExt=strPath.FindExtension();	//拡張子の.の位置
	if(-1==nExt)return false;	//拡張子は見つからず

	CString strExt((LPCTSTR)strPath+nExt);
	strExt.MakeLower();	//小文字に
	if(strExt==_T(".rar")){
		//---RAR
		if(strPath.MatchSpec(_T("*.part*.rar"))){
			//検索文字列の作成
			CPath tempPath(lpszPath);
			tempPath.RemoveExtension();	//.rarの削除
			tempPath.RemoveExtension();	//.part??の削除
			tempPath.AddExtension(_T(".part*.rar"));

			r_strFindParam=(CString)tempPath;
			return true;
		}else{
			return false;
		}
	}
	//TODO:使用頻度と実装の簡便さを考えてrarのみ対応とする
	return false;
}


//強制的にメッセージループを回す
bool UtilDoMessageLoop()
{
	MSG msg;
	if(PeekMessage (&msg,NULL,0,0,PM_NOREMOVE)){
		if(!GetMessage (&msg,NULL,0,0)){
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
		return true;
	}
	return false;
}

VOID CALLBACK UtilMessageLoopTimerProc(HWND,UINT,UINT,DWORD)
{
	while(UtilDoMessageLoop())continue;
}


//標準の設定ファイルのパスを取得
//bUserCommonはユーザー間で共通設定を使う場合にtrueが代入される
//lpszDirはApplicationDataに入れるときに必要なディレクトリ名
//lpszFileは探すファイル名
void UtilGetDefaultFilePath(CString &strPath,LPCTSTR lpszDir,LPCTSTR lpszFile,bool &bUserCommon)
{
	//---ユーザー間で共通の設定を用いる
	//LhaForgeフォルダと同じ場所にINIがあれば使用する
	{
		TCHAR szCommonIniPath[_MAX_PATH+1]={0};
		_tcsncpy_s(szCommonIniPath,UtilGetModuleDirectoryPath(),_MAX_PATH);
		PathAppend(szCommonIniPath,lpszFile);
		if(PathFileExists(szCommonIniPath)){
			//共通設定
			bUserCommon=true;
			strPath=szCommonIniPath;
			TRACE(_T("Common INI(Old Style) '%s' found.\n"),strPath);
			return;
		}
	}
	//CSIDL_COMMON_APPDATAにINIがあれば使用する
	{
		TCHAR szCommonIniPath[_MAX_PATH+1]={0};
		SHGetFolderPath(NULL,CSIDL_COMMON_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,szCommonIniPath);
		PathAppend(szCommonIniPath,lpszDir);
		PathAppend(szCommonIniPath,lpszFile);
		if(PathFileExists(szCommonIniPath)){
			//共通設定
			bUserCommon=true;
			strPath=szCommonIniPath;
			TRACE(_T("Common INI '%s' found.\n"),strPath);
			return;
		}
	}

	//--------------------

	//---ユーザー別設定を用いる
	//LhaForgeインストールフォルダ以下にファイルが存在する場合、それを使用
	{
		//ユーザー名取得
		TCHAR UserName[UNLEN+1]={0};
		DWORD Length=UNLEN;
		GetUserName(UserName,&Length);

		TCHAR szIniPath[_MAX_PATH+1];
		_tcsncpy_s(szIniPath,UtilGetModuleDirectoryPath(),_MAX_PATH);
		PathAppend(szIniPath,UserName);
		PathAddBackslash(szIniPath);
		//MakeSureDirectoryPathExists(szIniPath);

		PathAppend(szIniPath,lpszFile);

		if(PathFileExists(szIniPath)){
			bUserCommon=false;
			strPath=szIniPath;
			TRACE(_T("Personal INI(Old Style) '%s' found.\n"),strPath);
			return;
		}
	}
	//---デフォルト
	//CSIDL_APPDATAにINIがあれば使用する:Vistaではこれ以外はアクセス権限不足になる可能性がある
	TCHAR szIniPath[_MAX_PATH+1]={0};
	SHGetFolderPath(NULL,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,szIniPath);
	PathAppend(szIniPath,lpszDir);
	PathAddBackslash(szIniPath);
	UtilMakeSureDirectoryPathExists(szIniPath);
	PathAppend(szIniPath,lpszFile);
	bUserCommon=false;
	strPath=szIniPath;
	TRACE(_T("Personal INI '%s' found.\n"),strPath);
	return;
}




