#include "StdAfx.h"
#include "FontCatalog.h"

namespace {

	bool IsControlOrNonChar(wchar_t ch)
	{
		if( ch <= 0x20 ) return true; // 含空格：首格空白会看起来像预览失败
		if( ch >= 0x7F && ch <= 0x9F ) return true;
		if( ch == 0xA0 || ch == 0xAD ) return true; // NBSP / soft hyphen
		if( ch >= 0x2000 && ch <= 0x200F ) return true; // 各类空格 / 格式符
		if( ch >= 0x2028 && ch <= 0x202F ) return true;
		if( ch == 0x205F || ch == 0x2060 || ch == 0xFEFF ) return true;
		if( ch == 0x3000 ) return true; // ideographic space
		return false;
	}

} // namespace

FontCatalog& FontCatalog::Instance()
{
	static FontCatalog inst;
	return inst;
}

FontCatalog::FontCatalog()
{
}

FontCatalog::~FontCatalog()
{
	ClearPrivateFonts();
}

int CALLBACK FontCatalog::EnumFaceProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam)
{
	if( lf == NULL || lParam == 0 ) return 1;
	if( lf->lfFaceName[0] == 0 ) return 1;
	if( lf->lfFaceName[0] == L'@' ) return 1; // 竖排字体跳过

	auto* names = reinterpret_cast<std::set<std::wstring>*>(lParam);
	names->insert(lf->lfFaceName);
	return 1;
}

std::wstring FontCatalog::GetExeDir()
{
	wchar_t szPath[MAX_PATH] = {};
	DWORD n = ::GetModuleFileNameW(NULL, szPath, MAX_PATH);
	if( n == 0 || n >= MAX_PATH ) return std::wstring();
	wchar_t* pSlash = wcsrchr(szPath, L'\\');
	if( pSlash == NULL ) pSlash = wcsrchr(szPath, L'/');
	if( pSlash != NULL ) *pSlash = L'\0';
	return szPath;
}

bool FontCatalog::IsFontFilePath(const std::wstring& path)
{
	size_t dot = path.find_last_of(L'.');
	if( dot == std::wstring::npos || dot + 1 >= path.size() )
		return false;
	std::wstring ext = path.substr(dot + 1);
	for( size_t i = 0; i < ext.size(); ++i )
		ext[i] = (wchar_t)towlower(ext[i]);
	return ext == L"ttf" || ext == L"otf" || ext == L"ttc"
		|| ext == L"otc" || ext == L"fon";
}

void FontCatalog::RefreshSystemFonts()
{
	std::vector<FontEntry> keepFiles;
	for( size_t i = 0; i < m_fonts.size(); ++i ) {
		if( m_fonts[i].isFileFont )
			keepFiles.push_back(m_fonts[i]);
	}

	m_fonts.clear();

	std::set<std::wstring> names;
	HDC hdc = ::GetDC(NULL);
	if( hdc ) {
		LOGFONTW lf = {};
		lf.lfCharSet = DEFAULT_CHARSET;
		::EnumFontFamiliesExW(hdc, &lf, EnumFaceProc, (LPARAM)&names, 0);
		::ReleaseDC(NULL, hdc);
	}

	m_fonts.reserve(names.size() + keepFiles.size());
	for( const auto& name : names ) {
		FontEntry e;
		e.faceName = name;
		e.isFileFont = false;
		m_fonts.push_back(e);
	}
	for( size_t i = 0; i < keepFiles.size(); ++i )
		m_fonts.push_back(keepFiles[i]);

	SortFonts();
}

void FontCatalog::RemoveBundledFonts()
{
	std::vector<FontEntry> keep;
	keep.reserve(m_fonts.size());
	for( size_t i = 0; i < m_fonts.size(); ++i ) {
		if( !m_fonts[i].isBundled ) {
			keep.push_back(m_fonts[i]);
			continue;
		}
		const std::wstring& path = m_fonts[i].filePath;
		if( !path.empty() ) {
			::RemoveFontResourceExW(path.c_str(), FR_PRIVATE, NULL);
			for( size_t j = 0; j < m_privatePaths.size(); ++j ) {
				if( _wcsicmp(m_privatePaths[j].c_str(), path.c_str()) == 0 ) {
					m_privatePaths.erase(m_privatePaths.begin() + (ptrdiff_t)j);
					break;
				}
			}
		}
	}
	m_fonts.swap(keep);
}

void FontCatalog::LoadBundledFonts()
{
	std::wstring dir = GetExeDir();
	if( dir.empty() ) return;
	dir += L"\\font";

	DWORD attr = ::GetFileAttributesW(dir.c_str());
	if( attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0 )
		return;

	std::wstring pattern = dir + L"\\*";
	WIN32_FIND_DATAW fd = {};
	HANDLE hFind = ::FindFirstFileW(pattern.c_str(), &fd);
	if( hFind == INVALID_HANDLE_VALUE )
		return;

	do {
		if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			continue;
		std::wstring name = fd.cFileName;
		if( name.empty() || name[0] == L'.' )
			continue;
		std::wstring path = dir + L"\\" + name;
		if( !IsFontFilePath(path) )
			continue;
		AddFontFile(path, NULL, true);
	} while( ::FindNextFileW(hFind, &fd) );

	::FindClose(hFind);
}

void FontCatalog::ReloadBundledFonts()
{
	RemoveBundledFonts();
	LoadBundledFonts();
}

bool FontCatalog::AddFontFile(const std::wstring& path, std::wstring* outFaceName, bool bundled)
{
	if( path.empty() ) return false;

	for( size_t i = 0; i < m_fonts.size(); ++i ) {
		if( m_fonts[i].isFileFont && _wcsicmp(m_fonts[i].filePath.c_str(), path.c_str()) == 0 ) {
			if( bundled )
				m_fonts[i].isBundled = true;
			if( outFaceName ) *outFaceName = m_fonts[i].faceName;
			return true;
		}
	}

	std::set<std::wstring> before;
	HDC hdc = ::GetDC(NULL);
	if( hdc ) {
		LOGFONTW lf = {};
		lf.lfCharSet = DEFAULT_CHARSET;
		::EnumFontFamiliesExW(hdc, &lf, EnumFaceProc, (LPARAM)&before, 0);
		::ReleaseDC(NULL, hdc);
	}

	int added = ::AddFontResourceExW(path.c_str(), FR_PRIVATE, NULL);
	if( added <= 0 )
		return false;

	m_privatePaths.push_back(path);

	std::set<std::wstring> after;
	hdc = ::GetDC(NULL);
	if( hdc ) {
		LOGFONTW lf = {};
		lf.lfCharSet = DEFAULT_CHARSET;
		::EnumFontFamiliesExW(hdc, &lf, EnumFaceProc, (LPARAM)&after, 0);
		::ReleaseDC(NULL, hdc);
	}

	std::wstring face;
	for( const auto& name : after ) {
		if( before.find(name) == before.end() ) {
			face = name;
			break;
		}
	}

	if( face.empty() ) {
		size_t slash = path.find_last_of(L"\\/");
		face = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	}

	FontEntry e;
	e.faceName = face;
	e.filePath = path;
	e.isFileFont = true;
	e.isBundled = bundled;
	m_fonts.push_back(e);
	SortFonts();

	if( outFaceName ) *outFaceName = face;
	return true;
}

void FontCatalog::ClearPrivateFonts()
{
	for( size_t i = 0; i < m_privatePaths.size(); ++i )
		::RemoveFontResourceExW(m_privatePaths[i].c_str(), FR_PRIVATE, NULL);
	m_privatePaths.clear();
}

const FontEntry* FontCatalog::GetAt(size_t index) const
{
	if( index >= m_fonts.size() ) return NULL;
	return &m_fonts[index];
}

void FontCatalog::SortFonts()
{
	std::sort(m_fonts.begin(), m_fonts.end(), [](const FontEntry& a, const FontEntry& b) {
		if( a.isFileFont != b.isFileFont )
			return a.isFileFont; // 文件字体靠前
		if( a.isBundled != b.isBundled )
			return a.isBundled; // 自带字体靠前
		return _wcsicmp(a.faceName.c_str(), b.faceName.c_str()) < 0;
	});
}

bool FontCatalog::CollectCodepoints(const std::wstring& faceName, std::vector<wchar_t>& out) const
{
	out.clear();
	if( faceName.empty() ) return false;

	HDC hdc = ::CreateCompatibleDC(NULL);
	if( hdc == NULL ) return false;

	LOGFONTW lf = {};
	lf.lfHeight = -32;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcsncpy_s(lf.lfFaceName, faceName.c_str(), _TRUNCATE);

	HFONT hFont = ::CreateFontIndirectW(&lf);
	if( hFont == NULL ) {
		::DeleteDC(hdc);
		return false;
	}

	HGDIOBJ old = ::SelectObject(hdc, hFont);
	DWORD cb = ::GetFontUnicodeRanges(hdc, NULL);
	if( cb == 0 ) {
		::SelectObject(hdc, old);
		::DeleteObject(hFont);
		::DeleteDC(hdc);
		return false;
	}

	std::vector<BYTE> buf(cb);
	GLYPHSET* pgs = reinterpret_cast<GLYPHSET*>(buf.data());
	if( ::GetFontUnicodeRanges(hdc, pgs) == 0 ) {
		::SelectObject(hdc, old);
		::DeleteObject(hFont);
		::DeleteDC(hdc);
		return false;
	}

	out.reserve(pgs->cGlyphsSupported);
	for( DWORD i = 0; i < pgs->cRanges; ++i ) {
		const WCRANGE& r = pgs->ranges[i];
		for( WCHAR c = 0; c < r.cGlyphs; ++c ) {
			wchar_t ch = (wchar_t)(r.wcLow + c);
			if( IsControlOrNonChar(ch) ) continue;
			out.push_back(ch);
		}
	}

	::SelectObject(hdc, old);
	::DeleteObject(hFont);
	::DeleteDC(hdc);
	return !out.empty();
}
