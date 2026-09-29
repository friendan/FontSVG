#pragma once

#include <stdio.h>
#include <string>

enum class LogLevel
{
	Verbose = 0, // 高频（如每帧/每次 Refresh）
	Debug,
	Info,
	Warn,
	Error
};

/// 简易日志：默认关闭；开启后写入 {exe}\logs\YYYYMMDD.log。
/// 调试预览/布局：SetDebugEnabled(true)；刷屏级再 SetVerboseEnabled(true)。
class Logger
{
public:
	static Logger& Instance();

	/// 总开关（Info/Warn/Error 及写文件）
	void SetEnabled(bool enabled);
	bool IsEnabled() const { return m_enabled; }

	/// Debug 级别（低频诊断）；打开时自动打开总开关
	void SetDebugEnabled(bool enabled);
	bool IsDebugEnabled() const { return m_debugEnabled; }

	/// Verbose 级别（高频）；打开时自动打开 Debug + 总开关
	void SetVerboseEnabled(bool enabled);
	bool IsVerboseEnabled() const { return m_verboseEnabled; }

	static std::wstring LogDir();

	void Write(LogLevel level, const wchar_t* text);
	void Writef(LogLevel level, const wchar_t* fmt, ...);

private:
	Logger();
	~Logger();
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	void EnsureLock();
	void EnsureLogFile();
	bool ShouldWrite(LogLevel level) const;
	static const wchar_t* LevelName(LogLevel level);

	bool m_enabled;
	bool m_debugEnabled;
	bool m_verboseEnabled;
	FILE* m_fp;
	int m_dayKey;
	CRITICAL_SECTION m_lock;
	bool m_lockReady;
};

// 编译期默认（改 1 可免改代码即打开；也可运行时 Set*Enabled）
#ifndef FONTSVG_LOG_DEBUG_DEFAULT
#define FONTSVG_LOG_DEBUG_DEFAULT 0
#endif
#ifndef FONTSVG_LOG_VERBOSE_DEFAULT
#define FONTSVG_LOG_VERBOSE_DEFAULT 0
#endif

#define LOG_VERBOSE(msg)  Logger::Instance().Write(LogLevel::Verbose, msg)
#define LOG_DEBUG(msg)    Logger::Instance().Write(LogLevel::Debug, msg)
#define LOG_INFO(msg)     Logger::Instance().Write(LogLevel::Info, msg)
#define LOG_WARN(msg)     Logger::Instance().Write(LogLevel::Warn, msg)
#define LOG_ERROR(msg)    Logger::Instance().Write(LogLevel::Error, msg)

#define LOG_VERBOSEF(...) Logger::Instance().Writef(LogLevel::Verbose, __VA_ARGS__)
#define LOG_DEBUGF(...)   Logger::Instance().Writef(LogLevel::Debug, __VA_ARGS__)
#define LOG_INFOF(...)    Logger::Instance().Writef(LogLevel::Info, __VA_ARGS__)
#define LOG_WARNF(...)    Logger::Instance().Writef(LogLevel::Warn, __VA_ARGS__)
#define LOG_ERRORF(...)   Logger::Instance().Writef(LogLevel::Error, __VA_ARGS__)
