﻿// Inject.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <tlhelp32.h>
#include <memory>
#include <functional>


const UINT WM_PREUNLOAD = RegisterWindowMessage(_T("WinCustomDesktopPreUnload"));



// 提升进程特权，否则在XP中无法打开进程
BOOL EnablePrivilege(BOOL enable)
{
	// 得到令牌句柄
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_READ, &hToken))
		return FALSE;

	// 得到特权值
	LUID luid;
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
		return FALSE;

	// 提升令牌句柄权限
	TOKEN_PRIVILEGES tp = {};
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL))
		return FALSE;

	// 关闭令牌句柄
	CloseHandle(hToken);

	return TRUE;
}

// 注入DLL，返回模块句柄（64位程序只能返回低32位）
HMODULE InjectDll(HANDLE process, LPCTSTR dllPath)
{
	DWORD remoteModule = 0;
	const SIZE_T dllPathSize = static_cast<SIZE_T>(_tcslen(dllPath) + 1) * sizeof(TCHAR);

	// 申请内存用来存放DLL路径
	std::unique_ptr<void, std::function<void(void*)> > remoteMemory(nullptr, [process](void* p){
		VirtualFreeEx(process, p, 0, MEM_RELEASE);
	});
	remoteMemory.reset(VirtualAllocEx(process, NULL, dllPathSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	if (remoteMemory.get() == NULL)
	{
		printf("申请内存失败，错误代码：%u\n", GetLastError());
		return NULL;
	}

	// 写入DLL路径
	if (!WriteProcessMemory(process, remoteMemory.get(), dllPath, dllPathSize, NULL))
	{
		printf("写入内存失败，错误代码：%u\n", GetLastError());
		return NULL;
	}

	// 创建远线程调用LoadLibrary
	const HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibrary), remoteMemory.get(), 0, NULL);
	if (remoteThread == NULL)
	{
		printf("创建远线程失败，错误代码：%u\n", GetLastError());
		return NULL;
	}
	// 等待远线程结束
	WaitForSingleObject(remoteThread, INFINITE);
	// 取DLL在目标进程的句柄
	if (!GetExitCodeThread(remoteThread, &remoteModule))
		remoteModule = 0;
	if (remoteModule == 0)
		printf("注入失败\n");

	// 释放
	CloseHandle(remoteThread);

	return reinterpret_cast<HMODULE>(remoteModule);
}

// 卸载DLL
BOOL FreeRemoteDll(HANDLE process, HMODULE remoteModule)
{
	// 创建远线程调用FreeLibrary
	const HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, (LPVOID)remoteModule, 0, NULL);
	if (remoteThread == NULL)
	{
		printf("创建远线程失败，错误代码：%u\n", GetLastError());
		return FALSE;
	}
	// 等待远线程结束
	WaitForSingleObject(remoteThread, INFINITE);
	// 取返回值
	DWORD result = 0;
	if (!GetExitCodeThread(remoteThread, &result))
		result = 0;
	if (result == 0)
		printf("卸载失败\n");

	// 释放
	CloseHandle(remoteThread);
	return result != 0;
}

// 取其他进程模块句柄
HMODULE GetRemoteModuleHandle(DWORD pid, LPCTSTR moduleName)
{
	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	MODULEENTRY32 moduleentry;
	moduleentry.dwSize = sizeof(moduleentry);

	BOOL hasNext = Module32First(snapshot, &moduleentry);
	HMODULE handle = NULL;
	while (hasNext)
	{
		if (_tcsicmp(moduleentry.szModule, moduleName) == 0)
		{
			handle = moduleentry.hModule;
			break;
		}
		hasNext = Module32Next(snapshot, &moduleentry);
	}

	CloseHandle(snapshot);
	return handle;
}


int _tmain(int argc, _TCHAR* argv[])
{
	EnablePrivilege(TRUE);

	// 打开进程
	HWND hwnd = GetShellWindow();
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	const HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (process == NULL)
	{
		printf("打开进程失败，错误代码：%u\n", GetLastError());
		return 1;
	}


	HMODULE remoteModule = GetRemoteModuleHandle(pid, _T("CustomDesktop.dll"));
	if (remoteModule != NULL)
	{
		puts("卸载DLL");

		// 发消息卸载所有插件
		hwnd = FindWindowEx(hwnd, NULL, _T("SHELLDLL_DefView"), _T(""));
		hwnd = FindWindowEx(hwnd, NULL, _T("SysListView32"), _T("FolderView"));
		SendMessage(hwnd, WM_PREUNLOAD, NULL, NULL);

		FreeRemoteDll(process, remoteModule);
	}
	else
	{
		puts("注入DLL");

		// 要将CustomDesktop.dll放在本程序当前目录下
		TCHAR dllPath[MAX_PATH]; // 要用绝对路径
		GetCurrentDirectory(_countof(dllPath), dllPath);
		_tcscat_s(dllPath, _T("\\CustomDesktop.dll"));

		InjectDll(process, dllPath);
	}


	// 关闭进程
	CloseHandle(process);

	return 0;
}

