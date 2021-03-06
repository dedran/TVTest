// MediaViewer.cpp: CMediaViewer クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <Dvdmedia.h>
#include "MediaViewer.h"
#include "StdUtil.h"
#include "../DirectShowFilter/BonSrcFilter.h"
#ifdef BONTSENGINE_MPEG2_SUPPORT
#include "../DirectShowFilter/Mpeg2ParserFilter.h"
#endif
#ifdef BONTSENGINE_H264_SUPPORT
#include "../DirectShowFilter/H264ParserFilter.h"
#endif
#ifdef BONTSENGINE_H265_SUPPORT
#include "../DirectShowFilter/H265ParserFilter.h"
#endif
#include "../Common/DebugDef.h"

#pragma comment(lib,"quartz.lib")


//const CLSID CLSID_NullRenderer = {0xc1f400a4, 0x3f08, 0x11d3, {0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37}};
EXTERN_C const CLSID CLSID_NullRenderer;

static const DWORD LOCK_TIMEOUT = 2000;


static HRESULT SetVideoMediaType(CMediaType *pMediaType, BYTE VideoStreamType, int Width, int Height)
{
	static const REFERENCE_TIME TIME_PER_FRAME =
		static_cast<REFERENCE_TIME>(10000000.0 / 29.97 + 0.5);

	switch (VideoStreamType) {
#ifdef BONTSENGINE_MPEG2_SUPPORT
	case STREAM_TYPE_MPEG2_VIDEO:
		// MPEG-2
		{
			// 映像メディアフォーマット設定
			pMediaType->InitMediaType();
			pMediaType->SetType(&MEDIATYPE_Video);
			pMediaType->SetSubtype(&MEDIASUBTYPE_MPEG2_VIDEO);
			pMediaType->SetVariableSize();
			pMediaType->SetTemporalCompression(TRUE);
			pMediaType->SetSampleSize(0);
			pMediaType->SetFormatType(&FORMAT_MPEG2Video);
			// フォーマット構造体確保
			MPEG2VIDEOINFO *pVideoInfo =
				pointer_cast<MPEG2VIDEOINFO *>(pMediaType->AllocFormatBuffer(sizeof(MPEG2VIDEOINFO)));
			if (!pVideoInfo)
				return E_OUTOFMEMORY;
			::ZeroMemory(pVideoInfo, sizeof(MPEG2VIDEOINFO));
			// ビデオヘッダ設定
			VIDEOINFOHEADER2 &VideoHeader = pVideoInfo->hdr;
			//::SetRect(&VideoHeader.rcSource, 0, 0, Width, Height);
			VideoHeader.AvgTimePerFrame = TIME_PER_FRAME;
			VideoHeader.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
			VideoHeader.bmiHeader.biWidth = Width;
			VideoHeader.bmiHeader.biHeight = Height;
		}
		break;
#endif	// BONTSENGINE_MPEG2_SUPPORT

#ifdef BONTSENGINE_H264_SUPPORT
	case STREAM_TYPE_H264:
		// H.264
		{
			pMediaType->InitMediaType();
			pMediaType->SetType(&MEDIATYPE_Video);
			pMediaType->SetSubtype(&MEDIASUBTYPE_H264);
			pMediaType->SetVariableSize();
			pMediaType->SetTemporalCompression(TRUE);
			pMediaType->SetSampleSize(0);
			pMediaType->SetFormatType(&FORMAT_VideoInfo);
			VIDEOINFOHEADER *pVideoInfo =
				pointer_cast<VIDEOINFOHEADER *>(pMediaType->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
			if (!pVideoInfo)
				return E_OUTOFMEMORY;
			::ZeroMemory(pVideoInfo, sizeof(VIDEOINFOHEADER));
			pVideoInfo->dwBitRate = 32000000;
			pVideoInfo->AvgTimePerFrame = TIME_PER_FRAME;
			pVideoInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			pVideoInfo->bmiHeader.biWidth = Width;
			pVideoInfo->bmiHeader.biHeight = Height;
			pVideoInfo->bmiHeader.biCompression = MAKEFOURCC('h','2','6','4');
		}
		break;
#endif	// BONTSENGINE_H264_SUPPORT

#ifdef BONTSENGINE_H265_SUPPORT
	case STREAM_TYPE_H265:
		// H.265
		{
			pMediaType->InitMediaType();
			pMediaType->SetType(&MEDIATYPE_Video);
			pMediaType->SetSubtype(&MEDIASUBTYPE_HEVC);
			pMediaType->SetVariableSize();
			pMediaType->SetTemporalCompression(TRUE);
			pMediaType->SetSampleSize(0);
			pMediaType->SetFormatType(&FORMAT_VideoInfo);
			VIDEOINFOHEADER *pVideoInfo =
				pointer_cast<VIDEOINFOHEADER *>(pMediaType->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
			if (!pVideoInfo)
				return E_OUTOFMEMORY;
			::ZeroMemory(pVideoInfo, sizeof(VIDEOINFOHEADER));
			pVideoInfo->dwBitRate = 32000000;
			pVideoInfo->AvgTimePerFrame = TIME_PER_FRAME;
			pVideoInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			pVideoInfo->bmiHeader.biWidth = Width;
			pVideoInfo->bmiHeader.biHeight = Height;
			pVideoInfo->bmiHeader.biCompression = MAKEFOURCC('H','E','V','C');
		}
		break;
#endif	// BONTSENGINE_H265_SUPPORT

	default:
		return E_UNEXPECTED;
	}

	return S_OK;
}


//////////////////////////////////////////////////////////////////////
// 構築/消滅
//////////////////////////////////////////////////////////////////////

CMediaViewer::CMediaViewer(CMediaDecoder::IEventHandler *pEventHandler)
	: CMediaDecoder(pEventHandler, 1UL, 0UL)
	, m_bInit(false)

	, m_pFilterGraph(NULL)
	, m_pMediaControl(NULL)

	, m_pSrcFilter(NULL)

	, m_pAudioDecoder(NULL)

	, m_pVideoDecoderFilter(NULL)

	, m_pVideoRenderer(NULL)
	, m_pAudioRenderer(NULL)

	, m_pszAudioFilterName(NULL)
	, m_pAudioFilter(NULL)

	, m_pVideoParserFilter(NULL)
	, m_pVideoParser(NULL)

	, m_pszVideoDecoderName(NULL)

	, m_pMp2DemuxFilter(NULL)
	, m_pMp2DemuxVideoMap(NULL)
	, m_pMp2DemuxAudioMap(NULL)

	, m_wVideoEsPID(PID_INVALID)
	, m_wAudioEsPID(PID_INVALID)
	, m_MapAudioPID(PID_INVALID)

	, m_wVideoWindowX(0)
	, m_wVideoWindowY(0)
	, m_VideoInfo()
	, m_hOwnerWnd(NULL)
#ifdef _DEBUG
	, m_dwRegister(0)
#endif

	, m_VideoRendererType(CVideoRenderer::RENDERER_UNDEFINED)
	, m_pszAudioRendererName(NULL)
	, m_VideoStreamType(STREAM_TYPE_UNINITIALIZED)
	, m_AudioStreamType(STREAM_TYPE_UNINITIALIZED)
	, m_ForceAspectX(0)
	, m_ForceAspectY(0)
	, m_ViewStretchMode(STRETCH_KEEPASPECTRATIO)
	, m_bNoMaskSideCut(false)
	, m_bIgnoreDisplayExtension(false)
	, m_bClipToDevice(true)
	, m_bUseAudioRendererClock(true)
	, m_b1SegMode(false)
	, m_bAdjustAudioStreamTime(false)
	, m_bEnablePTSSync(false)
	, m_bAdjust1SegVideoSampleTime(true)
	, m_bAdjust1SegFrameRate(true)
	, m_BufferSize(0)
	, m_InitialPoolPercentage(0)
	, m_PacketInputWait(0)
	, m_pVideoStreamCallback(NULL)
	, m_pAudioStreamCallback(NULL)
	, m_pAudioStreamCallbackParam(NULL)
	, m_pImageMixer(NULL)
{
	// COMライブラリ初期化
	//::CoInitialize(NULL);
}

CMediaViewer::~CMediaViewer()
{
	CloseViewer();

	if (m_pszAudioFilterName)
		delete [] m_pszAudioFilterName;

	// COMライブラリ開放
	//::CoUninitialize();
}


void CMediaViewer::Reset()
{
	TRACE(TEXT("CMediaViewer::Reset()\n"));

	CTryBlockLock Lock(&m_DecoderLock);
	Lock.TryLock(LOCK_TIMEOUT);

	Flush();

	SetVideoPID(PID_INVALID);
	SetAudioPID(PID_INVALID);

	//m_VideoInfo.Reset();
}

const bool CMediaViewer::InputMedia(CMediaData *pMediaData, const DWORD dwInputIndex)
{
	CBlockLock Lock(&m_DecoderLock);

	/*
	if(dwInputIndex >= GetInputNum())return false;

	CTsPacket *pTsPacket = dynamic_cast<CTsPacket *>(pMediaData);

	// 入力メディアデータは互換性がない
	if(!pTsPacket)return false;
	*/
	CTsPacket *pTsPacket = static_cast<CTsPacket *>(pMediaData);

	// フィルタグラフに入力
	if (m_pSrcFilter
			&& pTsPacket->GetPID() != 0x1FFF
			&& !pTsPacket->IsScrambled()) {
		return m_pSrcFilter->InputMedia(pTsPacket);
	}

	return false;
}

bool CMediaViewer::OpenViewer(
	HWND hOwnerHwnd, HWND hMessageDrainHwnd,
	CVideoRenderer::RendererType RendererType,
	BYTE VideoStreamType,
	LPCWSTR pszVideoDecoder, LPCWSTR pszAudioDevice)
{
	bool bNoVideo = false;

	switch (VideoStreamType) {
	default:
		SetError(TEXT("対応していない映像形式です。"));
		return false;
	case STREAM_TYPE_INVALID:
		bNoVideo = true;
		break;
#ifdef BONTSENGINE_MPEG2_SUPPORT
	case STREAM_TYPE_MPEG2_VIDEO:
#endif
#ifdef BONTSENGINE_H264_SUPPORT
	case STREAM_TYPE_H264:
#endif
#ifdef BONTSENGINE_H265_SUPPORT
	case STREAM_TYPE_H265:
#endif
		break;
	}

	CTryBlockLock Lock(&m_DecoderLock);
	if (!Lock.TryLock(LOCK_TIMEOUT)) {
		SetError(TEXT("タイムアウトエラーです。"));
		return false;
	}

	if (m_bInit) {
		SetError(TEXT("既にフィルタグラフが構築されています。"));
		return false;
	}

	TRACE(TEXT("CMediaViewer::OpenViewer() フィルタグラフ作成開始\n"));

	HRESULT hr=S_OK;

	IPin *pOutput=NULL;
	IPin *pOutputVideo=NULL;
	IPin *pOutputAudio=NULL;

	try {
		// フィルタグラフマネージャを構築する
		hr=::CoCreateInstance(CLSID_FilterGraph,NULL,CLSCTX_INPROC_SERVER,
				IID_IGraphBuilder,pointer_cast<LPVOID*>(&m_pFilterGraph));
		if (FAILED(hr)) {
			throw CBonException(hr,TEXT("フィルタグラフマネージャを作成できません。"));
		}

		SendDecoderEvent(EID_FILTER_GRAPH_INITIALIZE, m_pFilterGraph);

#ifdef _DEBUG
		AddToRot(m_pFilterGraph, &m_dwRegister);
#endif

		// IMediaControlインタフェースのクエリー
		hr=m_pFilterGraph->QueryInterface(IID_IMediaControl, pointer_cast<void**>(&m_pMediaControl));
		if (FAILED(hr)) {
			throw CBonException(hr,TEXT("メディアコントロールを取得できません。"));
		}

		Trace(CTracer::TYPE_INFORMATION, TEXT("ソースフィルタの接続中..."));

		/* CBonSrcFilter */
		{
			// インスタンス作成
			m_pSrcFilter = static_cast<CBonSrcFilter*>(CBonSrcFilter::CreateInstance(NULL, &hr));
			if (m_pSrcFilter == NULL || FAILED(hr))
				throw CBonException(hr, TEXT("ソースフィルタを作成できません。"));
			m_pSrcFilter->SetOutputWhenPaused(RendererType == CVideoRenderer::RENDERER_DEFAULT);
			// フィルタグラフに追加
			hr = m_pFilterGraph->AddFilter(m_pSrcFilter, L"BonSrcFilter");
			if (FAILED(hr))
				throw CBonException(hr, TEXT("ソースフィルタをフィルタグラフに追加できません。"));
			// 出力ピンを取得
			pOutput = DirectShowUtil::GetFilterPin(m_pSrcFilter, PINDIR_OUTPUT);
			if (pOutput==NULL)
				throw CBonException(TEXT("ソースフィルタの出力ピンを取得できません。"));
			m_pSrcFilter->EnableSync(m_bEnablePTSSync, m_b1SegMode);
			if (m_BufferSize != 0)
				m_pSrcFilter->SetBufferSize(m_BufferSize);
			m_pSrcFilter->SetInitialPoolPercentage(m_InitialPoolPercentage);
			m_pSrcFilter->SetInputWait(m_PacketInputWait);
		}

		Trace(CTracer::TYPE_INFORMATION, TEXT("MPEG-2 Demultiplexerフィルタの接続中..."));

		/* MPEG-2 Demultiplexer */
		{
			IMpeg2Demultiplexer *pMpeg2Demuxer;

			hr=::CoCreateInstance(CLSID_MPEG2Demultiplexer,NULL,
					CLSCTX_INPROC_SERVER,IID_IBaseFilter,
					pointer_cast<LPVOID*>(&m_pMp2DemuxFilter));
			if (FAILED(hr))
				throw CBonException(hr,TEXT("MPEG-2 Demultiplexerフィルタを作成できません。"),
									TEXT("MPEG-2 Demultiplexerフィルタがインストールされているか確認してください。"));
			hr=DirectShowUtil::AppendFilterAndConnect(m_pFilterGraph,
								m_pMp2DemuxFilter,L"Mpeg2Demuxer",&pOutput);
			if (FAILED(hr))
				throw CBonException(hr,TEXT("MPEG-2 Demultiplexerをフィルタグラフに追加できません。"));
			// この時点でpOutput==NULLのはずだが念のため
			SAFE_RELEASE(pOutput);

			// IMpeg2Demultiplexerインタフェースのクエリー
			hr=m_pMp2DemuxFilter->QueryInterface(IID_IMpeg2Demultiplexer,
												 pointer_cast<void**>(&pMpeg2Demuxer));
			if (FAILED(hr))
				throw CBonException(hr,TEXT("MPEG-2 Demultiplexerインターフェースを取得できません。"),
									TEXT("互換性のないスプリッタの優先度がMPEG-2 Demultiplexerより高くなっている可能性があります。"));

			if (!bNoVideo) {
				CMediaType MediaTypeVideo;

				// 映像メディアフォーマット設定
				hr = SetVideoMediaType(&MediaTypeVideo, VideoStreamType, 1920, 1080);
				if (FAILED(hr))
					throw CBonException(TEXT("メモリが確保できません。"));
				// 映像出力ピン作成
				hr = pMpeg2Demuxer->CreateOutputPin(&MediaTypeVideo, L"Video", &pOutputVideo);
				if (FAILED(hr)) {
					pMpeg2Demuxer->Release();
					throw CBonException(hr, TEXT("MPEG-2 Demultiplexerの映像出力ピンを作成できません。"));
				}
			}

			// 音声メディアフォーマット設定
			CMediaType MediaTypeAudio;
			MediaTypeAudio.InitMediaType();
			MediaTypeAudio.SetType(&MEDIATYPE_Audio);
			MediaTypeAudio.SetSubtype(&MEDIASUBTYPE_NULL);
			MediaTypeAudio.SetVariableSize();
			MediaTypeAudio.SetTemporalCompression(TRUE);
			MediaTypeAudio.SetSampleSize(0);
			MediaTypeAudio.SetFormatType(&FORMAT_None);
			// 音声出力ピン作成
			hr=pMpeg2Demuxer->CreateOutputPin(&MediaTypeAudio,L"Audio",&pOutputAudio);
			pMpeg2Demuxer->Release();
			if (FAILED(hr))
				throw CBonException(hr,TEXT("MPEG-2 Demultiplexerの音声出力ピンを作成できません。"));
			if (pOutputVideo) {
				// 映像出力ピンのIMPEG2PIDMapインタフェースのクエリー
				hr=pOutputVideo->QueryInterface(__uuidof(IMPEG2PIDMap),pointer_cast<void**>(&m_pMp2DemuxVideoMap));
				if (FAILED(hr))
					throw CBonException(hr,TEXT("映像出力ピンのIMPEG2PIDMapを取得できません。"));
			}
			// 音声出力ピンのIMPEG2PIDMapインタフェースのクエリ
			hr=pOutputAudio->QueryInterface(__uuidof(IMPEG2PIDMap),pointer_cast<void**>(&m_pMp2DemuxAudioMap));
			if (FAILED(hr))
				throw CBonException(hr,TEXT("音声出力ピンのIMPEG2PIDMapを取得できません。"));
		}

		// 映像パーサフィルタの接続
		switch (VideoStreamType) {
#ifdef BONTSENGINE_MPEG2_SUPPORT
		case STREAM_TYPE_MPEG2_VIDEO:
			{
				Trace(CTracer::TYPE_INFORMATION, TEXT("MPEG-2パーサフィルタの接続中..."));

				// インスタンス作成
				CMpeg2ParserFilter *pMpeg2Parser =
					static_cast<CMpeg2ParserFilter*>(CMpeg2ParserFilter::CreateInstance(NULL, &hr));
				if (!pMpeg2Parser || FAILED(hr))
					throw CBonException(hr, TEXT("MPEG-2パーサフィルタを作成できません。"));
				m_pVideoParserFilter = pMpeg2Parser;
				m_pVideoParser = pMpeg2Parser;
				// フィルタの追加と接続
				hr = DirectShowUtil::AppendFilterAndConnect(
					m_pFilterGraph, pMpeg2Parser, L"Mpeg2ParserFilter", &pOutputVideo);
				if (FAILED(hr))
					throw CBonException(hr, TEXT("MPEG-2パーサフィルタをフィルタグラフに追加できません。"));
			}
			break;
#endif	// BONTSENGINE_MPEG2_SUPPORT

#ifdef BONTSENGINE_H264_SUPPORT
		case STREAM_TYPE_H264:
			{
				Trace(CTracer::TYPE_INFORMATION, TEXT("H.264パーサフィルタの接続中..."));

				// インスタンス作成
				CH264ParserFilter *pH264Parser =
					static_cast<CH264ParserFilter*>(CH264ParserFilter::CreateInstance(NULL, &hr));
				if (!pH264Parser || FAILED(hr))
					throw CBonException(TEXT("H.264パーサフィルタを作成できません。"));
				m_pVideoParserFilter = pH264Parser;
				m_pVideoParser = pH264Parser;
				// フィルタの追加と接続
				hr = DirectShowUtil::AppendFilterAndConnect(
					m_pFilterGraph, pH264Parser, L"H264ParserFilter", &pOutputVideo);
				if (FAILED(hr))
					throw CBonException(hr,TEXT("H.264パーサフィルタをフィルタグラフに追加できません。"));
			}
			break;
#endif	// BONTSENGINE_H264_SUPPORT

#ifdef BONTSENGINE_H265_SUPPORT
		case STREAM_TYPE_H265:
			{
				Trace(CTracer::TYPE_INFORMATION, TEXT("H.265パーサフィルタの接続中..."));

				// インスタンス作成
				CH265ParserFilter *pH265Parser =
					static_cast<CH265ParserFilter*>(CH265ParserFilter::CreateInstance(NULL, &hr));
				if (!pH265Parser || FAILED(hr))
					throw CBonException(TEXT("H.265パーサフィルタを作成できません。"));
				m_pVideoParserFilter = pH265Parser;
				m_pVideoParser = pH265Parser;
				// フィルタの追加と接続
				hr = DirectShowUtil::AppendFilterAndConnect(
					m_pFilterGraph, pH265Parser, L"H265ParserFilter", &pOutputVideo);
				if (FAILED(hr))
					throw CBonException(hr,TEXT("H.265パーサフィルタをフィルタグラフに追加できません。"));
			}
			break;
#endif	// BONTSENGINE_H265_SUPPORT
		}

		Trace(CTracer::TYPE_INFORMATION, TEXT("音声デコーダの接続中..."));

#if 1
		/* CAudioDecFilter */
		{
			// CAudioDecFilterインスタンス作成
			m_pAudioDecoder = static_cast<CAudioDecFilter*>(CAudioDecFilter::CreateInstance(NULL, &hr));
			if (!m_pAudioDecoder || FAILED(hr))
				throw CBonException(hr,TEXT("音声デコーダフィルタを作成できません。"));
			// フィルタの追加と接続
			hr=DirectShowUtil::AppendFilterAndConnect(
				m_pFilterGraph,m_pAudioDecoder,L"AudioDecFilter",&pOutputAudio);
			if (FAILED(hr))
				throw CBonException(hr,TEXT("音声デコーダフィルタをフィルタグラフに追加できません。"));

			SetAudioDecoderType(m_AudioStreamType);

			m_pAudioDecoder->SetEventHandler(this);
			m_pAudioDecoder->SetJitterCorrection(m_bAdjustAudioStreamTime);
			if (m_pAudioStreamCallback)
				m_pAudioDecoder->SetStreamCallback(m_pAudioStreamCallback,
												   m_pAudioStreamCallbackParam);
		}
#else
		/*
			外部AACデコーダを利用すると、チャンネル数が切り替わった際に音が出なくなる、
			デュアルモノラルがステレオとして再生される、といった問題が出る
		*/

		/* CAacParserFilter */
		{
			CAacParserFilter *m_pAacParser;
			// CAacParserFilterインスタンス作成
			m_pAacParser=static_cast<CAacParserFilter*>(CAacParserFilter::CreateInstance(NULL, &hr));
			if (!m_pAacParser || FAILED(hr))
				throw CBonException(hr,TEXT("AACパーサフィルタを作成できません。"));
			// フィルタの追加と接続
			hr=DirectShowUtil::AppendFilterAndConnect(
				m_pFilterGraph,m_pAacParser,L"AacParserFilter",&pOutputAudio);
			if (FAILED(hr))
				throw CBonException(TEXT("AACパーサフィルタをフィルタグラフに追加できません。"));
			m_pAacParser->Release();
		}

		/* AACデコーダー */
		{
			CDirectShowFilterFinder FilterFinder;

			// 検索
			if(!FilterFinder.FindFilter(&MEDIATYPE_Audio,&MEDIASUBTYPE_AAC))
				throw CBonException(TEXT("AACデコーダが見付かりません。"),
									TEXT("AACデコーダがインストールされているか確認してください。"));

			WCHAR szAacDecoder[128];
			CLSID idAac;
			bool bConnectSuccess=false;
			IBaseFilter *pAacDecFilter=NULL;

			for (int i=0;i<FilterFinder.GetFilterCount();i++){
				if (FilterFinder.GetFilterInfo(i,&idAac,szAacDecoder,128)) {
					if (pszAudioDecoder!=NULL && pszAudioDecoder[0]!='\0'
							&& ::lstrcmpi(szAacDecoder,pszAudioDecoder)!=0)
						continue;
					hr=DirectShowUtil::AppendFilterAndConnect(m_pFilterGraph,
							idAac,szAacDecoder,&pAacDecFilter,
							&pOutputAudio);
					if (SUCCEEDED(hr)) {
						TRACE(TEXT("AAC decoder connected : %s\n"),szAacDecoder);
						bConnectSuccess=true;
						break;
					}
				}
			}
			// どれかのフィルタで接続できたか
			if (bConnectSuccess) {
				SAFE_RELEASE(pAacDecFilter);
				//m_pszAacDecoderName=StdUtil::strdup(szAacDecoder);
			} else {
				throw CBonException(TEXT("AACデコーダフィルタをフィルタグラフに追加できません。"),
									TEXT("設定で有効なAACデコーダが選択されているか確認してください。"));
			}
		}
#endif

		/* ユーザー指定の音声フィルタの接続 */
		if (m_pszAudioFilterName) {
			Trace(CTracer::TYPE_INFORMATION, TEXT("音声フィルタの接続中..."));

			// 検索
			bool bConnectSuccess=false;
			CDirectShowFilterFinder FilterFinder;
			if (FilterFinder.FindFilter(&MEDIATYPE_Audio,&MEDIASUBTYPE_PCM)) {
				WCHAR szAudioFilter[128];
				CLSID idAudioFilter;

				for (int i=0;i<FilterFinder.GetFilterCount();i++) {
					if (FilterFinder.GetFilterInfo(i,&idAudioFilter,szAudioFilter,128)) {
						if (::lstrcmpi(m_pszAudioFilterName,szAudioFilter)!=0)
							continue;
						hr=DirectShowUtil::AppendFilterAndConnect(m_pFilterGraph,
								idAudioFilter,szAudioFilter,&m_pAudioFilter,
								&pOutputAudio,NULL,true);
						if (SUCCEEDED(hr)) {
							TRACE(TEXT("音声フィルタ接続 : %s\n"),szAudioFilter);
							bConnectSuccess=true;
						}
						break;
					}
				}
			}
			if (!bConnectSuccess) {
				throw CBonException(hr,
					TEXT("音声フィルタをフィルタグラフに追加できません。"),
					TEXT("音声フィルタが利用できないか、音声デバイスに対応していない可能性があります。"));
			}
		}

		// 映像デコーダの接続
		switch (VideoStreamType) {
#ifdef BONTSENGINE_MPEG2_SUPPORT
		case STREAM_TYPE_MPEG2_VIDEO:
			ConnectVideoDecoder(TEXT("MPEG-2"), MEDIASUBTYPE_MPEG2_VIDEO,
								pszVideoDecoder, &pOutputVideo);
			break;
#endif	// BONTSENGINE_MPEG2_SUPPORT

#ifdef BONTSENGINE_H264_SUPPORT
		case STREAM_TYPE_H264:
			ConnectVideoDecoder(TEXT("H.264"), MEDIASUBTYPE_H264,
								pszVideoDecoder, &pOutputVideo);
			break;
#endif	// BONTSENGINE_H264_SUPPORT

#ifdef BONTSENGINE_H265_SUPPORT
		case STREAM_TYPE_H265:
			ConnectVideoDecoder(TEXT("H.265"), MEDIASUBTYPE_HEVC,
								pszVideoDecoder, &pOutputVideo);
			break;
#endif	// BONTSENGINE_H265_SUPPORT
		}

		m_VideoStreamType = VideoStreamType;

		if (m_pVideoParser) {
			m_pVideoParser->SetVideoInfoCallback(OnVideoInfo, this);
			// madVR は映像サイズの変化時に MediaType を設定しないと新しいサイズが適用されない
			m_pVideoParser->SetAttachMediaType(RendererType == CVideoRenderer::RENDERER_madVR);
			if (m_pVideoStreamCallback)
				m_pVideoParser->SetStreamCallback(m_pVideoStreamCallback);
			ApplyAdjustVideoSampleOptions();
		}

		if (!bNoVideo) {
			Trace(CTracer::TYPE_INFORMATION, TEXT("映像レンダラの構築中..."));

			if (!CVideoRenderer::CreateRenderer(RendererType, &m_pVideoRenderer)) {
				throw CBonException(TEXT("映像レンダラを作成できません。"),
									TEXT("設定で有効なレンダラが選択されているか確認してください。"));
			}
			m_pVideoRenderer->SetClipToDevice(m_bClipToDevice);
			if (!m_pVideoRenderer->Initialize(m_pFilterGraph, pOutputVideo,
											  hOwnerHwnd, hMessageDrainHwnd)) {
				throw CBonException(m_pVideoRenderer->GetLastErrorException());
			}
			m_VideoRendererType = RendererType;
		}

		Trace(CTracer::TYPE_INFORMATION, TEXT("音声レンダラの構築中..."));

		// 音声レンダラ構築
		{
			bool fOK = false;

			if (pszAudioDevice != NULL && pszAudioDevice[0] != '\0') {
				CDirectShowDeviceEnumerator DevEnum;

				if (DevEnum.CreateFilter(CLSID_AudioRendererCategory,
										 pszAudioDevice, &m_pAudioRenderer)) {
					m_pszAudioRendererName=StdUtil::strdup(pszAudioDevice);
					fOK = true;
				}
			}
			if (!fOK) {
				hr = ::CoCreateInstance(CLSID_DSoundRender, NULL,
										CLSCTX_INPROC_SERVER, IID_IBaseFilter,
										pointer_cast<LPVOID*>(&m_pAudioRenderer));
				if (SUCCEEDED(hr)) {
					m_pszAudioRendererName=StdUtil::strdup(TEXT("DirectSound Renderer"));
					fOK = true;
				}
			}
			if (fOK) {
				hr = DirectShowUtil::AppendFilterAndConnect(m_pFilterGraph,
						m_pAudioRenderer, L"Audio Renderer", &pOutputAudio);
				if (SUCCEEDED(hr)) {
#ifdef _DEBUG
					if (pszAudioDevice != NULL && pszAudioDevice[0] != '\0')
						TRACE(TEXT("音声デバイス %s を接続\n"), pszAudioDevice);
#endif
					if (m_bUseAudioRendererClock) {
						IMediaFilter *pMediaFilter;

						if (SUCCEEDED(m_pFilterGraph->QueryInterface(IID_IMediaFilter,
								pointer_cast<void**>(&pMediaFilter)))) {
							IReferenceClock *pReferenceClock;

							if (SUCCEEDED(m_pAudioRenderer->QueryInterface(IID_IReferenceClock,
									pointer_cast<void**>(&pReferenceClock)))) {
								pMediaFilter->SetSyncSource(pReferenceClock);
								pReferenceClock->Release();
								TRACE(TEXT("グラフのクロックに音声レンダラを選択\n"));
							}
							pMediaFilter->Release();
						}
					}
					fOK = true;
				} else {
					fOK = false;
				}
				if (!fOK) {
					hr = m_pFilterGraph->Render(pOutputAudio);
					if (FAILED(hr))
						throw CBonException(hr, TEXT("音声レンダラを接続できません。"),
							TEXT("設定で有効な音声デバイスが選択されているか確認してください。"));
				}
			} else {
				// 音声デバイスが無い?
				// Nullレンダラを繋げておく
				hr = ::CoCreateInstance(CLSID_NullRenderer, NULL,
										CLSCTX_INPROC_SERVER, IID_IBaseFilter,
										pointer_cast<LPVOID*>(&m_pAudioRenderer));
				if (SUCCEEDED(hr)) {
					hr = DirectShowUtil::AppendFilterAndConnect(m_pFilterGraph,
						m_pAudioRenderer, L"Null Audio Renderer", &pOutputAudio);
					if (FAILED(hr)) {
						throw CBonException(hr, TEXT("Null音声レンダラを接続できません。"));
					}
					m_pszAudioRendererName=StdUtil::strdup(TEXT("Null Renderer"));
					TRACE(TEXT("Nullレンダラを接続\n"));
				}
			}
		}

		/*
			デフォルトでMPEG-2 Demultiplexerがグラフのクロックに
			設定されるらしいが、一応設定しておく
		*/
		if (!m_bUseAudioRendererClock) {
			IMediaFilter *pMediaFilter;

			if (SUCCEEDED(m_pFilterGraph->QueryInterface(
					IID_IMediaFilter,pointer_cast<void**>(&pMediaFilter)))) {
				IReferenceClock *pReferenceClock;

				if (SUCCEEDED(m_pMp2DemuxFilter->QueryInterface(
						IID_IReferenceClock,pointer_cast<void**>(&pReferenceClock)))) {
					pMediaFilter->SetSyncSource(pReferenceClock);
					pReferenceClock->Release();
					TRACE(TEXT("グラフのクロックにMPEG-2 Demultiplexerを選択\n"));
				}
				pMediaFilter->Release();
			}
		}

		// オーナウィンドウ設定
		m_hOwnerWnd = hOwnerHwnd;
		RECT rc;
		::GetClientRect(hOwnerHwnd, &rc);
		m_wVideoWindowX = (WORD)rc.right;
		m_wVideoWindowY = (WORD)rc.bottom;

		m_bInit=true;

		if (m_pMp2DemuxVideoMap && m_wVideoEsPID != PID_INVALID) {
			if (!MapVideoPID(m_wVideoEsPID))
				m_wVideoEsPID = PID_INVALID;
		}
		if (m_wAudioEsPID != PID_INVALID) {
			if (!MapAudioPID(m_wAudioEsPID))
				m_wAudioEsPID = PID_INVALID;
		}
	} catch (CBonException &Exception) {
		SetError(Exception);
		if (Exception.GetErrorCode()!=0) {
			TCHAR szText[MAX_ERROR_TEXT_LEN+32];
			int Length;

			Length=::AMGetErrorText(Exception.GetErrorCode(),szText,MAX_ERROR_TEXT_LEN);
			StdUtil::snprintf(szText+Length,_countof(szText)-Length,
							  TEXT("\nエラーコード(HRESULT) 0x%08X"),Exception.GetErrorCode());
			SetErrorSystemMessage(szText);
		}

		SAFE_RELEASE(pOutput);
		SAFE_RELEASE(pOutputVideo);
		SAFE_RELEASE(pOutputAudio);
		CloseViewer();

		TRACE(TEXT("フィルタグラフ構築失敗 : %s\n"), GetLastErrorText());
		return false;
	}

	SAFE_RELEASE(pOutputVideo);
	SAFE_RELEASE(pOutputAudio);

	SendDecoderEvent(EID_FILTER_GRAPH_INITIALIZED, m_pFilterGraph);

	ClearError();

	TRACE(TEXT("フィルタグラフ構築成功\n"));
	return true;
}

void CMediaViewer::CloseViewer()
{
	CTryBlockLock Lock(&m_DecoderLock);
	Lock.TryLock(LOCK_TIMEOUT);

	/*
	if (!m_bInit)
		return;
	*/

	if (m_pFilterGraph) {
		Trace(CTracer::TYPE_INFORMATION, TEXT("フィルタグラフを停止しています..."));
		m_pFilterGraph->Abort();
		Stop();

		SendDecoderEvent(EID_FILTER_GRAPH_FINALIZE, m_pFilterGraph);
	}

	Trace(CTracer::TYPE_INFORMATION, TEXT("COMインスタンスを解放しています..."));

	// COMインスタンスを開放する
	if (m_pVideoRenderer!=NULL) {
		m_pVideoRenderer->Finalize();
	}

	if (m_pImageMixer!=NULL) {
		delete m_pImageMixer;
		m_pImageMixer=NULL;
	}

	if (m_pszVideoDecoderName!=NULL) {
		delete [] m_pszVideoDecoderName;
		m_pszVideoDecoderName=NULL;
	}

	SAFE_RELEASE(m_pVideoDecoderFilter);

	SAFE_RELEASE(m_pAudioDecoder);

	SAFE_RELEASE(m_pAudioRenderer);

	SAFE_RELEASE(m_pVideoParserFilter);
	m_pVideoParser = NULL;

	SAFE_RELEASE(m_pMp2DemuxAudioMap);
	SAFE_RELEASE(m_pMp2DemuxVideoMap);
	SAFE_RELEASE(m_pMp2DemuxFilter);
	m_MapAudioPID = PID_INVALID;

	SAFE_RELEASE(m_pSrcFilter);

	SAFE_RELEASE(m_pAudioFilter);

	SAFE_RELEASE(m_pMediaControl);

#ifdef _DEBUG
	if(m_dwRegister!=0){
		RemoveFromRot(m_dwRegister);
		m_dwRegister = 0;
	}
#endif

	if (m_pFilterGraph) {
		Trace(CTracer::TYPE_INFORMATION, TEXT("フィルタグラフを解放しています..."));
		SendDecoderEvent(EID_FILTER_GRAPH_FINALIZED, m_pFilterGraph);
#ifdef _DEBUG
		TRACE(TEXT("FilterGraph RefCount = %d\n"),DirectShowUtil::GetRefCount(m_pFilterGraph));
#endif
		SAFE_RELEASE(m_pFilterGraph);
	}

	if (m_pVideoRenderer!=NULL) {
		delete m_pVideoRenderer;
		m_pVideoRenderer=NULL;
	}

	if (m_pszAudioRendererName!=NULL) {
		delete [] m_pszAudioRendererName;
		m_pszAudioRendererName=NULL;
	}

	m_VideoStreamType = STREAM_TYPE_UNINITIALIZED;

	m_bInit=false;
}


bool CMediaViewer::IsOpen() const
{
	return m_bInit;
}


bool CMediaViewer::Play()
{
	TRACE(TEXT("CMediaViewer::Play()\n"));

	CTryBlockLock Lock(&m_DecoderLock);
	if (!Lock.TryLock(LOCK_TIMEOUT))
		return false;

	if(!m_pMediaControl)return false;

	// フィルタグラフを再生する

	//return m_pMediaControl->Run()==S_OK;

	if (m_pMediaControl->Run()!=S_OK) {
		int i;
		OAFilterState fs;

		for (i=0;i<20;i++) {
			if (m_pMediaControl->GetState(100,&fs)==S_OK && fs==State_Running)
				return true;
		}
		return false;
	}
	return true;
}


bool CMediaViewer::Stop()
{
	TRACE(TEXT("CMediaViewer::Stop()\n"));

	CTryBlockLock Lock(&m_DecoderLock);
	if (!Lock.TryLock(LOCK_TIMEOUT))
		return false;

	if (!m_pMediaControl)
		return false;

	if (m_pSrcFilter)
		//m_pSrcFilter->Reset();
		m_pSrcFilter->Flush();

	// フィルタグラフを停止する
	return m_pMediaControl->Stop()==S_OK;
}


bool CMediaViewer::Pause()
{
	TRACE(TEXT("CMediaViewer::Pause()\n"));

	CTryBlockLock Lock(&m_DecoderLock);
	if (!Lock.TryLock(LOCK_TIMEOUT))
		return false;

	if (!m_pMediaControl)
		return false;

	if (m_pSrcFilter)
		//m_pSrcFilter->Reset();
		m_pSrcFilter->Flush();

	if (m_pMediaControl->Pause()!=S_OK) {
		int i;
		OAFilterState fs;
		HRESULT hr;

		for (i=0;i<20;i++) {
			hr=m_pMediaControl->GetState(100,&fs);
			if ((hr==S_OK || hr==VFW_S_CANT_CUE) && fs==State_Paused)
				return true;
		}
		return false;
	}
	return true;
}


bool CMediaViewer::Flush()
{
	TRACE(TEXT("CMediaViewer::Flush()\n"));

	/*
	CTryBlockLock Lock(&m_DecoderLock);
	if (!Lock.TryLock(LOCK_TIMEOUT))
		return false;
	*/

	if (!m_pSrcFilter)
		return false;

	m_pSrcFilter->Flush();
	return true;
}


bool CMediaViewer::SetVisible(bool fVisible)
{
	if (m_pVideoRenderer)
		return m_pVideoRenderer->SetVisible(fVisible);
	return false;
}


void CMediaViewer::HideCursor(bool bHide)
{
	if (m_pVideoRenderer)
		m_pVideoRenderer->ShowCursor(!bHide);
}


bool CMediaViewer::RepaintVideo(HWND hwnd,HDC hdc)
{
	if (m_pVideoRenderer)
		return m_pVideoRenderer->RepaintVideo(hwnd,hdc);
	return false;
}


bool CMediaViewer::DisplayModeChanged()
{
	if (m_pVideoRenderer)
		return m_pVideoRenderer->DisplayModeChanged();
	return false;
}


void CMediaViewer::Set1SegMode(bool b1Seg)
{
	if (m_b1SegMode != b1Seg) {
		TRACE(TEXT("CMediaViewer::Set1SegMode(%d)\n"), b1Seg);

		m_b1SegMode = b1Seg;

		if (m_pSrcFilter != NULL)
			m_pSrcFilter->EnableSync(m_bEnablePTSSync, m_b1SegMode);
		ApplyAdjustVideoSampleOptions();
	}
}


bool CMediaViewer::Is1SegMode() const
{
	return m_b1SegMode;
}


bool CMediaViewer::SetVideoPID(const WORD wPID)
{
	// 映像出力ピンにPIDをマッピングする

	CBlockLock Lock(&m_DecoderLock);

	if (wPID == m_wVideoEsPID)
		return true;

	TRACE(TEXT("CMediaViewer::SetVideoPID() %04X <- %04X\n"), wPID, m_wVideoEsPID);

	if (m_pMp2DemuxVideoMap) {
		// 現在のPIDをアンマップ
		if (m_wVideoEsPID != PID_INVALID) {
			ULONG TempPID = m_wVideoEsPID;
			if (m_pMp2DemuxVideoMap->UnmapPID(1UL, &TempPID) != S_OK)
				return false;
		}
	}

	if (!MapVideoPID(wPID)) {
		m_wVideoEsPID = PID_INVALID;
		return false;
	}

	m_wVideoEsPID = wPID;

	return true;
}


bool CMediaViewer::SetAudioPID(const WORD wPID, const bool bUseMap)
{
	// 音声出力ピンにPIDをマッピングする

	CBlockLock Lock(&m_DecoderLock);

	if (wPID == m_wAudioEsPID
			&& (bUseMap || wPID == m_MapAudioPID))
		return true;

	TRACE(TEXT("CMediaViewer::SetAudioPID() %04X <- %04X\n"), wPID, m_wAudioEsPID);

	if (bUseMap && wPID != PID_INVALID && m_MapAudioPID != PID_INVALID) {
		/*
			bUseMap が true の場合、PID を書き換えて音声ストリームを変更する
			IMPEG2PIDMap::MapPID() を呼ぶと再生が一瞬止まるので、それを回避するため
		*/
		if (m_pSrcFilter)
			m_pSrcFilter->MapAudioPID(wPID, m_MapAudioPID);
	} else {
		if (m_pMp2DemuxAudioMap) {
			// 現在のPIDをアンマップ
			if (m_MapAudioPID != PID_INVALID) {
				ULONG TempPID = m_MapAudioPID;
				if (m_pMp2DemuxAudioMap->UnmapPID(1UL, &TempPID) != S_OK)
					return false;
				m_MapAudioPID = PID_INVALID;
			}
		}

		if (!MapAudioPID(wPID)) {
			m_wAudioEsPID = PID_INVALID;
			return false;
		}
	}

	m_wAudioEsPID = wPID;

	return true;
}


WORD CMediaViewer::GetVideoPID() const
{
	return m_wVideoEsPID;
}


WORD CMediaViewer::GetAudioPID() const
{
	return m_wAudioEsPID;
}


void CMediaViewer::OnVideoInfo(const CVideoParser::VideoInfo *pVideoInfo, const LPVOID pParam)
{
	// ビデオ情報の更新
	CMediaViewer *pThis=static_cast<CMediaViewer*>(pParam);

	/*if (pThis->m_VideoInfo != *pVideoInfo)*/ {
		// ビデオ情報の更新
		CBlockLock Lock(&pThis->m_ResizeLock);

		pThis->m_VideoInfo = *pVideoInfo;
		//pThis->AdjustVideoPosition();
	}

	pThis->SendDecoderEvent(EID_VIDEO_SIZE_CHANGED);
}


bool CMediaViewer::AdjustVideoPosition()
{
	// 映像の位置を調整する
	if (m_pVideoRenderer && m_wVideoWindowX > 0 && m_wVideoWindowY > 0
			&& m_VideoInfo.m_OrigWidth > 0 && m_VideoInfo.m_OrigHeight > 0) {
		long WindowWidth, WindowHeight, DestWidth, DestHeight;

		WindowWidth = m_wVideoWindowX;
		WindowHeight = m_wVideoWindowY;
		if (m_ViewStretchMode == STRETCH_FIT) {
			// ウィンドウサイズに合わせる
			DestWidth = WindowWidth;
			DestHeight = WindowHeight;
		} else {
			int AspectX, AspectY;

			if (m_ForceAspectX > 0 && m_ForceAspectY > 0) {
				// アスペクト比が指定されている
				AspectX = m_ForceAspectX;
				AspectY = m_ForceAspectY;
			} else if (m_VideoInfo.m_AspectRatioX > 0 && m_VideoInfo.m_AspectRatioY > 0) {
				// 映像のアスペクト比を使用する
				AspectX = m_VideoInfo.m_AspectRatioX;
				AspectY = m_VideoInfo.m_AspectRatioY;
				if (m_bIgnoreDisplayExtension
						&& m_VideoInfo.m_DisplayWidth > 0
						&& m_VideoInfo.m_DisplayHeight > 0) {
					AspectX = AspectX * 3 * m_VideoInfo.m_OrigWidth / m_VideoInfo.m_DisplayWidth;
					AspectY = AspectY * 3 * m_VideoInfo.m_OrigHeight / m_VideoInfo.m_DisplayHeight;
				}
			} else {
				// アスペクト比不明
				if (m_VideoInfo.m_DisplayHeight == 1080) {
					AspectX = 16;
					AspectY = 9;
				} else if (m_VideoInfo.m_DisplayWidth > 0 && m_VideoInfo.m_DisplayHeight > 0) {
					AspectX = m_VideoInfo.m_DisplayWidth;
					AspectY = m_VideoInfo.m_DisplayHeight;
				} else {
					AspectX = WindowWidth;
					AspectY = WindowHeight;
				}
			}
			double WindowRatio = (double)WindowWidth / (double)WindowHeight;
			double AspectRatio = (double)AspectX / (double)AspectY;
			if ((m_ViewStretchMode == STRETCH_KEEPASPECTRATIO && AspectRatio > WindowRatio)
					|| (m_ViewStretchMode == STRETCH_CUTFRAME && AspectRatio < WindowRatio)) {
				DestWidth = WindowWidth;
				DestHeight = ::MulDiv(DestWidth, AspectY, AspectX);
			} else {
				DestHeight = WindowHeight;
				DestWidth = ::MulDiv(DestHeight, AspectX, AspectY);
			}
		}

		RECT rcSrc,rcDst,rcWindow;
		CalcSourceRect(&rcSrc);
#if 0
		// 座標値がマイナスになるとマルチディスプレイでおかしくなる?
		rcDst.left = (WindowWidth - DestWidth) / 2;
		rcDst.top = (WindowHeight - DestHeight) / 2,
		rcDst.right = rcDst.left + DestWidth;
		rcDst.bottom = rcDst.top + DestHeight;
#else
		if (WindowWidth < DestWidth) {
			rcDst.left = 0;
			rcDst.right = WindowWidth;
			rcSrc.left += ::MulDiv(DestWidth - WindowWidth, rcSrc.right - rcSrc.left, DestWidth) / 2;
			rcSrc.right = m_VideoInfo.m_OrigWidth - rcSrc.left;
		} else {
			if (m_bNoMaskSideCut
					&& WindowWidth > DestWidth
					&& rcSrc.right - rcSrc.left < m_VideoInfo.m_OrigWidth) {
				int NewDestWidth = ::MulDiv(m_VideoInfo.m_OrigWidth, DestWidth, rcSrc.right-rcSrc.left);
				if (NewDestWidth > WindowWidth)
					NewDestWidth = WindowWidth;
				int NewSrcWidth = ::MulDiv(rcSrc.right-rcSrc.left, NewDestWidth, DestWidth);
				rcSrc.left = (m_VideoInfo.m_OrigWidth - NewSrcWidth) / 2;
				rcSrc.right = rcSrc.left + NewSrcWidth;
				TRACE(TEXT("Adjust %d x %d -> %d x %d [%d - %d (%d)]\n"),
					  DestWidth, DestHeight, NewDestWidth, DestHeight,
					  rcSrc.left, rcSrc.right, NewSrcWidth);
				DestWidth = NewDestWidth;
			}
			rcDst.left = (WindowWidth - DestWidth) / 2;
			rcDst.right = rcDst.left + DestWidth;
		}
		if (WindowHeight < DestHeight) {
			rcDst.top = 0;
			rcDst.bottom = WindowHeight;
			rcSrc.top += ::MulDiv(DestHeight - WindowHeight, rcSrc.bottom - rcSrc.top, DestHeight) / 2;
			rcSrc.bottom = m_VideoInfo.m_OrigHeight - rcSrc.top;
		} else {
			rcDst.top = (WindowHeight - DestHeight) / 2,
			rcDst.bottom = rcDst.top + DestHeight;
		}
#endif

		rcWindow.left = 0;
		rcWindow.top = 0;
		rcWindow.right = WindowWidth;
		rcWindow.bottom = WindowHeight;

#if 0
		TRACE(TEXT("SetVideoPosition %d,%d,%d,%d -> %d,%d,%d,%d [%d,%d,%d,%d]\n"),
			  rcSrc.left, rcSrc.top, rcSrc.right, rcSrc.bottom,
			  rcDst.left, rcDst.top, rcDst.right, rcDst.bottom,
			  rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom);
#endif

		return m_pVideoRenderer->SetVideoPosition(
			m_VideoInfo.m_OrigWidth, m_VideoInfo.m_OrigHeight, &rcSrc, &rcDst, &rcWindow);
	}

	return false;
}


// 映像ウィンドウのサイズを設定する
bool CMediaViewer::SetViewSize(const int Width, const int Height)
{
	CBlockLock Lock(&m_ResizeLock);

	if (Width > 0 && Height > 0) {
		m_wVideoWindowX = Width;
		m_wVideoWindowY = Height;
		return AdjustVideoPosition();
	}
	return false;
}


// 映像のサイズを取得する
bool CMediaViewer::GetVideoSize(WORD *pwWidth, WORD *pwHeight) const
{
	if (m_bIgnoreDisplayExtension)
		return GetOriginalVideoSize(pwWidth, pwHeight);

	CBlockLock Lock(&m_ResizeLock);

	if (m_VideoInfo.m_DisplayWidth > 0 && m_VideoInfo.m_DisplayHeight > 0) {
		if (pwWidth)
			*pwWidth = m_VideoInfo.m_DisplayWidth;
		if (pwHeight)
			*pwHeight = m_VideoInfo.m_DisplayHeight;
		return true;
	}
	return false;
}


// 映像のアスペクト比を取得する
bool CMediaViewer::GetVideoAspectRatio(BYTE *pbyAspectRatioX, BYTE *pbyAspectRatioY) const
{
	CBlockLock Lock(&m_ResizeLock);

	if (m_VideoInfo.m_AspectRatioX > 0 && m_VideoInfo.m_AspectRatioY > 0) {
		if (pbyAspectRatioX)
			*pbyAspectRatioX = m_VideoInfo.m_AspectRatioX;
		if (pbyAspectRatioY)
			*pbyAspectRatioY = m_VideoInfo.m_AspectRatioY;
		return true;
	}
	return false;
}


// 映像のアスペクト比を設定する
bool CMediaViewer::ForceAspectRatio(int AspectX, int AspectY)
{
	m_ForceAspectX=AspectX;
	m_ForceAspectY=AspectY;
	return true;
}


// 設定されたアスペクト比を取得する
bool CMediaViewer::GetForceAspectRatio(int *pAspectX, int *pAspectY) const
{
	if (pAspectX)
		*pAspectX=m_ForceAspectX;
	if (pAspectY)
		*pAspectY=m_ForceAspectY;
	return true;
}


// 有効なアスペクト比を取得する
bool CMediaViewer::GetEffectiveAspectRatio(BYTE *pAspectX, BYTE *pAspectY) const
{
	if (m_ForceAspectX > 0 && m_ForceAspectY > 0) {
		if (pAspectX)
			*pAspectX = m_ForceAspectX;
		if (pAspectY)
			*pAspectY = m_ForceAspectY;
		return true;
	}
	BYTE AspectX, AspectY;
	if (!GetVideoAspectRatio(&AspectX, &AspectY))
		return false;
	if (m_bIgnoreDisplayExtension
			&& (m_VideoInfo.m_DisplayWidth != m_VideoInfo.m_OrigWidth
				|| m_VideoInfo.m_DisplayHeight != m_VideoInfo.m_OrigHeight)) {
		if (m_VideoInfo.m_DisplayWidth == 0
				|| m_VideoInfo.m_DisplayHeight == 0)
			return false;
		AspectX = AspectX * 3 * m_VideoInfo.m_OrigWidth / m_VideoInfo.m_DisplayWidth;
		AspectY = AspectY * 3 * m_VideoInfo.m_OrigHeight / m_VideoInfo.m_DisplayHeight;
		if (AspectX % 4 == 0 && AspectY % 4 == 0) {
			AspectX /= 4;
			AspectY /= 4;
		}
	}
	if (pAspectX)
		*pAspectX = AspectX;
	if (pAspectY)
		*pAspectY = AspectY;
	return true;
}


bool CMediaViewer::SetPanAndScan(int AspectX,int AspectY,const ClippingInfo *pClipping)
{
	if (m_ForceAspectX!=AspectX || m_ForceAspectY!=AspectY || pClipping!=NULL) {
		CBlockLock Lock(&m_ResizeLock);

		m_ForceAspectX=AspectX;
		m_ForceAspectY=AspectY;
		if (pClipping!=NULL)
			m_Clipping=*pClipping;
		else
			::ZeroMemory(&m_Clipping,sizeof(m_Clipping));
		AdjustVideoPosition();
	}
	return true;
}


bool CMediaViewer::GetClippingInfo(ClippingInfo *pClipping) const
{
	if (pClipping==NULL)
		return false;
	*pClipping=m_Clipping;
	return true;
}


bool CMediaViewer::SetViewStretchMode(ViewStretchMode Mode)
{
	if (m_ViewStretchMode!=Mode) {
		CBlockLock Lock(&m_ResizeLock);

		m_ViewStretchMode=Mode;
		return AdjustVideoPosition();
	}
	return true;
}


bool CMediaViewer::SetNoMaskSideCut(bool bNoMask, bool bAdjust)
{
	if (m_bNoMaskSideCut != bNoMask) {
		CBlockLock Lock(&m_ResizeLock);

		m_bNoMaskSideCut = bNoMask;
		if (bAdjust)
			AdjustVideoPosition();
	}
	return true;
}


bool CMediaViewer::SetIgnoreDisplayExtension(bool bIgnore)
{
	if (bIgnore != m_bIgnoreDisplayExtension) {
		CBlockLock Lock(&m_ResizeLock);

		m_bIgnoreDisplayExtension = bIgnore;
		if (m_VideoInfo.m_DisplayWidth != m_VideoInfo.m_OrigWidth
				|| m_VideoInfo.m_DisplayHeight != m_VideoInfo.m_OrigHeight)
			AdjustVideoPosition();
	}
	return true;
}


bool CMediaViewer::SetClipToDevice(bool bClip)
{
	if (bClip != m_bClipToDevice) {
		m_bClipToDevice = bClip;
		if (m_pVideoRenderer)
			m_pVideoRenderer->SetClipToDevice(m_bClipToDevice);
	}
	return true;
}


bool CMediaViewer::GetOriginalVideoSize(WORD *pWidth, WORD *pHeight) const
{
	CBlockLock Lock(&m_ResizeLock);

	if (m_VideoInfo.m_OrigWidth > 0 && m_VideoInfo.m_OrigHeight > 0) {
		if (pWidth)
			*pWidth = m_VideoInfo.m_OrigWidth;
		if (pHeight)
			*pHeight = m_VideoInfo.m_OrigHeight;
		return true;
	}
	return false;
}


bool CMediaViewer::GetCroppedVideoSize(WORD *pWidth, WORD *pHeight) const
{
	RECT rc;

	if (!GetSourceRect(&rc))
		return false;
	if (pWidth)
		*pWidth = (WORD)(rc.right - rc.left);
	if (pHeight)
		*pHeight = (WORD)(rc.bottom - rc.top);
	return true;
}


bool CMediaViewer::GetSourceRect(RECT *pRect) const
{
	CBlockLock Lock(&m_ResizeLock);

	if (!pRect)
		return false;
	return CalcSourceRect(pRect);
}


bool CMediaViewer::CalcSourceRect(RECT *pRect) const
{
	if (m_VideoInfo.m_OrigWidth == 0 || m_VideoInfo.m_OrigHeight == 0)
		return false;

	long SrcX, SrcY, SrcWidth, SrcHeight;

	if (m_Clipping.HorzFactor != 0) {
		long ClipLeft, ClipRight;
		ClipLeft = ::MulDiv(m_VideoInfo.m_OrigWidth, m_Clipping.Left, m_Clipping.HorzFactor);
		ClipRight = ::MulDiv(m_VideoInfo.m_OrigWidth, m_Clipping.Right, m_Clipping.HorzFactor);
		SrcWidth = m_VideoInfo.m_OrigWidth - (ClipLeft + ClipRight);
		SrcX = ClipLeft;
	} else if (m_bIgnoreDisplayExtension) {
		SrcWidth = m_VideoInfo.m_OrigWidth;
		SrcX = 0;
	} else {
		SrcWidth = m_VideoInfo.m_DisplayWidth;
		SrcX = m_VideoInfo.m_PosX;
	}
	if (m_Clipping.VertFactor != 0) {
		long ClipTop, ClipBottom;
		ClipTop = ::MulDiv(m_VideoInfo.m_OrigHeight, m_Clipping.Top, m_Clipping.VertFactor);
		ClipBottom = ::MulDiv(m_VideoInfo.m_OrigHeight, m_Clipping.Bottom, m_Clipping.VertFactor);
		SrcHeight = m_VideoInfo.m_OrigHeight - (ClipTop + ClipBottom);
		SrcY = ClipTop;
	} else if (m_bIgnoreDisplayExtension) {
		SrcHeight = m_VideoInfo.m_OrigHeight;
		SrcY = 0;
	} else {
		SrcHeight = m_VideoInfo.m_DisplayHeight;
		SrcY = m_VideoInfo.m_PosY;
	}

	pRect->left = SrcX;
	pRect->top = SrcY;
	pRect->right = SrcX + SrcWidth;
	pRect->bottom = SrcY + SrcHeight;

	return true;
}


bool CMediaViewer::GetDestRect(RECT *pRect) const
{
	if (m_pVideoRenderer && pRect) {
		if (m_pVideoRenderer->GetDestPosition(pRect))
			return true;
	}
	return false;
}


bool CMediaViewer::GetDestSize(WORD *pWidth, WORD *pHeight) const
{
	RECT rc;

	if (!GetDestRect(&rc))
		return false;
	if (pWidth)
		*pWidth=(WORD)(rc.right-rc.left);
	if (pHeight)
		*pHeight=(WORD)(rc.bottom-rc.top);
	return true;
}


IBaseFilter *CMediaViewer::GetVideoDecoderFilter()
{
	if (!m_pVideoDecoderFilter)
		return NULL;
	m_pVideoDecoderFilter->AddRef();
	return m_pVideoDecoderFilter;
}


void CMediaViewer::SetVideoDecoderSettings(const CInternalDecoderManager::VideoDecoderSettings &Settings)
{
	m_InternalDecoderManager.SetVideoDecoderSettings(Settings);
}


bool CMediaViewer::GetVideoDecoderSettings(CInternalDecoderManager::VideoDecoderSettings *pSettings) const
{
	return m_InternalDecoderManager.GetVideoDecoderSettings(pSettings);
}


void CMediaViewer::SaveVideoDecoderSettings()
{
	if (m_pVideoDecoderFilter)
		m_InternalDecoderManager.SaveVideoDecoderSettings(m_pVideoDecoderFilter);
}


bool CMediaViewer::SetVolume(const float fVolume)
{
	// オーディオボリュームをdBで設定する( -100.0(無音) < fVolume < 0(最大) )
	IBasicAudio *pBasicAudio;
	bool fOK=false;

	if (m_pFilterGraph) {
		if (SUCCEEDED(m_pFilterGraph->QueryInterface(
				IID_IBasicAudio, pointer_cast<void**>(&pBasicAudio)))) {
			long lVolume = (long)(fVolume * 100.0f);

			if (lVolume>=-10000 && lVolume<=0) {
					TRACE(TEXT("Volume Control = %d\n"),lVolume);
				if (SUCCEEDED(pBasicAudio->put_Volume(lVolume)))
					fOK=true;
			}
			pBasicAudio->Release();
		}
	}
	return fOK;
}


BYTE CMediaViewer::GetAudioChannelNum() const
{
	// オーディオの入力チャンネル数を取得する
	if (m_pAudioDecoder)
		return m_pAudioDecoder->GetCurrentChannelNum();
	return AUDIO_CHANNEL_INVALID;
}


bool CMediaViewer::SetDualMonoMode(CAudioDecFilter::DualMonoMode Mode)
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->SetDualMonoMode(Mode);
	return false;
}


CAudioDecFilter::DualMonoMode CMediaViewer::GetDualMonoMode() const
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->GetDualMonoMode();
	return CAudioDecFilter::DUALMONO_INVALID;
}


bool CMediaViewer::SetStereoMode(CAudioDecFilter::StereoMode Mode)
{
	// ステレオ出力チャンネルの設定
	if (m_pAudioDecoder)
		return m_pAudioDecoder->SetStereoMode(Mode);
	return false;
}


CAudioDecFilter::StereoMode CMediaViewer::GetStereoMode() const
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->GetStereoMode();
	return CAudioDecFilter::STEREOMODE_STEREO;
}


bool CMediaViewer::SetSpdifOptions(const CAudioDecFilter::SpdifOptions *pOptions)
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->SetSpdifOptions(pOptions);
	return false;
}


bool CMediaViewer::GetSpdifOptions(CAudioDecFilter::SpdifOptions *pOptions) const
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->GetSpdifOptions(pOptions);
	return false;
}


bool CMediaViewer::IsSpdifPassthrough() const
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->IsSpdifPassthrough();
	return false;
}


bool CMediaViewer::SetDownMixSurround(bool bDownMix)
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->SetDownMixSurround(bDownMix);
	return false;
}


bool CMediaViewer::GetDownMixSurround() const
{
	if (m_pAudioDecoder)
		return m_pAudioDecoder->GetDownMixSurround();
	return false;
}


bool CMediaViewer::SetAudioGainControl(bool bGainControl, float Gain, float SurroundGain)
{
	if (m_pAudioDecoder==NULL)
		return false;
	return m_pAudioDecoder->SetGainControl(bGainControl, Gain, SurroundGain);
}


bool CMediaViewer::SetAudioDelay(LONGLONG Delay)
{
	if (m_pAudioDecoder == NULL)
		return false;
	return m_pAudioDecoder->SetDelay(Delay);
}


LONGLONG CMediaViewer::GetAudioDelay() const
{
	if (m_pAudioDecoder == NULL)
		return 0;
	return m_pAudioDecoder->GetDelay();
}


bool CMediaViewer::GetAudioDecFilter(CAudioDecFilter **ppFilter)
{
	if (ppFilter == NULL)
		return false;

	if (m_pAudioDecoder == NULL) {
		*ppFilter = NULL;
		return false;
	}

	*ppFilter = m_pAudioDecoder;
	m_pAudioDecoder->AddRef();

	return true;
}


bool CMediaViewer::SetAudioStreamType(BYTE StreamType)
{
	m_AudioStreamType = StreamType;

	if (m_pAudioDecoder) {
		SetAudioDecoderType(m_AudioStreamType);
	}

	return true;
}


bool CMediaViewer::GetVideoDecoderName(LPWSTR pszName, int Length) const
{
	// 選択されているビデオデコーダー名の取得
	if (pszName == NULL || Length < 1)
		return false;

	if (m_pszVideoDecoderName == NULL) {
		pszName[0] = '\0';
		return false;
	}

	::lstrcpynW(pszName, m_pszVideoDecoderName, Length);
	return true;
}


bool CMediaViewer::GetVideoRendererName(LPTSTR pszName, int Length) const
{
	if (pszName == NULL || Length < 1)
		return false;

	LPCTSTR pszRenderer = CVideoRenderer::EnumRendererName((int)m_VideoRendererType);
	if (pszRenderer == NULL) {
		pszName[0] = '\0';
		return false;
	}

	::lstrcpyn(pszName, pszRenderer, Length);
	return true;
}


bool CMediaViewer::GetAudioRendererName(LPWSTR pszName, int Length) const
{
	if (pszName == NULL || Length < 1)
		return false;

	if (m_pszAudioRendererName==NULL) {
		pszName[0] = '\0';
		return false;
	}

	::lstrcpyn(pszName, m_pszAudioRendererName, Length);
	return true;
}


CVideoRenderer::RendererType CMediaViewer::GetVideoRendererType() const
{
	return m_VideoRendererType;
}


BYTE CMediaViewer::GetVideoStreamType() const
{
	return m_VideoStreamType;
}


bool CMediaViewer::DisplayFilterProperty(PropertyFilter Filter, HWND hwndOwner)
{
	switch (Filter) {
	case PROPERTY_FILTER_VIDEODECODER:
		if (m_pVideoDecoderFilter)
			return DirectShowUtil::ShowPropertyPage(m_pVideoDecoderFilter,hwndOwner);
		break;
	case PROPERTY_FILTER_VIDEORENDERER:
		if (m_pVideoRenderer)
			return m_pVideoRenderer->ShowProperty(hwndOwner);
		break;
	case PROPERTY_FILTER_MPEG2DEMULTIPLEXER:
		if (m_pMp2DemuxFilter)
			return DirectShowUtil::ShowPropertyPage(m_pMp2DemuxFilter,hwndOwner);
		break;
	case PROPERTY_FILTER_AUDIOFILTER:
		if (m_pAudioFilter)
			return DirectShowUtil::ShowPropertyPage(m_pAudioFilter,hwndOwner);
		break;
	case PROPERTY_FILTER_AUDIORENDERER:
		if (m_pAudioRenderer)
			return DirectShowUtil::ShowPropertyPage(m_pAudioRenderer,hwndOwner);
		break;
	}
	return false;
}

bool CMediaViewer::FilterHasProperty(PropertyFilter Filter)
{
	switch (Filter) {
	case PROPERTY_FILTER_VIDEODECODER:
		if (m_pVideoDecoderFilter)
			return DirectShowUtil::HasPropertyPage(m_pVideoDecoderFilter);
		break;
	case PROPERTY_FILTER_VIDEORENDERER:
		if (m_pVideoRenderer)
			return m_pVideoRenderer->HasProperty();
		break;
	case PROPERTY_FILTER_MPEG2DEMULTIPLEXER:
		if (m_pMp2DemuxFilter)
			return DirectShowUtil::HasPropertyPage(m_pMp2DemuxFilter);
		break;
	case PROPERTY_FILTER_AUDIOFILTER:
		if (m_pAudioFilter)
			return DirectShowUtil::HasPropertyPage(m_pAudioFilter);
		break;
	case PROPERTY_FILTER_AUDIORENDERER:
		if (m_pAudioRenderer)
			return DirectShowUtil::HasPropertyPage(m_pAudioRenderer);
		break;
	}
	return false;
}


bool CMediaViewer::SetUseAudioRendererClock(bool bUse)
{
	m_bUseAudioRendererClock = bUse;
	return true;
}


bool CMediaViewer::SetAdjustAudioStreamTime(bool bAdjust)
{
	m_bAdjustAudioStreamTime = bAdjust;
	if (m_pAudioDecoder == NULL)
		return true;
	return m_pAudioDecoder->SetJitterCorrection(bAdjust);
}


bool CMediaViewer::SetAudioStreamCallback(CAudioDecFilter::StreamCallback pCallback, void *pParam)
{
	m_pAudioStreamCallback=pCallback;
	m_pAudioStreamCallbackParam=pParam;
	if (m_pAudioDecoder == NULL)
		return true;
	return m_pAudioDecoder->SetStreamCallback(pCallback, pParam);
}


bool CMediaViewer::SetAudioFilter(LPCWSTR pszFilterName)
{
	if (m_pszAudioFilterName) {
		delete [] m_pszAudioFilterName;
		m_pszAudioFilterName = NULL;
	}
	if (pszFilterName && pszFilterName[0] != '\0')
		m_pszAudioFilterName = StdUtil::strdup(pszFilterName);
	return true;
}


void CMediaViewer::SetVideoStreamCallback(CVideoParser::IStreamCallback *pCallback)
{
	m_pVideoStreamCallback = pCallback;
	if (m_pVideoParser)
		m_pVideoParser->SetStreamCallback(pCallback);
}


bool CMediaViewer::GetCurrentImage(BYTE **ppDib)
{
	bool fOK=false;

	if (m_pVideoRenderer) {
		void *pBuffer;

		if (m_pVideoRenderer->GetCurrentImage(&pBuffer)) {
			fOK=true;
			*ppDib=static_cast<BYTE*>(pBuffer);
		}
	}
	return fOK;
}


bool CMediaViewer::DrawText(LPCTSTR pszText,int x,int y,
							HFONT hfont,COLORREF crColor,int Opacity)
{
	IBaseFilter *pRenderer;
	int Width,Height;

	if (m_pVideoRenderer==NULL || !IsDrawTextSupported())
		return false;
	pRenderer=m_pVideoRenderer->GetRendererFilter();
	if (pRenderer==NULL)
		return false;
	if (m_pImageMixer==NULL) {
		m_pImageMixer=CImageMixer::CreateImageMixer(m_VideoRendererType,pRenderer);
		if (m_pImageMixer==NULL)
			return false;
	}
	if (!m_pImageMixer->GetMapSize(&Width,&Height))
		return false;
	m_ResizeLock.Lock();
	if (m_VideoInfo.m_OrigWidth==0 || m_VideoInfo.m_OrigHeight==0)
		return false;
	x=x*Width/m_VideoInfo.m_OrigWidth;
	y=y*Height/m_VideoInfo.m_OrigHeight;
	m_ResizeLock.Unlock();
	return m_pImageMixer->SetText(pszText,x,y,hfont,crColor,Opacity);
}


bool CMediaViewer::IsDrawTextSupported() const
{
	return CImageMixer::IsSupported(m_VideoRendererType);
}


bool CMediaViewer::ClearOSD()
{
	if (m_pVideoRenderer==NULL)
		return false;
	if (m_pImageMixer!=NULL)
		m_pImageMixer->Clear();
	return true;
}


bool CMediaViewer::EnablePTSSync(bool bEnable)
{
	TRACE(TEXT("CMediaViewer::EnablePTSSync(%s)\n"), bEnable ? TEXT("true") : TEXT("false"));
	if (m_pSrcFilter != NULL) {
		if (!m_pSrcFilter->EnableSync(bEnable, m_b1SegMode))
			return false;
	}
	m_bEnablePTSSync = bEnable;
	return true;
}


bool CMediaViewer::IsPTSSyncEnabled() const
{
	/*
	if (m_pSrcFilter != NULL)
		return m_pSrcFilter->IsSyncEnabled();
	*/
	return m_bEnablePTSSync;
}


bool CMediaViewer::SetAdjust1SegVideoSample(bool bAdjustTime, bool bAdjustFrameRate)
{
	TRACE(TEXT("CMediaViewer::SetAdjust1SegVideoSample() : Adjust time %d / Adjust frame rate %d\n"),
		  bAdjustTime, bAdjustFrameRate);

	m_bAdjust1SegVideoSampleTime = bAdjustTime;
	m_bAdjust1SegFrameRate = bAdjustFrameRate;
	ApplyAdjustVideoSampleOptions();

	return true;
}


void CMediaViewer::ResetBuffer()
{
	if (m_pSrcFilter != NULL)
		m_pSrcFilter->Reset();
}


bool CMediaViewer::SetBufferSize(size_t Size)
{
	TRACE(TEXT("CMediaViewer::SetBufferSize(%Iu)\n"), Size);

	if (m_pSrcFilter != NULL) {
		if (!m_pSrcFilter->SetBufferSize(Size))
			return false;
	}

	m_BufferSize = Size;

	return true;
}


bool CMediaViewer::SetInitialPoolPercentage(int Percentage)
{
	TRACE(TEXT("CMediaViewer::SetInitialPoolPercentage(%d)\n"), Percentage);

	if (m_pSrcFilter != NULL) {
		if (!m_pSrcFilter->SetInitialPoolPercentage(Percentage))
			return false;
	}

	m_InitialPoolPercentage = Percentage;

	return true;
}


int CMediaViewer::GetBufferFillPercentage() const
{
	if (m_pSrcFilter != NULL)
		return m_pSrcFilter->GetBufferFillPercentage();
	return 0;
}


bool CMediaViewer::SetPacketInputWait(DWORD Wait)
{
	TRACE(TEXT("CMediaViewer::SetPacketInputWait(%u)\n"), Wait);

	if (m_pSrcFilter != NULL) {
		if (!m_pSrcFilter->SetInputWait(Wait))
			return false;
	}

	m_PacketInputWait = Wait;

	return true;
}


void CMediaViewer::ConnectVideoDecoder(
	LPCTSTR pszCodecName, const GUID &MediaSubType, LPCTSTR pszDecoderName, IPin **ppOutputPin)
{
	Trace(CTracer::TYPE_INFORMATION, TEXT("%sデコーダの接続中..."), pszCodecName);

	const bool bDefault = (pszDecoderName == NULL || pszDecoderName[0] == '\0');
	bool bConnectSuccess = false;
	HRESULT hr;
	WCHAR szFilter[256];

	if (m_InternalDecoderManager.IsMediaSupported(MediaSubType)
			&& ((bDefault && m_InternalDecoderManager.IsDecoderAvailable(MediaSubType))
			|| (!bDefault && ::lstrcmpi(m_InternalDecoderManager.GetDecoderName(MediaSubType), pszDecoderName) == 0))) {
		IBaseFilter *pFilter;

		hr = m_InternalDecoderManager.CreateInstance(MediaSubType, &pFilter);
		if (SUCCEEDED(hr)) {
			::lstrcpyn(szFilter, m_InternalDecoderManager.GetDecoderName(MediaSubType), _countof(szFilter));
			hr = DirectShowUtil::AppendFilterAndConnect(
				m_pFilterGraph, pFilter, szFilter, ppOutputPin, NULL, true);
			if (SUCCEEDED(hr)) {
				m_pVideoDecoderFilter = pFilter;
				bConnectSuccess = true;
			} else {
				pFilter->Release();
			}
		}
	}

	TCHAR szText1[128], szText2[128];

	if (!bConnectSuccess) {
		CDirectShowFilterFinder FilterFinder;

		// 検索
		if (!FilterFinder.FindFilter(&MEDIATYPE_Video, &MediaSubType)) {
			StdUtil::snprintf(szText1, _countof(szText1),
							  TEXT("%sデコーダが見付かりません。"), pszCodecName);
			StdUtil::snprintf(szText2, _countof(szText2),
							  TEXT("%sデコーダがインストールされているか確認してください。"), pszCodecName);
			throw CBonException(szText1, szText2);
		}

		if (bDefault) {
			CLSID id = m_InternalDecoderManager.GetDecoderCLSID(MediaSubType);

			if (id != GUID_NULL)
				FilterFinder.SetPreferredFilter(id);
		}

		for (int i = 0; i < FilterFinder.GetFilterCount(); i++) {
			CLSID clsidFilter;

			if (FilterFinder.GetFilterInfo(i, &clsidFilter, szFilter, _countof(szFilter))) {
				if (!bDefault && ::lstrcmpi(szFilter, pszDecoderName) != 0)
					continue;
				hr = DirectShowUtil::AppendFilterAndConnect(m_pFilterGraph,
					clsidFilter, szFilter, &m_pVideoDecoderFilter,
					ppOutputPin, NULL, true);
				if (SUCCEEDED(hr)) {
					bConnectSuccess = true;
					break;
				}
			}
		}
	}

	// どれかのフィルタで接続できたか
	if (bConnectSuccess) {
		m_pszVideoDecoderName = StdUtil::strdup(szFilter);
	} else {
		StdUtil::snprintf(szText1, _countof(szText1),
						  TEXT("%sデコーダフィルタをフィルタグラフに追加できません。"),
						  pszCodecName);
		throw CBonException(hr, szText1,
			TEXT("設定で有効なデコーダが選択されているか確認してください。\nまた、レンダラを変えてみてください。"));
	}
}


bool CMediaViewer::MapVideoPID(WORD PID)
{
	if (m_pMp2DemuxVideoMap) {
		// 新規にPIDをマップ
		if (PID != PID_INVALID) {
			ULONG TempPID = PID;
			if (m_pMp2DemuxVideoMap->MapPID(1UL, &TempPID, MEDIA_ELEMENTARY_STREAM) != S_OK)
				return false;
		}
	}

	if (m_pSrcFilter)
		m_pSrcFilter->SetVideoPID(PID);

	return true;
}


bool CMediaViewer::MapAudioPID(WORD PID)
{
	if (m_pMp2DemuxAudioMap) {
		// 新規にPIDをマップ
		if (PID != PID_INVALID) {
			ULONG TempPID = PID;
			if (m_pMp2DemuxAudioMap->MapPID(1UL, &TempPID, MEDIA_ELEMENTARY_STREAM) != S_OK)
				return false;
			m_MapAudioPID = PID;
		}
	}

	if (m_pSrcFilter)
		m_pSrcFilter->SetAudioPID(PID);

	return true;
}


void CMediaViewer::ApplyAdjustVideoSampleOptions()
{
	if (m_pVideoParser != NULL) {
		unsigned int Flags = 0;

		if (m_b1SegMode) {
			Flags = CVideoParser::ADJUST_SAMPLE_1SEG;
			// Microsoft DTV-DVD Video Decoder では何故か映像が出なくなってしまうため無効とする
			if (::lstrcmpi(m_pszVideoDecoderName, TEXT("Microsoft DTV-DVD Video Decoder")) != 0) {
				if (m_bAdjust1SegVideoSampleTime)
					Flags |= CVideoParser::ADJUST_SAMPLE_TIME;
				if (m_bAdjust1SegFrameRate)
					Flags |= CVideoParser::ADJUST_SAMPLE_FRAME_RATE;
			}
		}

		m_pVideoParser->SetAdjustSampleOptions(Flags);
	}
}


void CMediaViewer::SetAudioDecoderType(BYTE StreamType)
{
	if (m_pAudioDecoder) {
		m_pAudioDecoder->SetDecoderType(
#ifdef BONTSENGINE_MPEG_AUDIO_SUPPORT
			StreamType == STREAM_TYPE_MPEG1_AUDIO ||
			StreamType == STREAM_TYPE_MPEG2_AUDIO ?
				CAudioDecFilter::DECODER_MPEG_AUDIO :
#endif
#ifdef BONTSENGINE_AC3_SUPPORT
			StreamType == STREAM_TYPE_AC3 ?
				CAudioDecFilter::DECODER_AC3 :
#endif
				CAudioDecFilter::DECODER_AAC);
	}
}


void CMediaViewer::OnSpdifPassthroughError(HRESULT hr)
{
	SendDecoderEvent(EID_SPDIF_PASSTHROUGH_ERROR, &hr);
}


#ifdef _DEBUG

HRESULT CMediaViewer::AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) const
{
	// デバッグ用
	IMoniker * pMoniker;
	IRunningObjectTable *pROT;
	if(FAILED(::GetRunningObjectTable(0, &pROT)))return E_FAIL;

	WCHAR wsz[256];
	wsprintfW(wsz, L"FilterGraph %08p pid %08x", (DWORD_PTR)pUnkGraph, ::GetCurrentProcessId());

	HRESULT hr = ::CreateItemMoniker(L"!", wsz, &pMoniker);

	if(SUCCEEDED(hr)){
		hr = pROT->Register(0, pUnkGraph, pMoniker, pdwRegister);
		pMoniker->Release();
		}

	pROT->Release();

	return hr;
}


void CMediaViewer::RemoveFromRot(const DWORD dwRegister) const
{
	// デバッグ用
	IRunningObjectTable *pROT;

	if(SUCCEEDED(::GetRunningObjectTable(0, &pROT))){
		pROT->Revoke(dwRegister);
		pROT->Release();
		}
}

#endif	// _DEBUG
