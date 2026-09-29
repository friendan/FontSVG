#include "StdAfx.h"
#include "GlyphSvg.h"
#include <cmath>
#include <sstream>
#include <vector>

namespace {

double FixedToDouble(const FIXED& f)
{
	return (double)f.value + (double)f.fract / 65536.0;
}

void AppendNum(std::ostringstream& os, double v)
{
	char buf[64];
	sprintf_s(buf, "%.4f", v);
	os << buf;
}

bool OutlineToPath(const BYTE* pData, DWORD cb, std::string& pathOut,
	double& minX, double& minY, double& maxX, double& maxY)
{
	pathOut.clear();
	minX = minY = 1e300;
	maxX = maxY = -1e300;
	if( pData == NULL || cb == 0 ) return false;

	std::ostringstream os;
	const BYTE* p = pData;
	const BYTE* pEnd = pData + cb;

	auto track = [&](double x, double y) {
		if( x < minX ) minX = x;
		if( y < minY ) minY = y;
		if( x > maxX ) maxX = x;
		if( y > maxY ) maxY = y;
	};

	while( p + sizeof(TTPOLYGONHEADER) <= pEnd ) {
		const TTPOLYGONHEADER* th = (const TTPOLYGONHEADER*)p;
		if( th->cb < sizeof(TTPOLYGONHEADER) || p + th->cb > pEnd )
			break;

		const BYTE* polyEnd = p + th->cb;
		const BYTE* cur = p + sizeof(TTPOLYGONHEADER);

		double x = FixedToDouble(th->pfxStart.x);
		double y = -FixedToDouble(th->pfxStart.y); // GDI Y↑ → SVG Y↓
		track(x, y);
		os << "M";
		AppendNum(os, x);
		os << " ";
		AppendNum(os, y);

		while( cur + sizeof(TTPOLYCURVE) <= polyEnd ) {
			const TTPOLYCURVE* pc = (const TTPOLYCURVE*)cur;
			const size_t need = sizeof(WORD) * 2 + sizeof(POINTFX) * pc->cpfx;
			if( cur + need > polyEnd ) break;

			if( pc->wType == TT_PRIM_LINE ) {
				for( WORD i = 0; i < pc->cpfx; ++i ) {
					x = FixedToDouble(pc->apfx[i].x);
					y = -FixedToDouble(pc->apfx[i].y);
					track(x, y);
					os << "L";
					AppendNum(os, x);
					os << " ";
					AppendNum(os, y);
				}
			}
			else if( pc->wType == TT_PRIM_QSPLINE ) {
				for( WORD u = 0; u + 1 < pc->cpfx; ++u ) {
					POINTFX b = pc->apfx[u];
					POINTFX c = pc->apfx[u + 1];
					if( u + 2 < pc->cpfx ) {
						// 连续 off-curve：插入中点作为 on-curve
						*(int*)&c.x = (*(int*)&b.x + *(int*)&c.x) / 2;
						*(int*)&c.y = (*(int*)&b.y + *(int*)&c.y) / 2;
					}
					const double bx = FixedToDouble(b.x);
					const double by = -FixedToDouble(b.y);
					const double cx = FixedToDouble(c.x);
					const double cy = -FixedToDouble(c.y);
					track(bx, by);
					track(cx, cy);
					os << "Q";
					AppendNum(os, bx);
					os << " ";
					AppendNum(os, by);
					os << " ";
					AppendNum(os, cx);
					os << " ";
					AppendNum(os, cy);
					x = cx;
					y = cy;
				}
			}
			else if( pc->wType == TT_PRIM_CSPLINE ) {
				for( WORD i = 0; i + 2 < pc->cpfx; i += 3 ) {
					const double x1 = FixedToDouble(pc->apfx[i].x);
					const double y1 = -FixedToDouble(pc->apfx[i].y);
					const double x2 = FixedToDouble(pc->apfx[i + 1].x);
					const double y2 = -FixedToDouble(pc->apfx[i + 1].y);
					const double x3 = FixedToDouble(pc->apfx[i + 2].x);
					const double y3 = -FixedToDouble(pc->apfx[i + 2].y);
					track(x1, y1);
					track(x2, y2);
					track(x3, y3);
					os << "C";
					AppendNum(os, x1);
					os << " ";
					AppendNum(os, y1);
					os << " ";
					AppendNum(os, x2);
					os << " ";
					AppendNum(os, y2);
					os << " ";
					AppendNum(os, x3);
					os << " ";
					AppendNum(os, y3);
					x = x3;
					y = y3;
				}
			}

			cur += need;
		}

		os << "Z";
		p += th->cb;
	}

	pathOut = os.str();
	return !pathOut.empty() && maxX >= minX && maxY >= minY;
}

} // namespace

namespace GlyphSvg {

bool BuildUtf8(const std::wstring& faceName, wchar_t ch, std::string& outSvg)
{
	outSvg.clear();
	if( faceName.empty() || ch == 0 ) return false;

	HDC hdc = ::CreateCompatibleDC(NULL);
	if( hdc == NULL ) return false;

	LOGFONTW lf = {};
	lf.lfHeight = -4096; // 高精度轮廓，与最终图标像素尺寸无关
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = NONANTIALIASED_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcsncpy_s(lf.lfFaceName, faceName.c_str(), _TRUNCATE);

	HFONT hFont = ::CreateFontIndirectW(&lf);
	if( hFont == NULL ) {
		::DeleteDC(hdc);
		return false;
	}

	HGDIOBJ old = ::SelectObject(hdc, hFont);
	MAT2 mat = { {0,1}, {0,0}, {0,0}, {0,1} };
	GLYPHMETRICS gm = {};
	DWORD need = ::GetGlyphOutlineW(hdc, (UINT)ch, GGO_NATIVE | GGO_UNHINTED, &gm, 0, NULL, &mat);
	if( need == GDI_ERROR || need == 0 ) {
		::SelectObject(hdc, old);
		::DeleteObject(hFont);
		::DeleteDC(hdc);
		return false;
	}

	std::vector<BYTE> buf(need);
	DWORD got = ::GetGlyphOutlineW(hdc, (UINT)ch, GGO_NATIVE | GGO_UNHINTED, &gm, need, buf.data(), &mat);
	::SelectObject(hdc, old);
	::DeleteObject(hFont);
	::DeleteDC(hdc);
	if( got == GDI_ERROR || got == 0 ) return false;

	std::string path;
	double minX = 0, minY = 0, maxX = 0, maxY = 0;
	if( !OutlineToPath(buf.data(), got, path, minX, minY, maxX, maxY) )
		return false;

	const double gw = maxX - minX;
	const double gh = maxY - minY;
	if( gw <= 0.0 || gh <= 0.0 ) return false;

	// 图标边距宜小：0.12 会让字只占约 80% 画布，缩到 16/24 更糊
	const double pad = (std::max)(gw, gh) * 0.04;
	const double box = (std::max)(gw, gh) + pad * 2.0;
	const double ox = minX - (box - gw) * 0.5;
	const double oy = minY - (box - gh) * 0.5;

	std::ostringstream svg;
	svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		<< "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"";
	AppendNum(svg, ox);
	svg << " ";
	AppendNum(svg, oy);
	svg << " ";
	AppendNum(svg, box);
	svg << " ";
	AppendNum(svg, box);
	svg << "\">"
		<< "<path fill=\"currentColor\" stroke=\"none\" d=\"" << path << "\"/>"
		<< "</svg>";
	outSvg = svg.str();
	return true;
}

bool BuildUtf8ForFile(const std::wstring& faceName, wchar_t ch, std::string& outSvg)
{
	if( !BuildUtf8(faceName, ch, outSvg) ) return false;
	const char* from = "currentColor";
	const char* to = "#000000";
	size_t pos = 0;
	while( (pos = outSvg.find(from, pos)) != std::string::npos ) {
		outSvg.replace(pos, strlen(from), to);
		pos += strlen(to);
	}
	return true;
}

void ColorToRgbHex(DWORD color, char out[8])
{
	sprintf_s(out, 8, "%02X%02X%02X",
		(unsigned)DuiColorR(color),
		(unsigned)DuiColorG(color),
		(unsigned)DuiColorB(color));
}

bool BuildAvatarUtf8(const std::wstring& faceName, wchar_t ch,
	DWORD bgColor, DWORD fgColor, AvatarShape shape, double margin,
	std::string& outSvg)
{
	outSvg.clear();
	std::string glyph;
	if( !BuildUtf8(faceName, ch, glyph) ) return false;

	const size_t vbKey = glyph.find("viewBox=\"");
	if( vbKey == std::string::npos ) return false;
	const size_t vbStart = vbKey + 9;
	const size_t vbEnd = glyph.find('"', vbStart);
	if( vbEnd == std::string::npos || vbEnd <= vbStart ) return false;
	const std::string viewBox = glyph.substr(vbStart, vbEnd - vbStart);

	const size_t dKey = glyph.find(" d=\"");
	if( dKey == std::string::npos ) return false;
	const size_t dStart = dKey + 4;
	const size_t dEnd = glyph.find('"', dStart);
	if( dEnd == std::string::npos || dEnd <= dStart ) return false;
	const std::string pathD = glyph.substr(dStart, dEnd - dStart);

	if( margin < 0.05 ) margin = 0.05;
	if( margin > 0.40 ) margin = 0.40;
	const double m = margin * 100.0;
	const double inner = 100.0 - m * 2.0;

	char bgHex[8] = {};
	char fgHex[8] = {};
	ColorToRgbHex(bgColor, bgHex);
	ColorToRgbHex(fgColor, fgHex);

	std::ostringstream svg;
	svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		<< "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">";
	// Alpha=0：无底色，仅保留文字
	if( DuiColorA(bgColor) != 0 ) {
		if( shape == AvatarCircle ) {
			svg << "<circle cx=\"50\" cy=\"50\" r=\"50\" fill=\"#" << bgHex << "\"/>";
		} else {
			svg << "<rect width=\"100\" height=\"100\" rx=\"22\" ry=\"22\" fill=\"#" << bgHex << "\"/>";
		}
	}
	svg << "<svg x=\"";
	AppendNum(svg, m);
	svg << "\" y=\"";
	AppendNum(svg, m);
	svg << "\" width=\"";
	AppendNum(svg, inner);
	svg << "\" height=\"";
	AppendNum(svg, inner);
	svg << "\" viewBox=\"" << viewBox << "\">"
		<< "<path fill=\"#" << fgHex << "\" d=\"" << pathD << "\"/>"
		<< "</svg></svg>";
	outSvg = svg.str();
	return true;
}

bool WriteUtf8File(const std::wstring& path, const std::string& utf8)
{
	if( path.empty() || utf8.empty() ) return false;
	HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) return false;
	DWORD written = 0;
	const BOOL ok = ::WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, NULL);
	::CloseHandle(hFile);
	return ok && written == (DWORD)utf8.size();
}

CDuiString MakeIconBaseName(wchar_t ch)
{
	CDuiString s;
	s.Format(_T("U+%04X_"), (unsigned)(unsigned short)ch);

	// Windows 文件名非法字符：\ / : * ? " < > |
	const wchar_t illegal[] = L"\\/:*?\"<>|";
	wchar_t safe = ch;
	if( ch == 0 || wcschr(illegal, ch) != NULL || ch < 0x20 )
		safe = L'_';

	TCHAR chText[2] = { safe, 0 };
	s += chText;
	return s;
}

CDuiString GetDefaultExportDir()
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

} // namespace GlyphSvg
