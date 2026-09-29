#include "StdAfx.h"
#include "Logger.h"
#include "UIlib.h"

#include <stdio.h>
#include <stdarg.h>
#include <ShlObj.h>

using namespace DuiLib;

Logger& Logger::Instance()
{
	static Logger inst;
	return inst;
}

Logger::Logger()
	: m_enabled(false)
	, m_debugEnabled(FONTSVG_LOG_DEBUG_DEFAULT != 0)
	, m_verboseEnabled(FONTSVG_LOG_VERBOSE_DEFAULT != 0)
	, m_fp(NULL)
	, m_dayKey(0)
	, m_lockReady(false)
{
	memset(&m_lock, 0, sizeof(m_lock));
	if( m_debugEnabled || m_verboseEnabled )
		m_enabled = true;
}

Logger::~Logger()
{
	if( m_fp != NULL ) {
		fflush(m_fp);
		fclose(m_fp);
		m_fp = NULL;
	}
	if( m_lockReady )
		::DeleteCriticalSection(&m_lock);
}

void Logger::EnsureLock()
{
	if( m_lockReady ) return;
	::InitializeCriticalSection(&m_lock);
	m_lockReady = true;
}

std::wstring Logger::LogDir()
{
	CDuiString path = CPaintManagerUI::GetInstancePath();
	return std::wstring(path.GetData()) + L"logs";
}

const wchar_t* Logger::LevelName(LogLevel level)
{
	switch( level ) {
	case LogLevel::Verbose: return L"VERBOSE";
	case LogLevel::Debug:   return L"DEBUG";
	case LogLevel::Info:    return L"INFO";
	case LogLevel::Warn:    return L"WARN";
	case LogLevel::Error:   return L"ERROR";
	}
	return L"INFO";
}

void Logger::SetEnabled(bool enabled)
{
	EnsureLock();
	::EnterCriticalSection(&m_lock);
	m_enabled = enabled;
	if( !enabled && m_fp != NULL ) {
		fflush(m_fp);
		fclose(m_fp);
		m_fp = NULL;
		m_dayKey = 0;
	}
	::LeaveCriticalSection(&m_lock);
}

void Logger::SetDebugEnabled(bool enabled)
{
	EnsureLock();
	::EnterCriticalSection(&m_lock);
	m_debugEnabled = enabled;
	if( enabled )
		m_enabled = true;
	::LeaveCriticalSection(&m_lock);
}

void Logger::SetVerboseEnabled(bool enabled)
{
	EnsureLock();
	::EnterCriticalSection(&m_lock);
	m_verboseEnabled = enabled;
	if( enabled ) {
		m_debugEnabled = true;
		m_enabled = true;
	}
	::LeaveCriticalSection(&m_lock);
}

bool Logger::ShouldWrite(LogLevel level) const
{
	if( !m_enabled ) return false;
	switch( level ) {
	case LogLevel::Verbose: return m_verboseEnabled;
	case LogLevel::Debug:   return m_debugEnabled;
	default:                return true;
	}
}

void Logger::EnsureLogFile()
{
	SYSTEMTIME st = {};
	::GetLocalTime(&st);
	const int dayKey = st.wYear * 10000 + st.wMonth * 100 + st.wDay;
	if( m_fp != NULL && m_dayKey == dayKey )
		return;

	if( m_fp != NULL ) {
		fclose(m_fp);
		m_fp = NULL;
	}

	std::wstring dir = LogDir();
	::SHCreateDirectoryExW(NULL, dir.c_str(), NULL);

	wchar_t filePath[MAX_PATH] = {};
	_snwprintf_s(filePath, _TRUNCATE, L"%s\\%04d%02d%02d.log",
		dir.c_str(), (int)st.wYear, (int)st.wMonth, (int)st.wDay);

	m_fp = _wfsopen(filePath, L"a+, ccs=UTF-8", _SH_DENYNO);
	m_dayKey = (m_fp != NULL) ? dayKey : 0;
}

void Logger::Write(LogLevel level, const wchar_t* text)
{
	if( text == NULL || !ShouldWrite(level) ) return;

	EnsureLock();
	::EnterCriticalSection(&m_lock);
	if( !ShouldWrite(level) ) {
		::LeaveCriticalSection(&m_lock);
		return;
	}

	SYSTEMTIME st = {};
	::GetLocalTime(&st);
	wchar_t line[1400] = {};
	_snwprintf_s(line, _TRUNCATE, L"%02d:%02d:%02d.%03d [%s] %s",
		(int)st.wHour, (int)st.wMinute, (int)st.wSecond, (int)st.wMilliseconds,
		LevelName(level), text);

	EnsureLogFile();
	if( m_fp != NULL ) {
		fputws(line, m_fp);
		fputws(L"\n", m_fp);
		fflush(m_fp);
	}
	::LeaveCriticalSection(&m_lock);
}

void Logger::Writef(LogLevel level, const wchar_t* fmt, ...)
{
	if( fmt == NULL || !ShouldWrite(level) ) return;

	wchar_t buf[1024] = {};
	va_list ap;
	va_start(ap, fmt);
	_vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
	va_end(ap);
	Write(level, buf);
}
