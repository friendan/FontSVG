#pragma once

#include <string>
#include <vector>

/// SVG path（M/L/Q/C/Z）解析与变形，供轮廓编辑
class GlyphPath
{
public:
	enum CmdType { MoveTo = 0, LineTo = 1, QuadTo = 2, CubicTo = 3, Close = 4 };

	struct Cmd
	{
		CmdType type = Close;
		double x[3] = {};
		double y[3] = {};
		int n = 0; // 有效点数
	};

	struct EditPoint
	{
		size_t cmdIndex = 0;
		int ptIndex = 0; // 0..n-1
		double x = 0;
		double y = 0;
		bool isControl = false;
	};

	bool ParseFromSvg(const std::string& svgUtf8);
	bool ParseD(const std::string& d);
	std::string ToD() const;
	std::string ToSvgUtf8(const char* fill = "currentColor") const;

	void Clear();
	bool Empty() const { return m_cmds.empty(); }
	const std::vector<Cmd>& Cmds() const { return m_cmds; }

	void GetBounds(double& minX, double& minY, double& maxX, double& maxY) const;
	double MaxExtent() const;

	void CollectEditPoints(std::vector<EditPoint>& out) const;
	bool SetEditPoint(size_t editIndex, double x, double y);

	/// 基于本路径生成随机变体（amount 0~3，越大越夸张）
	GlyphPath MakeRandomVariant(unsigned seed, double amount) const;
	void ResetFrom(const GlyphPath& other);

private:
	std::vector<Cmd> m_cmds;

	static bool ParseNumber(const char*& p, const char* end, double& out);
	static void SkipSep(const char*& p, const char* end);
	void TrackBounds(double x, double y, double& minX, double& minY, double& maxX, double& maxY) const;
};
