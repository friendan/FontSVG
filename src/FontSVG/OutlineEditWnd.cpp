#include "StdAfx.h"
#include "OutlineEditWnd.h"
#include "OutlineCanvasUI.h"
#include "GlyphSvg.h"
#include "Core/UITheme.h"

namespace {

DWORD OutlineThemeColor(LPCTSTR token, DWORD fallback)
{
	CThemeManager* tm = CThemeManager::GetInstance();
	if( tm == NULL ) return fallback;
	return tm->GetColor(token, fallback);
}

} // namespace

DUI_BEGIN_MESSAGE_MAP(COutlineEditWnd, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_CLICK, COutlineEditWnd::OnClick)
DUI_END_MESSAGE_MAP()

COutlineEditWnd::COutlineEditWnd(const std::wstring& faceName, wchar_t ch, GlyphPath path)
	: m_faceName(faceName)
	, m_ch(ch)
	, m_basePath(path)
	, m_currentPath(path)
	, m_pCanvas(NULL)
	, m_pCandHost(NULL)
	, m_pIntensity(NULL)
{
}

COutlineEditWnd::~COutlineEditWnd()
{
}

void COutlineEditWnd::Open(HWND hOwner, const std::wstring& faceName, wchar_t ch)
{
	if( faceName.empty() || ch == 0 ) {
		CMessageBox::ShowInfo(hOwner, _T("提示"), _T("请先选择字体与文字"));
		return;
	}

	std::string svg;
	if( !GlyphSvg::BuildUtf8(faceName, ch, svg) ) {
		CMessageBox::ShowInfo(hOwner, _T("提示"), _T("无法提取字形轮廓（可能不是 TrueType/OpenType）"));
		return;
	}

	GlyphPath path;
	if( !path.ParseFromSvg(svg) ) {
		CMessageBox::ShowInfo(hOwner, _T("提示"), _T("轮廓路径解析失败"));
		return;
	}

	COutlineEditWnd* pWnd = new COutlineEditWnd(faceName, ch, path);
	pWnd->Create(hOwner, _T("轮廓修改"), UI_WNDSTYLE_FRAME, WS_EX_WINDOWEDGE, 0, 0, 1000, 720);
	if( pWnd->GetHWND() == NULL ) {
		delete pWnd;
		return;
	}
	pWnd->CenterWindow();
	pWnd->ShowModal();
}

void COutlineEditWnd::OnFinalMessage(HWND hWnd)
{
	WindowImplBase::OnFinalMessage(hWnd);
	delete this;
}

CDuiString COutlineEditWnd::GetSkinFile()
{
	return _T("outlineedit.html");
}

LPCTSTR COutlineEditWnd::GetWindowClassName() const
{
	return _T("FontSVGOutlineEditWnd");
}

CControlUI* COutlineEditWnd::CreateControl(LPCTSTR pstrClass)
{
	if( pstrClass != NULL && _tcsicmp(pstrClass, _T("OutlineCanvas")) == 0 )
		return new COutlineCanvasUI;
	return NULL;
}

DWORD COutlineEditWnd::ThemeTextColor() const
{
	return OutlineThemeColor(_T("color-text"), 0x000000E0);
}

void COutlineEditWnd::SyncTitle()
{
	CTitleBarUI* pBar = static_cast<CTitleBarUI*>(m_pm.FindControl(_T("titlebar")));
	if( pBar == NULL ) return;
	CDuiString s;
	s.Format(_T("轮廓修改 — %s / %c (U+%04X)"),
		m_faceName.c_str(), m_ch, (unsigned)(unsigned short)m_ch);
	pBar->SetTitle(s.GetData());
}

void COutlineEditWnd::ApplyPathToCanvas(const GlyphPath& path)
{
	m_currentPath = path;
	if( m_pCanvas ) {
		m_pCanvas->SetFillColor(ThemeTextColor());
		m_pCanvas->SetPath(m_currentPath);
	}
}

void COutlineEditWnd::ClearCandidates()
{
	m_candidates.clear();
	if( m_pCandHost )
		m_pCandHost->RemoveAll();
}

void COutlineEditWnd::OnReset()
{
	ApplyPathToCanvas(m_basePath);
}

double COutlineEditWnd::IntensityScale() const
{
	if( m_pIntensity == NULL ) return 1.0;
	LPCTSTR v = m_pIntensity->GetSelectedValue();
	if( v == NULL ) return 1.0;
	if( _tcscmp(v, _T("mild")) == 0 ) return 0.45;
	if( _tcscmp(v, _T("wild")) == 0 ) return 2.4;
	return 1.0; // medium
}

void COutlineEditWnd::OnRandomCandidates()
{
	if( m_pCandHost == NULL ) return;
	ClearCandidates();
	m_candidates.reserve(100);

	const DWORD tint = ThemeTextColor();
	const int cell = 52;
	const int gap = 6;
	const int cols = 4;
	const double intensity = IntensityScale();

	CHorizontalLayoutUI* pRow = NULL;
	int col = 0;
	unsigned seed = (unsigned)::GetTickCount() ^ ((unsigned)m_ch << 8);

	for( int i = 0; i < 100; ++i ) {
		const double amount = intensity * (i + 1) / 100.0;
		GlyphPath var = m_basePath.MakeRandomVariant(seed + (unsigned)i * 97u, amount);
		m_candidates.push_back(var);

		if( pRow == NULL || col >= cols ) {
			pRow = new CHorizontalLayoutUI;
			pRow->SetFixedHeight(cell);
			pRow->SetAttribute(_T("gap"), _T("6"));
			m_pCandHost->Add(pRow);
			col = 0;
		}

		CSvgBoxUI* pBox = new CSvgBoxUI;
		pBox->SetName(_T("cand_item"));
		pBox->SetTag((UINT_PTR)i);
		pBox->SetFixedWidth(cell);
		pBox->SetFixedHeight(cell);
		pBox->SetAttribute(_T("border"), _T("1px solid"));
		pBox->SetAttribute(_T("border-color"), _T("var(--color-border)"));
		pBox->SetAttribute(_T("border-radius"), _T("4"));
		pBox->SetAttribute(_T("padding"), _T("4,4,4,4"));
		pBox->SetAttribute(_T("background-color-hover"), _T("var(--color-bg-hover-medium)"));
		const std::string svg = var.ToSvgUtf8("currentColor");
		pBox->LoadFromUtf8Data(svg.c_str());
		pBox->SetColor(tint);
		pRow->Add(pBox);
		++col;
	}

	(void)gap;
	m_pCandHost->NeedUpdate();
}

void COutlineEditWnd::OnPickCandidate(int index)
{
	if( index < 0 || index >= (int)m_candidates.size() ) return;
	ApplyPathToCanvas(m_candidates[(size_t)index]);
}

void COutlineEditWnd::InitWindow()
{
	m_pCanvas = static_cast<COutlineCanvasUI*>(m_pm.FindControl(_T("canvas")));
	m_pCandHost = static_cast<CVerticalLayoutUI*>(m_pm.FindControl(_T("cand_host")));
	m_pIntensity = static_cast<CSegmentedUI*>(m_pm.FindControl(_T("seg_intensity")));
	SyncTitle();
	ApplyPathToCanvas(m_basePath);
}

void COutlineEditWnd::Notify(TNotifyUI& msg)
{
	if( msg.sType == DUI_MSGTYPE_VALUECHANGED && msg.pSender == m_pCanvas && m_pCanvas ) {
		m_currentPath = m_pCanvas->GetPath();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_SELECTCHANGED && msg.pSender
		&& msg.pSender->GetName() == _T("sw_points") && m_pCanvas ) {
		COptionUI* pSw = static_cast<COptionUI*>(msg.pSender->GetInterface(DUI_CTR_OPTION));
		if( pSw )
			m_pCanvas->SetShowEditPoints(pSw->IsSelected());
		return;
	}
	WindowImplBase::Notify(msg);
}

void COutlineEditWnd::OnClick(TNotifyUI& msg)
{
	CDuiString name = msg.pSender->GetName();
	if( name == _T("btn_reset") ) {
		OnReset();
		return;
	}
	if( name == _T("btn_random") ) {
		OnRandomCandidates();
		return;
	}
	if( name == _T("cand_item") ) {
		OnPickCandidate((int)msg.pSender->GetTag());
		return;
	}
	WindowImplBase::OnClick(msg);
}
