#pragma once

#include <string>

/// 将字体字形轮廓转为 SVG（黑填充 path，便于 SvgBox 着色导出）
namespace GlyphSvg
{
	enum AvatarShape
	{
		AvatarCircle = 0,
		AvatarRounded = 1,
	};

	bool BuildUtf8(const std::wstring& faceName, wchar_t ch, std::string& outSvg);
	/// 导出到文件用：fill 为 #000000（可独立打开预览）
	bool BuildUtf8ForFile(const std::wstring& faceName, wchar_t ch, std::string& outSvg);
	/// 头像 SVG：底形 + 前景字，颜色写入文件（不依赖 currentColor）
	bool BuildAvatarUtf8(const std::wstring& faceName, wchar_t ch,
		DWORD bgColor, DWORD fgColor, AvatarShape shape, double margin,
		std::string& outSvg);
	bool WriteUtf8File(const std::wstring& path, const std::string& utf8);
	CDuiString MakeIconBaseName(wchar_t ch);
	CDuiString GetDefaultExportDir();
	void ColorToRgbHex(DWORD color, char out[8]);
}
