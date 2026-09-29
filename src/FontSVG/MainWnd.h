#pragma once

class CMainWnd : public WindowImplBase, public IThemeNotifyUI
{
public:
	CMainWnd();
	~CMainWnd();

	CDuiString GetSkinFile() override;
	CDuiString GetSkinFolder();
	LPCTSTR GetWindowClassName() const override;
	void InitWindow() override;
	void OnFinalMessage(HWND hWnd) override;
	void Notify(TNotifyUI& msg) override;
	LRESULT HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) override;
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) override;
	void OnThemeChanged(LPCTSTR oldId, LPCTSTR newId, bool bPreview) override;

	DUI_DECLARE_MESSAGE_MAP()
	void OnClick(TNotifyUI& msg) override;
	void OnItemSelect(TNotifyUI& msg);
	void OnTextChanged(TNotifyUI& msg);

private:
	void SyncFontListThemeColors();
	void RefreshFontList(int selectIndex = -1);
	bool IsBundledFontCategory() const;
	void OnFontSelected();
	void ReloadGlyphsForCurrentFont();
	void ApplyCharFilter();
	void RefreshCharPage();
	void ScheduleCharPageRefresh();
	void UpdatePageControl();
	void SyncPageWindow();
	void GotoPageIndex(int pageIndex);
	void OnPageGo();
	void UpdateSelectedFontLabel();
	void UpdateStatusBar();
	void SetStatusMessage(LPCTSTR text);
	void OnOpenFontFile();
	void EnsureRootLayout();
	void ShowFontContextMenu();
	void OnCopySelectedFontName();
	void ShowCharContextMenu(CControlUI* pSender);
	void OnCopySelectedChar();
	void OnEditSelectedCharOutline();
	void OnExportSelectedChar();
	void OnExportSelectedCharImage();
	void OnExportSelectedCharAvatar();
	void OnExportSelectedCharSvg();
	void ShowCharInStatus(wchar_t ch);
	void AdjustGlyphZoom(int delta);
	void UpdateGlyphZoomButtons();
	int GetGlyphCellSize() const;
	static CDuiString FormatCharInfo(wchar_t ch);
	bool IsFontListNotify(CControlUI* pSender) const;
	bool IsGlyphCell(CControlUI* pSender) const;
	int GetSelectedFontIndex() const;
	int GetPageSize() const;
	int GetPageCount() const;
	bool IsCharGridReady() const;

	static const int kPageSlotCount = 5;
	static const int kGlyphFontDefault = 36;
	static const int kGlyphFontMin = 1;
	static const int kGlyphFontStep = 1;

	CListUI* m_pFontList;
	CSegmentedUI* m_pFontCat;
	CVerticalLayoutUI* m_pCharGrid;
	CLabelUI* m_pStatusMessage;
	CLabelUI* m_pSelectedFont;
	CLabelUI* m_pPageInfo;
	CButtonUI* m_pBtnPagePrev;
	CButtonUI* m_pBtnPageNext;
	CButtonUI* m_pBtnPageGo;
	CButtonUI* m_pBtnZoomIn;
	CButtonUI* m_pBtnZoomOut;
	CButtonUI* m_pBtnPageSlots[kPageSlotCount];
	CEditUI* m_pEditPageGoto;

	std::wstring m_currentFace;
	std::vector<wchar_t> m_allChars;
	std::vector<wchar_t> m_filteredChars;
	wchar_t m_menuChar;
	bool m_hasMenuChar;
	int m_glyphFontSize;
	int m_pageIndex;
	int m_pageWindowStart;
	int m_lastPageSize;
	int m_lastGridHeight;
	int m_refreshRetry;
};
