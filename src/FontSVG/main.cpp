#include "StdAfx.h"
#include "duilib.h"
#include "resource.h"

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hr = ::CoInitialize(NULL);
	if( FAILED(hr) ) return 0;
	::OleInitialize(NULL);

	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::SetResourceType(UILIB_ZIPRESOURCE);

	CDuiString path = CPaintManagerUI::GetInstancePath();
	path += _T("skin\\FontSVG\\");
	CPaintManagerUI::SetResourcePath(path.GetData());

	HRSRC hRes = ::FindResource(hInstance, MAKEINTRESOURCE(IDR_FONTSVG_SKIN), _T("ZIPRES"));
	if( hRes != NULL ) {
		HGLOBAL hGlobal = ::LoadResource(hInstance, hRes);
		DWORD size = ::SizeofResource(hInstance, hRes);
		if( hGlobal != NULL && size > 0 )
			CPaintManagerUI::SetResourceZip((LPBYTE)::LockResource(hGlobal), size);
	}

	if( CThemeManager* tm = CThemeManager::GetInstance() ) {
		tm->SetDefaultThemeId(_T("dark"));
		tm->ApplyTheme(_T("dark"));
	}

	CMainWnd* pMain = new CMainWnd();
	pMain->Create(NULL, _T("FontSVG"), UI_WNDSTYLE_FRAME, 0L, 0, 0, 1280, 800);
	pMain->ShowWindow(true);

	CPaintManagerUI::MessageLoop();

	delete pMain;
	::OleUninitialize();
	::CoUninitialize();
	return 0;
}
