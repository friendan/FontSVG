#include "StdAfx.h"
#include "duilib.h"
#include "Logger.h"

namespace {

	const UINT WM_FONTSVG_REFRESH_CHARS = WM_USER + 0x4601;
	const int kCellGap = 6;
	const int kFallbackPageSize = 96;

	bool CopyTextToClipboard(HWND owner, const std::wstring& text)
	{
		if( !::OpenClipboard(owner) ) return false;
		::EmptyClipboard();

		const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
		HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
		if( hMem == NULL ) {
			::CloseClipboard();
			return false;
		}

		void* p = ::GlobalLock(hMem);
		if( p == NULL ) {
			::GlobalFree(hMem);
			::CloseClipboard();
			return false;
		}
		memcpy(p, text.c_str(), bytes);
		::GlobalUnlock(hMem);

		::SetClipboardData(CF_UNICODETEXT, hMem);
		::CloseClipboard();
		return true;
	}

	std::wstring ToLowerText(const std::wstring& text)
	{
		std::wstring out = text;
		for( size_t i = 0; i < out.size(); ++i )
			out[i] = (wchar_t)towlower(out[i]);
		return out;
	}

	bool MatchFilter(const std::wstring& filter, const std::wstring& haystack)
	{
		if( filter.empty() ) return true;
		return ToLowerText(haystack).find(filter) != std::wstring::npos;
	}

	void SyncListColumns(CListUI* pList)
	{
		if( pList == NULL || pList->GetHeader() == NULL ) return;
		pList->SetPos(pList->GetPos());
	}

	bool ParseHexCodepoint(const std::wstring& text, wchar_t& outCh)
	{
		if( text.empty() ) return false;
		std::wstring s = text;
		if( s.size() >= 2 && s[0] == L'U' && (s[1] == L'+' || s[1] == L'-') )
			s = s.substr(2);
		else if( s.size() >= 2 && s[0] == L'0' && (s[1] == L'x' || s[1] == L'X') )
			s = s.substr(2);

		if( s.empty() || s.size() > 4 ) return false;
		for( size_t i = 0; i < s.size(); ++i ) {
			wchar_t c = s[i];
			bool ok = (c >= L'0' && c <= L'9')
				|| (c >= L'a' && c <= L'f')
				|| (c >= L'A' && c <= L'F');
			if( !ok ) return false;
		}
		unsigned long v = wcstoul(s.c_str(), NULL, 16);
		if( v > 0xFFFF ) return false;
		outCh = (wchar_t)v;
		return true;
	}

	bool CharMatchFilter(wchar_t ch, const std::wstring& filterRaw)
	{
		if( filterRaw.empty() ) return true;

		std::wstring filter = filterRaw;
		while( !filter.empty() && (filter.front() == L' ' || filter.front() == L'\t') )
			filter.erase(filter.begin());
		while( !filter.empty() && (filter.back() == L' ' || filter.back() == L'\t') )
			filter.pop_back();
		if( filter.empty() ) return true;

		if( filter.size() == 1 )
			return ch == filter[0];

		wchar_t hexCh = 0;
		if( ParseHexCodepoint(filter, hexCh) )
			return ch == hexCh;

		wchar_t buf[8] = {};
		buf[0] = ch;
		std::wstring glyph(buf);
		if( MatchFilter(ToLowerText(filter), glyph) )
			return true;

		CDuiString hex;
		hex.Format(_T("%04X"), (unsigned)(unsigned short)ch);
		return MatchFilter(ToLowerText(filter), hex.GetData());
	}

} // namespace

DUI_BEGIN_MESSAGE_MAP(CMainWnd, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_CLICK, CMainWnd::OnClick)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_ITEMSELECT, CMainWnd::OnItemSelect)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_TEXTCHANGED, CMainWnd::OnTextChanged)
DUI_END_MESSAGE_MAP()

CMainWnd::CMainWnd()
	: m_pFontList(NULL)
	, m_pFontCat(NULL)
	, m_pCharGrid(NULL)
	, m_pStatusMessage(NULL)
	, m_pSelectedFont(NULL)
	, m_pPageInfo(NULL)
	, m_pBtnPagePrev(NULL)
	, m_pBtnPageNext(NULL)
	, m_pBtnPageGo(NULL)
	, m_pBtnZoomIn(NULL)
	, m_pBtnZoomOut(NULL)
	, m_pEditPageGoto(NULL)
	, m_menuChar(0)
	, m_hasMenuChar(false)
	, m_glyphFontSize(kGlyphFontDefault)
	, m_pageIndex(0)
	, m_pageWindowStart(0)
	, m_lastPageSize(0)
	, m_lastGridHeight(-1)
	, m_refreshRetry(0)
{
	for( int i = 0; i < kPageSlotCount; ++i )
		m_pBtnPageSlots[i] = NULL;
}

CMainWnd::~CMainWnd()
{
	if( CThemeManager* tm = CThemeManager::GetInstance() )
		tm->RemoveThemeNotify(this);
}

void CMainWnd::OnThemeChanged(LPCTSTR /*oldId*/, LPCTSTR /*newId*/, bool /*bPreview*/)
{
	SyncFontListThemeColors();
	ScheduleCharPageRefresh();
}

void CMainWnd::SyncFontListThemeColors()
{
	CThemeManager* tm = CThemeManager::GetInstance();
	if( tm == NULL || m_pFontList == NULL ) return;

	const DWORD text = tm->GetColor(_T("color-text"), 0x000000E0);
	const DWORD textSec = tm->GetColor(_T("color-text-secondary"), 0x000000A6);
	const DWORD primary = tm->GetColor(_T("color-primary"), 0x0D6EFDFF);
	const DWORD ctrlBg = tm->GetColor(_T("color-control-bg"),
		tm->GetColor(_T("color-bg"), 0xFFFFFFFF));
	const DWORD bgElev = tm->GetColor(_T("color-bg-elevated"), 0xF8F9FAFF);
	const DWORD bgHover = tm->GetColor(_T("color-bg-hover"), bgElev);
	const DWORD selection = tm->GetColor(_T("color-selection"), bgElev);
	const DWORD border = tm->GetColor(_T("color-border"), 0xDEE2E6FF);

	m_pFontList->SetItemColor(text);
	m_pFontList->SetHoverItemColor(text);
	m_pFontList->SetSelectedItemColor(primary);
	m_pFontList->SetDisabledItemColor(textSec);
	m_pFontList->SetItemBackgroundColor(ctrlBg);
	m_pFontList->SetHoverItemBackgroundColor(bgHover);
	m_pFontList->SetSelectedItemBackgroundColor(selection);
	m_pFontList->SetAlternateBkColor(bgElev);
	m_pFontList->SetItemLineColor(border);
	m_pFontList->SetBackgroundColor(ctrlBg);
	m_pFontList->SetBorderColor(border);

	if( CListHeaderUI* pHdr = m_pFontList->GetHeader() ) {
		pHdr->SetBackgroundColor(bgElev);
		pHdr->SetBorderColor(border);
		for( int i = 0; i < pHdr->GetCount(); ++i ) {
			CControlUI* pCol = pHdr->GetItemAt(i);
			CListHeaderItemUI* pItem = pCol
				? static_cast<CListHeaderItemUI*>(pCol->GetInterface(DUI_CTR_LISTHEADERITEM))
				: NULL;
			if( pItem != NULL )
				pItem->SetColor(text);
		}
	}

	m_pFontList->Invalidate();
}

CDuiString CMainWnd::GetSkinFile()
{
	return _T("main.html");
}

CDuiString CMainWnd::GetSkinFolder()
{
	return _T("");
}

LPCTSTR CMainWnd::GetWindowClassName() const
{
	return _T("FontSVGMainWnd");
}

void CMainWnd::InitWindow()
{
	SetIcon(IDI_FONTSVG);

	m_pFontList = static_cast<CListUI*>(m_pm.FindControl(_T("list_font")));
	m_pFontCat = static_cast<CSegmentedUI*>(m_pm.FindControl(_T("seg_font_cat")));
	m_pCharGrid = static_cast<CVerticalLayoutUI*>(m_pm.FindControl(_T("grid_chars")));
	m_pStatusMessage = static_cast<CLabelUI*>(m_pm.FindControl(_T("status_message")));
	m_pSelectedFont = static_cast<CLabelUI*>(m_pm.FindControl(_T("label_selected_font")));
	m_pPageInfo = static_cast<CLabelUI*>(m_pm.FindControl(_T("label_page_info")));
	m_pBtnPagePrev = static_cast<CButtonUI*>(m_pm.FindControl(_T("btn_page_prev")));
	m_pBtnPageNext = static_cast<CButtonUI*>(m_pm.FindControl(_T("btn_page_next")));
	m_pBtnPageGo = static_cast<CButtonUI*>(m_pm.FindControl(_T("btn_page_go")));
	m_pBtnZoomIn = static_cast<CButtonUI*>(m_pm.FindControl(_T("btn_zoom_in")));
	m_pBtnZoomOut = static_cast<CButtonUI*>(m_pm.FindControl(_T("btn_zoom_out")));
	m_pEditPageGoto = static_cast<CEditUI*>(m_pm.FindControl(_T("edit_page_goto")));
	for( int i = 0; i < kPageSlotCount; ++i ) {
		CDuiString name;
		name.Format(_T("btn_page_n%d"), i);
		m_pBtnPageSlots[i] = static_cast<CButtonUI*>(m_pm.FindControl(name.GetData()));
	}

	LOG_DEBUGF(L"[Init] fontList=%p charGrid=%p",
		(void*)m_pFontList, (void*)m_pCharGrid);

	if( CThemeManager* tm = CThemeManager::GetInstance() )
		tm->AddThemeNotify(this);
	SyncFontListThemeColors();

	FontCatalog::Instance().RefreshSystemFonts();
	FontCatalog::Instance().LoadBundledFonts();
	RefreshFontList();
	UpdateGlyphZoomButtons();
	RefreshCharPage();
	ScheduleCharPageRefresh();
}

void CMainWnd::OnFinalMessage(HWND hWnd)
{
	WindowImplBase::OnFinalMessage(hWnd);
}

void CMainWnd::Notify(TNotifyUI& msg)
{
	if( msg.sType == DUI_MSGTYPE_MENU ) {
		if( IsFontListNotify(msg.pSender) ) {
			ShowFontContextMenu();
			return;
		}
		if( IsGlyphCell(msg.pSender) ) {
			ShowCharContextMenu(msg.pSender);
			return;
		}
	}
	if( msg.sType == DUI_MSGTYPE_VALUECHANGED
		&& msg.pSender != NULL
		&& msg.pSender->GetName() == _T("content")
		&& !m_filteredChars.empty() ) {
		// 左右分栏拖拽结束后按新宽度重排字符网格
		ScheduleCharPageRefresh();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_SELECTCHANGED && msg.pSender == m_pFontCat ) {
		RefreshFontList();
		return;
	}
	WindowImplBase::Notify(msg);
}

LRESULT CMainWnd::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if( uMsg == WM_MENUCLICK ) {
		MenuCmd* pMenuCmd = (MenuCmd*)wParam;
		if( pMenuCmd != NULL ) {
			CDuiString sName = pMenuCmd->szName;
			m_pm.DeletePtr(pMenuCmd);
			if( sName == _T("menu_font_copy_name") )
				OnCopySelectedFontName();
			else if( sName == _T("menu_char_copy") )
				OnCopySelectedChar();
			else if( sName == _T("menu_char_outline") )
				OnEditSelectedCharOutline();
			else if( sName == _T("menu_char_export") )
				OnExportSelectedChar();
			else if( sName == _T("menu_char_export_image") )
				OnExportSelectedCharImage();
			else if( sName == _T("menu_char_export_avatar") )
				OnExportSelectedCharAvatar();
			else if( sName == _T("menu_char_export_svg") )
				OnExportSelectedCharSvg();
		}
		bHandled = TRUE;
		return 0;
	}
	if( uMsg == WM_FONTSVG_REFRESH_CHARS ) {
		EnsureRootLayout();
		const bool readyBefore = IsCharGridReady();
		LOG_VERBOSEF(L"[Retry] readyBefore=%d retry=%d", readyBefore ? 1 : 0, m_refreshRetry);
		RefreshCharPage();
		// 布局未完成时继续重试，直到拿到真实高度
		if( !readyBefore && !IsCharGridReady() && !m_filteredChars.empty() && m_refreshRetry < 60 ) {
			++m_refreshRetry;
			::PostMessage(GetHWND(), WM_FONTSVG_REFRESH_CHARS, 0, 0);
		} else {
			m_refreshRetry = 0;
		}
		bHandled = TRUE;
		return 0;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT CMainWnd::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lr = WindowImplBase::OnSize(uMsg, wParam, lParam, bHandled);
	if( wParam != SIZE_MINIMIZED && !m_filteredChars.empty() ) {
		EnsureRootLayout();
		RECT rc = m_pCharGrid ? m_pCharGrid->GetPos() : RECT{};
		int gridH = rc.bottom - rc.top;
		int pageSize = GetPageSize();
		if( pageSize != m_lastPageSize || gridH != m_lastGridHeight || (m_pCharGrid && m_pCharGrid->GetCount() == 0) )
			ScheduleCharPageRefresh();
	}
	return lr;
}

void CMainWnd::ScheduleCharPageRefresh()
{
	HWND hWnd = GetHWND();
	if( hWnd == NULL ) return;
	::PostMessage(hWnd, WM_FONTSVG_REFRESH_CHARS, 0, 0);
}

void CMainWnd::EnsureRootLayout()
{
	HWND hWnd = GetHWND();
	if( hWnd == NULL ) return;

	RECT rcClient = {};
	if( !::GetClientRect(hWnd, &rcClient) ) return;
	if( rcClient.right - rcClient.left < 8 || rcClient.bottom - rcClient.top < 8 )
		return;

	CControlUI* pRoot = m_pm.GetRoot();
	if( pRoot )
		pRoot->SetPos(rcClient);
}

void CMainWnd::SetStatusMessage(LPCTSTR text)
{
	if( m_pStatusMessage )
		m_pStatusMessage->SetText(text ? text : _T(""));
}

void CMainWnd::UpdateSelectedFontLabel()
{
	if( !m_pSelectedFont ) return;
	if( m_currentFace.empty() ) {
		m_pSelectedFont->SetText(_T("未选择字体"));
		return;
	}

	CDuiString text;
	text.Format(_T("当前：%s"), m_currentFace.c_str());
	int idx = GetSelectedFontIndex();
	const FontEntry* e = FontCatalog::Instance().GetAt((size_t)idx);
	if( e && e->isBundled )
		text += _T("  [自带]");
	else if( e && e->isFileFont )
		text += _T("  [文件]");
	m_pSelectedFont->SetText(text.GetData());
	m_pSelectedFont->SetToolTip(e && e->isFileFont ? e->filePath.c_str() : m_currentFace.c_str());
}

void CMainWnd::UpdateStatusBar()
{
	CDuiString msg;
	if( m_currentFace.empty() ) {
		msg.Format(_T("字体 %d 个"), (int)FontCatalog::Instance().Fonts().size());
	} else if( m_filteredChars.size() == m_allChars.size() ) {
		msg.Format(_T("「%s」· %d 字"), m_currentFace.c_str(), (int)m_allChars.size());
	} else {
		msg.Format(_T("「%s」· 显示 %d / %d 字"),
			m_currentFace.c_str(),
			(int)m_filteredChars.size(),
			(int)m_allChars.size());
	}
	SetStatusMessage(msg.GetData());
}

int CMainWnd::GetSelectedFontIndex() const
{
	if( !m_pFontList ) return -1;
	int sel = m_pFontList->GetCurSel();
	if( sel < 0 ) return -1;
	CControlUI* pItem = m_pFontList->GetItemAt(sel);
	if( pItem == NULL ) return -1;
	return (int)pItem->GetTag();
}

bool CMainWnd::IsFontListNotify(CControlUI* pSender) const
{
	if( pSender == NULL || m_pFontList == NULL ) return false;
	if( pSender == m_pFontList ) return true;
	return m_pFontList->GetItemIndex(pSender) >= 0;
}

void CMainWnd::ShowFontContextMenu()
{
	POINT pt = {};
	::GetCursorPos(&pt);
	CMenuWnd::CreateMenu(NULL, _T("menu_font.html"), pt, &m_pm);
}

bool CMainWnd::IsGlyphCell(CControlUI* pSender) const
{
	return pSender != NULL && pSender->GetName() == _T("glyph_cell");
}

CDuiString CMainWnd::FormatCharInfo(wchar_t ch)
{
	CDuiString s;
	s.Format(_T("U+%04X:"), (unsigned)(unsigned short)ch);
	TCHAR chText[2] = { ch, 0 };
	s += chText;
	return s;
}

void CMainWnd::ShowCharInStatus(wchar_t ch)
{
	SetStatusMessage(FormatCharInfo(ch).GetData());
}

void CMainWnd::ShowCharContextMenu(CControlUI* pSender)
{
	if( !IsGlyphCell(pSender) ) return;
	m_menuChar = (wchar_t)(unsigned short)pSender->GetTag();
	m_hasMenuChar = true;
	POINT pt = {};
	::GetCursorPos(&pt);
	CMenuWnd::CreateMenu(NULL, _T("menu_char.html"), pt, &m_pm);
}

void CMainWnd::OnCopySelectedChar()
{
	if( !m_hasMenuChar ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择文字"));
		return;
	}

	CDuiString info = FormatCharInfo(m_menuChar);
	if( !CopyTextToClipboard(GetHWND(), std::wstring(info.GetData())) ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("复制到剪贴板失败"));
		return;
	}

	CDuiString msg;
	msg.Format(_T("已复制：%s"), info.GetData());
	SetStatusMessage(msg.GetData());
}

void CMainWnd::OnEditSelectedCharOutline()
{
	if( !m_hasMenuChar ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择文字"));
		return;
	}
	if( m_currentFace.empty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择字体"));
		return;
	}
	COutlineEditWnd::Open(GetHWND(), m_currentFace, m_menuChar);
}

void CMainWnd::OnExportSelectedChar()
{
	if( !m_hasMenuChar ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择文字"));
		return;
	}
	if( m_currentFace.empty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择字体"));
		return;
	}
	CIconExportWnd::Open(GetHWND(), m_currentFace, m_menuChar);
}

void CMainWnd::OnExportSelectedCharImage()
{
	if( !m_hasMenuChar ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择文字"));
		return;
	}
	if( m_currentFace.empty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择字体"));
		return;
	}
	CImageExportWnd::Open(GetHWND(), m_currentFace, m_menuChar);
}

void CMainWnd::OnExportSelectedCharAvatar()
{
	if( !m_hasMenuChar ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择文字"));
		return;
	}
	if( m_currentFace.empty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择字体"));
		return;
	}
	CAvatarExportWnd::Open(GetHWND(), m_currentFace, m_menuChar);
}

void CMainWnd::OnExportSelectedCharSvg()
{
	if( !m_hasMenuChar ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择文字"));
		return;
	}
	if( m_currentFace.empty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择字体"));
		return;
	}

	std::string svg;
	if( !GlyphSvg::BuildUtf8ForFile(m_currentFace, m_menuChar, svg) ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("无法提取字形轮廓（可能不是 TrueType/OpenType）"));
		return;
	}

	CDuiString sDir = GlyphSvg::GetDefaultExportDir();
	DWORD attr = ::GetFileAttributes(sDir.GetData());
	if( attr == INVALID_FILE_ATTRIBUTES ) {
		if( !::CreateDirectory(sDir.GetData(), NULL)
			&& ::GetLastError() != ERROR_ALREADY_EXISTS ) {
			CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("无法创建导出目录"));
			return;
		}
	} else if( (attr & FILE_ATTRIBUTE_DIRECTORY) == 0 ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("导出路径不是目录"));
		return;
	}

	CDuiString sFile = GlyphSvg::MakeIconBaseName(m_menuChar);
	TCHAR szPath[MAX_PATH] = { 0 };
	_sntprintf_s(szPath, _TRUNCATE, _T("%s\\%s.svg"), sDir.GetData(), sFile.GetData());

	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetHWND();
	ofn.lpstrFilter = _T("SVG 文件 (*.svg)\0*.svg\0所有文件 (*.*)\0*.*\0");
	ofn.lpstrFile = szPath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = sDir.GetData();
	ofn.lpstrDefExt = _T("svg");
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	if( !::GetSaveFileName(&ofn) )
		return;

	if( !GlyphSvg::WriteUtf8File(szPath, svg) ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("写入 SVG 失败"));
		return;
	}

	CDuiString tip;
	tip.Format(_T("已导出：%s"), szPath);
	CMessageBox::Show(GetHWND(), _T("导出成功"), tip.GetData(),
		CModalOptions()
			.Kind(CONTROLKIND_INFO)
			.ShowCancel(false)
			.Width(560)
			.Height(220));

	CDuiString status;
	status.Format(_T("已导出 SVG：%s"), sFile.GetData());
	SetStatusMessage(status.GetData());
}

void CMainWnd::OnCopySelectedFontName()
{
	const int idx = GetSelectedFontIndex();
	const FontEntry* e = FontCatalog::Instance().GetAt((size_t)idx);
	if( e == NULL || e->faceName.empty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请先选择字体"));
		return;
	}

	if( !CopyTextToClipboard(GetHWND(), e->faceName) ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("复制到剪贴板失败"));
		return;
	}

	CDuiString msg;
	msg.Format(_T("已复制字体名称：%s"), e->faceName.c_str());
	SetStatusMessage(msg.GetData());
}

void CMainWnd::RefreshFontList(int selectIndex)
{
	if( !m_pFontList ) return;

	int keepIndex = selectIndex;
	if( keepIndex < 0 )
		keepIndex = GetSelectedFontIndex();

	CControlUI* pFilter = m_pm.FindControl(_T("filter_font"));
	std::wstring filter;
	if( pFilter ) {
		CDuiString t = pFilter->GetText();
		t.TrimLeft();
		t.TrimRight();
		filter = ToLowerText(t.GetData());
	}

	const bool showBundled = IsBundledFontCategory();

	m_pFontList->RemoveAll();
	SyncListColumns(m_pFontList);

	const std::vector<FontEntry>& fonts = FontCatalog::Instance().Fonts();
	int selectRow = -1;
	int row = 0;
	for( size_t i = 0; i < fonts.size(); ++i ) {
		const bool isLocal = fonts[i].isFileFont;
		if( showBundled ? !isLocal : isLocal )
			continue;

		if( !MatchFilter(filter, fonts[i].faceName)
			&& !MatchFilter(filter, fonts[i].filePath) )
			continue;

		CListTextElementUI* pItem = new CListTextElementUI;
		pItem->SetTag((UINT_PTR)i);
		pItem->SetFixedHeight(28);
		m_pFontList->Add(pItem);

		CDuiString sNo;
		sNo.Format(_T("%d"), row + 1);
		pItem->SetText(0, sNo.GetData());
		pItem->SetText(1, fonts[i].faceName.c_str());
		LPCTSTR source = _T("系统");
		if( fonts[i].isBundled )
			source = _T("自带");
		else if( fonts[i].isFileFont )
			source = _T("文件");
		pItem->SetText(2, source);

		if( keepIndex >= 0 && (size_t)keepIndex == i )
			selectRow = row;
		++row;
	}

	if( selectRow >= 0 )
		m_pFontList->SelectItem(selectRow);
	else if( m_pFontList->GetCount() > 0 )
		m_pFontList->SelectItem(0);

	OnFontSelected();
}

bool CMainWnd::IsBundledFontCategory() const
{
	if( !m_pFontCat ) return false;
	LPCTSTR v = m_pFontCat->GetSelectedValue();
	return v != NULL && _tcscmp(v, _T("bundled")) == 0;
}

void CMainWnd::OnFontSelected()
{
	int idx = GetSelectedFontIndex();
	const FontEntry* e = FontCatalog::Instance().GetAt((size_t)idx);
	std::wstring face = e ? e->faceName : L"";
	if( face == m_currentFace && !m_allChars.empty() ) {
		UpdateSelectedFontLabel();
		UpdateStatusBar();
		return;
	}

	m_currentFace = face;
	UpdateSelectedFontLabel();
	ReloadGlyphsForCurrentFont();
}

void CMainWnd::ReloadGlyphsForCurrentFont()
{
	m_allChars.clear();
	m_filteredChars.clear();
	m_pageIndex = 0;
	m_pageWindowStart = 0;

	if( m_currentFace.empty() ) {
		if( m_pCharGrid ) m_pCharGrid->RemoveAll();
		UpdatePageControl();
		UpdateStatusBar();
		return;
	}

	SetStatusMessage(_T("正在枚举字形…"));
	const bool ok = FontCatalog::Instance().CollectCodepoints(m_currentFace, m_allChars);
	LOG_DEBUGF(L"[Glyphs] face=%s ok=%d count=%d", m_currentFace.c_str(), ok ? 1 : 0, (int)m_allChars.size());
	ApplyCharFilter();
}

void CMainWnd::ApplyCharFilter()
{
	m_filteredChars.clear();
	m_pageIndex = 0;
	m_pageWindowStart = 0;

	CControlUI* pFilter = m_pm.FindControl(_T("filter_char"));
	std::wstring filter;
	if( pFilter ) {
		CDuiString t = pFilter->GetText();
		filter = t.GetData();
	}

	m_filteredChars.reserve(m_allChars.size());
	for( size_t i = 0; i < m_allChars.size(); ++i ) {
		if( CharMatchFilter(m_allChars[i], filter) )
			m_filteredChars.push_back(m_allChars[i]);
	}

	RefreshCharPage();
}

bool CMainWnd::IsCharGridReady() const
{
	if( !m_pCharGrid ) return false;
	const int cell = GetGlyphCellSize();
	RECT rc = m_pCharGrid->GetPos();
	return (rc.right - rc.left) >= cell && (rc.bottom - rc.top) >= cell;
}

int CMainWnd::GetGlyphCellSize() const
{
	return m_glyphFontSize + 20;
}

void CMainWnd::UpdateGlyphZoomButtons()
{
	if( m_pBtnZoomOut )
		m_pBtnZoomOut->SetEnabled(m_glyphFontSize > kGlyphFontMin);
	if( m_pBtnZoomIn )
		m_pBtnZoomIn->SetEnabled(true);
}

void CMainWnd::AdjustGlyphZoom(int delta)
{
	int next = m_glyphFontSize + delta;
	if( next < kGlyphFontMin ) next = kGlyphFontMin;
	if( next == m_glyphFontSize ) return;

	m_glyphFontSize = next;
	UpdateGlyphZoomButtons();
	m_lastPageSize = 0;
	m_lastGridHeight = -1;
	RefreshCharPage();

	CDuiString msg;
	msg.Format(_T("预览字号：%d"), m_glyphFontSize);
	SetStatusMessage(msg.GetData());
}

int CMainWnd::GetPageSize() const
{
	if( !m_pCharGrid || !IsCharGridReady() )
		return kFallbackPageSize;

	RECT rc = m_pCharGrid->GetPos();
	int gap = m_pCharGrid->GetGap();
	if( gap < 0 ) gap = kCellGap;

	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;
	RECT pad = m_pCharGrid->GetPadding();
	w -= (pad.left + pad.right);
	h -= (pad.top + pad.bottom);
	if( w < 1 ) w = 1;
	if( h < 1 ) h = 1;

	const int cell = GetGlyphCellSize();
	int cols = (w + gap) / (cell + gap);
	int rows = (h + gap) / (cell + gap);
	if( cols < 1 ) cols = 1;
	if( rows < 1 ) rows = 1;
	return cols * rows;
}

int CMainWnd::GetPageCount() const
{
	int pageSize = GetPageSize();
	if( pageSize <= 0 ) pageSize = kFallbackPageSize;
	if( m_filteredChars.empty() ) return 0;
	return ((int)m_filteredChars.size() + pageSize - 1) / pageSize;
}

void CMainWnd::RefreshCharPage()
{
	if( !m_pCharGrid ) {
		LOG_DEBUGF(L"[Refresh] grid is NULL");
		SetStatusMessage(_T("预览控件未找到"));
		return;
	}

	EnsureRootLayout();

	RECT rc = m_pCharGrid->GetPos();
	CControlUI* pPane = m_pm.FindControl(_T("pane_chars"));
	CControlUI* pContent = m_pm.FindControl(_T("content"));
	CControlUI* pRoot = m_pm.GetRoot();
	RECT rcPane = pPane ? pPane->GetPos() : RECT{};
	RECT rcContent = pContent ? pContent->GetPos() : RECT{};
	RECT rcRoot = pRoot ? pRoot->GetPos() : RECT{};

	int pageSize = GetPageSize();
	int pageCount = GetPageCount();

	// 高频：每次填充都会打；查布局高度/每页容量时 SetVerboseEnabled(true)
	LOG_VERBOSEF(L"[Refresh] face=%s filt=%d ready=%d grid=%dx%d pane=%dx%d content=%dx%d root=%dx%d pageSize=%d pageCount=%d",
		m_currentFace.c_str(),
		(int)m_filteredChars.size(),
		IsCharGridReady() ? 1 : 0,
		rc.right - rc.left, rc.bottom - rc.top,
		rcPane.right - rcPane.left, rcPane.bottom - rcPane.top,
		rcContent.right - rcContent.left, rcContent.bottom - rcContent.top,
		rcRoot.right - rcRoot.left, rcRoot.bottom - rcRoot.top,
		pageSize, pageCount);

	if( pageCount <= 0 ) {
		m_pageIndex = 0;
		m_lastPageSize = pageSize;
		RECT rcNow = m_pCharGrid->GetPos();
		m_lastGridHeight = rcNow.bottom - rcNow.top;
		m_pCharGrid->RemoveAll();
		UpdatePageControl();
		UpdateStatusBar();
		return;
	}
	if( m_pageIndex >= pageCount )
		m_pageIndex = pageCount - 1;
	if( m_pageIndex < 0 )
		m_pageIndex = 0;

	m_lastPageSize = pageSize;
	m_lastGridHeight = rc.bottom - rc.top;
	size_t start = (size_t)m_pageIndex * (size_t)pageSize;
	size_t end = start + (size_t)pageSize;
	if( end > m_filteredChars.size() )
		end = m_filteredChars.size();

	m_pCharGrid->RemoveAll();

	const int cell = GetGlyphCellSize();
	RECT pad = m_pCharGrid->GetPadding();
	int gap = m_pCharGrid->GetGap();
	if( gap < 0 ) gap = kCellGap;
	int innerW = (rc.right - rc.left) - pad.left - pad.right;
	if( innerW < cell ) innerW = cell;
	int cols = (innerW + gap) / (cell + gap);
	if( cols < 1 ) cols = 1;

	CHorizontalLayoutUI* pRow = NULL;
	int col = 0;
	for( size_t i = start; i < end; ++i ) {
		if( pRow == NULL || col >= cols ) {
			pRow = new CHorizontalLayoutUI;
			pRow->SetFixedHeight(cell);
			pRow->SetAttribute(_T("gap"), _T("6"));
			pRow->SetAttribute(_T("align-items"), _T("vcenter"));
			m_pCharGrid->Add(pRow);
			col = 0;
		}

		wchar_t ch = m_filteredChars[i];
		wchar_t text[2] = { ch, 0 };

		CGlyphCellUI* pLabel = new CGlyphCellUI;
		pLabel->SetName(_T("glyph_cell"));
		pLabel->SetText(text);
		pLabel->SetTag((UINT_PTR)(unsigned short)ch);
		pLabel->SetFixedWidth(cell);
		pLabel->SetFixedHeight(cell);
		pLabel->SetAttribute(_T("clickable"), _T("true"));
		pLabel->SetAttribute(_T("menu"), _T("true"));
		pLabel->SetAttribute(_T("text-align"), _T("center"));
		pLabel->SetAttribute(_T("vertical-align"), _T("vcenter"));
		pLabel->SetAttribute(_T("color"), _T("var(--color-text)"));
		pLabel->SetAttribute(_T("border-radius"), _T("4"));
		pLabel->SetAttribute(_T("background-color-hover"), _T("var(--color-bg-hover-medium)"));

		// 先挂到树再设字体，确保 EnsureFont / ResolveCssFont 能拿到 PaintManager
		pRow->Add(pLabel);
		pLabel->SetFontFamily(m_currentFace.c_str());
		pLabel->SetFontSize(m_glyphFontSize);

		++col;
	}

	m_pCharGrid->NeedUpdate();
	LOG_VERBOSEF(L"[Refresh] added=%d rows=%d cols=%d", (int)(end - start), m_pCharGrid->GetCount(), cols);
	UpdatePageControl();
	UpdateStatusBar();
}

void CMainWnd::SyncPageWindow()
{
	const int pageCount = GetPageCount();
	if( pageCount <= 0 ) {
		m_pageWindowStart = 0;
		return;
	}
	if( m_pageIndex < m_pageWindowStart )
		m_pageWindowStart = m_pageIndex;
	if( m_pageIndex >= m_pageWindowStart + kPageSlotCount )
		m_pageWindowStart = m_pageIndex - (kPageSlotCount - 1);
	if( m_pageWindowStart < 0 )
		m_pageWindowStart = 0;
}

void CMainWnd::GotoPageIndex(int pageIndex)
{
	const int pageCount = GetPageCount();
	if( pageCount <= 0 ) return;
	if( pageIndex < 0 ) pageIndex = 0;
	if( pageIndex >= pageCount ) pageIndex = pageCount - 1;
	if( pageIndex == m_pageIndex ) {
		UpdatePageControl();
		return;
	}
	m_pageIndex = pageIndex;
	RefreshCharPage();
}

void CMainWnd::OnPageGo()
{
	if( !m_pEditPageGoto ) return;

	const int pageCount = GetPageCount();
	if( pageCount <= 0 ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("当前没有可跳转的页"));
		return;
	}

	CDuiString text = m_pEditPageGoto->GetText();
	text.TrimLeft();
	text.TrimRight();
	if( text.IsEmpty() ) {
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("请输入页码"));
		return;
	}

	// 仅允许纯数字页码
	for( int i = 0; i < text.GetLength(); ++i ) {
		const TCHAR ch = text.GetAt(i);
		if( ch < _T('0') || ch > _T('9') ) {
			CMessageBox::ShowInfo(GetHWND(), _T("提示"), _T("页码必须是正整数"));
			return;
		}
	}

	const int pageNo = _ttoi(text.GetData());
	if( pageNo < 1 || pageNo > pageCount ) {
		CDuiString msg;
		msg.Format(_T("页码无效，请输入 1～%d 之间的整数"), pageCount);
		CMessageBox::ShowInfo(GetHWND(), _T("提示"), msg.GetData());
		return;
	}

	GotoPageIndex(pageNo - 1);
}

void CMainWnd::UpdatePageControl()
{
	SyncPageWindow();
	const int pageCount = GetPageCount();

	if( m_pPageInfo ) {
		CDuiString info;
		if( m_currentFace.empty() ) {
			info = _T("请选择字体");
		} else if( pageCount <= 0 ) {
			info.Format(_T("共 %d 字 · 无匹配"), (int)m_allChars.size());
		} else {
			info.Format(_T("共 %d 字 · 第 %d / %d 页"),
				(int)m_filteredChars.size(), m_pageIndex + 1, pageCount);
		}
		m_pPageInfo->SetText(info.GetData());
	}

	if( m_pBtnPagePrev )
		m_pBtnPagePrev->SetEnabled(pageCount > 0 && m_pageIndex > 0);
	if( m_pBtnPageNext )
		m_pBtnPageNext->SetEnabled(pageCount > 0 && m_pageIndex + 1 < pageCount);
	if( m_pBtnPageGo )
		m_pBtnPageGo->SetEnabled(pageCount > 0);
	if( m_pEditPageGoto )
		m_pEditPageGoto->SetEnabled(pageCount > 0);

	for( int i = 0; i < kPageSlotCount; ++i ) {
		CButtonUI* pBtn = m_pBtnPageSlots[i];
		if( !pBtn ) continue;

		const int pageNo = m_pageWindowStart + i + 1; // 1-based
		const bool exists = (pageCount > 0 && pageNo <= pageCount);
		CDuiString text;
		text.Format(_T("%d"), pageNo);
		pBtn->SetText(text.GetData());
		pBtn->SetEnabled(exists);
		pBtn->SetTag(exists ? (UINT_PTR)(pageNo - 1) : (UINT_PTR)-1);

		const bool selected = exists && (pageNo - 1 == m_pageIndex);
		if( selected ) {
			pBtn->SetAttribute(_T("color"), _T("var(--color-primary)"));
			pBtn->SetAttribute(_T("font-weight"), _T("bold"));
		} else if( exists ) {
			pBtn->SetAttribute(_T("color"), _T("var(--color-text)"));
			pBtn->SetAttribute(_T("font-weight"), _T("normal"));
		} else {
			pBtn->SetAttribute(_T("color"), _T("var(--color-text-secondary)"));
			pBtn->SetAttribute(_T("font-weight"), _T("normal"));
		}
	}
}

void CMainWnd::OnClick(TNotifyUI& msg)
{
	CDuiString name = msg.pSender->GetName();
	if( name == _T("btn_open_font") ) {
		OnOpenFontFile();
		return;
	}
	if( name == _T("btn_refresh_font") ) {
		FontCatalog::Instance().RefreshSystemFonts();
		FontCatalog::Instance().ReloadBundledFonts();
		RefreshFontList(GetSelectedFontIndex());
		SetStatusMessage(_T("已刷新字体列表"));
		return;
	}
	if( name == _T("btn_zoom_in") ) {
		AdjustGlyphZoom(kGlyphFontStep);
		return;
	}
	if( name == _T("btn_zoom_out") ) {
		AdjustGlyphZoom(-kGlyphFontStep);
		return;
	}
	if( name == _T("btn_page_prev") ) {
		GotoPageIndex(m_pageIndex - 1);
		return;
	}
	if( name == _T("btn_page_next") ) {
		GotoPageIndex(m_pageIndex + 1);
		return;
	}
	if( name == _T("btn_page_go") ) {
		OnPageGo();
		return;
	}
	if( name.Left(10) == _T("btn_page_n") ) {
		int slot = _ttoi(name.Mid(10).GetData());
		if( slot >= 0 && slot < kPageSlotCount && m_pBtnPageSlots[slot]
			&& m_pBtnPageSlots[slot]->IsEnabled() ) {
			GotoPageIndex((int)m_pBtnPageSlots[slot]->GetTag());
		}
		return;
	}
	if( IsGlyphCell(msg.pSender) ) {
		ShowCharInStatus((wchar_t)(unsigned short)msg.pSender->GetTag());
		return;
	}

	WindowImplBase::OnClick(msg);
}

void CMainWnd::OnItemSelect(TNotifyUI& msg)
{
	if( msg.pSender == m_pFontList || (msg.pSender && msg.pSender->GetName() == _T("list_font")) )
		OnFontSelected();
}

void CMainWnd::OnTextChanged(TNotifyUI& msg)
{
	CDuiString name = msg.pSender->GetName();
	if( name == _T("filter_font") )
		RefreshFontList();
	else if( name == _T("filter_char") )
		ApplyCharFilter();
}

void CMainWnd::OnOpenFontFile()
{
	wchar_t fileBuf[MAX_PATH] = {};
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetHWND();
	ofn.lpstrFile = fileBuf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = L"Font Files\0*.ttf;*.otf;*.ttc;*.otc\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	ofn.lpstrTitle = L"选择字体文件";

	if( !::GetOpenFileNameW(&ofn) )
		return;

	std::wstring face;
	if( !FontCatalog::Instance().AddFontFile(fileBuf, &face) ) {
		SetStatusMessage(_T("加载字体文件失败"));
		::MessageBoxW(GetHWND(), L"无法加载该字体文件。", L"FontSVG", MB_OK | MB_ICONWARNING);
		return;
	}

	if( m_pFontCat )
		m_pFontCat->SetSelectedValue(_T("bundled"));

	const std::vector<FontEntry>& fonts = FontCatalog::Instance().Fonts();
	int selectIndex = -1;
	for( size_t i = 0; i < fonts.size(); ++i ) {
		if( fonts[i].isFileFont && _wcsicmp(fonts[i].filePath.c_str(), fileBuf) == 0 ) {
			selectIndex = (int)i;
			break;
		}
	}

	RefreshFontList(selectIndex);
	CDuiString msg;
	msg.Format(_T("已加载字体文件：%s"), face.c_str());
	SetStatusMessage(msg.GetData());
}
