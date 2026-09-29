#include "StdAfx.h"
#include "GlyphPath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace {

double Rand01(unsigned& seed)
{
	seed = seed * 1664525u + 1013904223u;
	return (double)(seed & 0xFFFFFFu) / (double)0xFFFFFFu;
}

double RandRange(unsigned& seed, double a, double b)
{
	return a + (b - a) * Rand01(seed);
}

} // namespace

void GlyphPath::Clear()
{
	m_cmds.clear();
}

void GlyphPath::ResetFrom(const GlyphPath& other)
{
	m_cmds = other.m_cmds;
}

void GlyphPath::SkipSep(const char*& p, const char* end)
{
	while( p < end && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r' || *p == '\n') )
		++p;
}

bool GlyphPath::ParseNumber(const char*& p, const char* end, double& out)
{
	SkipSep(p, end);
	if( p >= end ) return false;
	char* e = NULL;
	out = strtod(p, &e);
	if( e == p ) return false;
	p = e;
	return true;
}

bool GlyphPath::ParseFromSvg(const std::string& svgUtf8)
{
	const size_t dKey = svgUtf8.find(" d=\"");
	if( dKey == std::string::npos ) return false;
	const size_t dStart = dKey + 4;
	const size_t dEnd = svgUtf8.find('"', dStart);
	if( dEnd == std::string::npos || dEnd <= dStart ) return false;
	return ParseD(svgUtf8.substr(dStart, dEnd - dStart));
}

bool GlyphPath::ParseD(const std::string& d)
{
	Clear();
	if( d.empty() ) return false;

	const char* p = d.c_str();
	const char* end = p + d.size();
	char cmd = 0;
	double cx = 0, cy = 0;

	while( p < end ) {
		SkipSep(p, end);
		if( p >= end ) break;

		if( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ) {
			cmd = *p++;
		}
		if( cmd == 0 ) break;

		if( cmd == 'Z' || cmd == 'z' ) {
			Cmd c;
			c.type = Close;
			m_cmds.push_back(c);
			continue;
		}

		if( cmd == 'M' || cmd == 'm' ) {
			double x = 0, y = 0;
			if( !ParseNumber(p, end, x) || !ParseNumber(p, end, y) ) break;
			if( cmd == 'm' ) { x += cx; y += cy; }
			Cmd c;
			c.type = MoveTo;
			c.n = 1;
			c.x[0] = x; c.y[0] = y;
			m_cmds.push_back(c);
			cx = x; cy = y;
			cmd = (cmd == 'm') ? 'l' : 'L'; // 后续隐式 L
			continue;
		}

		if( cmd == 'L' || cmd == 'l' ) {
			double x = 0, y = 0;
			if( !ParseNumber(p, end, x) || !ParseNumber(p, end, y) ) break;
			if( cmd == 'l' ) { x += cx; y += cy; }
			Cmd c;
			c.type = LineTo;
			c.n = 1;
			c.x[0] = x; c.y[0] = y;
			m_cmds.push_back(c);
			cx = x; cy = y;
			continue;
		}

		if( cmd == 'Q' || cmd == 'q' ) {
			double x1 = 0, y1 = 0, x = 0, y = 0;
			if( !ParseNumber(p, end, x1) || !ParseNumber(p, end, y1)
				|| !ParseNumber(p, end, x) || !ParseNumber(p, end, y) ) break;
			if( cmd == 'q' ) { x1 += cx; y1 += cy; x += cx; y += cy; }
			Cmd c;
			c.type = QuadTo;
			c.n = 2;
			c.x[0] = x1; c.y[0] = y1;
			c.x[1] = x;  c.y[1] = y;
			m_cmds.push_back(c);
			cx = x; cy = y;
			continue;
		}

		if( cmd == 'C' || cmd == 'c' ) {
			double x1 = 0, y1 = 0, x2 = 0, y2 = 0, x = 0, y = 0;
			if( !ParseNumber(p, end, x1) || !ParseNumber(p, end, y1)
				|| !ParseNumber(p, end, x2) || !ParseNumber(p, end, y2)
				|| !ParseNumber(p, end, x) || !ParseNumber(p, end, y) ) break;
			if( cmd == 'c' ) {
				x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy;
			}
			Cmd c;
			c.type = CubicTo;
			c.n = 3;
			c.x[0] = x1; c.y[0] = y1;
			c.x[1] = x2; c.y[1] = y2;
			c.x[2] = x;  c.y[2] = y;
			m_cmds.push_back(c);
			cx = x; cy = y;
			continue;
		}

		// 未知命令，跳过
		break;
	}

	return !m_cmds.empty();
}

std::string GlyphPath::ToD() const
{
	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(4);
	for( size_t i = 0; i < m_cmds.size(); ++i ) {
		const Cmd& c = m_cmds[i];
		switch( c.type ) {
		case MoveTo:
			os << "M" << c.x[0] << " " << c.y[0];
			break;
		case LineTo:
			os << "L" << c.x[0] << " " << c.y[0];
			break;
		case QuadTo:
			os << "Q" << c.x[0] << " " << c.y[0] << " " << c.x[1] << " " << c.y[1];
			break;
		case CubicTo:
			os << "C" << c.x[0] << " " << c.y[0] << " "
				<< c.x[1] << " " << c.y[1] << " "
				<< c.x[2] << " " << c.y[2];
			break;
		case Close:
			os << "Z";
			break;
		}
	}
	return os.str();
}

void GlyphPath::TrackBounds(double x, double y, double& minX, double& minY, double& maxX, double& maxY) const
{
	if( x < minX ) minX = x;
	if( y < minY ) minY = y;
	if( x > maxX ) maxX = x;
	if( y > maxY ) maxY = y;
}

void GlyphPath::GetBounds(double& minX, double& minY, double& maxX, double& maxY) const
{
	minX = minY = 1e300;
	maxX = maxY = -1e300;
	for( size_t i = 0; i < m_cmds.size(); ++i ) {
		const Cmd& c = m_cmds[i];
		for( int k = 0; k < c.n; ++k )
			TrackBounds(c.x[k], c.y[k], minX, minY, maxX, maxY);
	}
	if( maxX < minX ) {
		minX = minY = 0;
		maxX = maxY = 1;
	}
}

double GlyphPath::MaxExtent() const
{
	double minX, minY, maxX, maxY;
	GetBounds(minX, minY, maxX, maxY);
	const double w = maxX - minX;
	const double h = maxY - minY;
	return (w > h) ? w : h;
}

std::string GlyphPath::ToSvgUtf8(const char* fill) const
{
	double minX, minY, maxX, maxY;
	GetBounds(minX, minY, maxX, maxY);
	const double gw = maxX - minX;
	const double gh = maxY - minY;
	const double pad = ((gw > gh) ? gw : gh) * 0.04;
	const double box = ((gw > gh) ? gw : gh) + pad * 2.0;
	const double ox = minX - (box - gw) * 0.5;
	const double oy = minY - (box - gh) * 0.5;

	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(4);
	os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		<< "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\""
		<< ox << " " << oy << " " << box << " " << box << "\">"
		<< "<path fill=\"" << (fill ? fill : "currentColor") << "\" stroke=\"none\" d=\""
		<< ToD() << "\"/></svg>";
	return os.str();
}

void GlyphPath::CollectEditPoints(std::vector<EditPoint>& out) const
{
	out.clear();
	for( size_t i = 0; i < m_cmds.size(); ++i ) {
		const Cmd& c = m_cmds[i];
		for( int k = 0; k < c.n; ++k ) {
			EditPoint ep;
			ep.cmdIndex = i;
			ep.ptIndex = k;
			ep.x = c.x[k];
			ep.y = c.y[k];
			ep.isControl = (c.type == QuadTo && k == 0)
				|| (c.type == CubicTo && (k == 0 || k == 1));
			out.push_back(ep);
		}
	}
}

bool GlyphPath::SetEditPoint(size_t editIndex, double x, double y)
{
	std::vector<EditPoint> pts;
	CollectEditPoints(pts);
	if( editIndex >= pts.size() ) return false;
	const EditPoint& ep = pts[editIndex];
	if( ep.cmdIndex >= m_cmds.size() ) return false;
	Cmd& c = m_cmds[ep.cmdIndex];
	if( ep.ptIndex < 0 || ep.ptIndex >= c.n ) return false;
	c.x[ep.ptIndex] = x;
	c.y[ep.ptIndex] = y;
	return true;
}

GlyphPath GlyphPath::MakeRandomVariant(unsigned seed, double amount) const
{
	GlyphPath out;
	out.ResetFrom(*this);
	if( out.Empty() ) return out;
	if( amount < 0 ) amount = 0;
	if( amount > 3 ) amount = 3;

	double minX, minY, maxX, maxY;
	GetBounds(minX, minY, maxX, maxY);
	const double cx = (minX + maxX) * 0.5;
	const double cy = (minY + maxY) * 0.5;
	const double extent = MaxExtent();
	if( extent <= 0 ) return out;

	const double jitter = extent * (0.01 + amount * 0.08);
	const double scaleAmp = (std::min)(amount * 0.08, 0.28);
	const double scale = RandRange(seed, 1.0 - scaleAmp, 1.0 + scaleAmp);
	const double angleAmp = (std::min)(amount * 0.12, 0.42);
	const double angle = RandRange(seed, -angleAmp, angleAmp); // 弧度
	const double cosA = std::cos(angle);
	const double sinA = std::sin(angle);

	for( size_t i = 0; i < out.m_cmds.size(); ++i ) {
		Cmd& c = out.m_cmds[i];
		for( int k = 0; k < c.n; ++k ) {
			double x = c.x[k] - cx;
			double y = c.y[k] - cy;
			double rx = (x * cosA - y * sinA) * scale;
			double ry = (x * sinA + y * cosA) * scale;
			rx += RandRange(seed, -jitter, jitter);
			ry += RandRange(seed, -jitter, jitter);
			// 控制点扰动略大，外形变化更明显
			const bool ctrl = (c.type == QuadTo && k == 0)
				|| (c.type == CubicTo && (k == 0 || k == 1));
			if( ctrl ) {
				rx += RandRange(seed, -jitter, jitter);
				ry += RandRange(seed, -jitter, jitter);
			}
			c.x[k] = rx + cx;
			c.y[k] = ry + cy;
		}
	}
	return out;
}
