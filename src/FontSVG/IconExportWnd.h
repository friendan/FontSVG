#pragma once

#include <string>

/// 字体字形导出为 Windows ICO（尺寸写死为系统常用七档）
class CIconExportWnd : public WindowImplBase
{
public:
	CIconExportWnd(const std::wstring& faceName, wchar_t ch, std::string svgUtf8);
	~CIconExportWnd();

	static void Open(HWND hOwner, const std::wstring& faceName, wchar_t ch);

	void OnFinalMessage(HWND hWnd) override;
	CDuiString GetSkinFile() override;
	LPCTSTR GetWindowClassName() const override;
	void InitWindow() override;
	void Notify(TNotifyUI& msg) override;

	DUI_DECLARE_MESSAGE_MAP()
	void OnClick(TNotifyUI& msg) override;

	LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, bool& /*bHandled*/) override
	{
		if( uMsg == WM_KEYDOWN && wParam == VK_ESCAPE ) {
			Close(0);
			return TRUE;
		}
		return FALSE;
	}

private:
	void BuildColorSwatches();
	void ApplyTint(DWORD dwColor);
	void SyncPreview();
	void SyncPreviewPlate();
	void SyncDirLabel();
	void SyncSourceLabels();
	bool EnsureExportDir() const;
	bool BrowseExportDir();
	bool DoExport();
	DWORD ThemeToken(LPCTSTR pstrName, DWORD dwFallback) const;

	static CDuiString GetDefaultExportDir();
	static CDuiString& SharedExportDir();

private:
	std::wstring m_faceName;
	wchar_t m_ch;
	std::string m_svgUtf8;
	CSvgBoxUI* m_pPreview;
	CColorPaletteUI* m_pPalette;
	CLabelUI* m_pColorLabel;
	CLabelUI* m_pDirLabel;
	CHorizontalLayoutUI* m_pColorRow;
	DWORD m_dwTint;
};
