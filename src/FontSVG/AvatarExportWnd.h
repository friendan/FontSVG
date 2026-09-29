#pragma once

#include <string>
#include "GlyphSvg.h"

/// 导出文字头像（正方形、圆形/圆角底、字色+底色）
class CAvatarExportWnd : public WindowImplBase
{
public:
	CAvatarExportWnd(const std::wstring& faceName, wchar_t ch);
	~CAvatarExportWnd();

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
	void BuildFgSwatches();
	void BuildBgSwatches();
	void ApplyFg(DWORD dwColor);
	void ApplyBg(DWORD dwColor);
	void RebuildPreview();
	void SyncSourceLabels();
	void SyncFormatHint();
	void SetSizeEdit(int size);
	bool ReadExportSize(int& size) const;
	bool DoExport();
	CDuiString GetFormatExt() const;
	GlyphSvg::AvatarShape GetShape() const;
	DWORD ThemeToken(LPCTSTR pstrName, DWORD dwFallback) const;

private:
	std::wstring m_faceName;
	wchar_t m_ch;
	std::string m_svgUtf8;
	CSvgBoxUI* m_pPreview;
	CSegmentedUI* m_pFormat;
	CSegmentedUI* m_pShape;
	CColorPaletteUI* m_pPaletteFg;
	CColorPaletteUI* m_pPaletteBg;
	CLabelUI* m_pFgLabel;
	CLabelUI* m_pBgLabel;
	CEditUI* m_pEditSize;
	CHorizontalLayoutUI* m_pFgRow;
	CHorizontalLayoutUI* m_pBgRow;
	DWORD m_dwFg;
	DWORD m_dwBg;
	bool m_bEditBg;
};
