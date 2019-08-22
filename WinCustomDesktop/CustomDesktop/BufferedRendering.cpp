﻿#include "stdafx.h"
#include "BufferedRendering.h"
#include "Global.h"
#include <CDEvents.h>
using namespace std::placeholders;
#include <CDAPI.h>
#ifdef _WIN64
#include <VersionHelpers.h>
#endif


namespace cd
{
	BufferedRendering::BufferedRendering() :
		m_bgBackupClipRgn(CreateRectRgn(0, 0, 0, 0))
	{
#ifndef _WIN64
		DWORD majorVersion = LOBYTE(LOWORD(GetVersion()));
		if (majorVersion <= 5) // FUCKING XP
			m_controlRendering = false;
#else
		if (!IsWindowsVistaOrGreater())
			m_controlRendering = false;
#endif

		Init();
	}

	BufferedRendering::~BufferedRendering()
	{
		Uninit();
	}


	bool BufferedRendering::Init()
	{
		if (m_hasInit)
			return true;

		// 创建各种DC
		if (!InitDC())
			return false;

		// 监听事件
		g_fileListWndProcEvent.AddListener(std::bind(&BufferedRendering::OnFileListWndProc, this, _1, _2, _3, _4, _5));
		g_parentWndProcEvent.AddListener(std::bind(&BufferedRendering::OnParentWndProc, this, _1, _2, _3, _4, _5));

		g_postDrawIconEvent.AddListener(std::bind(&BufferedRendering::PostDrawIcon, this, _1));

		g_fileListRedrawWindowEvent.AddListener([](CONST RECT*, HRGN, UINT) { g_global.m_needUpdateIcon = true; });
		g_fileListBeginPaintEvent.AddListener(std::bind(&BufferedRendering::OnFileListBeginPaint, this, _1, _2));
		g_fileListEndPaintEvent.AddListener(std::bind(&BufferedRendering::OnFileListEndPaint, this, _1));

		// 获取图标层
		ExecInMainThread([]{ g_global.m_needUpdateIcon = true; RedrawDesktop(); });

		m_hasInit = true;
		return true;
	}

	bool BufferedRendering::Uninit()
	{
		if (!m_hasInit)
			return true;

		if (!m_bufferImg.IsNull())
			m_bufferImg.ReleaseDC();

		CImage* imgs[] = { &m_bufferImg, &m_bgBackupImg, &m_iconBufferImg, &m_wallpaperImg };
		for (auto& img : imgs)
		{
			if (!img->IsNull())
				img->Destroy();
		}

		CImage::ReleaseGDIPlus();

		m_hasInit = false;
		return true;
	}

	// 更新各种buffer尺寸
	bool BufferedRendering::InitDC()
	{
		if (!m_bufferImg.IsNull())
			m_bufferImg.ReleaseDC();

		CImage* imgs[] = { &m_bufferImg, &m_bgBackupImg, &m_iconBufferImg };
		for (auto& img : imgs)
		{
			if (!img->IsNull())
				img->Destroy();
			if (!img->Create(g_global.m_wndSize.cx, g_global.m_wndSize.cy, 32, CImage::createAlphaChannel))
				return false;
		}

		m_bufferDC = m_bufferImg.GetDC();
		SetRectRgn(m_bgBackupClipRgn.get(), 0, 0, g_global.m_wndSize.cx, g_global.m_wndSize.cy);

		InitWallpaperDC();
		return true;
	}

	bool BufferedRendering::InitWallpaperDC()
	{
		// 取壁纸路径
		std::wstring wallpaperPath(MAX_PATH, L'\0');
		SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, &wallpaperPath.front(), 0);
		wallpaperPath.resize(wcslen(wallpaperPath.c_str()));

		// 创建壁纸DC
		CImage img;
		if (FAILED(img.Load(wallpaperPath.c_str())))
			return false;
		if (!m_wallpaperImg.IsNull())
			m_wallpaperImg.Destroy();
		// 如果是多屏幕应该对每个屏幕分别画壁纸，但是我懒得做了...
		if (!m_wallpaperImg.Create(g_global.m_wndSize.cx, g_global.m_wndSize.cy, 32, CImage::createAlphaChannel))
			return false;

		HDC wallpaperDC = m_wallpaperImg.GetDC();
		{
			Gdiplus::Graphics graph(wallpaperDC);
			Gdiplus::SolidBrush brush(Gdiplus::Color::Black);
			graph.FillRectangle(&brush, 0, 0, g_global.m_wndSize.cx, g_global.m_wndSize.cy);
			img.Draw(wallpaperDC, 0, 0, g_global.m_wndSize.cx, g_global.m_wndSize.cy);
		}
		m_wallpaperImg.ReleaseDC();
		return true;
	}


	/*
	使用了双缓冲和保存图标层结果来优化

	渲染路线：

	1. 不需要更新图标层时（使用已保存的图标层m_iconBufferImg）：
	    文件列表WM_PAINT -> BeginPaint -> g_fileListBeginPaintEvent（把窗口DC替换成缓冲DC）
	    -> 父窗口WM_ERASEBKGND -> g_preDrawBackgroundEvent -> 画背景 -> g_postDrawBackgroundEvent -> 画图标
	    -> g_postDrawIconEvent -> g_fileListEndPaintEvent（把缓冲DC复制回窗口DC） -> EndPaint
	
	2. 需要更新图标层时（由原窗口过程渲染图标）：
	    （原窗口过程）文件列表WM_PAINT -> BeginPaint -> g_fileListBeginPaintEvent（把窗口DC替换成缓冲DC）
	    -> 父窗口WM_ERASEBKGND -> g_preDrawBackgroundEvent -> 画背景 -> g_postDrawBackgroundEvent
		-> 此时缓冲DC是背景，把缓冲图像复制到m_bgBackupImg，再把缓冲图像的alpha清0准备画图标 -> 画图标
		-> g_postDrawIconEvent（此时缓冲DC是图标，把缓冲图像复制到m_iconBufferImg，然后把背景和图标画回缓冲DC）
		-> g_fileListEndPaintEvent（把缓冲DC复制回窗口DC） -> EndPaint
	
	3. 不接管渲染时（XP下，由原窗口过程渲染图标）：
		（原窗口过程）文件列表WM_PAINT -> BeginPaint -> 父窗口WM_ERASEBKGND -> g_preDrawBackgroundEvent
	    -> 画背景（用m_wallpaperImg）-> g_postDrawBackgroundEvent -> g_fileListBeginPaintEvent（把窗口DC替换成缓冲DC）
		-> 画图标 -> g_postDrawIconEvent -> g_fileListEndPaintEvent（把缓冲DC复制回窗口DC） -> EndPaint
	*/

	// 处理size、接管paint
	void BufferedRendering::OnFileListWndProc(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& res, bool& pass)
	{
		switch (message)
		{
		case WM_SIZE:
			g_global.m_wndSize = { LOWORD(lParam), HIWORD(lParam) };
			InitDC();

			res = 1;
			return g_fileListWndSizeEvent(g_global.m_wndSize.cx, g_global.m_wndSize.cy);

		case WM_PAINT:
		{
			_RPTFW0(_CRT_WARN, L"---------- WM_PAINT ----------\n");
			// 交给原窗口过程
			if (!m_controlRendering)
			{
				_RPTFW0(_CRT_WARN, L"渲染路线3：不接管渲染\n");
				return;
			}
			if (g_global.m_needUpdateIcon)
			{
				_RPTFW0(_CRT_WARN, L"渲染路线2：需要更新图标层\n");
				return;
			}
			_RPTFW0(_CRT_WARN, L"渲染路线1：不需要更新图标层\n");

			PAINTSTRUCT paint;
			g_global.m_isInBeginPaint = true;
			HDC hdc = BeginPaint(g_global.m_fileListWnd, &paint);
			g_global.m_isInBeginPaint = false;
			g_fileListBeginPaintEvent(&paint, hdc);

			int x = paint.rcPaint.left;
			int y = paint.rcPaint.top;
			int width = paint.rcPaint.right - paint.rcPaint.left;
			int height = paint.rcPaint.bottom - paint.rcPaint.top;

			// 背景层
			SendMessage(g_global.m_parentWnd, WM_ERASEBKGND, (WPARAM)hdc, 0);
			// 图标层
			m_iconBufferImg.AlphaBlend(hdc, x, y, width, height, x, y, width, height);
			g_postDrawIconEvent(hdc);

			g_fileListEndPaintEvent(&paint);
			EndPaint(g_global.m_fileListWnd, &paint);
			res = 1;
			pass = false;
		}
		}
	}

	// 画背景
	void BufferedRendering::OnParentWndProc(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& res, bool& pass)
	{
		switch (message)
		{
		case WM_ERASEBKGND: // BUG：当顶级窗口是WorkerW时收不到这个消息？可能是发送WM_PRINTCLIENT
			// 此时wParam不一定是m_bufferDC，因为comctl内部也用了双缓冲

			RECT rect;
			GetClipBox((HDC)wParam, &rect);
			int x = rect.left;
			int y = rect.top;
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;
			_RPTFW4(_CRT_WARN, L"WM_ERASEBKGND (%4d, %4d) %4d * %4d\n", x, y, width, height);

			// 画背景
			bool _pass = true;
			g_preDrawBackgroundEvent((HDC&)wParam, _pass);
			if (_pass)
			{
				if (!g_global.m_isInBeginPaint)
					CallWindowProc(g_global.m_oldParentWndProc, g_global.m_parentWnd, message, wParam, lParam);
				else
				{
					// 已知只有XP下BeginPaint才会擦除背景
					// 此时wParam是窗口DC，直接画上去会闪烁，所以禁用XP下BeginPaint擦背景，自己画背景
					// XP下用PaintDesktop画背景，但是这个函数不能画在内存DC，所以只好自己画背景到缓冲DC
					m_wallpaperImg.BitBlt(m_bufferDC, x, y, width, height, x, y);
				}
			}
			g_postDrawBackgroundEvent((HDC&)wParam);

			// 准备更新图标层
			if (m_controlRendering && g_global.m_needUpdateIcon)
			{
				// 由于一次WM_PAINT中WM_ERASEBKGND可能被发送多次，这里不能设置g_global.m_needUpdateIcon = false
				m_isUpdatingIcon = true;

				// 背景复制到m_bgBackupImg
				HDC bgBackupDC = m_bgBackupImg.GetDC();
				// 用裁剪区防止多次WM_ERASEBKGND覆盖之前的内容
				SelectClipRgn(bgBackupDC, m_bgBackupClipRgn.get());
				BitBlt(bgBackupDC, x, y, width, height, (HDC)wParam, x, y, SRCCOPY);
				SelectClipRgn(bgBackupDC, NULL);
				m_bgBackupImg.ReleaseDC();

				// 设置背景备份的裁剪区，防止多次画背景覆盖之前的背景备份
				std::unique_ptr<HRGN, HRGNDeleter> rgn(CreateRectRgnIndirect(&rect));
				CombineRgn(m_bgBackupClipRgn.get(), m_bgBackupClipRgn.get(), rgn.get(), RGN_DIFF);

				// wParam alpha清0，准备画图标
				FillRect((HDC)wParam, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
			}

			// 不需要上级CallWindowProc了
			res = 1;
			pass = false;
		}
	}

	// 取画图标层结果
	void BufferedRendering::PostDrawIcon(HDC& hdc)
	{
		// 此时hdc不一定是m_bufferDC，comctl内部也用了双缓冲

		// 现在hdc是图标层
		if (m_controlRendering && m_isUpdatingIcon)
		{
			m_isUpdatingIcon = false;
			g_global.m_needUpdateIcon = false;

			// 恢复背景备份的裁剪区
			SetRectRgn(m_bgBackupClipRgn.get(), 0, 0, g_global.m_wndSize.cx, g_global.m_wndSize.cy);

			RECT rect;
			GetClipBox(hdc, &rect);
			int x = rect.left;
			int y = rect.top;
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;
			_RPTFW4(_CRT_WARN, L"取图标层 (%4d, %4d) %4d * %4d\n", x, y, width, height);

			// 取图标层
			BitBlt(m_iconBufferImg.GetDC(), x, y, width, height, hdc, x, y, SRCCOPY);
			m_iconBufferImg.ReleaseDC();

			// 处理GDI渲染alpha为0的问题
			_ASSERT(m_iconBufferImg.GetBPP() == 32); // 32位图
			for (int i = 0; i < height; i++)
			{
				BYTE* pPixel = (BYTE*)m_iconBufferImg.GetPixelAddress(x, y + i);
				for (int j = 0; j < width; j++, pPixel += 4)
				{
					// alpha = 0，RGB != 0，说明是GDI渲染的
					if (pPixel[3] == 0 && (pPixel[0] != 0 || pPixel[1] != 0 || pPixel[2] != 0))
						pPixel[3] = 255;
				}
			}


			// 背景层
			m_bgBackupImg.BitBlt(hdc, x, y, width, height, x, y);
			// 图标层
			m_iconBufferImg.AlphaBlend(hdc, x, y, width, height, x, y, width, height);
		}
	}

	// 把窗口DC替换成缓冲DC
	void BufferedRendering::OnFileListBeginPaint(LPPAINTSTRUCT lpPaint, HDC& res)
	{
		_RPTFW0(_CRT_WARN, L"BeginPaint\n");
		if (res != NULL)
		{
			m_originalDC = res;
			res = lpPaint->hdc = m_bufferDC;

			std::unique_ptr<HRGN, HRGNDeleter> rgn(CreateRectRgnIndirect(&lpPaint->rcPaint));
			SelectClipRgn(m_bufferDC, rgn.get());
		}
	}

	// 把缓冲DC复制回窗口DC
	void BufferedRendering::OnFileListEndPaint(LPPAINTSTRUCT lpPaint)
	{
		_RPTFW0(_CRT_WARN, L"EndPaint\n");
		if (lpPaint->hdc != NULL && m_originalDC != NULL)
		{
			_ASSERT(lpPaint->hdc == m_bufferDC);
			lpPaint->hdc = m_originalDC;

			SelectClipRgn(m_bufferDC, NULL);

			int x = lpPaint->rcPaint.left;
			int y = lpPaint->rcPaint.top;
			int width = lpPaint->rcPaint.right - lpPaint->rcPaint.left;
			int height = lpPaint->rcPaint.bottom - lpPaint->rcPaint.top;
			m_bufferImg.BitBlt(m_originalDC, x, y, width, height, x, y);
		}
	}
}
