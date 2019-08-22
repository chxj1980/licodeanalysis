﻿#include "stdafx.h"
#include "DesktopBrowser.h"
#include <CDEvents.h>
using namespace std::placeholders;
#include <CDAPI.h>
#include "Config.h"
#include <CommCtrl.h>
#include <thread>
#include <shellapi.h>


DesktopBrowser::DesktopBrowser(HMODULE hModule) : 
	m_module(hModule),
	m_menuID(cd::GetMenuID())
{
	cd::g_fileListWndProcEvent.AddListener(std::bind(&DesktopBrowser::OnFileListWndProc, this, _1, _2, _3, _4, _5), m_module);
	cd::g_postDrawBackgroundEvent.AddListener(std::bind(&DesktopBrowser::OnPostDrawBackground, this, _1), m_module);
	cd::g_fileListWndSizeEvent.AddListener([this](int width, int height){
		if (m_browser == nullptr)
			return;
		RECT pos = { 0, 0, width, height };
		m_browser->SetPos(pos);
	}, m_module);
	cd::g_preDrawBackgroundEvent.AddListener([](HDC&, bool& pass) { pass = false; }, m_module);
	cd::g_desktopCoveredEvent.AddListener([this]{ m_pauseFlag = true; }, m_module);
	cd::g_desktopUncoveredEvent.AddListener([this]{ m_pauseFlag = false; }, m_module);
	cd::g_appendTrayMenuEvent.AddListener(std::bind(&DesktopBrowser::OnAppendTrayMenu, this, _1), m_module);
	cd::g_chooseMenuItemEvent.AddListener(std::bind(&DesktopBrowser::OnChooseMenuItem, this, _1, _2), m_module);

	cd::ExecInMainThread([this]{
		SIZE size;
		cd::GetDesktopSize(size);
		RECT pos = { 0, 0, size.cx, size.cy };
		HRESULT hr;
		m_browser = std::make_unique<Browser>(cd::GetParentHwnd(), pos, hr);
		if (FAILED(hr))
		{
			MessageBox(cd::GetTopHwnd(), _T("加载浏览器失败！"), APPNAME, MB_ICONERROR);
			m_browser = nullptr;
			return;
		}
		m_browser->Navigate(g_config.m_homePage.c_str());

		// 创建一个线程以30fps速率刷新，没什么好办法了
		m_redrawThread = std::make_unique<std::thread>([this]{
			while (m_runThreadFlag)
			{
				if (!m_pauseFlag)
					cd::RedrawDesktop();
				Sleep(33);
			}
		});
	});
	
	cd::RedrawDesktop();
}

DesktopBrowser::~DesktopBrowser()
{
	m_runThreadFlag = false;
	if (m_redrawThread != nullptr && m_redrawThread->joinable())
		m_redrawThread->join();
	m_redrawThread = nullptr;
}


void DesktopBrowser::OnFileListWndProc(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& res, bool& pass)
{
	switch (message)
	{
	case WM_NCHITTEST:
	{
		HWND hwnd = cd::GetFileListHwnd();
		LVHITTESTINFO hitTestInfo;
		hitTestInfo.pt = { MAKEPOINTS(lParam).x, MAKEPOINTS(lParam).y };
		ScreenToClient(hwnd, &hitTestInfo.pt);
		if (ListView_HitTest(hwnd, &hitTestInfo) != -1)
			return;
		// 设置图标以外的地方不属于文件列表窗口
		res = HTTRANSPARENT;
		pass = false;
	}
	}
}

void DesktopBrowser::OnPostDrawBackground(HDC& hdc)
{
	if (m_browser == nullptr)
		return;
	SIZE size;
	cd::GetDesktopSize(size);
	RECT rect = { 0, 0, size.cx, size.cy };
	m_browser->Draw(hdc, rect);
}


void DesktopBrowser::OnAppendTrayMenu(HMENU menu)
{
	AppendMenu(menu, MF_STRING, m_menuID, APPNAME);
}

void DesktopBrowser::OnChooseMenuItem(UINT menuID, bool& pass)
{
	if (menuID != m_menuID)
		return;

	std::thread([this]{
		SHELLEXECUTEINFOW info = {};
		info.cbSize = sizeof(info);
		info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
		info.lpVerb = L"open";
		info.lpFile = L"notepad.exe";
		std::wstring param = cd::GetPluginDir() + L"\\Data\\DesktopBrowser.ini";
		info.lpParameters = param.c_str();
		info.nShow = SW_SHOWNORMAL;
		ShellExecuteExW(&info);

		WaitForSingleObject(info.hProcess, INFINITE);
		CloseHandle(info.hProcess);

		cd::ExecInMainThread([this]{
			Config newConfig;
			if (newConfig.m_homePage != g_config.m_homePage && m_browser != nullptr)
				m_browser->Navigate(newConfig.m_homePage.c_str());
			g_config = newConfig;
		});
	}).detach();
	pass = false;
}
