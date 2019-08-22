﻿#include "stdafx.h"
#include "VideoPlayer.h"
#include <Dvdmedia.h>


VideoPlayer::VideoPlayer(LPCWSTR fileName, HRESULT* phr) :
	CBaseVideoRenderer(CLSID_NULL, _T("Renderer"), NULL, &(*phr = NOERROR))
{
	AddRef(); // 防止多次析构

	if (FAILED(*phr = m_graph.CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER))) return;
	if (FAILED(*phr = m_graph.QueryInterface(&m_control))) return;
	if (FAILED(*phr = m_graph.QueryInterface(&m_seeking))) return;
	m_seeking->SetTimeFormat(&TIME_FORMAT_FRAME);
	if (FAILED(*phr = m_graph.QueryInterface(&m_event))) return;
	if (FAILED(*phr = m_graph.QueryInterface(&m_basicAudio))) return;
	if (FAILED(*phr = m_audioRenderer.CoCreateInstance(CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER))
		&& FAILED(*phr = m_audioRenderer.CoCreateInstance(CLSID_AudioRender, NULL, CLSCTX_INPROC_SERVER))) return;

	if (FAILED(*phr = m_graph->AddSourceFilter(fileName, L"Source", &m_source))) return;
	if (FAILED(*phr = m_graph->AddFilter(this, L"Video Renderer"))) return;
	if (FAILED(*phr = m_graph->AddFilter(m_audioRenderer, L"Audio Renderer"))) return;

	// 连接源和渲染器
	{
		CComPtr<IEnumPins> enumPins;
		CComPtr<IPin> sourcePin;
		CComPtr<IFilterGraph2> graph2;
		if (FAILED(*phr = m_graph.QueryInterface(&graph2))) return;

		if (FAILED(*phr = m_source->EnumPins(&enumPins))) return;
		while (enumPins->Next(1, &sourcePin, NULL) == S_OK)
		{
			if (FAILED(*phr = graph2->RenderEx(sourcePin, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL)))
			{
				sourcePin.Release();
				return;
			}
			sourcePin.Release();
		}
	}

	// 获取视频尺寸
	CMediaType mediaType;
	if (FAILED(*phr = GetPin(0)->ConnectionMediaType(&mediaType))) return;
	if (mediaType.formattype == FORMAT_VideoInfo)
	{
		const auto info = reinterpret_cast<VIDEOINFOHEADER*>(mediaType.pbFormat);
		m_videoSize.cx = info->bmiHeader.biWidth;
		m_videoSize.cy = info->bmiHeader.biHeight;
	}
	else if (mediaType.formattype == FORMAT_VideoInfo2)
	{
		const auto info = reinterpret_cast<VIDEOINFOHEADER2*>(mediaType.pbFormat);
		m_videoSize.cx = info->bmiHeader.biWidth;
		m_videoSize.cy = info->bmiHeader.biHeight;
	}
	else
	{
		*phr = E_UNEXPECTED;
		return;
	}

	*phr = S_OK;
}

VideoPlayer::~VideoPlayer()
{
	SetNotifyWindow(NULL, 0);
	StopVideo();
}


void VideoPlayer::RunVideo()
{
	if (m_control.p != NULL)
		m_control->Run();
}

void VideoPlayer::PauseVideo()
{
	if (m_control.p != NULL)
		m_control->Pause();
}

void VideoPlayer::StopVideo()
{
	if (m_control.p != NULL)
		m_control->Stop();
}

void VideoPlayer::SeekVideo(LONGLONG pos)
{
	if (m_seeking.p != NULL)
		m_seeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning | AM_SEEKING_NoFlush, NULL, AM_SEEKING_NoPositioning);
}


void VideoPlayer::GetVideoSize(SIZE& size)
{
	size = m_videoSize;
}

int VideoPlayer::GetVolume()
{
	if (m_basicAudio.p == NULL)
		return 0;
	long volume = 0;
	if (SUCCEEDED(m_basicAudio->get_Volume(&volume)))
		return volume / 100;
	return 0;
}

void VideoPlayer::SetVolume(int volume)
{
	if (m_basicAudio.p == NULL)
		return;
	if (volume < -100)
		volume = -100;
	else if (volume > 0)
		volume = 0;
	m_basicAudio->put_Volume(volume * 100);
}


void VideoPlayer::SetOnPresent(std::function<void(IMediaSample*)> onPresent)
{
	m_onPresent = std::move(onPresent);
}

void VideoPlayer::SetNotifyWindow(HWND hwnd, UINT messageID)
{
	if (m_event.p == NULL)
		return;
	m_event->SetNotifyWindow(reinterpret_cast<OAHWND>(hwnd), messageID, reinterpret_cast<LONG_PTR>(m_event.p));
}


// CBaseVideoRenderer
HRESULT VideoPlayer::CheckMediaType(const CMediaType * mediaType)
{
	if (mediaType->majortype == MEDIATYPE_Video && mediaType->subtype == MEDIASUBTYPE_RGB32)
		return S_OK;
	return VFW_E_INVALIDMEDIATYPE;
}

HRESULT VideoPlayer::DoRenderSample(IMediaSample *pMediaSample)
{
	//if (!m_onPresent._Empty())
		m_onPresent(pMediaSample);
	return S_OK;
}
