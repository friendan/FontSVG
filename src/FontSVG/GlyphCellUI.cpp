#include "StdAfx.h"
#include "duilib.h"

LPVOID CGlyphCellUI::GetInterface(LPCTSTR pstrName)
{
	if( _tcsicmp(pstrName, _T("GlyphCellUI")) == 0 ) return static_cast<CGlyphCellUI*>(this);
	return CLabelUI::GetInterface(pstrName);
}

void CGlyphCellUI::PaintText(IRenderContext& ctx)
{
	if( m_pManager == NULL ) return;

	CDuiString sText = GetText();
	if( sText.IsEmpty() ) return;

	if( m_dwColor == 0 ) m_dwColor = m_pManager->GetDefaultFontColor();
	if( m_dwDisabledColor == 0 ) m_dwDisabledColor = m_pManager->GetDefaultDisabledColor();

	RECT rc = m_rcItem;
	RECT rcPadding = GetPadding();
	RECT rcTextPadding = GetTextPadding();
	rc.left += rcPadding.left + rcTextPadding.left;
	rc.right -= rcPadding.right + rcTextPadding.right;
	rc.top += rcPadding.top + rcTextPadding.top;
	rc.bottom -= rcPadding.bottom + rcTextPadding.bottom;

	DWORD clrColor = IsEnabled() ? m_dwColor : m_dwDisabledColor;
	if( IsEnabled() ) {
		if( (m_uControlState & UISTATE_PUSHED) != 0 && m_dwActiveColor != 0 )
			clrColor = m_dwActiveColor;
		else if( (m_uControlState & UISTATE_HOT) != 0 && m_dwHoverColor != 0 )
			clrColor = m_dwHoverColor;
		else if( IsFocused() && m_dwFocusedColor != 0 )
			clrColor = m_dwFocusedColor;
	}
	clrColor = GetAdjustColor(clrColor);

	// D2D 的 CreateTextFormat 常按族名找不到中文字体而静默回退；
	// GDI CreateFontIndirect 与枚举/导出一致，预览必须走这条路径。
	ctx.ReleaseNativeDC();
	HDC hdc = ctx.GetGdiPaintDC();
	if( hdc == NULL ) {
		CLabelUI::PaintText(ctx);
		return;
	}

	int nSave = ::SaveDC(hdc);
	::IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
	CRenderEngine::DrawText(hdc, m_pManager, rc, sText.GetData(), clrColor, m_iFont, m_uTextStyle | DT_NOPREFIX);
	::RestoreDC(hdc, nSave);
}
