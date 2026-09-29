#include "StdAfx.h"
#include "AvatarExportWnd.h"
#include "Core/UITheme.h"

namespace {

DWORD AvThemeToken(LPCTSTR pstrName, DWORD dwFallback)
{
	CThemeManager* pTm = CThemeManager::GetInstance();
	if( pTm == NULL ) return dwFallback;
	CTheme* pTh = pTm->GetCurrentTheme();
	if( pTh == NULL ) pTh = pTm->FindTheme(pTm->GetDefaultThemeId());
	if( pTh == NULL ) return dwFallback;
	return pTh->GetToken(pstrName, dwFallback);
}

int ClampAvatarSize(int v)
{
	if( v < 16 ) return 16;
	if( v > 2048 ) return 2048;
	return v;
}

void AddSwatch(CHorizontalLayoutUI* row, int index, LPCTSTR tip, DWORD color)
{
	if( row == NULL ) return;
	CFontIconUI* p = new CFontIconUI;
	p->SetSizePreset(28);
	p->SetShape(CFontIconUI::ShapeRounded);
	CDuiString sName;
	sName.Format(_T("swatch_%d"), index);
	p->SetName(sName.GetData());
	p->SetClickable(true);
	p->SetToolTip(tip);
	if( DuiColorA(color) == 0 ) {
		p->SetText(_T("无"));
		p->SetAttribute(_T("background-color"), _T("#00000000"));
		p->SetAttribute(_T("border"), _T("1px solid"));
		p->SetAttribute(_T("border-color"), _T("var(--color-border)"));
		p->SetAttribute(_T("color"), _T("var(--color-text-secondary)"));
	} else {
		CDuiString sBk;
		sBk.Format(_T("#%08X"), color);
		p->SetAttribute(_T("background-color"), sBk.GetData());
	}
	CDuiString sTag;
	sTag.Format(_T("%08X"), color);
	p->SetUserData(sTag.GetData());
	row->Add(p);
}

} // namespace

DUI_BEGIN_MESSAGE_MAP(CAvatarExportWnd, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_CLICK, CAvatarExportWnd::OnClick)
DUI_END_MESSAGE_MAP()

CAvatarExportWnd::CAvatarExportWnd(const std::wstring& faceName, wchar_t ch)
	: m_faceName(faceName)
	, m_ch(ch)
	, m_pPreview(NULL)
	, m_pFormat(NULL)
	, m_pShape(NULL)
	, m_pPaletteFg(NULL)
	, m_pPaletteBg(NULL)
	, m_pFgLabel(NULL)
	, m_pBgLabel(NULL)
	, m_pEditSize(NULL)
	, m_pFgRow(NULL)
	, m_pBgRow(NULL)
	, m_dwFg(0xFFFFFFFF)
	, m_dwBg(0x0D6EFDFF)
	, m_bEditBg(false)
{
	m_dwBg = AvThemeToken(_T("color-primary"), 0x0D6EFDFF);
	m_dwFg = 0xFFFFFFFF;
}

CAvatarExportWnd::~CAvatarExportWnd()
{
}

void CAvatarExportWnd::Open(HWND hOwner, const std::wstring& faceName, wchar_t ch)
{
	if( faceName.empty() || ch == 0 ) {
		CMessageBox::ShowInfo(hOwner, _T("提示"), _T("请先选择字体与文字"));
		return;
	}

	std::string probe;
	if( !GlyphSvg::BuildUtf8(faceName, ch, probe) ) {
		CMessageBox::ShowInfo(hOwner, _T("提示"), _T("无法提取字形轮廓（可能不是 TrueType/OpenType）"));
		return;
	}

	CAvatarExportWnd* pWnd = new CAvatarExportWnd(faceName, ch);
	pWnd->Create(hOwner, _T("导出头像"), UI_WNDSTYLE_FRAME, WS_EX_WINDOWEDGE, 0, 0, 800, 680);
	if( pWnd->GetHWND() == NULL ) {
		delete pWnd;
		return;
	}
	pWnd->CenterWindow();
	pWnd->ShowModal();
}

void CAvatarExportWnd::OnFinalMessage(HWND hWnd)
{
	WindowImplBase::OnFinalMessage(hWnd);
	delete this;
}

CDuiString CAvatarExportWnd::GetSkinFile()
{
	return _T("avatarexport.html");
}

LPCTSTR CAvatarExportWnd::GetWindowClassName() const
{
	return _T("FontSVGAvatarExportWnd");
}

DWORD CAvatarExportWnd::ThemeToken(LPCTSTR pstrName, DWORD dwFallback) const
{
	return AvThemeToken(pstrName, dwFallback);
}

void CAvatarExportWnd::BuildFgSwatches()
{
	if( m_pFgRow == NULL ) return;
	m_pFgRow->RemoveAll();
	AddSwatch(m_pFgRow, 100, _T("主色"), ThemeToken(_T("color-primary"), 0x0D6EFDFF));
	AddSwatch(m_pFgRow, 101, _T("黑"), 0x000000FF);
	AddSwatch(m_pFgRow, 102, _T("白"), 0xFFFFFFFF);
	AddSwatch(m_pFgRow, 103, _T("灰"), 0x6C757DFF);
	AddSwatch(m_pFgRow, 104, _T("红"), 0xDC3545FF);
	AddSwatch(m_pFgRow, 105, _T("绿"), 0x198754FF);
	AddSwatch(m_pFgRow, 106, _T("蓝"), 0x0D6EFDFF);
	AddSwatch(m_pFgRow, 107, _T("橙"), 0xFD7E14FF);
	AddSwatch(m_pFgRow, 108, _T("紫"), 0x722ED1FF);
}

void CAvatarExportWnd::BuildBgSwatches()
{
	if( m_pBgRow == NULL ) return;
	m_pBgRow->RemoveAll();
	AddSwatch(m_pBgRow, 200, _T("无（透明）"), 0x00000000);
	AddSwatch(m_pBgRow, 201, _T("主色"), ThemeToken(_T("color-primary"), 0x0D6EFDFF));
	AddSwatch(m_pBgRow, 202, _T("黑"), 0x000000FF);
	AddSwatch(m_pBgRow, 203, _T("白"), 0xFFFFFFFF);
	AddSwatch(m_pBgRow, 204, _T("灰"), 0x6C757DFF);
	AddSwatch(m_pBgRow, 205, _T("红"), 0xDC3545FF);
	AddSwatch(m_pBgRow, 206, _T("绿"), 0x198754FF);
	AddSwatch(m_pBgRow, 207, _T("蓝"), 0x0D6EFDFF);
	AddSwatch(m_pBgRow, 208, _T("橙"), 0xFD7E14FF);
	AddSwatch(m_pBgRow, 209, _T("紫"), 0x722ED1FF);
}

void CAvatarExportWnd::ApplyFg(DWORD dwColor)
{
	m_dwFg = dwColor;
	if( m_pFgLabel != NULL ) {
		CDuiString s;
		s.Format(_T("#%08X"), m_dwFg);
		m_pFgLabel->SetText(s.GetData());
	}
	if( m_pPaletteFg != NULL )
		m_pPaletteFg->SetSelectColor(m_dwFg);
	RebuildPreview();
}

void CAvatarExportWnd::ApplyBg(DWORD dwColor)
{
	m_dwBg = dwColor;
	if( m_pBgLabel != NULL ) {
		if( DuiColorA(m_dwBg) == 0 )
			m_pBgLabel->SetText(_T("透明"));
		else {
			CDuiString s;
			s.Format(_T("#%08X"), m_dwBg);
			m_pBgLabel->SetText(s.GetData());
		}
	}
	if( m_pPaletteBg != NULL && DuiColorA(m_dwBg) != 0 )
		m_pPaletteBg->SetSelectColor(m_dwBg);
	RebuildPreview();
}

GlyphSvg::AvatarShape CAvatarExportWnd::GetShape() const
{
	if( m_pShape == NULL ) return GlyphSvg::AvatarCircle;
	LPCTSTR v = m_pShape->GetSelectedValue();
	if( v != NULL && _tcsicmp(v, _T("rounded")) == 0 )
		return GlyphSvg::AvatarRounded;
	return GlyphSvg::AvatarCircle;
}

void CAvatarExportWnd::RebuildPreview()
{
	if( !GlyphSvg::BuildAvatarUtf8(m_faceName, m_ch, m_dwBg, m_dwFg, GetShape(), 0.18, m_svgUtf8) )
		return;
	if( m_pPreview == NULL ) return;
	m_pPreview->LoadFromUtf8Data(m_svgUtf8.c_str());
	m_pPreview->SetColor(0); // 颜色已写入 SVG，不再着色
	m_pPreview->Invalidate();
}

void CAvatarExportWnd::SyncSourceLabels()
{
	CTitleBarUI* pBar = static_cast<CTitleBarUI*>(m_pm.FindControl(_T("titlebar")));
	if( pBar != NULL ) {
		CDuiString s;
		s.Format(_T("导出头像 — %s"), GlyphSvg::MakeIconBaseName(m_ch).GetData());
		pBar->SetTitle(s.GetData());
	}
}

void CAvatarExportWnd::SyncFormatHint()
{
	CLabelUI* pHint = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_fmt_hint")));
	if( pHint == NULL ) return;
	CDuiString ext = GetFormatExt();
	if( ext == _T("jpg") )
		pHint->SetText(_T("JPG：无透明（底色仍会绘制）"));
	else if( ext == _T("bmp") )
		pHint->SetText(_T("BMP：位图导出"));
	else
		pHint->SetText(_T("PNG：推荐（透明边缘更干净）"));
}

void CAvatarExportWnd::SetSizeEdit(int size)
{
	size = ClampAvatarSize(size);
	CDuiString s;
	s.Format(_T("%d"), size);
	if( m_pEditSize != NULL )
		m_pEditSize->SetText(s.GetData());
}

bool CAvatarExportWnd::ReadExportSize(int& size) const
{
	size = 256;
	if( m_pEditSize != NULL )
		size = _ttoi(m_pEditSize->GetText().GetData());
	if( size <= 0 ) size = 256;
	size = ClampAvatarSize(size);
	return true;
}

CDuiString CAvatarExportWnd::GetFormatExt() const
{
	if( m_pFormat == NULL ) return _T("png");
	LPCTSTR v = m_pFormat->GetSelectedValue();
	if( v == NULL || *v == _T('\0') ) return _T("png");
	return v;
}

bool CAvatarExportWnd::DoExport()
{
	RebuildPreview();
	if( m_pPreview == NULL || m_svgUtf8.empty() ) return false;

	int size = 256;
	ReadExportSize(size);
	CDuiString ext = GetFormatExt();
	CDuiString sFile = GlyphSvg::MakeIconBaseName(m_ch);

	CDuiString sDir = GlyphSvg::GetDefaultExportDir();
	DWORD attr = ::GetFileAttributes(sDir.GetData());
	if( attr == INVALID_FILE_ATTRIBUTES )
		::CreateDirectory(sDir.GetData(), NULL);

	TCHAR szPath[MAX_PATH] = { 0 };
	_sntprintf_s(szPath, _TRUNCATE, _T("%s\\%s_avatar.%s"),
		sDir.GetData(), sFile.GetData(), ext.GetData());

	static TCHAR sFilterPng[] = _T("PNG 图片 (*.png)\0*.png\0所有文件 (*.*)\0*.*\0");
	static TCHAR sFilterJpg[] = _T("JPEG 图片 (*.jpg)\0*.jpg;*.jpeg\0所有文件 (*.*)\0*.*\0");
	static TCHAR sFilterBmp[] = _T("BMP 图片 (*.bmp)\0*.bmp\0所有文件 (*.*)\0*.*\0");
	LPCTSTR filter = sFilterPng;
	if( ext == _T("jpg") ) filter = sFilterJpg;
	else if( ext == _T("bmp") ) filter = sFilterBmp;

	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = szPath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = sDir.GetData();
	ofn.lpstrDefExt = ext.GetData();
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	if( !::GetSaveFileName(&ofn) )
		return false;

	// 颜色已烘焙进 SVG，导出时不再着色
	const bool ok = m_pPreview->ExportToFile(szPath, size, size, 0, 90);
	if( ok ) {
		const GlyphSvg::AvatarShape shape = GetShape();
		LPCTSTR shapeTip = (shape == GlyphSvg::AvatarRounded)
			? _T("圆角方形")
			: _T("圆形");
		CDuiString sTip;
		sTip.Format(_T("已导出：%s\n外形：%s\n画布：%d × %d"),
			szPath, shapeTip, size, size);
		CMessageBox::Show(m_hWnd, _T("导出成功"), sTip.GetData(),
			CModalOptions()
				.Kind(CONTROLKIND_INFO)
				.ShowCancel(false)
				.Width(560)
				.Height(260));
	}
	else {
		CMessageBox::ShowInfo(m_hWnd, _T("提示"), _T("导出失败"));
	}
	return ok;
}

void CAvatarExportWnd::InitWindow()
{
	m_pPreview = static_cast<CSvgBoxUI*>(m_pm.FindControl(_T("preview")));
	m_pFormat = static_cast<CSegmentedUI*>(m_pm.FindControl(_T("seg_format")));
	m_pShape = static_cast<CSegmentedUI*>(m_pm.FindControl(_T("seg_shape")));
	m_pPaletteFg = static_cast<CColorPaletteUI*>(m_pm.FindControl(_T("palette_fg")));
	m_pPaletteBg = static_cast<CColorPaletteUI*>(m_pm.FindControl(_T("palette_bg")));
	m_pFgLabel = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_fg")));
	m_pBgLabel = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_bg")));
	m_pEditSize = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_size")));
	m_pFgRow = static_cast<CHorizontalLayoutUI*>(m_pm.FindControl(_T("fg_row")));
	m_pBgRow = static_cast<CHorizontalLayoutUI*>(m_pm.FindControl(_T("bg_row")));

	SetSizeEdit(256);
	BuildFgSwatches();
	BuildBgSwatches();
	ApplyFg(m_dwFg);
	ApplyBg(m_dwBg);
	SyncFormatHint();
	SyncSourceLabels();
}

void CAvatarExportWnd::Notify(TNotifyUI& msg)
{
	if( msg.sType == DUI_MSGTYPE_SELECTCHANGED ) {
		if( msg.pSender == m_pFormat ) {
			SyncFormatHint();
			return;
		}
		if( msg.pSender == m_pShape ) {
			RebuildPreview();
			return;
		}
	}
	if( (msg.sType == DUI_MSGTYPE_COLORCHANGING || msg.sType == DUI_MSGTYPE_COLORCHANGED) ) {
		if( msg.pSender == m_pPaletteFg ) {
			ApplyFg((DWORD)msg.wParam);
			return;
		}
		if( msg.pSender == m_pPaletteBg ) {
			ApplyBg((DWORD)msg.wParam);
			return;
		}
	}
	WindowImplBase::Notify(msg);
}

void CAvatarExportWnd::OnClick(TNotifyUI& msg)
{
	CDuiString sName = msg.pSender->GetName();
	if( sName.CompareNoCase(_T("btn_cancel")) == 0 || sName.CompareNoCase(_T("closebtn")) == 0 ) {
		Close(0);
		return;
	}
	if( sName.CompareNoCase(_T("btn_export")) == 0 ) {
		DoExport();
		return;
	}
	if( sName.CompareNoCase(_T("btn_size_64")) == 0 ) { SetSizeEdit(64); return; }
	if( sName.CompareNoCase(_T("btn_size_128")) == 0 ) { SetSizeEdit(128); return; }
	if( sName.CompareNoCase(_T("btn_size_256")) == 0 ) { SetSizeEdit(256); return; }
	if( sName.CompareNoCase(_T("btn_size_512")) == 0 ) { SetSizeEdit(512); return; }
	if( sName.CompareNoCase(_T("btn_custom_fg")) == 0 ) {
		m_bEditBg = false;
		if( m_pPaletteFg != NULL ) {
			bool bShow = !m_pPaletteFg->IsVisible();
			m_pPaletteFg->SetVisible(bShow);
			if( m_pPaletteBg != NULL ) m_pPaletteBg->SetVisible(false);
			if( bShow ) m_pPaletteFg->SetSelectColor(m_dwFg);
		}
		return;
	}
	if( sName.CompareNoCase(_T("btn_custom_bg")) == 0 ) {
		m_bEditBg = true;
		if( m_pPaletteBg != NULL ) {
			bool bShow = !m_pPaletteBg->IsVisible();
			m_pPaletteBg->SetVisible(bShow);
			if( m_pPaletteFg != NULL ) m_pPaletteFg->SetVisible(false);
			if( bShow ) m_pPaletteBg->SetSelectColor(m_dwBg);
		}
		return;
	}
	if( sName.Find(_T("swatch_")) == 0 ) {
		LPCTSTR ud = msg.pSender->GetUserData().GetData();
		DWORD c = 0;
		if( ud == NULL || _stscanf_s(ud, _T("%08X"), &c) != 1 ) return;
		const int id = _ttoi(sName.Mid(7).GetData());
		if( id >= 200 )
			ApplyBg(c);
		else
			ApplyFg(c);
		return;
	}
	WindowImplBase::OnClick(msg);
}
