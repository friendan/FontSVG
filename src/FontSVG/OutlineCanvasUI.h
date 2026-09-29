#pragma once

#include "GlyphPath.h"

/// 字形轮廓编辑画布：填充预览 + 可拖拽锚点/控制点
class COutlineCanvasUI : public CControlUI
{
public:
	COutlineCanvasUI();

	LPCTSTR GetClass() const override { return _T("OutlineCanvasUI"); }
	LPVOID GetInterface(LPCTSTR pstrName) override;
	UINT GetControlFlags() const override { return UIFLAG_SETCURSOR; }

	void SetPath(const GlyphPath& path);
	const GlyphPath& GetPath() const { return m_path; }
	void SetFillColor(DWORD color) { m_dwFill = color; Invalidate(); }
	void SetShowEditPoints(bool show);
	bool GetShowEditPoints() const { return m_showEditPoints; }

	void DoEvent(TEventUI& event) override;
	bool DoPaint(IRenderContext& ctx, const RECT& rcPaint, CControlUI* pStopControl) override;

private:
	void RebuildEditPoints();
	void UpdateViewBox();
	bool PathToClient(double x, double y, POINT& out) const;
	bool ClientToPath(int x, int y, double& outX, double& outY) const;
	int HitTestEditPoint(POINT pt) const;
	void PaintPath(HDC hdc, const RECT& rc);

	GlyphPath m_path;
	std::vector<GlyphPath::EditPoint> m_editPts;
	double m_viewOx = 0;
	double m_viewOy = 0;
	double m_viewBox = 1;
	int m_dragIndex = -1;
	bool m_showEditPoints = true;
	DWORD m_dwFill = 0x000000E0;
};
