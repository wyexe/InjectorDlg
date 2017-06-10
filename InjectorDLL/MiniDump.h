#include <windows.h>
#include <tlhelp32.h>

//#include "dbghelp.h"
//#define DEBUG_DPRINTF		1	//allow d()
//#include "wfun.h"

#pragma optimize("y", off)		//generate stack frame pointers for all functions - same as /Oy- in the project
#pragma warning(disable: 4200)	//nonstandard extension used : zero-sized array in struct/union
#pragma warning(disable: 4100)	//unreferenced formal parameter
#pragma warning(disable: 4996)

/*BOOL WINAPI Get_Module_By_Ret_Addr(PBYTE Ret_Addr, PCHAR Module_Name, PBYTE & Module_Addr);
int	 WINAPI Get_Call_Stack(PEXCEPTION_POINTERS pException, PCHAR Str);
int  WINAPI Get_Version_Str(PCHAR Str);
PCHAR WINAPI Get_Exception_Info(PEXCEPTION_POINTERS pException);
void WINAPI Create_Dump(PEXCEPTION_POINTERS pException, BOOL File_Flag, BOOL Show_Flag);*/

// In case you don't have dbghelp.h.
#ifndef _DBGHELP_

#ifndef MINIDUMP_EXCEPTION_INFORMATION_
#define MINIDUMP_EXCEPTION_INFORMATION_
typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
	DWORD	ThreadId;
	PEXCEPTION_POINTERS	ExceptionPointers;
	BOOL	ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;
#endif
#ifndef MINIDUMP_TYPE_
#define MINIDUMP_TYPE_
typedef enum _MINIDUMP_TYPE {
	MiniDumpNormal =			0x00000000,
		MiniDumpWithDataSegs =		0x00000001,
} MINIDUMP_TYPE;
#endif
typedef	BOOL (WINAPI * MINIDUMP_WRITE_DUMP)(
											IN HANDLE			hProcess,
											IN DWORD			ProcessId,
											IN HANDLE			hFile,
											IN MINIDUMP_TYPE	DumpType,
											IN CONST PMINIDUMP_EXCEPTION_INFORMATION	ExceptionParam, OPTIONAL
											IN PVOID									UserStreamParam, OPTIONAL
											IN PVOID									CallbackParam OPTIONAL
											);

#else

typedef	BOOL (WINAPI * MINIDUMP_WRITE_DUMP)(
											IN HANDLE			hProcess,
											IN DWORD			ProcessId,
											IN HANDLE			hFile,
											IN MINIDUMP_TYPE	DumpType,
											IN CONST PMINIDUMP_EXCEPTION_INFORMATION	ExceptionParam, OPTIONAL
											IN PMINIDUMP_USER_STREAM_INFORMATION		UserStreamParam, OPTIONAL
											IN PMINIDUMP_CALLBACK_INFORMATION			CallbackParam OPTIONAL
											);
#endif //#ifndef _DBGHELP_

// Tool Help functions.
#define SelfDLLName		L"BnsProjects.dll"

typedef	HANDLE (WINAPI * CREATE_TOOL_HELP32_SNAPSHOT)(DWORD dwFlags, DWORD th32ProcessID);
typedef	BOOL (WINAPI * MODULE32_FIRST)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
typedef	BOOL (WINAPI * MODULE32_NEST)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);


extern void WINAPI Create_Dump(PEXCEPTION_POINTERS pException, BOOL File_Flag, BOOL Show_Flag);

extern HMODULE	hDbgHelp;
extern MINIDUMP_WRITE_DUMP	MiniDumpWriteDump_;

extern CREATE_TOOL_HELP32_SNAPSHOT	CreateToolhelp32Snapshot_;
extern MODULE32_FIRST	Module32First_;
extern MODULE32_NEST	Module32Next_;

void RegDumpFunction();
void CreateMiniDump(LPEXCEPTION_POINTERS lpExceptionInfo);

extern BOOL g_bStopDumpFile;
extern WCHAR g_wszDumpPath[MAX_PATH];