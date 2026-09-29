#include "StdAfx.h"
#include "ImageExportWnd.h"
#include "GlyphSvg.h"
#include "Core/UITheme.h"
#include <shlobj.h>

namespace {

DWORD ImgExportThemeToken(LPCTSTR pstrName, DWORD dwFallback)
{
	CThemeManager* pTm = CThemeManager::GetInstance();
	if( pTm == NULL ) return dwFallback;
	CTheme* pTh = pTm->GetCurrentTheme();
	if( pTh == NULL ) pTh = pTm->FindTheme(pTm->GetDefaultThemeId());
	if( pTh == NULL ) return dwFallback;
	return pTh->GetToken(pstrName, dwFallback);
}

int ImgColorLuma(DWORD dw)
{
	return (int)((DuiColorR(dw) * 299 + DuiColorG(dw) * 587 + DuiColorB(dw) * 114) / 1000);
}

DWORD ImgDefaultExportTint()
{
	const DWORD text = ImgExportThemeToken(_T("color-text"), 0x000000FF);
	if( ImgColorLuma(text) >= 160 )
		return 0x000000FF;
	return text;
}

int ClampImgExportSize(int v)
{
	if( v < 1 ) return 1;
	if( v > 4096 ) return 4096;
	return v;
}

int CALLBACK ImgBrowseDirCallback(HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData)
{
	if( uMsg == BFFM_INITIALIZED && lpData != 0 )
		::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	return 0;
}

} // namespace

DUI_BEGIN_MESSAGE_MAP(CImageExportWnd, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_CLICK, CImageExportWnd::OnClick)
DUI_END_MESSAGE_MAP()

CDuiString CImageExportWnd::GetDefaultExportDir()
{
	TCHAR szPath[MAX_PATH] = { 0 };
	DWORD n = ::GetModuleFileName(NULL, szPath, MAX_PATH);
	if( n == 0 || n >= MAX_PATH ) return _T("icons");
	TCHAR* pSlash = _tcsrchr(szPath, _T('\\'));
	if( pSlash == NULL ) pSlash = _tcsrchr(szPath, _T('/'));
	if( pSlash != NULL ) *pSlash = _T('\0');
	CDuiString sDir = szPath;
	sDir += _T("\\icons");
	return sDir;
}

CDuiString& CImageExportWnd::SharedExportDir()
{
	static CDuiString sDir;
	if( sDir.IsEmpty() )
		sDir = GetDefaultExportDir();
	return sDir;
}

CImageExportWnd::CImageExportWnd(const std::wstring& faceName, wchar_t ch, std::string svgUtf8)
	: m_faceName(faceName)
	, m_ch(ch)
	, m_svgUtf8(std::move(svgUtf8))
	, m_pPreview(NULL)
	, m_pFormat(NULL)
	, m_pPalette(NULL)
	, m_pColorLabel(NULL)
	, m_pDirLabel(NULL)
	, m_pEditW(NULL)
	, m_pEditH(NULL)
	, m_pColorRow(NULL)
	, m_dwTint(0x000000FF)
{
	m_dwTint = ImgDefaultExportTint();
}

CImageExportWnd::~CImageExportWnd()
{
}

void CImageExportWnd::Open(HWND hOwner, const std::wstring& faceName, wchar_t ch)
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

	CImageExportWnd* pWnd = new CImageExportWnd(faceName, ch, std::move(svg));
	pWnd->Create(hOwner, _T("导出图片"), UI_WNDSTYLE_FRAME, WS_EX_WINDOWEDGE, 0, 0, 440, 600);
	if( pWnd->GetHWND() == NULL ) {
		delete pWnd;
		return;
	}
	pWnd->CenterWindow();
	pWnd->ShowModal();
}

void CImageExportWnd::OnFinalMessage(HWND hWnd)
{
	WindowImplBase::OnFinalMessage(hWnd);
	delete this;
}

CDuiString CImageExportWnd::GetSkinFile()
{
	return _T("imageexport.html");
}

LPCTSTR CImageExportWnd::GetWindowClassName() const
{
	return _T("FontSVGImageExportWnd");
}

DWORD CImageExportWnd::ThemeToken(LPCTSTR pstrName, DWORD dwFallback) const
{
	return ImgExportThemeToken(pstrName, dwFallback);
}

void CImageExportWnd::BuildColorSwatches()
{
	if( m_pColorRow == NULL ) return;
	m_pColorRow->RemoveAll();

	struct Swatch { LPCTSTR name; DWORD color; bool theme; };
	const Swatch items[] = {
		{ _T("主题字"), 0, true },
		{ _T("主色"), 0, true },
		{ _T("黑"), 0x000000FF, false },
		{ _T("白"), 0xFFFFFFFF, false },
		{ _T("灰"), 0x6C757DFF, false },
		{ _T("红"), 0xDC3545FF, false },
		{ _T("绿"), 0x198754FF, false },
		{ _T("蓝"), 0x0D6EFDFF, false },
		{ _T("橙"), 0xFD7E14FF, false },
		{ _T("紫"), 0x722ED1FF, false },
	};

	for( int i = 0; i < (int)(sizeof(items) / sizeof(items[0])); ++i ) {
		DWORD c = items[i].color;
		if( items[i].theme ) {
			if( i == 0 ) c = ThemeToken(_T("color-text"), 0x333333FF);
			else c = ThemeToken(_T("color-primary"), 0x0D6EFDFF);
		}

		CFontIconUI* p = new CFontIconUI;
		p->SetSizePreset(28);
		p->SetShape(CFontIconUI::ShapeRounded);
		CDuiString sName;
		sName.Format(_T("swatch_%d"), i);
		p->SetName(sName.GetData());
		p->SetClickable(true);
		p->SetToolTip(items[i].name);
		CDuiString sBk;
		sBk.Format(_T("#%08X"), c);
		p->SetAttribute(_T("background-color"), sBk.GetData());
		CDuiString sTag;
		sTag.Format(_T("%08X"), c);
		p->SetUserData(sTag.GetData());
		m_pColorRow->Add(p);
	}
}

void CImageExportWnd::ApplyTint(DWORD dwColor)
{
	m_dwTint = dwColor;
	SyncPreview();
	SyncPreviewPlate();
	if( m_pColorLabel != NULL ) {
		CDuiString s;
		s.Format(_T("#%08X"), m_dwTint);
		m_pColorLabel->SetText(s.GetData());
	}
	if( m_pPalette != NULL )
		m_pPalette->SetSelectColor(m_dwTint);
}

void CImageExportWnd::SyncPreview()
{
	if( m_pPreview == NULL ) return;
	m_pPreview->SetColor(m_dwTint);
	m_pPreview->Invalidate();
}

void CImageExportWnd::SyncPreviewPlate()
{
	CControlUI* pPlate = m_pm.FindControl(_T("preview_plate"));
	if( pPlate == NULL ) return;
	const DWORD plate = (ImgColorLuma(m_dwTint) >= 160) ? 0x4A4A4AFF : 0xD8D8D8FF;
	CDuiString sBk;
	sBk.Format(_T("#%08X"), plate);
	pPlate->SetAttribute(_T("background-color"), sBk.GetData());
	pPlate->Invalidate();
}

void CImageExportWnd::SyncDirLabel()
{
	if( m_pDirLabel == NULL ) return;
	m_pDirLabel->SetText(SharedExportDir().GetData());
	m_pDirLabel->SetToolTip(SharedExportDir().GetData());
}

void CImageExportWnd::SyncSourceLabels()
{
	CLabelUI* pName = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_name")));
	if( pName != NULL )
		pName->SetText(GlyphSvg::MakeIconBaseName(m_ch).GetData());

	CLabelUI* pLib = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_lib")));
	if( pLib != NULL ) {
		pLib->SetText(m_faceName.c_str());
		pLib->SetToolTip(m_faceName.c_str());
	}

	CTitleBarUI* pBar = static_cast<CTitleBarUI*>(m_pm.FindControl(_T("titlebar")));
	if( pBar != NULL ) {
		CDuiString s;
		s.Format(_T("导出图片 — %s"), GlyphSvg::MakeIconBaseName(m_ch).GetData());
		pBar->SetTitle(s.GetData());
	}
}

void CImageExportWnd::SyncFormatHint()
{
	CLabelUI* pHint = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_fmt_hint")));
	if( pHint == NULL ) return;
	CDuiString ext = GetFormatExt();
	if( ext == _T("jpg") )
		pHint->SetText(_T("JPG：叠白底（无透明）"));
	else if( ext == _T("bmp") )
		pHint->SetText(_T("BMP：位图导出"));
	else
		pHint->SetText(_T("PNG：保留透明"));
}

void CImageExportWnd::SetSizeEdits(int w, int h)
{
	w = ClampImgExportSize(w);
	h = ClampImgExportSize(h);
	CDuiString sw, sh;
	sw.Format(_T("%d"), w);
	sh.Format(_T("%d"), h);
	if( m_pEditW != NULL ) m_pEditW->SetText(sw.GetData());
	if( m_pEditH != NULL ) m_pEditH->SetText(sh.GetData());
}

bool CImageExportWnd::ReadExportSize(int& w, int& h) const
{
	w = 256;
	h = 256;
	if( m_pEditW != NULL )
		w = _ttoi(m_pEditW->GetText().GetData());
	if( m_pEditH != NULL )
		h = _ttoi(m_pEditH->GetText().GetData());
	if( w <= 0 ) w = 256;
	if( h <= 0 ) h = 256;
	w = ClampImgExportSize(w);
	h = ClampImgExportSize(h);
	return true;
}

bool CImageExportWnd::EnsureExportDir() const
{
	const CDuiString& sDir = SharedExportDir();
	if( sDir.IsEmpty() ) return false;
	DWORD attr = ::GetFileAttributes(sDir.GetData());
	if( attr != INVALID_FILE_ATTRIBUTES ) {
		if( (attr & FILE_ATTRIBUTE_DIRECTORY) == 0 ) return false;
		return true;
	}
	return ::CreateDirectory(sDir.GetData(), NULL) != FALSE
		|| ::GetLastError() == ERROR_ALREADY_EXISTS;
}

bool CImageExportWnd::BrowseExportDir()
{
	TCHAR szDisplay[MAX_PATH] = { 0 };
	BROWSEINFO bi;
	::ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner = m_hWnd;
	bi.pszDisplayName = szDisplay;
	bi.lpszTitle = _T("选择导出目录");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = ImgBrowseDirCallback;
	bi.lParam = (LPARAM)SharedExportDir().GetData();
	LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
	if( pidl == NULL ) return false;
	TCHAR szPath[MAX_PATH] = { 0 };
	BOOL ok = ::SHGetPathFromIDList(pidl, szPath);
	::CoTaskMemFree(pidl);
	if( !ok || szPath[0] == _T('\0') ) return false;
	SharedExportDir() = szPath;
	SyncDirLabel();
	return true;
}

CDuiString CImageExportWnd::GetFormatExt() const
{
	if( m_pFormat == NULL ) return _T("png");
	LPCTSTR v = m_pFormat->GetSelectedValue();
	if( v == NULL || *v == _T('\0') ) return _T("png");
	return v;
}

bool CImageExportWnd::DoExport()
{
	if( m_pPreview == NULL || m_svgUtf8.empty() ) return false;
	if( !EnsureExportDir() ) {
		CMessageBox::ShowInfo(m_hWnd, _T("提示"), _T("无法创建导出目录"));
		return false;
	}

	int w = 0, h = 0;
	ReadExportSize(w, h);
	CDuiString ext = GetFormatExt();
	CDuiString sFile = GlyphSvg::MakeIconBaseName(m_ch);
	CDuiString sPath;
	sPath.Format(_T("%s\\%s.%s"), SharedExportDir().GetData(), sFile.GetData(), ext.GetData());

	if( ::GetFileAttributes(sPath.GetData()) != INVALID_FILE_ATTRIBUTES ) {
		CDuiString sAsk;
		sAsk.Format(_T("文件已存在，是否覆盖？\n%s"), sPath.GetData());
		if( ::MessageBox(m_hWnd, sAsk.GetData(), _T("导出图片"), MB_YESNO | MB_ICONQUESTION) != IDYES )
			return false;
	}

	const bool ok = m_pPreview->ExportToFile(sPath.GetData(), w, h, m_dwTint, 90);
	if( ok ) {
		CDuiString sTip;
		sTip.Format(_T("已导出：%s\n尺寸：%d × %d\n格式：%s"),
			sPath.GetData(), w, h, ext.GetData());
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

void CImageExportWnd::InitWindow()
{
	m_pPreview = static_cast<CSvgBoxUI*>(m_pm.FindControl(_T("preview")));
	m_pFormat = static_cast<CSegmentedUI*>(m_pm.FindControl(_T("seg_format")));
	m_pPalette = static_cast<CColorPaletteUI*>(m_pm.FindControl(_T("palette")));
	m_pColorLabel = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_color")));
	m_pDirLabel = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_dir")));
	m_pEditW = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_w")));
	m_pEditH = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_h")));
	m_pColorRow = static_cast<CHorizontalLayoutUI*>(m_pm.FindControl(_T("color_row")));

	if( m_pPreview != NULL ) {
		m_pPreview->LoadFromUtf8Data(m_svgUtf8.c_str());
		SyncPreview();
	}

	SetSizeEdits(256, 256);
	BuildColorSwatches();
	ApplyTint(m_dwTint);
	SyncFormatHint();
	EnsureExportDir();
	SyncDirLabel();
	SyncSourceLabels();
}

void CImageExportWnd::Notify(TNotifyUI& msg)
{
	if( msg.sType == DUI_MSGTYPE_SELECTCHANGED && msg.pSender == m_pFormat ) {
		SyncFormatHint();
		return;
	}
	if( (msg.sType == DUI_MSGTYPE_COLORCHANGING || msg.sType == DUI_MSGTYPE_COLORCHANGED)
		&& msg.pSender == m_pPalette ) {
		ApplyTint((DWORD)msg.wParam);
		return;
	}
	WindowImplBase::Notify(msg);
}

void CImageExportWnd::OnClick(TNotifyUI& msg)
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
	if( sName.CompareNoCase(_T("btn_browse_dir")) == 0 ) {
		BrowseExportDir();
		return;
	}
	if( sName.CompareNoCase(_T("btn_size_32")) == 0 ) { SetSizeEdits(32, 32); return; }
	if( sName.CompareNoCase(_T("btn_size_64")) == 0 ) { SetSizeEdits(64, 64); return; }
	if( sName.CompareNoCase(_T("btn_size_128")) == 0 ) { SetSizeEdits(128, 128); return; }
	if( sName.CompareNoCase(_T("btn_size_256")) == 0 ) { SetSizeEdits(256, 256); return; }
	if( sName.CompareNoCase(_T("btn_size_512")) == 0 ) { SetSizeEdits(512, 512); return; }
	if( sName.CompareNoCase(_T("btn_custom_color")) == 0 ) {
		if( m_pPalette != NULL ) {
			bool bShow = !m_pPalette->IsVisible();
			m_pPalette->SetVisible(bShow);
			if( bShow )
				m_pPalette->SetSelectColor(m_dwTint);
		}
		return;
	}
	if( sName.Find(_T("swatch_")) == 0 ) {
		LPCTSTR ud = msg.pSender->GetUserData().GetData();
		DWORD c = 0;
		if( ud != NULL && _stscanf_s(ud, _T("%08X"), &c) == 1 )
			ApplyTint(c);
		return;
	}
	WindowImplBase::OnClick(msg);
}
