#pragma once

#include <string>
#include <vector>

struct FontEntry
{
	std::wstring faceName;
	std::wstring filePath; // 空表示系统字体
	bool isFileFont = false;
	bool isBundled = false; // 来自 exe 目录下 font/
};

class FontCatalog
{
public:
	static FontCatalog& Instance();

	void RefreshSystemFonts();
	/// 扫描 exe\font，加载其中的字体文件（跳过非字体）
	void LoadBundledFonts();
	/// 卸载并重新扫描自带字体
	void ReloadBundledFonts();
	bool AddFontFile(const std::wstring& path, std::wstring* outFaceName = NULL, bool bundled = false);
	void ClearPrivateFonts();

	const std::vector<FontEntry>& Fonts() const { return m_fonts; }
	const FontEntry* GetAt(size_t index) const;

	/// 枚举字体中的 Unicode 码位（BMP，跳过控制字符）
	bool CollectCodepoints(const std::wstring& faceName, std::vector<wchar_t>& out) const;

private:
	FontCatalog();
	~FontCatalog();
	FontCatalog(const FontCatalog&) = delete;
	FontCatalog& operator=(const FontCatalog&) = delete;

	void SortFonts();
	void RemoveBundledFonts();
	static bool IsFontFilePath(const std::wstring& path);
	static std::wstring GetExeDir();
	static int CALLBACK EnumFaceProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam);

	std::vector<FontEntry> m_fonts;
	std::vector<std::wstring> m_privatePaths;
};
