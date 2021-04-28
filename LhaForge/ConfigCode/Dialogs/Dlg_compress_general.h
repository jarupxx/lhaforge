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
#include "Dlg_Base.h"
#include "resource.h"
#include "ConfigCode/ConfigFile.h"
#include "ConfigCode/ConfigCompress.h"
#include "ArchiverCode/archive.h"

class CConfigDlgCompressGeneral : public LFConfigDialogBase<CConfigDlgCompressGeneral>
{
protected:
	CConfigCompress	m_Config;
	CUpDownCtrl UpDown_MaxCompressFileCount;

	void SetParameterInfo();
public:
	enum { IDD = IDD_PROPPAGE_CONFIG_COMPRESS_GENERAL };

	BEGIN_DDX_MAP(CConfigGeneral)
		DDX_CHECK(IDC_CHECK_ALWAYS_SPEFICY_OUTPUT_FILENAME, m_Config.SpecifyOutputFilename)
		DDX_CHECK(IDC_CHECK_OPEN_FOLDER_AFTER_COMPRESS, m_Config.OpenDir)
		DDX_CHECK(IDC_CHECK_LIMIT_COMPRESS_FILECOUNT,m_Config.LimitCompressFileCount)
		DDX_CHECK(IDC_CHECK_USE_DEFAULTPARAMETER,m_Config.UseDefaultParameter)
		DDX_CHECK(IDC_CHECK_DELETE_AFTER_COMPRESS,m_Config.DeleteAfterCompress)
		DDX_CHECK(IDC_CHECK_MOVETO_RECYCLE_BIN,m_Config.MoveToRecycleBin)
		DDX_CHECK(IDC_CHECK_DELETE_NOCONFIRM,m_Config.DeleteNoConfirm)
		DDX_CHECK(IDC_CHECK_FORCE_DELETE,m_Config.ForceDelete)
		DDX_CHECK(IDC_CHECK_IGNORE_TOP_DIRECTORY,m_Config.IgnoreTopDirectory)
		DDX_RADIO(IDC_RADIO_COMPRESS_TO_DESKTOP, m_Config.OutputDirType)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(CConfigDlgCompressGeneral)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_RANGE_HANDLER(IDC_RADIO_COMPRESS_TO_DESKTOP,IDC_RADIO_COMPRESS_TO_ALWAYS_ASK_WHERE, OnRadioCompressTo)
		COMMAND_ID_HANDLER(IDC_BUTTON_COMPRESS_BROWSE_FOLDER,OnBrowseFolder)
		COMMAND_ID_HANDLER(IDC_CHECK_LIMIT_COMPRESS_FILECOUNT,OnCheckLimitCompressFileCount)
		COMMAND_ID_HANDLER(IDC_CHECK_USE_DEFAULTPARAMETER,OnCheckUseDefaultParameter)
		COMMAND_ID_HANDLER(IDC_BUTTON_SELECT_DEFAULTPARAMETER,OnSelectDefaultParameter)
		COMMAND_ID_HANDLER(IDC_CHECK_DELETE_AFTER_COMPRESS,OnCheckDelete)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	LRESULT OnApply();
	LRESULT OnRadioCompressTo(WORD,WORD,HWND,BOOL&);
	LRESULT OnBrowseFolder(WORD,WORD,HWND,BOOL&);
	LRESULT OnCheckLimitCompressFileCount(WORD,WORD,HWND,BOOL&);
	LRESULT OnCheckUseDefaultParameter(WORD,WORD,HWND,BOOL&);
	LRESULT OnSelectDefaultParameter(WORD,WORD,HWND,BOOL&);
	LRESULT OnCheckDelete(WORD,WORD,HWND,BOOL&);

	void LoadConfig(CConfigFile& Config){
		m_Config.load(Config);
	}
	void StoreConfig(CConfigFile& Config, CConfigFile& assistant){
		m_Config.store(Config);
	}
};

