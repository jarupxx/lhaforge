#pragma once

class CLFShellFileOpenDialog : public CShellFileOpenDialog
{
public:
	CLFShellFileOpenDialog(LPCWSTR lpszFileName = nullptr,
		DWORD dwOptions = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST,
		LPCWSTR lpszDefExt = nullptr,
		const COMDLG_FILTERSPEC* arrFilterSpec = nullptr,
		UINT uFilterSpecCount = 0U) : CShellFileOpenDialog(lpszFileName ? std::filesystem::path(lpszFileName).filename().c_str() : nullptr, dwOptions, lpszDefExt, arrFilterSpec, uFilterSpecCount)
	{
		if (lpszFileName) {
			ATL::CComPtr<IShellItem> spItem;
			auto path = std::filesystem::path(lpszFileName).make_preferred();
			HRESULT hr = SHCreateItemFromParsingName(
				path.parent_path().c_str(),
				nullptr, IID_IShellItem, (void**)&spItem);
			if (SUCCEEDED(hr)) {
				GetPtr()->SetFolder(spItem);
			}
		}
	}

	virtual ~CLFShellFileOpenDialog()
	{ }
	std::vector<std::filesystem::path> GetMultipleFiles() {
		std::vector<std::filesystem::path> files;
		{
			auto ptr = GetPtr();
			ATL::CComPtr<IShellItemArray> spArray;
			HRESULT hRet = ptr->GetResults(&spArray);

			if (SUCCEEDED(hRet)) {
				DWORD count;
				spArray->GetCount(&count);
				for (DWORD i = 0; i < count; i++) {
					ATL::CComPtr<IShellItem> spItem;
					spArray->GetItemAt(i, &spItem);
					CString path;
					GetFileNameFromShellItem(spItem, SIGDN_FILESYSPATH, path);
					files.push_back(path.operator LPCWSTR());
				}
			}
		}
		return files;
	}
};

class CLFShellFileSaveDialog : public CShellFileSaveDialog
{
public:
	CLFShellFileSaveDialog(LPCWSTR lpszFileName = nullptr,
		DWORD dwOptions = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST,
		LPCWSTR lpszDefExt = nullptr,
		const COMDLG_FILTERSPEC* arrFilterSpec = nullptr,
		UINT uFilterSpecCount = 0U) : CShellFileSaveDialog(lpszFileName ? std::filesystem::path(lpszFileName).filename().c_str() : nullptr, dwOptions, lpszDefExt, arrFilterSpec, uFilterSpecCount)
	{
		if (lpszFileName) {
			ATL::CComPtr<IShellItem> spItem;
			auto path = std::filesystem::path(lpszFileName).make_preferred();
			HRESULT hr = SHCreateItemFromParsingName(
				path.parent_path().c_str(),
				nullptr, IID_IShellItem, (void**)&spItem);
			if (SUCCEEDED(hr)) {
				GetPtr()->SetFolder(spItem);
			}
		}
	}

	virtual ~CLFShellFileSaveDialog()
	{ }
};

class CLFComboListViewCtrl :public CWindowImpl<CLFComboListViewCtrl, CListViewCtrl>, public CCustomDraw< CLFComboListViewCtrl>{
public:
	struct CONTENT_DATA {
		std::wstring key;
		std::vector<std::wstring> options;	//separator if options is empty
		int selection;
		void* userData;
	};
protected:
	CComboBox _combo;
	CImageList _dummyImageList;	//to keep item height
	std::vector<CONTENT_DATA> _data;
public:
	BEGIN_MSG_MAP(CLFComboListViewCtrl)
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_ITEMCHANGED, OnItemChanged)	//move combo box
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_ENDSCROLL, OnItemScrolled)	//follow list view scroll
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_KEYDOWN, OnKeyDown)	//open combo box menu
		COMMAND_CODE_HANDLER_EX(CBN_SELCHANGE, OnOptionChanged)	//update selection
		REFLECTED_NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	void SetSubjectWindow(HWND hWnd) {
		SubclassWindow(hWnd);
		SetExtendedListViewStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
		DWORD Style = GetWindowLong(GWL_STYLE);
		Style &= ~(LVS_ICON | LVS_REPORT | LVS_SMALLICON | LVS_LIST);
		SetWindowLong(GWL_STYLE, Style | LVS_REPORT | LVS_SINGLESEL);
	}
	int GetFontHeight() {
		CDC hDC;
		hDC.CreateCompatibleDC(nullptr);
		HFONT hFontOld = (HFONT)SelectObject(hDC, GetFont());

		TEXTMETRIC tm = {};
		hDC.GetTextMetrics(&tm);
		SelectObject(hDC, hFontOld);
		return tm.tmHeight + tm.tmExternalLeading;
	}
	void SetContentData(const std::wstring& column0, const std::wstring& column1, const std::vector<CONTENT_DATA>& d) {
		if (!_combo.IsWindow()) {
			_combo.Create(m_hWnd, NULL, NULL, WS_CHILD | CBS_DROPDOWNLIST);
			_combo.SetFont(GetFont());
		}
		//---reset old
		DeleteAllItems();

		int nColumnCount = GetHeader().GetItemCount();
		for (int i = 0; i < nColumnCount; i++) {
			DeleteColumn(0);
		}

		//---item height spacer
		if (_dummyImageList.IsNull()) {
			//int itemHeight = GetFontHeight() * 2;
			CRect rc;
			_combo.GetWindowRect(rc);
			int itemHeight = rc.Height();
			_dummyImageList.Create(1, itemHeight, ILC_COLOR32, 1, 1);
			SetImageList(_dummyImageList, LVSIL_NORMAL);
			SetImageList(_dummyImageList, LVSIL_SMALL);
		}
		//---set new
		_data = d;

		CRect rc;
		GetClientRect(&rc);
		InsertColumn(0, column0.c_str(), LVCFMT_LEFT, 80, -1);
		InsertColumn(1, column1.c_str(), LVCFMT_LEFT, rc.Width() - 80 - GetSystemMetrics(SM_CXVSCROLL), -1);

		UINT columns[] = { 0,1 };
		for (int i = 0; i < (int)_data.size(); i++) {
			LVITEM li = {};
			li.iItem = i;
			li.cColumns = COUNTOF(columns);
			li.puColumns = columns;
			li.iImage = 0;
			li.mask |= LVIF_COLUMNS | LVIF_IMAGE;
			InsertItem(&li);

			auto& item = _data[i];
			if (item.options.empty()) {
				//separator
				item.selection = -1;
				SetItemText(i, 0, item.key.c_str());
				SetItemText(i, 1, L"");
			} else {
				//standard item
				item.selection = std::min(std::max(0, item.selection), (int)item.options.size() - 1);
				SetItemText(i, 0, item.key.c_str());
				SetItemText(i, 1, item.options[item.selection].c_str());
			}
		}
	}
	const std::vector<CONTENT_DATA>& GetContentData()const { return _data; }

	LRESULT OnItemChanged(LPNMHDR pnmh) {
		NM_LISTVIEW* pView = (NM_LISTVIEW*)pnmh;
		if (pView->iItem >= 0
			&& pView->iItem < (int)_data.size()
			&& (pView->uNewState & LVIS_SELECTED)
			&& !_data[pView->iItem].options.empty()) {

			//_combo.LockWindowUpdate(TRUE);
			_combo.ResetContent();
			for (const auto& opt : _data[pView->iItem].options) {
				_combo.AddString(opt.c_str());
			}
			_combo.SetCurSel(_data[pView->iItem].selection);

			//move combo box
			CRect rect;
			GetSubItemRect(pView->iItem, 1, LVIR_BOUNDS, &rect);
			_combo.MoveWindow(rect);
			_combo.ShowWindow(SW_SHOW);
			//_combo.LockWindowUpdate(FALSE);
		} else {
			_combo.ShowWindow(SW_HIDE);
		}
		return 0;
	}
	LRESULT OnItemScrolled(LPNMHDR pnmh) {
		int selected = GetSelectedIndex();
		if (selected != -1 && _combo.IsWindow() && _combo.IsWindowVisible()) {
			CRect rect;
			GetSubItemRect(selected, 1, LVIR_BOUNDS, &rect);
			_combo.MoveWindow(rect);
		} else {
			_combo.ShowWindow(SW_HIDE);
		}
		//Invalidate();
		return 0;
	}
	LRESULT OnOptionChanged(UINT uNotifyCode, int nID, HWND hCtrl) {
		int selected = GetSelectedIndex();
		if (selected != -1 && _combo.IsWindow() && _combo.IsWindowVisible()) {
			auto& item = _data[selected];
			item.selection = _combo.GetCurSel();
			SetItemText(selected, 1, item.options[item.selection].c_str());
		}
		return 0;
	}
	LRESULT OnKeyDown(LPNMHDR pnmh) {
		LPNMLVKEYDOWN pKey = (LPNMLVKEYDOWN)pnmh;
		if (pKey->wVKey == VK_DOWN && (pKey->flags & KF_ALTDOWN)) {
			_combo.SetFocus();
			_combo.ShowDropDown(TRUE);
		}
		return 0;
	}

	//---custom draw
	DWORD OnPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd) {
		if (lpnmcd->hdr.hwndFrom == m_hWnd)return CDRF_NOTIFYITEMDRAW;
		return CDRF_DODEFAULT;
	}
	DWORD OnItemPrePaint(int nID, LPNMCUSTOMDRAW lpnmcd) {
		if (lpnmcd->hdr.hwndFrom == m_hWnd) {
			//change color to indicate separator
			LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lpnmcd;
			if (_data[lpnmcd->dwItemSpec].options.empty()) {
				lpnmlv->clrText = RGB(255, 255, 255);
				lpnmlv->clrTextBk = RGB(64, 64, 64);
				return CDRF_NOTIFYITEMDRAW;
			}else{
				return CDRF_DODEFAULT;
			}
		}
		return CDRF_DODEFAULT;
	}
};
