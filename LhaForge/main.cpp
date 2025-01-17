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
#include "ConfigCode/Dialogs/configwnd.h"
#include "compress.h"
#include "extract.h"
#include "ArchiverCode/archive.h"
#include "FileListWindow/FileListFrame.h"
#include "Dialogs/SelectDlg.h"
#include "Dialogs/ProgressDlg.h"
#include "Utilities/OSUtil.h"
#include "Utilities/StringUtil.h"
#include "CmdLineInfo.h"

#include "ConfigCode/ConfigOpenAction.h"
#include "ConfigCode/ConfigGeneral.h"

CAppModule _Module;



//enumerates files, removes directory
std::vector<std::filesystem::path> enumerateFiles(const std::vector<std::filesystem::path>& input, const std::vector<std::wstring>& denyExts)
{
	std::vector<std::filesystem::path> out;
	for (const auto &item: input) {
		std::vector<std::filesystem::path> children;
		if (std::filesystem::is_directory(item)) {
			children = UtilRecursiveEnumFile(item);
		} else {
			children = { item };
		}
		for (const auto &subItem : children) {
			bool bDenied = false;
			for (const auto& deny : denyExts) {
				if (UtilExtMatchSpec(subItem, deny)) {
					bDenied = true;
					break;
				}
			}
			//finally
			if (!bDenied) {
				out.push_back(subItem);
			}
		}
	}
	return out;
}
#ifdef UNIT_TEST
TEST(main, enumerateFiles)
{
	auto dir = UtilGetTempPath() / L"lhaforge_test/enumerateFiles";
	UtilDeletePath(dir);
	EXPECT_FALSE(std::filesystem::exists(dir));
	std::filesystem::create_directories(dir / L"abc");
	touchFile(dir / L"abc/ghi.txt");
	std::filesystem::create_directories(dir / L"def");
	touchFile(dir / L"def/test.exe");
	touchFile(dir / L"def/test.bat");

	auto out = enumerateFiles({ dir / L"abc", dir / L"def" }, { L".exe", L".bat" });
	EXPECT_EQ(1, out.size());
	if (out.size() > 0) {
		EXPECT_EQ(dir / L"abc/ghi.txt", out[0]);
	}

	UtilDeletePath(dir);
	EXPECT_FALSE(std::filesystem::exists(dir));
}

#endif

PROCESS_MODE selectOpenAction()
{
	class COpenActionDialog : public CDialogImpl<COpenActionDialog> {
	public:
		enum { IDD = IDD_DIALOG_OPENACTION_SELECT };
		BEGIN_MSG_MAP_EX(COpenActionDialog)
			COMMAND_ID_HANDLER_EX(IDC_BUTTON_OPENACTION_EXTRACT, OnButton)
			COMMAND_ID_HANDLER_EX(IDC_BUTTON_OPENACTION_LIST, OnButton)
			COMMAND_ID_HANDLER_EX(IDC_BUTTON_OPENACTION_TEST, OnButton)
			COMMAND_ID_HANDLER_EX(IDCANCEL, OnButton)
			END_MSG_MAP()

		void OnButton(UINT uNotifyCode, int nID, HWND hWndCtl) {
			EndDialog(nID);
		}
	};

	COpenActionDialog Dialog;
	switch (Dialog.DoModal()) {
	case IDC_BUTTON_OPENACTION_EXTRACT:
		return PROCESS_MODE::EXTRACT;
	case IDC_BUTTON_OPENACTION_LIST:
		return PROCESS_MODE::LIST;
	case IDC_BUTTON_OPENACTION_TEST:
		return PROCESS_MODE::TEST;
	default:
		return PROCESS_MODE::INVALID;
	}
}

//---------------------------------------------

bool DoCompress(CConfigFile &config, CMDLINEINFO &cli)
{
	CConfigCompress ConfCompress;
	CConfigGeneral ConfGeneral;
	ConfCompress.load(config);
	ConfGeneral.load(config);

	if(LF_ARCHIVE_FORMAT::INVALID == cli.CompressType){
		if(ConfCompress.UseDefaultParameter){
			cli.CompressType = ConfCompress.DefaultType;
			cli.Options = ConfCompress.DefaultOptions;
		}else{	//not default parameter
			auto [format, options, singleCompression, deleteAfterCompress] = GUI_SelectCompressType();
			if(LF_ARCHIVE_FORMAT::INVALID ==format){	//cancel
				return false;
			}else{
				cli.CompressType = format;
				cli.Options = options;
				cli.bSingleCompression = singleCompression;
				if (deleteAfterCompress) {
					cli.DeleteAfterProcess = CMDLINEINFO::ACTION::True;
				} else {
					cli.DeleteAfterProcess = CMDLINEINFO::ACTION::False;
				}
			}
		}
	}

	//--------------------
	return GUI_compress_multiple_files(
		cli.FileList,
		cli.CompressType,
		(LF_WRITE_OPTIONS)cli.Options,
		CLFProgressHandlerGUI(nullptr),
		config,
		&cli);
}

bool DoExtract(CConfigFile &config,CMDLINEINFO &cli)
{
	CConfigExtract ConfExtract;
	ConfExtract.load(config);
	const auto denyList = UtilSplitString(ConfExtract.DenyExt, L";");

	auto tmp = enumerateFiles(cli.FileList, denyList);
	remove_item_if(tmp, [](const std::wstring& file) {return !CLFArchive::is_known_format(file); });

	if(tmp.empty()){
		ErrorMessage(UtilLoadString(IDS_ERROR_FILE_NOT_SPECIFIED));
		return false;
	}
	return GUI_extract_multiple_files(tmp, CLFProgressHandlerGUI(nullptr), &cli);
}

bool DoList(CConfigFile &config,CMDLINEINFO &cli)
{
	CConfigExtract ConfExtract;
	ConfExtract.load(config);
	const auto denyList = UtilSplitString(ConfExtract.DenyExt, L";");

	auto tmp = enumerateFiles(cli.FileList, denyList);
	remove_item_if(tmp, [](const std::wstring& file) {return !CLFArchive::is_known_format(file); });

	if(!cli.FileList.empty() && tmp.empty()){
		ErrorMessage(UtilLoadString(IDS_ERROR_FILE_NOT_SPECIFIED));
		return false;
	}

	CFileListFrame ListWindow(config);
	ListWindow.CreateEx();
	ListWindow.ShowWindow(SW_SHOW);
	ListWindow.UpdateWindow();
	bool bAllFailed = !tmp.empty();
	for (const auto& item : tmp) {
		HRESULT hr = ListWindow.OpenArchiveFile(item);
		if (SUCCEEDED(hr)) {
			if (hr != S_FALSE)bAllFailed = false;
		} else if (hr == E_ABORT) {
			break;
		}
	}
	if(bAllFailed)ListWindow.DestroyWindow();

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->Run();
	return true;
}

bool DoTest(CConfigFile &config,CMDLINEINFO &cli)
{
	CConfigExtract ConfExtract;
	ConfExtract.load(config);
	const auto denyList = UtilSplitString(ConfExtract.DenyExt, L";");

	auto tmp = enumerateFiles(cli.FileList, denyList);

	if(tmp.empty()){
		ErrorMessage(UtilLoadString(IDS_ERROR_FILE_NOT_SPECIFIED));
		return false;
	}

	return GUI_test_multiple_files(tmp, CLFProgressHandlerGUI(nullptr), &cli);
}

void procMain()
{
	auto[ProcessMode, cli] = ParseCommandLine(GetCommandLineW(), ErrorMessage);
	if (PROCESS_MODE::INVALID == ProcessMode) {
		return;
	}

	CConfigFile config;
	if (cli.ConfigPath.empty()) {
		config.setDefaultPath();
	} else {
		config.setPath(cli.ConfigPath.c_str());
	}
	try{
		config.load();
	} catch (const LF_EXCEPTION &e) {
		ErrorMessage(e.what());
	}

	//key modifier
	{
		bool shift = GetKeyState(VK_SHIFT) < 0;
		bool control = GetKeyState(VK_CONTROL) < 0;

		switch (ProcessMode) {
		case PROCESS_MODE::COMPRESS:
			if (control) {
				//single compression if ctrl is pressed
				cli.bSingleCompression = true;
			}
			break;
		case PROCESS_MODE::EXTRACT:
			if (shift) {
				ProcessMode = PROCESS_MODE::LIST;	//list mode if shift is pressed
			} else if (control) {
				ProcessMode = PROCESS_MODE::TEST;	//test mode if ctrl is pressed
			}
			break;
		case PROCESS_MODE::MANAGED:
		{
			CConfigOpenAction ConfOpenAction;
			ConfOpenAction.load(config);
			OPENACTION OpenAction;
			if (shift) {	//---when shift is pressed
				OpenAction = (OPENACTION)ConfOpenAction.OpenAction_Shift;
			} else if (control) {	//---when ctrl is pressed
				OpenAction = (OPENACTION)ConfOpenAction.OpenAction_Ctrl;
			} else {	//---default
				OpenAction = (OPENACTION)ConfOpenAction.OpenAction;
			}
			switch (OpenAction) {
			case OPENACTION::EXTRACT:
				ProcessMode = PROCESS_MODE::EXTRACT;
				break;
			case OPENACTION::LIST:
				ProcessMode = PROCESS_MODE::LIST;
				break;
			case OPENACTION::TEST:
				ProcessMode = PROCESS_MODE::TEST;
				break;
			case OPENACTION::ASK:
			default:
				ProcessMode = selectOpenAction();
				if (ProcessMode == PROCESS_MODE::INVALID) {
					return;
				}
				break;
			}
		}
		break;
		}
	}

	CConfigGeneral ConfGeneral;
	ConfGeneral.load(config);
	LF_setProcessTempPath(ConfGeneral.TempPath);

	switch (ProcessMode) {
	case PROCESS_MODE::COMPRESS:
		DoCompress(config, cli);
		break;
	case PROCESS_MODE::EXTRACT:
		DoExtract(config, cli);
		break;
	case PROCESS_MODE::AUTOMATIC:
		if (std::filesystem::is_directory(cli.FileList.front())) {
			DoCompress(config, cli);
		} else {
			CConfigExtract ConfExtract;
			ConfExtract.load(config);
			bool allowed = ConfExtract.isPathAcceptableToExtract(cli.FileList.front());
			if (allowed && CLFArchive::is_known_format(cli.FileList.front())) {
				DoExtract(config, cli);
			} else {
				DoCompress(config, cli);
			}
		}
		break;
	case PROCESS_MODE::LIST:
		DoList(config, cli);
		break;
	case PROCESS_MODE::TEST:
		DoTest(config, cli);
		break;
	case PROCESS_MODE::CONFIGURE:
	default:
	{
		CConfigDialog confdlg(config);
		if (IDOK == confdlg.DoModal()) {
			try {
				config.save();
			}catch(const LF_EXCEPTION& e){
				ErrorMessage(e.what());
			}
		}
		break;
	}
	}
}



int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
#if defined(_DEBUG)
	// detect memory leaks
	_CrtSetDbgFlag(
		_CRTDBG_ALLOC_MEM_DF
		| _CRTDBG_LEAK_CHECK_DF
	);
#endif
	_wsetlocale(LC_ALL, L"");	//default locale

	HRESULT hRes = ::CoInitialize(nullptr);
	ATLASSERT(SUCCEEDED(hRes));
	OleInitialize(nullptr);

	// support control flags
	AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_COOL_CLASSES | ICC_BAR_CLASSES);
	_Module.Init(nullptr, hInstance);
	//CMessageLoop theLoop;
	CCustomMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	procMain();

	_Module.RemoveMessageLoop();
	_Module.Term();
	OleUninitialize();
	::CoUninitialize();
	return 0;
}


#ifdef UNIT_TEST
int wmain(int argc, wchar_t *argv[], wchar_t *envp[]){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
#endif
