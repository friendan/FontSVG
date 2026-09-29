#include "StdAfx.h"
#include "OutlineCanvasUI.h"

namespace {
	const int kHitRadius = 8;
	const int kPadPx = 16;
}

COutlineCanvasUI::COutlineCanvasUI()
{
	SetBackgroundColor(0x00000000);
}

LPVOID COutlineCanvasUI::GetInterface(LPCTSTR pstrName)
{
	if( _tcsicmp(pstrName, _T("OutlineCanvas")) == 0
		|| _tcsicmp(pstrName, _T("OutlineCanvasUI")) == 0 )
		return static_cast<COutlineCanvasUI*>(this);
	return CControlUI::GetInterface(pstrName);
}

void COutlineCanvasUI::SetPath(const GlyphPath& path)
{
	m_path = path;
	m_dragIndex = -1;
	RebuildEditPoints();
	UpdateViewBox();
	Invalidate();
}

void COutlineCanvasUI::SetShowEditPoints(bool show)
{
	if( m_showEditPoints == show ) return;
	m_showEditPoints = show;
	if( !m_showEditPoints )
		m_dragIndex = -1;
	Invalidate();
}

void COutlineCanvasUI::RebuildEditPoints()
{
	m_path.CollectEditPoints(m_editPts);
}

void COutlineCanvasUI::UpdateViewBox()
{
	double minX, minY, maxX, maxY;
	m_path.GetBounds(minX, minY, maxX, maxY);
	const double gw = maxX - minX;
	const double gh = maxY - minY;
	const double pad = ((gw > gh) ? gw : gh) * 0.08;
	m_viewBox = ((gw > gh) ? gw : gh) + pad * 2.0;
	if( m_viewBox < 1.0 ) m_viewBox = 1.0;
	m_viewOx = minX - (m_viewBox - gw) * 0.5;
	m_viewOy = minY - (m_viewBox - gh) * 0.5;
}

bool COutlineCanvasUI::PathToClient(double x, double y, POINT& out) const
{
	const RECT& rc = m_rcItem;
	const int w = rc.right - rc.left - kPadPx * 2;
	const int h = rc.bottom - rc.top - kPadPx * 2;
	if( w <= 0 || h <= 0 ) return false;
	const double side = (w < h) ? w : h;
	const double ox = rc.left + kPadPx + (w - side) * 0.5;
	const double oy = rc.top + kPadPx + (h - side) * 0.5;
	out.x = (LONG)(ox + (x - m_viewOx) / m_viewBox * side + 0.5);
	out.y = (LONG)(oy + (y - m_viewOy) / m_viewBox * side + 0.5);
	return true;
}

bool COutlineCanvasUI::ClientToPath(int x, int y, double& outX, double& outY) const
{
	const RECT& rc = m_rcItem;
	const int w = rc.right - rc.left - kPadPx * 2;
	const int h = rc.bottom - rc.top - kPadPx * 2;
	if( w <= 0 || h <= 0 || m_viewBox <= 0 ) return false;
	const double side = (w < h) ? w : h;
	const double ox = rc.left + kPadPx + (w - side) * 0.5;
	const double oy = rc.top + kPadPx + (h - side) * 0.5;
	outX = m_viewOx + (x - ox) / side * m_viewBox;
	outY = m_viewOy + (y - oy) / side * m_viewBox;
	return true;
}

int COutlineCanvasUI::HitTestEditPoint(POINT pt) const
{
	if( !m_showEditPoints ) return -1;
	int best = -1;
	int bestDist2 = kHitRadius * kHitRadius;
	for( size_t i = 0; i < m_editPts.size(); ++i ) {
		POINT c = {};
		if( !PathToClient(m_editPts[i].x, m_editPts[i].y, c) ) continue;
		const int dx = (int)(c.x - pt.x);
		const int dy = (int)(c.y - pt.y);
		const int d2 = dx * dx + dy * dy;
		if( d2 <= bestDist2 ) {
			bestDist2 = d2;
			best = (int)i;
		}
	}
	return best;
}

void COutlineCanvasUI::DoEvent(TEventUI& event)
{
	if( !IsMouseEnabled() || !IsEnabled() ) {
		CControlUI::DoEvent(event);
		return;
	}

	if( event.Type == UIEVENT_BUTTONDOWN || event.Type == UIEVENT_DBLCLICK ) {
		if( ::PtInRect(&m_rcItem, event.ptMouse) ) {
			m_dragIndex = HitTestEditPoint(event.ptMouse);
			if( m_dragIndex >= 0 )
				Invalidate();
		}
		return;
	}
	if( event.Type == UIEVENT_BUTTONUP ) {
		m_dragIndex = -1;
		return;
	}
	if( event.Type == UIEVENT_MOUSEMOVE ) {
		if( m_dragIndex >= 0 && (event.wParam & MK_LBUTTON) ) {
			double px = 0, py = 0;
			if( ClientToPath(event.ptMouse.x, event.ptMouse.y, px, py) ) {
				if( m_path.SetEditPoint((size_t)m_dragIndex, px, py) ) {
					RebuildEditPoints();
					Invalidate();
					if( m_pManager )
						m_pManager->SendNotify(this, DUI_MSGTYPE_VALUECHANGED);
				}
			}
		}
		return;
	}
	if( event.Type == UIEVENT_SETCURSOR ) {
		const int hit = (m_dragIndex >= 0) ? m_dragIndex : HitTestEditPoint(event.ptMouse);
		::SetCursor(::LoadCursor(NULL, hit >= 0 ? IDC_HAND : IDC_ARROW));
		return;
	}

	CControlUI::DoEvent(event);
}

void COutlineCanvasUI::PaintPath(HDC hdc, const RECT& rc)
{
	if( m_path.Empty() || hdc == NULL ) return;

	Gdiplus::Graphics g(hdc);
	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetClip(Gdiplus::Rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top));

	Gdiplus::GraphicsPath gp;
	double cx = 0, cy = 0;
	bool hasSub = false;
	const std::vector<GlyphPath::Cmd>& cmds = m_path.Cmds();
	for( size_t i = 0; i < cmds.size(); ++i ) {
		const GlyphPath::Cmd& c = cmds[i];
		POINT pt0 = {}, pt1 = {}, pt2 = {};
		switch( c.type ) {
		case GlyphPath::MoveTo:
			if( hasSub ) gp.CloseFigure();
			PathToClient(c.x[0], c.y[0], pt0);
			gp.StartFigure();
			cx = c.x[0]; cy = c.y[0];
			hasSub = true;
			break;
		case GlyphPath::LineTo:
			PathToClient(cx, cy, pt0);
			PathToClient(c.x[0], c.y[0], pt1);
			gp.AddLine((INT)pt0.x, (INT)pt0.y, (INT)pt1.x, (INT)pt1.y);
			cx = c.x[0]; cy = c.y[0];
			break;
		case GlyphPath::QuadTo: {
			// 二次转三次：CP1 = P0 + 2/3 (Q-P0), CP2 = P2 + 2/3 (Q-P2)
			PathToClient(cx, cy, pt0);
			PathToClient(c.x[0], c.y[0], pt1);
			PathToClient(c.x[1], c.y[1], pt2);
			const double x0 = pt0.x, y0 = pt0.y;
			const double qx = pt1.x, qy = pt1.y;
			const double x3 = pt2.x, y3 = pt2.y;
			const double x1 = x0 + (qx - x0) * 2.0 / 3.0;
			const double y1 = y0 + (qy - y0) * 2.0 / 3.0;
			const double x2 = x3 + (qx - x3) * 2.0 / 3.0;
			const double y2 = y3 + (qy - y3) * 2.0 / 3.0;
			Gdiplus::PointF pts[4] = {
				Gdiplus::PointF((Gdiplus::REAL)x0, (Gdiplus::REAL)y0),
				Gdiplus::PointF((Gdiplus::REAL)x1, (Gdiplus::REAL)y1),
				Gdiplus::PointF((Gdiplus::REAL)x2, (Gdiplus::REAL)y2),
				Gdiplus::PointF((Gdiplus::REAL)x3, (Gdiplus::REAL)y3),
			};
			gp.AddBeziers(pts, 4);
			cx = c.x[1]; cy = c.y[1];
			break;
		}
		case GlyphPath::CubicTo: {
			PathToClient(cx, cy, pt0);
			POINT a = {}, b = {}, e = {};
			PathToClient(c.x[0], c.y[0], a);
			PathToClient(c.x[1], c.y[1], b);
			PathToClient(c.x[2], c.y[2], e);
			Gdiplus::PointF pts[4] = {
				Gdiplus::PointF((Gdiplus::REAL)pt0.x, (Gdiplus::REAL)pt0.y),
				Gdiplus::PointF((Gdiplus::REAL)a.x, (Gdiplus::REAL)a.y),
				Gdiplus::PointF((Gdiplus::REAL)b.x, (Gdiplus::REAL)b.y),
				Gdiplus::PointF((Gdiplus::REAL)e.x, (Gdiplus::REAL)e.y),
			};
			gp.AddBeziers(pts, 4);
			cx = c.x[2]; cy = c.y[2];
			break;
		}
		case GlyphPath::Close:
			gp.CloseFigure();
			hasSub = false;
			break;
		}
	}

	Gdiplus::SolidBrush brush(
		Gdiplus::Color(DuiColorA(m_dwFill), DuiColorR(m_dwFill), DuiColorG(m_dwFill), DuiColorB(m_dwFill)));
	g.FillPath(&brush, &gp);

	if( !m_showEditPoints ) return;

	Gdiplus::Pen penLine(Gdiplus::Color(120, 80, 140, 220), 1.0f);
	// 控制柄连线
	cx = cy = 0;
	for( size_t i = 0; i < cmds.size(); ++i ) {
		const GlyphPath::Cmd& c = cmds[i];
		POINT p0 = {}, p1 = {}, p2 = {}, p3 = {};
		if( c.type == GlyphPath::MoveTo ) {
			cx = c.x[0]; cy = c.y[0];
		} else if( c.type == GlyphPath::LineTo ) {
			cx = c.x[0]; cy = c.y[0];
		} else if( c.type == GlyphPath::QuadTo ) {
			PathToClient(cx, cy, p0);
			PathToClient(c.x[0], c.y[0], p1);
			PathToClient(c.x[1], c.y[1], p2);
			g.DrawLine(&penLine, (INT)p0.x, (INT)p0.y, (INT)p1.x, (INT)p1.y);
			g.DrawLine(&penLine, (INT)p1.x, (INT)p1.y, (INT)p2.x, (INT)p2.y);
			cx = c.x[1]; cy = c.y[1];
		} else if( c.type == GlyphPath::CubicTo ) {
			PathToClient(cx, cy, p0);
			PathToClient(c.x[0], c.y[0], p1);
			PathToClient(c.x[1], c.y[1], p2);
			PathToClient(c.x[2], c.y[2], p3);
			g.DrawLine(&penLine, (INT)p0.x, (INT)p0.y, (INT)p1.x, (INT)p1.y);
			g.DrawLine(&penLine, (INT)p2.x, (INT)p2.y, (INT)p3.x, (INT)p3.y);
			cx = c.x[2]; cy = c.y[2];
		}
	}

	for( size_t i = 0; i < m_editPts.size(); ++i ) {
		POINT c = {};
		if( !PathToClient(m_editPts[i].x, m_editPts[i].y, c) ) continue;
		const bool hot = ((int)i == m_dragIndex);
		const int r = hot ? 6 : 5;
		Gdiplus::Color fill = m_editPts[i].isControl
			? Gdiplus::Color(230, 30, 136, 229)
			: Gdiplus::Color(230, 250, 82, 82);
		if( hot ) fill = Gdiplus::Color(255, 255, 193, 7);
		Gdiplus::SolidBrush b(fill);
		g.FillEllipse(&b, c.x - r, c.y - r, r * 2, r * 2);
		Gdiplus::Pen ring(Gdiplus::Color(255, 255, 255, 255), 1.0f);
		g.DrawEllipse(&ring, c.x - r, c.y - r, r * 2, r * 2);
	}
}

bool COutlineCanvasUI::DoPaint(IRenderContext& ctx, const RECT& rcPaint, CControlUI* /*pStopControl*/)
{
	RECT rc = m_rcItem;
	if( !::IntersectRect(&rc, &rc, &rcPaint) ) return true;

	DWORD bk = GetBackgroundColor();
	if( bk == 0 ) {
		CThemeManager* tm = CThemeManager::GetInstance();
		bk = tm ? tm->GetColor(_T("color-control-bg"), 0xFFFFFFFF) : 0xFFFFFFFF;
	}
	ctx.DrawColor(rc, GetAdjustColor(bk));

	ctx.ReleaseNativeDC();
	HDC hdc = ctx.GetGdiPaintDC();
	if( hdc != NULL ) {
		int nSave = ::SaveDC(hdc);
		::IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
		PaintPath(hdc, m_rcItem);
		::RestoreDC(hdc, nSave);
	}
	return true;
}
