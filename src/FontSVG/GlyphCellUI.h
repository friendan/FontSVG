#pragma once

/// 字形预览格：用 GDI HFONT 绘制，避免 DWrite 按族名回退导致预览不是所选字体
class CGlyphCellUI : public CLabelUI
{
public:
	LPCTSTR GetClass() const override { return _T("GlyphCellUI"); }
	LPVOID GetInterface(LPCTSTR pstrName) override;
	void PaintText(IRenderContext& ctx) override;
};
