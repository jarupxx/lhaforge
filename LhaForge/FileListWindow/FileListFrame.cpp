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
#include "FileListFrame.h"
#include "../ConfigCode/configwnd.h"
#include "../ConfigCode/ConfigFile.h"
#include "../ConfigCode/ConfigFileListWindow.h"
#include "../resource.h"
#include "Dialogs/LogListDialog.h"
#include "../Utilities/OSUtil.h"
#include "../Utilities/StringUtil.h"
#include "../CommonUtil.h"

HWND g_hFirstWindow = nullptr;
std::wstring g_FileToOpen;

std::wstring CFileListFrame::ms_strPropIdentifier(UtilLoadString(IDS_MESSAGE_CAPTION) + UtilLoadString(IDS_LHAFORGE_VERSION_STRING));

CFileListFrame::CFileListFrame(CConfigFile &conf):
	mr_Config(conf),
	m_DropTarget(this)
{
	m_ConfFLW.load(mr_Config);
	m_TabClientWnd = std::make_unique<CFileListTabClient>(conf, m_ConfFLW, *this);
}


BOOL CFileListFrame::PreTranslateMessage(MSG* pMsg)
{
	if (CFrameWindowImpl<CFileListFrame>::PreTranslateMessage(pMsg)) {
		return TRUE;
	}
	if (!m_AccelEx.IsNull() && m_AccelEx.TranslateAccelerator(m_hWnd, pMsg)) {
		return TRUE;
	}
	if (m_TabClientWnd->PreTranslateMessage(pMsg))return TRUE;
	return FALSE;
}

LRESULT CFileListFrame::OnCreate(LPCREATESTRUCT lpcs)
{
//========================================
//      フレームウィンドウの初期化
//========================================
	//ウィンドウプロパティの設定:LhaForgeウィンドウである事を示す
	::SetPropW(m_hWnd, ms_strPropIdentifier.c_str(), m_hWnd);

	//ウィンドウのサイズの設定
	if(m_ConfFLW.StoreSetting){
		if(m_ConfFLW.StoreWindowPosition){	//ウィンドウ位置を復元する場合
			MoveWindow(m_ConfFLW.WindowPos_x, m_ConfFLW.WindowPos_y, m_ConfFLW.Width, m_ConfFLW.Height);
		}else{
			CRect Rect;
			GetWindowRect(Rect);
			MoveWindow(Rect.left,Rect.top, m_ConfFLW.Width, m_ConfFLW.Height);
		}
	}else if(m_ConfFLW.StoreWindowPosition){	//ウィンドウ位置だけ復元する場合
		CRect Rect;
		GetWindowRect(Rect);
		MoveWindow(m_ConfFLW.WindowPos_x, m_ConfFLW.WindowPos_y,Rect.Width(),Rect.Height());
	}
	//ウィンドウサイズ取得
	GetWindowRect(m_WindowRect);

	// 大きいアイコン設定
	HICON hIcon = AtlLoadIconImage(IDI_APP, LR_DEFAULTCOLOR,::GetSystemMetrics(SM_CXICON),::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	// 小さいアイコン設定
	HICON hIconSmall = AtlLoadIconImage(IDI_APP, LR_DEFAULTCOLOR,::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	if(m_ConfFLW.ShowToolbar){
		// リバーを作成
		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		// ツールバーを作成してバンドに追加
		HIMAGELIST hImageList=NULL;
		if(!m_ConfFLW.strCustomToolbarImage.empty()){
			//カスタムツールバー
			hImageList = ImageList_LoadImage(NULL, m_ConfFLW.strCustomToolbarImage.c_str(), 0, 1, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE|LR_LOADFROMFILE);
		}
		HWND hWndToolBar=CreateToolBarCtrl(m_hWnd,IDR_MAINFRAME,hImageList);//CreateSimpleToolBarCtrl(m_hWnd,IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
		AddSimpleReBarBand(hWndToolBar);
		UIAddToolBar(hWndToolBar);
		SizeSimpleReBarBands();
	}

	// ステータスバーを作成
	m_hWndStatusBar=m_StatusBar.Create(m_hWnd);
	UIAddStatusBar(m_hWndStatusBar);
	int nPanes[] = {ID_DEFAULT_PANE, IDS_PANE_ITEMCOUNT_INITIAL,IDS_PANE_DLL_NAME_INITIAL};
	m_StatusBar.SetPanes(nPanes, COUNTOF(nPanes));
	{
		CString Text;
		Text.Format(IDS_PANE_ITEMCOUNT,0,0);
		m_StatusBar.SetPaneText(IDS_PANE_ITEMCOUNT_INITIAL,Text);
	}

//========================================
//      タブコントロールの初期化
//========================================
	m_TabClientWnd->Create(m_hWnd,rcDefault,NULL,WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN );
	m_TabClientWnd->addEventListener(m_hWnd);

	//タブを使わないなら非表示に
	if(m_ConfFLW.DisableTab)m_TabClientWnd->ShowTabCtrl(false);

	//---------
	//リストビュースタイル選択用メニューバーのラジオチェックを有効にする
	{
		CMenuHandle menuView = GetMenu();
		CMenuItemInfo mii;
		mii.fMask = MIIM_FTYPE;
		mii.fType = MFT_RADIOCHECK;
		menuView.SetMenuItemInfo(ID_MENUITEM_LISTVIEW_SMALLICON, FALSE, &mii);
		menuView.SetMenuItemInfo(ID_MENUITEM_LISTVIEW_LARGEICON, FALSE, &mii);
		menuView.SetMenuItemInfo(ID_MENUITEM_LISTVIEW_REPORT, FALSE, &mii);
		menuView.SetMenuItemInfo(ID_MENUITEM_LISTVIEW_LIST, FALSE, &mii);

		menuView.SetMenuItemInfo(ID_MENUITEM_LISTMODE_TREE, FALSE, &mii);
		menuView.SetMenuItemInfo(ID_MENUITEM_LISTMODE_FLAT, FALSE, &mii);
		menuView.SetMenuItemInfo(ID_MENUITEM_LISTMODE_FLAT_FILESONLY, FALSE, &mii);
	}

	//リストビュースタイルの設定
	if(m_ConfFLW.StoreSetting){
		//現在の表示設定のメニューにチェックを付ける
		switch(m_ConfFLW.ListStyle){
		case LVS_SMALLICON:
			UISetCheck(ID_MENUITEM_LISTVIEW_SMALLICON, TRUE);
			break;
		case LVS_ICON:
			UISetCheck(ID_MENUITEM_LISTVIEW_LARGEICON, TRUE);
			break;
		case LVS_LIST:
			UISetCheck(ID_MENUITEM_LISTVIEW_LIST, TRUE);
			break;
		case LVS_REPORT:
			UISetCheck(ID_MENUITEM_LISTVIEW_REPORT, TRUE);
			break;
		default:
			ASSERT(!"Error");
			break;
		}
	}else{
		UISetCheck(ID_MENUITEM_LISTVIEW_LARGEICON, TRUE);
		UISetCheck(ID_MENUITEM_LISTMODE_TREE, TRUE);
	}

	m_hWndClient = *m_TabClientWnd;
	UpdateLayout();

//========================================
//      メッセージハンドラの設定
//========================================
	// メッセージループにメッセージフィルタとアイドルハンドラを追加
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

//========================================
//      追加のキーボードアクセラレータ
//========================================
	if(m_ConfFLW.ExitWithEscape){
		m_AccelEx.LoadAccelerators(IDR_ACCEL_EX);
	}

	//メニュー更新
	EnableEntryExtractOperationMenu(false);
	EnableEntryDeleteOperationMenu(false);
	EnableAddItemsMenu(false);

//========================================
//    ファイル一覧ウィンドウのコマンド
//========================================
	MenuCommand_MakeSendToCommands();
	MenuCommand_UpdateUserAppCommands(m_ConfFLW);

	MenuCommand_MakeUserAppMenu(GetUserAppMenuHandle());
	MenuCommand_MakeSendToMenu(GetSendToMenuHandle());
	DrawMenuBar();

//==============================
// ウィンドウをアクティブにする
//==============================
	SetForegroundWindow(m_hWnd);
	UpdateLayout();

	//DnDによるファイル閲覧を可能に
	EnableDropTarget(true);
	return 0;
}

HMENU CFileListFrame::GetUserAppMenuHandle()
{
	CMenuHandle cMenu=GetMenu();
	CMenuHandle cSubMenu=cMenu.GetSubMenu(1);	//TODO:マジックナンバー
	int MenuCount=cSubMenu.GetMenuItemCount();
	int iIndex=-1;
	for(int i=0;i<=MenuCount;i++){
		if(-1==cSubMenu.GetMenuItemID(i)){	//ポップアップの親
			iIndex=i;
			break;
		}
	}
	ASSERT(-1!=iIndex);
	if(-1!=iIndex){
		return cSubMenu.GetSubMenu(iIndex);
	}else return NULL;
}

HMENU CFileListFrame::GetSendToMenuHandle()
{
	CMenuHandle cMenu=GetMenu();
	CMenuHandle cSubMenu=cMenu.GetSubMenu(1);	//TODO:マジックナンバー
	int MenuCount=cSubMenu.GetMenuItemCount();
	int iIndex=-1;
	for(int i=0;i<=MenuCount;i++){
		if(-1==cSubMenu.GetMenuItemID(i)){	//ポップアップの親
			iIndex=i;
			break;
		}
	}
	ASSERT(-1!=iIndex);
	if(-1!=iIndex){
		return cSubMenu.GetSubMenu(iIndex+1);
	}else return NULL;
}


LRESULT CFileListFrame::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	RemoveProp(m_hWnd, ms_strPropIdentifier.c_str());

	m_ConfFLW.load(mr_Config);

	bool bSave=false;
	//ウィンドウ設定の保存
	if(m_ConfFLW.StoreSetting){
		//ウィンドウサイズ
		m_ConfFLW.Width=m_WindowRect.Width();
		m_ConfFLW.Height=m_WindowRect.Height();

		m_TabClientWnd->StoreSettings(m_ConfFLW);

		if(m_ConfFLW.StoreWindowPosition){	//ウィンドウ位置を保存
			m_ConfFLW.WindowPos_x=m_WindowRect.left;
			m_ConfFLW.WindowPos_y=m_WindowRect.top;
		}

		m_ConfFLW.store(mr_Config);
		bSave=true;
	}
	if(m_ConfFLW.StoreWindowPosition){	//ウィンドウ位置だけ保存
		m_ConfFLW.WindowPos_x=m_WindowRect.left;
		m_ConfFLW.WindowPos_y=m_WindowRect.top;
		m_ConfFLW.store(mr_Config);
		bSave=true;
	}
	if(bSave){
		try {
			mr_Config.save();
		}catch(const LF_EXCEPTION& e){
			ErrorMessage(e.what());
		}
	}


	if(m_TabClientWnd->IsWindow())m_TabClientWnd->DestroyWindow();

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	PostQuitMessage(0);
	//bHandled=false;
	return 0;
}


HRESULT CFileListFrame::OpenArchiveFile(const std::filesystem::path& fname,bool bAllowRelayOpen)
{
	if(m_TabClientWnd->GetPageCount()>0 && !m_TabClientWnd->IsTabEnabled()){
		//tab is disabled; clone instance
		std::wstring strParam(L"/l ");

		auto filePath=fname;
		strParam += L"\"" + filePath.wstring() + L"\"";
		int ret = (int)ShellExecuteW(nullptr, nullptr, UtilGetModulePath().c_str(), strParam.c_str(), nullptr, SW_RESTORE);
		if(ret<=32){
			return E_FAIL;
		} else {
			return S_OK;
		}
	}else{
		//keep single instance
		if(bAllowRelayOpen && m_ConfFLW.KeepSingleInstance){
			g_hFirstWindow = nullptr;
			EnumWindows(EnumFirstFileListWindowProc,(LPARAM)m_hWnd);
			if(g_hFirstWindow){
				/*
				 * 1. set property {filename, my process id} to subject window
				 * 2. request subject window to find property containing my process id
				 * 3. open file found in property
				 * 4. remove my propety
				 */
				DWORD dwID = GetCurrentProcessId();
				::SetPropW(g_hFirstWindow, fname.c_str(), (HANDLE)dwID);
				HRESULT hr = ::SendMessageW(g_hFirstWindow, WM_FILELIST_OPEN_BY_PROPNAME, dwID, 0);
				::RemovePropW(g_hFirstWindow, fname.c_str());
				if (SUCCEEDED(hr))return S_FALSE;
			}
		}

		//prevent duplicated open
		std::wstring strMutex = L"LF" + replace(toLower(fname), L'\\', L'/');

		HANDLE hMutex=GetMultiOpenLockMutex(strMutex);
		if (hMutex) {
			//set title
			SetWindowTextW(UtilLoadString(IDR_MAINFRAME).c_str());
			EnableWindow(FALSE);

			//list content
			ARCLOG arcLog;
			try {
				m_TabClientWnd->OpenArchiveInTab(fname, strMutex, hMutex, arcLog);
			} catch (...) {
				//TODO
				ErrorMessage(arcLog.toString());
			}

			EnableWindow(TRUE);
		}else{
			//same file is opened; highlight existing window
			EnumWindows(EnumFileListWindowProc, (LPARAM)strMutex.c_str());
			return S_FALSE;
		}
	}
}

//ウィンドウプロパティの列挙
BOOL CALLBACK CFileListFrame::EnumPropProc(HWND hWnd,LPTSTR lpszString,HANDLE hData,ULONG_PTR dwData)
{
	if(dwData!=(ULONG_PTR)hData)return TRUE;
	else{
		g_FileToOpen=lpszString;
		return FALSE;
	}
}

LRESULT CFileListFrame::OnOpenByPropName(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	DWORD dwID=wParam;
	g_FileToOpen.clear();
	EnumPropsEx(m_hWnd,EnumPropProc,dwID);
	if(!g_FileToOpen.empty() && m_TabClientWnd->IsTabEnabled()){
		return OpenArchiveFile(g_FileToOpen,false);
	}else{
		return E_FAIL;
	}
}


HANDLE CFileListFrame::GetMultiOpenLockMutex(const std::wstring& strMutex)
{
	HANDLE hMutex=::CreateMutexW(nullptr, TRUE, strMutex.c_str());
	if(ERROR_ALREADY_EXISTS==GetLastError()){
		//already exists
		CloseHandle(hMutex);
		return nullptr;
	}else{
		return hMutex;
	}
}

std::wstring GetClassNameHelper(HWND hWnd)
{
	std::wstring name;
	name.resize(256);
	for (;;) {
		int bufsize = (int)name.size();
		auto nCopied = GetClassNameW(hWnd, &name[0], bufsize);
		if (nCopied < bufsize) {
			break;
		} else {
			name.resize(name.size() * 2);
		}
	}
	return name.c_str();
}


//ファイル一覧ウィンドウの列挙
BOOL CALLBACK CFileListFrame::EnumFileListWindowProc(HWND hWnd,LPARAM lParam)
{
	auto className = GetClassNameHelper(hWnd);
	if(LHAFORGE_FILE_LIST_CLASS == className){
		return TRUE;
	}

	if(GetPropW(hWnd,ms_strPropIdentifier.c_str())){
		HANDLE hProp=GetPropW(hWnd,(const wchar_t*)lParam);
		if(hProp){
			::SendMessageW(hWnd, WM_LHAFORGE_FILELIST_ACTIVATE_FILE, (WPARAM)hProp, NULL);
			return FALSE;
		}
	}
	return TRUE;
}

//最初のファイル一覧ウィンドウの列挙
BOOL CALLBACK CFileListFrame::EnumFirstFileListWindowProc(HWND hWnd,LPARAM lParam)
{
	if(hWnd!=(HWND)lParam){
		auto className = GetClassNameHelper(hWnd);
		if(LHAFORGE_FILE_LIST_CLASS == className){
			return TRUE;
		}

		//最初に見つけたLhaForgeのファイル一覧ウィンドウを記録する
		if(!g_hFirstWindow){
			g_hFirstWindow=hWnd;
			return FALSE;
		}
	}
	return TRUE;
}

LRESULT CFileListFrame::OnActivateFile(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_TabClientWnd->SetCurrentTab((HANDLE)wParam);
	ShowWindow(SW_RESTORE);
	SetForegroundWindow(m_hWnd);

	//点滅
	FLASHWINFO fi;
	FILL_ZERO(fi);
	fi.dwFlags=FLASHW_ALL;
	fi.hwnd=m_hWnd;
	fi.cbSize=sizeof(fi);
	fi.uCount=3;
	FlashWindowEx(&fi);
	return 0;
}


void CFileListFrame::EnableEntryExtractOperationMenu(bool bActive)
{
	// ファイルが選択されていないと無効なメニュー
	UINT menuList[]={
		ID_MENUITEM_EXTRACT_SELECTED,
		ID_MENUITEM_EXTRACT_SELECTED_SAMEDIR,
		ID_MENUITEM_OPEN_ASSOCIATION,
		ID_MENUITEM_OPEN_ASSOCIATION_OVERWRITE,
		ID_MENUITEM_EXTRACT_TEMPORARY,
	};
	for(size_t i=0;i<COUNTOF(menuList);i++){
		UIEnable(menuList[i],bActive);
	}

	//プログラムから開く/送るのメニュー
	CMenuHandle cMenu[]={GetUserAppMenuHandle(),GetSendToMenuHandle()};
	for(int iMenu=0;iMenu<COUNTOF(cMenu);iMenu++){
		int size=cMenu[iMenu].GetMenuItemCount();
		for(int i=0;i<size;i++){
			cMenu[iMenu].EnableMenuItem(i,MF_BYPOSITION | (bActive ? MF_ENABLED : MF_GRAYED));
		}
	};
}

void CFileListFrame::EnableEntryDeleteOperationMenu(bool bActive)
{
	UIEnable(ID_MENUITEM_DELETE_SELECTED,bActive);
}

void CFileListFrame::EnableAddItemsMenu(bool bActive)
{
	UIEnable(ID_MENUITEM_ADD_FILE,bActive);
	UIEnable(ID_MENUITEM_ADD_DIRECTORY,bActive);
}

void CFileListFrame::OnCommandCloseWindow(UINT uNotifyCode, int nID, HWND hWndCtl)
{
	DestroyWindow();
}

void CFileListFrame::OnConfigure(UINT uNotifyCode, int nID, HWND hWndCtl)
{
	//mr_Config.SaveConfig();
	CConfigDialog confdlg(mr_Config);
	if(IDOK==confdlg.DoModal()){
		try {
			mr_Config.save();
		}catch(const LF_EXCEPTION& e){
			ErrorMessage(e.what());
		}
		m_ConfFLW.load(mr_Config);

		MenuCommand_UpdateUserAppCommands(m_ConfFLW);
		MenuCommand_MakeUserAppMenu(GetUserAppMenuHandle());
		m_TabClientWnd->UpdateFileListConfig(m_ConfFLW);

		//アクセラレータの読み直し
		if(m_ConfFLW.ExitWithEscape){
			if(m_AccelEx.IsNull())m_AccelEx.LoadAccelerators(IDR_ACCEL_EX);
		}else{
			m_AccelEx.DestroyObject();
		}
	}else{
		//念のため再読み込み
		try {
			mr_Config.load();
		}catch(const LF_EXCEPTION& e) {
			ErrorMessage(e.what());
		}
	}

/*	else{	別にIDCANCELでもロードし直す必要はない。なぜならデータはダイアログ内で留まり、Config構造体に入らず捨てられているから
		Config.LoadConfig(CONFIG_LOAD_ALL);
	}*/
	//ファイル一覧ウィンドウのコマンド
	MenuCommand_MakeSendToCommands();

	MenuCommand_MakeSendToMenu(GetSendToMenuHandle());
	DrawMenuBar();
}

void CFileListFrame::OnSize(UINT uType, CSize)
{
	// 基底クラスのWM_SIZEメッセージハンドラも呼び出すため
	SetMsgHandled(false);

	if(0==uType){//0 (SIZE_RESTORED)ウィンドウがサイズ変更されました。ただし最小化または最大化ではありません。
		//最大化/最小化されているときにはウィンドウサイズは取得しない
		if(IsZoomed()||IsIconic())return;

		GetWindowRect(m_WindowRect);
	}
}

void CFileListFrame::OnMove(const CPoint&)
{
	// 基底クラスのWM_MOVEメッセージハンドラも呼び出すため
	SetMsgHandled(false);

	//最大化/最小化されているときにはウィンドウサイズは取得しない
	if(IsZoomed()||IsIconic())return;
	GetWindowRect(m_WindowRect);
}

void CFileListFrame::OnUpDir(UINT,int,HWND)
{
	CFileListTabItem* pTab=m_TabClientWnd->GetCurrentTab();
	if(pTab)pTab->Model.MoveUpDir();
}


void CFileListFrame::OnListViewStyle(UINT uNotifyCode,int nID,HWND hWndCtrl)
{
	DWORD dwStyle=0;

	switch(nID){
	case ID_MENUITEM_LISTVIEW_SMALLICON:
		dwStyle=LVS_SMALLICON;
		break;
	case ID_MENUITEM_LISTVIEW_LARGEICON:
		dwStyle=LVS_ICON;
		break;
	case ID_MENUITEM_LISTVIEW_LIST:
		dwStyle=LVS_LIST;
		break;
	case ID_MENUITEM_LISTVIEW_REPORT:
		dwStyle=LVS_REPORT;
		break;
	}

	m_TabClientWnd->SetListViewStyle(dwStyle);

	UISetCheck(ID_MENUITEM_LISTVIEW_SMALLICON, false);
	UISetCheck(ID_MENUITEM_LISTVIEW_LARGEICON, false);
	UISetCheck(ID_MENUITEM_LISTVIEW_REPORT, false);
	UISetCheck(ID_MENUITEM_LISTVIEW_LIST, false);
	UISetCheck(nID, true);
}


void CFileListFrame::UpdateUpDirButtonState()
{
	//「上に上る」ボタンの有効/無効
	CFileListTabItem* pTab=m_TabClientWnd->GetCurrentTab();
	if(pTab){
		if(pTab->Model.IsRoot()){
			UIEnable(ID_MENUITEM_UPDIR,false);
		}else{
			UIEnable(ID_MENUITEM_UPDIR,true);
		}
	}else{
		UIEnable(ID_MENUITEM_UPDIR,false);
	}
}

void CFileListFrame::UpdateMenuState()
{
	bool bActive=m_TabClientWnd->GetActivePage()!=-1;
	bool bTabActive=m_TabClientWnd->IsTabEnabled();

	std::vector<int> subjects = {
		ID_MENUITEM_CLOSETAB,
		ID_MENUITEM_EXTRACT_ARCHIVE,
		ID_MENUITEM_TEST_ARCHIVE,
		ID_MENUITEM_UPDIR,
		ID_MENUITEM_REFRESH,
		ID_MENUITEM_SELECT_ALL,
		ID_MENUITEM_CLEAR_TEMPORARY,
		ID_MENUITEM_EXTRACT_SELECTED,
		ID_MENUITEM_EXTRACT_SELECTED_SAMEDIR,
		ID_MENUITEM_DELETE_SELECTED,
		ID_MENUITEM_OPEN_ASSOCIATION,
		ID_MENUITEM_OPEN_ASSOCIATION_OVERWRITE,
		ID_MENUITEM_EXTRACT_TEMPORARY,
		ID_MENUITEM_FINDITEM,
		ID_MENUITEM_FINDITEM_END,
		ID_MENUITEM_LISTVIEW_SMALLICON,
		ID_MENUITEM_LISTVIEW_LARGEICON,
		ID_MENUITEM_LISTVIEW_REPORT,
		ID_MENUITEM_LISTVIEW_LIST,
		ID_MENUITEM_SHOW_COLUMNHEADER_MENU,
		ID_MENUITEM_LISTMODE_TREE,
		ID_MENUITEM_LISTMODE_FLAT,
		ID_MENUITEM_LISTMODE_FLAT_FILESONLY,
		ID_MENUITEM_SORT_FILENAME,
		ID_MENUITEM_SORT_FULLPATH,
		ID_MENUITEM_SORT_ORIGINALSIZE,
		ID_MENUITEM_SORT_TYPENAME,
		ID_MENUITEM_SORT_FILETIME,
		ID_MENUITEM_SORT_ATTRIBUTE,
		ID_MENUITEM_SORT_COMPRESSEDSIZE,
		ID_MENUITEM_SORT_METHOD,
		ID_MENUITEM_SORT_RATIO,
		ID_MENUITEM_SORT_CRC,
	};

	for (auto id : subjects) {
		UIEnable(id, bActive);
	}

	//tab menu
	UIEnable(ID_MENUITEM_NEXTTAB,bActive && bTabActive);
	UIEnable(ID_MENUITEM_PREVTAB,bActive && bTabActive);
	UIEnable(ID_MENUITEM_ADD_FILE,bActive && bTabActive);
	UIEnable(ID_MENUITEM_ADD_DIRECTORY,bActive && bTabActive);

	// open with program / sendto
	CMenuHandle cMenu[] = { GetUserAppMenuHandle(),GetSendToMenuHandle() };
	for(int iMenu=0;iMenu<COUNTOF(cMenu);iMenu++){
		int size=cMenu[iMenu].GetMenuItemCount();
		for(int i=0;i<size;i++){
			cMenu[iMenu].EnableMenuItem(i, MF_BYPOSITION | (bActive ? MF_ENABLED : MF_GRAYED));
		}
	}
}

void CFileListFrame::UpdateWindowTitle()
{
	CFileListTabItem* pTab=m_TabClientWnd->GetCurrentTab();
	if(pTab){
		//ウィンドウタイトルにファイル名設定
		CString Title;
		if(pTab->Model.IsArchiveEncrypted()){
			//パスワード付きの場合
			Title.Format(_T("[%s] %s - %s"),CString(MAKEINTRESOURCE(IDS_ENCRYPTED_ARCHIVE)),pTab->Model.GetArchiveFileName(),CString(MAKEINTRESOURCE(IDR_MAINFRAME)));
		}else{
			//通常アーカイブ
			Title.Format(_T("%s - %s"),pTab->Model.GetArchiveFileName().c_str(),CString(MAKEINTRESOURCE(IDR_MAINFRAME)));
		}
		SetWindowText(Title);
	}else{
		SetWindowText(CString(MAKEINTRESOURCE(IDR_MAINFRAME)));
	}
}

void CFileListFrame::UpdateStatusBar()
{
	CFileListTabItem* pTab=m_TabClientWnd->GetCurrentTab();
	if(pTab){
		CString Text;
		//---DLL情報
#pragma message("FIXME!")
		/*const CArchiverDLL *pDLL = pTab->Model.GetArchiver();
		if(pDLL){
			Text.Format(IDS_PANE_DLL_NAME,pDLL->GetName());
			m_StatusBar.SetPaneText(IDS_PANE_DLL_NAME_INITIAL,Text);
		}*/

		//---ファイル選択情報
		Text.Format(IDS_PANE_ITEMCOUNT,pTab->ListView.GetItemCount(),pTab->ListView.GetSelectedCount());
		m_StatusBar.SetPaneText(IDS_PANE_ITEMCOUNT_INITIAL,Text);
	}
}

LRESULT CFileListFrame::OnFileListModelChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UpdateWindowTitle();
	UpdateMenuState();
	UpdateStatusBar();
	UpdateUpDirButtonState();

	return 0;
}

LRESULT CFileListFrame::OnFileListArchiveLoaded(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UpdateUpDirButtonState();
	return 0;
}

LRESULT CFileListFrame::OnFileListNewContent(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UpdateUpDirButtonState();
	return 0;
}

LRESULT CFileListFrame::OnFileListUpdated(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UpdateUpDirButtonState();
	//nothing to do
	return 0;
}

LRESULT CFileListFrame::OnFileListWndStateChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UpdateWindowTitle();
	UpdateStatusBar();
	UpdateMenuState();
	UpdateUpDirButtonState();

	CFileListTabItem* pTab=m_TabClientWnd->GetCurrentTab();
	if(pTab && pTab->Model.IsOK()){
		bool bFileListActive=(::GetFocus()==pTab->ListView);

		int SelCount=pTab->ListView.GetSelectedCount();
		bool bSelected=SelCount>0;

		//UI更新
		EnableEntryExtractOperationMenu(bFileListActive && bSelected);
		EnableEntryDeleteOperationMenu(bFileListActive && pTab->Model.IsModifySupported() && bSelected);
		EnableAddItemsMenu(pTab->Model.IsModifySupported());

		//ステータスバー更新
		CString Text;
		Text.Format(IDS_PANE_ITEMCOUNT,pTab->ListView.GetItemCount(),SelCount);
		m_StatusBar.SetPaneText(IDS_PANE_ITEMCOUNT_INITIAL,Text);
	}else{
		//UI更新
		EnableEntryExtractOperationMenu(false);
		EnableEntryDeleteOperationMenu(false);
		EnableAddItemsMenu(false);

		//ステータスバー更新
		m_StatusBar.SetPaneText(IDS_PANE_ITEMCOUNT_INITIAL,_T(""));
	}
	return 0;
}

//リストビューとツリービューでフォーカス切り替え
void CFileListFrame::OnToggleFocus(UINT,int,HWND)
{
	CFileListTabItem* pTab=m_TabClientWnd->GetCurrentTab();
	if(pTab){
		pTab->Splitter.ActivateNextPane();
	}
}

//ファイルリスト更新
void CFileListFrame::OnRefresh(UINT,int,HWND)
{
	ReopenArchiveFile();
}

//ファイルリスト更新
LRESULT CFileListFrame::OnRefresh(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
	ReopenArchiveFile();
	return 0;
}


void CFileListFrame::ReopenArchiveFile()
{
	m_TabClientWnd->ReopenArchiveFile();
}

void CFileListFrame::OnOpenArchive(UINT uNotifyCode, int nID, HWND hWndCtrl)
{
	const COMDLG_FILTERSPEC filter[] = {
		{ L"All Files", L"*.*" },
	};

	LFShellFileOpenDialog dlg(nullptr, FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT, nullptr, filter, COUNTOF(filter));
	if (IDCANCEL == dlg.DoModal()) {	//cancel
		return;
	}
	auto files = dlg.GetMultipleFiles();

	for (const auto &file : files) {
		HRESULT hr = OpenArchiveFile(file, false);
		if (E_ABORT == hr)break;
	}
}

void CFileListFrame::OnCloseTab(UINT uNotifyCode,int nID,HWND hWndCtrl)
{
	m_TabClientWnd->CloseCurrentTab();
	if(!m_TabClientWnd->IsTabEnabled()){
		DestroyWindow();
	}
}

void CFileListFrame::OnNextTab(UINT uNotifyCode,int nID,HWND hWndCtrl)
{
	int size=m_TabClientWnd->GetPageCount();
	if(m_TabClientWnd && size>0){
		int nActive=m_TabClientWnd->GetActivePage();
		if(ID_MENUITEM_NEXTTAB==nID){
			m_TabClientWnd->SetCurrentTab((nActive+1)%size);
		}else{
			m_TabClientWnd->SetCurrentTab((nActive+size-1)%size);
		}
	}
}

LRESULT CFileListFrame::OnMouseWheel(UINT uCode,short delta,CPoint&)
{
	int size=m_TabClientWnd->GetPageCount();
	if(m_TabClientWnd && size>0 && uCode & MK_CONTROL){
		int step=-delta/WHEEL_DELTA;
		while(step<-size)step+=size;

		int nActive=m_TabClientWnd->GetActivePage();
		m_TabClientWnd->SetCurrentTab((nActive+size+step)%size);
	}else{
		SetMsgHandled(FALSE);
	}
	return 0;
}


void CFileListFrame::EnableDropTarget(bool bEnable)
{
	if(bEnable){
		//enable drop
		::RegisterDragDrop(m_hWnd, &m_DropTarget);
	}else{
		//disable drop
		::RevokeDragDrop(m_hWnd);
	}
}

// IDropCommunicator
HRESULT CFileListFrame::DragEnter(IDataObject *lpDataObject,POINTL &pt,DWORD &dwEffect)
{
	return DragOver(lpDataObject,pt,dwEffect);
}

HRESULT CFileListFrame::DragLeave()
{
	return S_OK;
}

HRESULT CFileListFrame::DragOver(IDataObject *lpDataObject,POINTL &pt,DWORD &dwEffect)
{
	//フォーマットに対応した処理をする
	if(!m_DropTarget.QueryFormat(CF_HDROP)){	//ファイル専用
		//ファイルではないので拒否
		dwEffect = DROPEFFECT_NONE;
	}else{
		dwEffect = DROPEFFECT_COPY;// : DROPEFFECT_NONE;
		//ファイル取得
		auto[hr, files] = m_DropTarget.GetDroppedFiles(lpDataObject);
		//---ディレクトリが含まれていたら拒否
		if(S_OK==hr){
			for(const auto &file:files){
				if(PathIsDirectory(file.c_str())){
					dwEffect = DROPEFFECT_NONE;
					break;
				}
			}
		}
	}
	return S_OK;
}

//ファイルのドロップ
HRESULT CFileListFrame::Drop(IDataObject *lpDataObject,POINTL &pt,DWORD &dwEffect)
{
	//ファイル取得
	auto[hr, files] = m_DropTarget.GetDroppedFiles(lpDataObject);
	if(S_OK==hr){
		dwEffect = DROPEFFECT_COPY;

		//開く
		for (const auto &file : files) {
			if (E_ABORT == OpenArchiveFile(file.c_str(), false)) {
				break;
			}
		}

		return S_OK;
	}else{
		//受け入れできない形式
		dwEffect = DROPEFFECT_NONE;
		return S_FALSE;	//S_OK
	}
}


//----------------
HWND CFileListFrame::CreateToolBarCtrl(HWND hWndParent, UINT nResourceID,HIMAGELIST hImageList)
{
	DWORD dwStyle = ATL_SIMPLE_TOOLBAR_PANE_STYLE;
	UINT nID = ATL_IDW_TOOLBAR;
	HINSTANCE hInst = ModuleHelper::GetResourceInstance();
	HRSRC hRsrc = ::FindResource(hInst, MAKEINTRESOURCE(nResourceID), RT_TOOLBAR);
	if (hRsrc == NULL)return NULL;

	HGLOBAL hGlobal = ::LoadResource(hInst, hRsrc);
	if (hGlobal == NULL)return NULL;

	_AtlToolBarData* pData = (_AtlToolBarData*)::LockResource(hGlobal);
	if (pData == NULL)return NULL;
	ATLASSERT(pData->wVersion == 1);

	WORD* pItems = pData->items();
	int nItems = pData->wItemCount;
	CTempBuffer<TBBUTTON, _WTL_STACK_ALLOC_THRESHOLD> buff;
	TBBUTTON* pTBBtn = buff.Allocate(nItems);
	ATLASSERT(pTBBtn != NULL);
	if(pTBBtn == NULL)return NULL;

	const int cxSeparator = 8;

	int nBmp = 0;
	for(int i = 0; i < pData->wItemCount; i++){
		if(pItems[i] != 0){
			pTBBtn[i].iBitmap = nBmp++;
			pTBBtn[i].idCommand = pItems[i];
			pTBBtn[i].fsState = TBSTATE_ENABLED;
			pTBBtn[i].fsStyle = TBSTYLE_BUTTON;
			pTBBtn[i].dwData = 0;
			pTBBtn[i].iString = 0;
		}else{
			pTBBtn[i].iBitmap = cxSeparator;
			pTBBtn[i].idCommand = 0;
			pTBBtn[i].fsState = 0;
			pTBBtn[i].fsStyle = TBSTYLE_SEP;
			pTBBtn[i].dwData = 0;
			pTBBtn[i].iString = 0;
		}
	}

	HWND hWnd = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, dwStyle, 0, 0, 100, 100, hWndParent, (HMENU)LongToHandle(nID), ModuleHelper::GetModuleInstance(), NULL);
	if(hWnd == NULL){
		ATLASSERT(FALSE);
		return NULL;
	}

	::SendMessage(hWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0L);

	// check if font is taller than our bitmaps
	CFontHandle font = (HFONT)::SendMessage(hWnd, WM_GETFONT, 0, 0L);
	if(font.IsNull())
		font = AtlGetDefaultGuiFont();
	LOGFONT lf = { 0 };
	font.GetLogFont(lf);
	WORD cyFontHeight = (WORD)abs(lf.lfHeight);

	WORD bitsPerPixel = AtlGetBitmapResourceBitsPerPixel(nResourceID);
	if(hImageList || bitsPerPixel > 4){
		COLORREF crMask = CLR_DEFAULT;
		if(bitsPerPixel == 32){
			// 32-bit color bitmap with alpha channel (valid for Windows XP and later)
			crMask = CLR_NONE;
		}
		if(!hImageList){
			hImageList = ImageList_LoadImage(ModuleHelper::GetResourceInstance(), MAKEINTRESOURCE(nResourceID), pData->wWidth, 1, crMask, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
		}
		ATLASSERT(hImageList != NULL);
		::SendMessage(hWnd, TB_SETIMAGELIST, 0, (LPARAM)hImageList);
	}else{
		TBADDBITMAP tbab = { 0 };
		tbab.hInst = hInst;
		tbab.nID = nResourceID;
		::SendMessage(hWnd, TB_ADDBITMAP, nBmp, (LPARAM)&tbab);
	}

	::SendMessage(hWnd, TB_ADDBUTTONS, nItems, (LPARAM)pTBBtn);
	::SendMessage(hWnd, TB_SETBITMAPSIZE, 0, MAKELONG(pData->wWidth, std::max(pData->wHeight, cyFontHeight)));
	const int cxyButtonMargin = 7;
	::SendMessage(hWnd, TB_SETBUTTONSIZE, 0, MAKELONG(pData->wWidth + cxyButtonMargin, std::max(pData->wHeight, cyFontHeight) + cxyButtonMargin));

	return hWnd;
}


