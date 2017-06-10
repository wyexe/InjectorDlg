// Game.cpp : 实现文件
//

#include "stdafx.h"
#include "Injector.h"
#include "Game.h"
#include "afxdialogex.h"
#include <MyTools/CLPublic.h>
#include <MyTools/Log.h>
#include <MyTools/CLFile.h>
// Game 对话框

IMPLEMENT_DYNAMIC(Game, CDialogEx)

Game::Game(CWnd* pParent /*=NULL*/)
	: CDialogEx(Game::IDD, pParent)
	, m_ShowPath(_T(""))
{
	
}

Game::~Game()
{
}

void Game::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_ShowPath);
}

BOOL Game::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	DragAcceptFiles(TRUE);

	//Check C:\\Injector.ini
	WCHAR szPath[MAX_PATH];
	::GetPrivateProfileStringW(L"Config", L"Path", L"", szPath, MAX_PATH, L"C:\\CLInjector\\Injector.ini");
	((CEdit*)this->GetDlgItem(IDC_EDIT1))->SetWindowTextW(szPath);
	return TRUE;  
}

BEGIN_MESSAGE_MAP(Game, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON1, &Game::OnBnClickedButton1)
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_BUTTON2, &Game::OnBnClickedButton2)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// Game 消息处理程序
void Game::OnDropFiles(HDROP hDropInfo)
{
	WCHAR FilePath[240];
	int PathLen = DragQueryFileW(hDropInfo,0,FilePath,sizeof(FilePath));
	m_ShowPath.Format(L"%s",FilePath);
	DragFinish(hDropInfo);
	UpdateData(FALSE);
	//CDialog::OnDropFiles(hDropInfo);
}
BOOL bClose = FALSE;
HMODULE hm = nullptr;
void Game::OnBnClickedButton1()
{
	UpdateData(TRUE);
	if (m_ShowPath.MakeLower() == L"close")
	{
		bClose = TRUE;
		this->PostMessageW(WM_CLOSE);
		return;
	}

	if (hm == NULL)
	{
		if (!MyTools::CLPublic::FileExit(L"C:\\CLInjector"))
			MyTools::CLFile::CreateMyDirectory(L"C:\\CLInjector");

		// Copy DLL to C:, so you can overwrite or Compile DLL. 
		::DeleteFileW(L"C:\\CLInjector\\Injector.dll");
		::CopyFileW(m_ShowPath.GetBuffer(), L"C:\\CLInjector\\Injector.dll", TRUE);
		WritePrivateProfileStringW(L"Config", L"Path", m_ShowPath.GetBuffer(), L"C:\\CLInjector\\Injector.ini");

		hm = ::LoadLibraryW(L"C:\\CLInjector\\Injector.dll");
		if(hm == NULL)
			::MessageBoxA(NULL, "Error", "", NULL);
	}
	else
	{
		AfxMessageBox(L"AlReady Injector~");
	}
}


void Game::OnBnClickedButton2()
{
	if (hm != NULL)
	{
		typedef BOOL (FAR WINAPI *fnRelease)(void);
		fnRelease fnRelease_ = (fnRelease)GetProcAddress(hm, "ReleaseDLL");
		if (fnRelease_())
		{
			FreeLibrary(hm);
			hm = NULL;
		}
	}
	else
	{
		AfxMessageBox(L"Injector DLL first!");
	}
}


void Game::OnClose()
{
	// TODO: Add your message handler code here and/or call default
	//if (bClose)
	//	CDialogEx::OnClose();
}
