// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "MiniDump.h"
#include <MyTools/CLPublic.h>
#include <MyTools/Log.h>
#include <MyTools/CmdLog.h>
#include <MyTools/ClassInstance.h>
#include <map>

#define _SELF L"dllmain.cpp"

class CExpr : public MyTools::CExprFunBase, public MyTools::CClassInstance<CExpr>
{
public:
	CExpr() : _LockMapModule(L"_LockMapModule")
	{
		
	}
	virtual ~CExpr()
	{

	};

	virtual VOID Release()
	{

	}
	virtual std::vector<MyTools::ExpressionFunPtr>& GetVec()
	{
		static std::vector<MyTools::ExpressionFunPtr> Vec =
		{
			{ std::bind(&CExpr::Help, this, std::placeholders::_1), L"Help" },
			{ std::bind(&CExpr::InjectorDLL, this, std::placeholders::_1), L"InjectorDLL" },
		};
		return Vec;
	}
private:
	virtual VOID Help(CONST std::vector<std::wstring>&)
	{
		auto& Vec = GetVec();
		for (CONST auto& itm : Vec)
			LOG_C(MyTools::CLog::em_Log_Type::em_Log_Type_Custome, L"FunctionName=%s", itm.wsFunName.c_str());
	}

	VOID InjectorDLL(CONST std::vector<std::wstring>& VecParm)
	{
		if (VecParm.size() != 2)
		{
			LOG_C(MyTools::CLog::em_Log_Type::em_Log_Type_Exception, L"InjectorDLL(Path,IsLoad)");
			return;
		}

		CONST std::wstring& wsPath = VecParm.at(0);
		BOOL IsLoad = VecParm.at(1) == L"1" ? TRUE : FALSE;

		if (IsLoad)
			Injector(wsPath);
		else
			FreeDLL(wsPath);
	}
private:
	BOOL Injector(_In_ CONST std::wstring& wsPath)
	{
		if (ExistDLL(wsPath))
		{
			LOG_C_D(L"Already Exist DLL:%s", wsPath.c_str());
			return FALSE;
		}

		HMODULE hmDLL = ::LoadLibraryW(wsPath.c_str());
		if (hmDLL == NULL)
		{
			LOG_C(MyTools::CLog::em_Log_Type_Exception, L"Load DLL Faild:%s", wsPath.c_str());
			return FALSE;
		}

		_LockMapModule.Access([hmDLL, wsPath, this]
		{
			_MapModule.insert(std::make_pair(wsPath, hmDLL));
		});
		return TRUE;
	}

	BOOL FreeDLL(_In_ CONST std::wstring& wsPath)
	{
		if (!ExistDLL(wsPath))
		{
			LOG_C_D(L"UnExist DLL:%s", wsPath.c_str());
			return FALSE;
		}

		HMODULE hmDLL = NULL;
		_LockMapModule.Access([&hmDLL, this, wsPath]
		{
			auto itr = _MapModule.find(wsPath);
			if (itr != _MapModule.end())
			{
				hmDLL = itr->second;
				_MapModule.erase(itr);
			}
		});

		if (hmDLL != NULL)
			::FreeLibrary(hmDLL);

		return TRUE;
	}

	BOOL ExistDLL(_In_ CONST std::wstring& wsPath) CONST
	{
		BOOL bExist = FALSE;
		_LockMapModule.Access([&bExist, this, wsPath]
		{
			bExist = _MapModule.find(wsPath) == _MapModule.end() ? FALSE : TRUE;
		});
		return bExist;
	}
private:
	std::map<std::wstring, HMODULE> _MapModule;
	MyTools::CLLock _LockMapModule;
};

HANDLE hWaitCmdThread = NULL;
DWORD WINAPI _WaitCmdThread(LPVOID)
{
	WCHAR wszSavePath[MAX_PATH] = { 0 };
	::GetCurrentDirectoryW(MAX_PATH, wszSavePath);
	::lstrcatW(wszSavePath, L"\\Log\\");
	MyTools::CLog::GetInstance().SetClientName(L"InjectorClient", wszSavePath, TRUE, 20 * 1024 * 1024);
	MyTools::CCmdLog::GetInstance().Run(L"InjectorClient", CExpr::GetInstance().GetVec());
	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID
					 )
{
	if (hWaitCmdThread == NULL)
	{
		hWaitCmdThread = cbBEGINTHREADEX(NULL, NULL, _WaitCmdThread, NULL, NULL, NULL);
		::CloseHandle(hWaitCmdThread);
		::DisableThreadLibraryCalls(hModule);
	}
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		wcscpy_s(g_wszDumpPath, 260, L"C:\\");
		RegDumpFunction();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		ExitProcess(0);
		break;
	}
	return TRUE;
}

