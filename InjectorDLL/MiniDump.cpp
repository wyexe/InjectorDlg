/*
	Author:	Vladimir Sedach.

	Purpose: demo of Call Stack creation by our own means,
	and with MiniDumpWriteDump() function of DbgHelp.dll.
*/
#include "stdafx.h"
#include "MiniDump.h"
#include <STDIO.H>
#include <TIME.H>

#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

BOOL g_bStopDumpFile = FALSE;
WCHAR g_wszDumpPath[MAX_PATH] = { 0 };

HMODULE	hDbgHelp;
MINIDUMP_WRITE_DUMP	MiniDumpWriteDump_;

CREATE_TOOL_HELP32_SNAPSHOT	CreateToolhelp32Snapshot_;
MODULE32_FIRST	Module32First_;
MODULE32_NEST	Module32Next_;

#define	DUMP_SIZE_MAX	8000	//max size of our dump
#define	CALL_TRACE_MAX	((DUMP_SIZE_MAX - 2000) / (MAX_PATH + 40))	//max number of traced calls

LONG WINAPI CrashReportEx(LPEXCEPTION_POINTERS ExceptionInfo)
{
	//char	szFileName[MAX_PATH] = {0x00};
	//UINT	nRet = 0;
	
	// 重启程序
	// 	::GetModuleFileName(NULL, szFileName, MAX_PATH);
	// 	nRet = WinExec(szFileName, SW_SHOW);
	
	// 创建DUMP文件
	if (!g_bStopDumpFile)
	{
		Create_Dump(ExceptionInfo, 1, 1);
		//CPrintLog::PrintLog_W(L"Dump",__LINE__,L"Account:%s", CPrintLog::s_wszUser);
		MessageBoxW(NULL, L"程序崩溃,创建转存文件成功!", L"", NULL);
	}
	
	return EXCEPTION_EXECUTE_HANDLER;
}
void RegDumpFunction()
{
	SetUnhandledExceptionFilter(CrashReportEx);
	HMODULE	hKernel32;
	
	// Try to get MiniDumpWriteDump() address.
	hDbgHelp = LoadLibraryA("DBGHELP.DLL");
	MiniDumpWriteDump_ = (MINIDUMP_WRITE_DUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
	//	d("hDbgHelp=%X, MiniDumpWriteDump_=%X", hDbgHelp, MiniDumpWriteDump_);
	
	// Try to get Tool Help library functions.
	hKernel32 = GetModuleHandleA("KERNEL32");
	CreateToolhelp32Snapshot_ = (CREATE_TOOL_HELP32_SNAPSHOT)GetProcAddress(hKernel32, "CreateToolhelp32Snapshot");
	Module32First_ = (MODULE32_FIRST)GetProcAddress(hKernel32, "Module32First");
	Module32Next_ = (MODULE32_NEST)GetProcAddress(hKernel32, "Module32Next");
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
//****************************************************************************************
BOOL WINAPI Get_Module_By_Ret_Addr(PBYTE Ret_Addr, PWCHAR Module_Name, PBYTE & Module_Addr)
//****************************************************************************************
// Find module by Ret_Addr (address in the module).
// Return Module_Name (full path) and Module_Addr (start address).
// Return TRUE if found.
{
	MODULEENTRY32W	M = { sizeof(M) };
	HANDLE	hSnapshot;

	Module_Name[0] = 0;
	
	if (CreateToolhelp32Snapshot_)
	{
		hSnapshot = CreateToolhelp32Snapshot_(TH32CS_SNAPMODULE, 0);
		
		if ((hSnapshot != INVALID_HANDLE_VALUE) &&
			Module32First_(hSnapshot, &M))
		{
			do
			{
				if (DWORD(Ret_Addr - M.modBaseAddr) < M.modBaseSize)
				{
					lstrcpynW(Module_Name, M.szExePath, MAX_PATH);
					Module_Addr = M.modBaseAddr;
					break;
				}
			} while (Module32Next_(hSnapshot, &M));
		}

		CloseHandle(hSnapshot);
	}

	return !!Module_Name[0];
} //Get_Module_By_Ret_Addr

//******************************************************************
int WINAPI Get_Call_Stack(PEXCEPTION_POINTERS pException, PWCHAR Str)
//******************************************************************
// Fill Str with call stack info.
// pException can be either GetExceptionInformation() or NULL.
// If pException = NULL - get current call stack.
{
	WCHAR	Module_Name[MAX_PATH];
	PBYTE	Module_Addr = 0;
	PBYTE	Module_Addr_1;
	int		Str_Len;
	
	typedef struct STACK
	{
		STACK *	Ebp;
		PBYTE	Ret_Addr;
		DWORD	Param[0];
	} STACK, * PSTACK;

	STACK	Stack = {0, 0};
	PSTACK	Ebp;

	if (pException)		//fake frame for exception address
	{
		Stack.Ebp = (PSTACK)pException->ContextRecord->Ebp;
		Stack.Ret_Addr = (PBYTE)pException->ExceptionRecord->ExceptionAddress;
		Ebp = &Stack;
	}
	else
	{
		Ebp = (PSTACK)&pException - 1;	//frame addr of Get_Call_Stack()

		// Skip frame of Get_Call_Stack().
		if (!IsBadReadPtr(Ebp, sizeof(PSTACK)))
			Ebp = Ebp->Ebp;		//caller ebp
	}

	Str[0] = 0;
	Str_Len = 0;

	// Trace CALL_TRACE_MAX calls maximum - not to exceed DUMP_SIZE_MAX.
	// Break trace on wrong stack frame.
	for (int Ret_Addr_I = 0;
		(Ret_Addr_I < CALL_TRACE_MAX) && !IsBadReadPtr(Ebp, sizeof(PSTACK)) && !IsBadCodePtr(FARPROC(Ebp->Ret_Addr));
		Ret_Addr_I++, Ebp = Ebp->Ebp)
	{
		// If module with Ebp->Ret_Addr found.
		if (Get_Module_By_Ret_Addr(Ebp->Ret_Addr, Module_Name, Module_Addr_1))
		{
			if (Module_Addr_1 != Module_Addr)	//new module
			{
				// Save module's address and full path.
				Module_Addr = Module_Addr_1;
				Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\n%08X  %s", reinterpret_cast<DWORD>(Module_Addr), Module_Name);
			}

			// Save call offset.
			Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX,
				L"\r\n  +%08X", Ebp->Ret_Addr - Module_Addr);

			// Save 5 params of the call. We don't know the real number of params.
			if (pException && !Ret_Addr_I)	//fake frame for exception address
				Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"  Exception Offset");
			else if (!IsBadReadPtr(Ebp, sizeof(PSTACK) + 5 * sizeof(DWORD)))
			{
				Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"  (%X, %X, %X, %X, %X)",
					Ebp->Param[0], Ebp->Param[1], Ebp->Param[2], Ebp->Param[3], Ebp->Param[4]);
			}
		}
		else
			Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\n%08X", reinterpret_cast<DWORD>(Ebp->Ret_Addr));
	}

	return Str_Len;
} //Get_Call_Stack

//***********************************
int WINAPI Get_Version_Str(PWCHAR Str)
//***********************************
// Fill Str with Windows version.
{
	OSVERSIONINFOEX	V = {sizeof(OSVERSIONINFOEX)};	//EX for NT 5.0 and later

	if (!GetVersionEx((POSVERSIONINFO)&V))
	{
		ZeroMemory(&V, sizeof(V));
		V.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx((POSVERSIONINFO)&V);
	}

	if (V.dwPlatformId != VER_PLATFORM_WIN32_NT)
		V.dwBuildNumber = LOWORD(V.dwBuildNumber);	//for 9x HIWORD(dwBuildNumber) = 0x04xx

	return swprintf_s(Str, DUMP_SIZE_MAX, L"\r\nWindows:  %d.%d.%d, SP %d.%d, Product Type %d",	//SP - service pack, Product Type - VER_NT_WORKSTATION,...
		V.dwMajorVersion, V.dwMinorVersion, V.dwBuildNumber, V.wServicePackMajor, V.wServicePackMinor, V.wProductType);
} //Get_Version_Str

//*************************************************************
PWCHAR WINAPI Get_Exception_Info(PEXCEPTION_POINTERS pException)
//*************************************************************
// Allocate Str[DUMP_SIZE_MAX] and return Str with dump, if !pException - just return call stack in Str.
{
	WCHAR		*Str = NULL;
	int			Str_Len = NULL;
	WCHAR		Module_Name[MAX_PATH] = {NULL};
	PBYTE		Module_Addr = NULL;
	HANDLE		hFile = NULL;
	FILETIME	Last_Write_Time = {NULL};
	FILETIME	Local_File_Time = {NULL};
	SYSTEMTIME	T = {NULL};
	
	Str = new WCHAR[DUMP_SIZE_MAX];

	if (!Str)
		return NULL;

	Str_Len = 0;
	Str_Len += Get_Version_Str(Str + Str_Len);

	Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\nProcess:  ");
	GetModuleFileName(GetModuleHandle(SelfDLLName), Str + Str_Len, MAX_PATH);
	Str_Len = lstrlen(Str);

	// If exception occurred.
	if (pException)
	{
		EXCEPTION_RECORD &	E = *pException->ExceptionRecord;
		CONTEXT &			C = *pException->ContextRecord;

		// If module with E.ExceptionAddress found - save its path and date.
		if (Get_Module_By_Ret_Addr((PBYTE)E.ExceptionAddress, Module_Name, Module_Addr))
		{
			Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX,
				L"\r\nModule:  %s", Module_Name);

			if ((hFile = CreateFile(Module_Name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
			{
				if (GetFileTime(hFile, NULL, NULL, &Last_Write_Time))
				{
					FileTimeToLocalFileTime(&Last_Write_Time, &Local_File_Time);
					FileTimeToSystemTime(&Local_File_Time, &T);

					Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX,
						L"\r\nDate Modified:  %02d/%02d/%d",
						T.wMonth, T.wDay, T.wYear);
				}
				CloseHandle(hFile);
			}
		}
		else
		{
			Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX,
				L"\r\nException Addr:  %08X", reinterpret_cast<DWORD>(E.ExceptionAddress));
		}
		
		Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX,
			L"\r\nException Code:  %08X", E.ExceptionCode);
		
		if (E.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
		{
			// Access violation type - Write/Read.
			Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX,
				L"\r\n%s Address:  %08X",
				(E.ExceptionInformation[0]) ? L"Write" : L"Read", E.ExceptionInformation[1]);
		}

		// Save instruction that caused exception.
		Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\n DLLBase:%X,%X ", (DWORD)::GetModuleHandleW(L"BnsProjects.dll"), (DWORD)::GetModuleHandleW(L"BnsProjects_dlg.dll"));

		// Save registers at exception.
		Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\nRegisters:");
		Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\nEAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X", C.Eax, C.Ebx, C.Ecx, C.Edx);
		Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\nESI: %08X  EDI: %08X  ESP: %08X  EBP: %08X", C.Esi, C.Edi, C.Esp, C.Ebp);
		Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\nEIP: %08X  EFlags: %08X", C.Eip, C.EFlags);
	} //if (pException)
	
	// Save call stack info.
	Str_Len += swprintf_s(Str + Str_Len, DUMP_SIZE_MAX, L"\r\nCall Stack:");
	Get_Call_Stack(pException, Str + Str_Len);

	if (Str[0] == L"\r\n"[0])
		lstrcpyW(Str, Str + sizeof(L"\r\n") - 1);



	return Str;
} //Get_Exception_Info
//*************************************************************************************
void WINAPI Create_Dump(PEXCEPTION_POINTERS pException, BOOL File_Flag, BOOL Show_Flag)
//*************************************************************************************
// Create dump. 
// pException can be either GetExceptionInformation() or NULL.
// If File_Flag = TRUE - write dump files (.dmz and .dmp) with the name of the current process.
// If Show_Flag = TRUE - show message with Get_Exception_Info() dump.
{
	WCHAR strDir[MAX_PATH],strTXTFile[MAX_PATH],strDMPFile[MAX_PATH];
	SYSTEMTIME Systime;
	GetLocalTime(&Systime);

	//////////////////////////////////////
	WCHAR pathbuf[260];
	swprintf_s(pathbuf, _countof(pathbuf), L"%s", g_wszDumpPath);
	//////////////////////////////////////
	swprintf_s(strDir, _countof(strDir), L"%s\\DumpLog", pathbuf);
	swprintf_s(strTXTFile, _countof(strTXTFile), L"%s\\DumpLog\\%04d-%02d-%02d %02d%02d%02d.txt", pathbuf, Systime.wYear, Systime.wMonth,
		Systime.wDay, Systime.wHour, Systime.wMinute, Systime.wSecond);
	swprintf_s(strDMPFile, _countof(strDMPFile), L"%s\\DumpLog\\%04d-%02d-%02d %02d%02d%02d.dmp", pathbuf, Systime.wYear, Systime.wMonth,
		Systime.wDay, Systime.wHour, Systime.wMinute, Systime.wSecond);
	OutputDebugStringW(strDMPFile);
	if(!PathFileExists(strDir))
		CreateDirectoryW(strDir,NULL);

	//if (Show_Flag && Str)
	//	MessageBox(NULL, Str, "MiniDump", MB_ICONHAND | MB_OK);

	if (File_Flag)
	{
		// If MiniDumpWriteDump() of DbgHelp.dll available.
		if (MiniDumpWriteDump_)
		{
			MINIDUMP_EXCEPTION_INFORMATION	M;

			M.ThreadId = GetCurrentThreadId();
			M.ExceptionPointers = pException;
			M.ClientPointers = 0;

			HANDLE hDump_File = CreateFile(strDMPFile,
				GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			MiniDumpWriteDump_(GetCurrentProcess(), GetCurrentProcessId(), hDump_File,
				MiniDumpNormal, (pException) ? &M : NULL, NULL, NULL);

			CloseHandle(hDump_File);
		}
	} //if (File_Flag)

//	delete Str;
} //Create_Dump
// 	__try 
// 	{  
// 		******
// 	}  
// 	__except(CreateMiniDump(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) 
// 	{
// 	}
void CreateMiniDump(LPEXCEPTION_POINTERS lpExceptionInfo)
{
	if ((hDbgHelp = GetModuleHandle(L"DBGHELP.DLL")) == NULL)
		hDbgHelp = LoadLibrary(L"DBGHELP.DLL");
	MiniDumpWriteDump_ = (MINIDUMP_WRITE_DUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
	/////////////////////////////////////////////////////
	/*
	WCHAR MOUDLEPATH[MAX_PATH] = {0}; 
	int  pathlen = ::GetModuleFileName(GetModuleHandle(SelfDLLName),MOUDLEPATH,260);
	// 替换掉单杠
	while(TRUE)
	{
		if(MOUDLEPATH[pathlen--]=='\\')
			break;
	}
	MOUDLEPATH[++pathlen] = 0x0;
	swprintf_s(FileName,L"%s\\DumpLog",MOUDLEPATH);

	wcscpy_s(FileName, 260, g_wszDumpPath);*/

	//if(!PathFileExists(FileName))
		//CreateDirectory(FileName,NULL);

	SYSTEMTIME Systime;
	GetLocalTime(&Systime);

	WCHAR FileName[MAX_PATH] = { 0 };
	swprintf_s(FileName, _countof(FileName), L"%s\\DumpLog\\Dump %04d-%02d-%02d %02d%02d%02d.dmp", g_wszDumpPath, Systime.wYear, Systime.wMonth,
		Systime.wDay, Systime.wHour, Systime.wMinute, Systime.wSecond);


	OutputDebugStringW(FileName);
	HANDLE hFile = CreateFile(FileName, GENERIC_WRITE,  
        0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  
	
    if ( hFile != NULL &&  hFile != INVALID_HANDLE_VALUE ) {  
		
        // Create the minidump   
        MINIDUMP_EXCEPTION_INFORMATION mdei;  
        mdei.ThreadId          = GetCurrentThreadId();  
        mdei.ExceptionPointers = lpExceptionInfo;  
        mdei.ClientPointers    = FALSE;  
		
        MINIDUMP_TYPE mdt      = MiniDumpNormal;  
        BOOL retv = MiniDumpWriteDump_( GetCurrentProcess(), GetCurrentProcessId(),  
            hFile, mdt, ( lpExceptionInfo != 0 ) ? &mdei : 0, 0, 0);  
		
        if ( !retv ) {  
            printf("MiniDumpWriteDump failed. Error: %u \n", GetLastError() );   
        } else {   
            printf("Minidump created.\n");   
        }  
		
        // Close the file  
        CloseHandle( hFile );
    }else{
        printf("CreateFile failed. Error: %u \n", GetLastError() );   
    }
}  