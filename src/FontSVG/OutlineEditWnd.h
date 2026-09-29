#pragma once

#include <string>
#include <vector>
#include "GlyphPath.h"

class COutlineCanvasUI;

/// 字形轮廓编辑：拖点修改 + 随机生成 100 个候选
class COutlineEditWnd : public WindowImplBase
{
public:
	COutlineEditWnd(const std::wstring& faceName, wchar_t ch, GlyphPath path);
	~COutlineEditWnd();

	static void Open(HWND hOwner, const std::wstring& faceName, wchar_t ch);

	void OnFinalMessage(HWND hWnd) override;
	CDuiString GetSkinFile() override;
	LPCTSTR GetWindowClassName() const override;
	CControlUI* CreateControl(LPCTSTR pstrClass) override;
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
	void SyncTitle();
	void ApplyPathToCanvas(const GlyphPath& path);
	void OnReset();
	void OnRandomCandidates();
	void OnPickCandidate(int index);
	void ClearCandidates();
	DWORD ThemeTextColor() const;
	double IntensityScale() const;

	std::wstring m_faceName;
	wchar_t m_ch;
	GlyphPath m_basePath;
	GlyphPath m_currentPath;
	std::vector<GlyphPath> m_candidates;

	COutlineCanvasUI* m_pCanvas;
	CVerticalLayoutUI* m_pCandHost;
	CSegmentedUI* m_pIntensity;
};
