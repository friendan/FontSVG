#pragma once

#include <string>

/// 导出文字图片（可设尺寸与 PNG/JPG/BMP）
class CImageExportWnd : public WindowImplBase
{
public:
	CImageExportWnd(const std::wstring& faceName, wchar_t ch, std::string svgUtf8);
	~CImageExportWnd();

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
	void SyncFormatHint();
	void SetSizeEdits(int w, int h);
	bool ReadExportSize(int& w, int& h) const;
	bool EnsureExportDir() const;
	bool BrowseExportDir();
	bool DoExport();
	CDuiString GetFormatExt() const;
	DWORD ThemeToken(LPCTSTR pstrName, DWORD dwFallback) const;

	static CDuiString GetDefaultExportDir();
	static CDuiString& SharedExportDir();

private:
	std::wstring m_faceName;
	wchar_t m_ch;
	std::string m_svgUtf8;
	CSvgBoxUI* m_pPreview;
	CSegmentedUI* m_pFormat;
	CColorPaletteUI* m_pPalette;
	CLabelUI* m_pColorLabel;
	CLabelUI* m_pDirLabel;
	CEditUI* m_pEditW;
	CEditUI* m_pEditH;
	CHorizontalLayoutUI* m_pColorRow;
	DWORD m_dwTint;
};
