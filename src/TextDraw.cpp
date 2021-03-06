#include "stdafx.h"
#include <cmath>
#include "TVTest.h"
#include "TextDraw.h"
#include "Util.h"
#include "Common/DebugDef.h"


namespace TVTest
{


// sŠÖĨķ
const LPCWSTR CTextDraw::m_pszStartProhibitChars=
	L")j]nĢvxzrthAB,C.D!I?H[`cE@BDFHbĄĢĨ§ÁáãåėTUX";
// sÖĨķ
const LPCWSTR CTextDraw::m_pszEndProhibitChars=
	L"(i[mĒuwyqsg#ĪĨ";


CTextDraw::CTextDraw()
	: m_pEngine(nullptr)
	, m_Flags(0)
{
}


CTextDraw::~CTextDraw()
{
}


bool CTextDraw::SetEngine(CTextDrawEngine *pEngine)
{
	if (pEngine!=nullptr)
		m_pEngine=pEngine;
	else
		m_pEngine=&m_DefaultEngine;
	return true;
}


bool CTextDraw::Begin(HDC hdc,const RECT &Rect,unsigned int Flags)
{
	if (m_pEngine==nullptr)
		m_pEngine=&m_DefaultEngine;
	m_Flags=Flags;
	return m_pEngine->BeginDraw(hdc,Rect);
}


void CTextDraw::End()
{
	if (m_pEngine!=nullptr)
		m_pEngine->EndDraw();
}


bool CTextDraw::BindDC(HDC hdc,const RECT &Rect)
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->BindDC(hdc,Rect);
}


bool CTextDraw::SetFont(HFONT hfont)
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->SetFont(hfont);
}


bool CTextDraw::SetFont(const LOGFONT &Font)
{
	if (m_pEngine==nullptr)
		return false;

	HFONT hfont=::CreateFontIndirect(&Font);
	if (hfont==nullptr)
		return false;

	bool fOK=m_pEngine->SetFont(hfont);

	::DeleteObject(hfont);

	return true;
}


bool CTextDraw::SetTextColor(COLORREF Color)
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->SetTextColor(Color);
}


int CTextDraw::CalcLineCount(LPCWSTR pszText,int Width)
{
	if (m_pEngine==nullptr || pszText==nullptr || Width<=0)
		return 0;

	int Lines=0;
	LPCWSTR p=pszText;

	while (*p!=L'\0') {
		if (*p==L'\r' || *p==L'\n') {
			p++;
			if (*p==L'\n')
				p++;
			if (*p==L'\0')
				break;
			Lines++;
			continue;
		}

		int Length=GetLineLength(p);
		if (Length==0)
			break;

		int Fit=GetFitCharCount(p,Length,Width);
		Fit=AdjustLineLength(p,Fit);
		Length-=Fit;
		p+=Fit;
		Lines++;

		if (*p==L'\r')
			p++;
		if (*p==L'\n')
			p++;
	}

	return Lines;
}


bool CTextDraw::Draw(LPCWSTR pszText,const RECT &Rect,int LineHeight,unsigned int Flags)
{
	if (m_pEngine==nullptr || pszText==nullptr
			|| Rect.left>=Rect.right || Rect.top>=Rect.bottom)
		return false;

	const int Width=Rect.right-Rect.left;

	LPCWSTR p=pszText;
	int y=Rect.top;

	while (*p!=L'\0' && y<Rect.bottom) {
		if (*p==L'\r' || *p==L'\n') {
			p++;
			if (*p==L'\n')
				p++;
			y+=LineHeight;
			continue;
		}

		int Length=GetLineLength(p);
		if (Length==0)
			break;

		int Fit=GetFitCharCount(p,Length,Width);

		if ((m_Flags & FLAG_END_ELLIPSIS)!=0 && Fit<Length && y+LineHeight>=Rect.bottom) {
			static const WCHAR szEllipses[]=L"...";
			const size_t BufferLength=Fit+lengthof(szEllipses);
			m_StringBuffer.clear();
			m_StringBuffer.resize(max(BufferLength,256));
			LPWSTR pszBuffer=&m_StringBuffer[0];
			::CopyMemory(pszBuffer,p,Fit*sizeof(WCHAR));
			LPWSTR pszCur=pszBuffer+Fit;
			for (;;) {
				::lstrcpyW(pszCur,szEllipses);
				Length=(int)(pszCur-pszBuffer)+(lengthof(szEllipses)-1);
				Fit=GetFitCharCount(pszBuffer,Length,Width);
				if (Fit>=Length || pszCur==pszBuffer)
					break;
				pszCur=StringPrevChar(pszBuffer,pszCur);
			}
			RECT rc={Rect.left,y,Rect.right,y+LineHeight};
			m_pEngine->DrawText(pszBuffer,Fit,rc,Flags);
			return true;
		}

		Fit=AdjustLineLength(p,Fit);
		RECT rc={Rect.left,y,Rect.right,y+LineHeight};
		unsigned int LineFlags=Flags;
		if ((Flags & DRAW_FLAG_JUSTIFY_MULTI_LINE)!=0) {
			if (p[Fit]!=L'\0' && p[Fit]!=L'\r' && p[Fit]!=L'\n')
				LineFlags|=DRAW_FLAG_ALIGN_JUSTIFIED;
		}
		m_pEngine->DrawText(p,Fit,rc,LineFlags);
		Length-=Fit;
		p+=Fit;
		y+=LineHeight;

		if (*p==L'\r')
			p++;
		if (*p==L'\n')
			p++;
	}

	return true;
}


bool CTextDraw::GetFontMetrics(FontMetrics *pMetrics)
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->GetFontMetrics(pMetrics);
}


bool CTextDraw::GetTextMetrics(LPCWSTR pText,int Length,TextMetrics *pMetrics)
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->GetTextMetrics(pText,Length,pMetrics);
}


bool CTextDraw::SetClippingRect(const RECT &Rect)
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->SetClippingRect(Rect);
}


bool CTextDraw::ResetClipping()
{
	if (m_pEngine==nullptr)
		return false;

	return m_pEngine->ResetClipping();
}


int CTextDraw::GetLineLength(LPCWSTR pszText)
{
	LPCWSTR p=pszText;
	while (*p!=L'\0' && *p!=L'\r' && *p!='\n')
		p++;
	return (int)(p-pszText);
}


int CTextDraw::AdjustLineLength(LPCWSTR pszText,int Length)
{
	if (Length<1) {
		Length=StringCharLength(pszText);
		if (Length<1)
			Length=1;
	} else if ((m_Flags & FLAG_JAPANESE_HYPHNATION)!=0 && Length>1) {
		LPCWSTR p=pszText+Length-1;
		if (IsEndProhibitChar(*p)) {
			p--;
			while (p>=pszText) {
				if (!IsEndProhibitChar(*p)) {
					Length=(int)(p-pszText)+1;
					break;
				}
				p--;
			}
		} else if (IsStartProhibitChar(*(p+1))) {
			while (p>pszText) {
				if (!IsStartProhibitChar(*p)) {
					if (IsEndProhibitChar(*(p-1))) {
						p--;
						while (p>=pszText) {
							if (!IsEndProhibitChar(*p)) {
								Length=(int)(p-pszText)+1;
								break;
							}
							p--;
						}
					} else {
						Length=(int)(p-pszText);
					}
					break;
				}
				p--;
			}
		}
	}

	return Length;
}


int CTextDraw::GetFitCharCount(LPCWSTR pText,int Length,int Width)
{
	if (m_pEngine==nullptr)
		return 0;

	return m_pEngine->GetFitCharCount(pText,Length,Width);
}


bool CTextDraw::IsStartProhibitChar(WCHAR Char)
{
	return ::StrChrW(m_pszStartProhibitChars,Char)!=nullptr;
}


bool CTextDraw::IsEndProhibitChar(WCHAR Char)
{
	return ::StrChrW(m_pszEndProhibitChars,Char)!=nullptr;
}




void CTextDrawEngine::Finalize()
{
}


bool CTextDrawEngine::BeginDraw(HDC hdc,const RECT &Rect)
{
	return true;
}


bool CTextDrawEngine::EndDraw()
{
	return true;
}


bool CTextDrawEngine::BindDC(HDC hdc,const RECT &Rect)
{
	return true;
}


bool CTextDrawEngine::SetClippingRect(const RECT &Rect)
{
	return true;
}


bool CTextDrawEngine::ResetClipping()
{
	return true;
}


bool CTextDrawEngine::OnWindowPosChanged()
{
	return true;
}




CTextDrawEngine_GDI::CTextDrawEngine_GDI()
	: m_hdc(nullptr)
	, m_hfontOld(nullptr)
{
}


CTextDrawEngine_GDI::~CTextDrawEngine_GDI()
{
	Finalize();
}


void CTextDrawEngine_GDI::Finalize()
{
	UnbindDC();
}


bool CTextDrawEngine_GDI::BeginDraw(HDC hdc,const RECT &Rect)
{
	UnbindDC();
	return BindDC(hdc,Rect);
}


bool CTextDrawEngine_GDI::EndDraw()
{
	if (m_hdc==nullptr)
		return false;
	UnbindDC();
	return true;
}


bool CTextDrawEngine_GDI::BindDC(HDC hdc,const RECT &Rect)
{
	UnbindDC();

	if (hdc==nullptr)
		return false;

	m_hdc=hdc;
	m_hfontOld=static_cast<HFONT>(::GetCurrentObject(m_hdc,OBJ_FONT));
	m_OldTextColor=::GetTextColor(m_hdc);

	return true;
}


bool CTextDrawEngine_GDI::SetFont(HFONT hfont)
{
	if (hfont==nullptr || m_hdc==nullptr)
		return false;

	::SelectObject(m_hdc,hfont);

	return true;
}


bool CTextDrawEngine_GDI::SetTextColor(COLORREF Color)
{
	if (m_hdc==nullptr)
		return false;

	::SetTextColor(m_hdc,Color);

	return true;
}


bool CTextDrawEngine_GDI::DrawText(LPCWSTR pText,int Length,const RECT &Rect,unsigned int Flags)
{
	if (m_hdc==nullptr)
		return false;

	if ((Flags & CTextDraw::DRAW_FLAG_ALIGN_JUSTIFIED)!=0) {
		Util::CTempBuffer<int,256> CharPos(Length);
		SIZE sz;

		::GetTextExtentExPointW(m_hdc,pText,Length,0,nullptr,CharPos.GetBuffer(),&sz);
		if (sz.cx==0)
			return false;
		int Prev=::MulDiv(CharPos[0],Rect.right-Rect.left,sz.cx);
		for (int i=1;i<Length;i++) {
			int Pos=::MulDiv(CharPos[i],Rect.right-Rect.left,sz.cx);
			CharPos[i]=Pos-Prev;
			Prev=Pos;
		}
		::ExtTextOutW(m_hdc,Rect.left,Rect.top,0,&Rect,pText,Length,CharPos.GetBuffer());
	} else {
		::TextOutW(m_hdc,Rect.left,Rect.top,pText,Length);
	}

	return true;
}


int CTextDrawEngine_GDI::GetFitCharCount(LPCWSTR pText,int Length,int Width)
{
	if (m_hdc==nullptr)
		return 0;

	GCP_RESULTSW Results={sizeof(GCP_RESULTSW)};

	Results.nGlyphs=Length;

	if (::GetCharacterPlacementW(m_hdc,pText,Length,Width,&Results,GCP_MAXEXTENT)==0)
		return 0;

	return Results.nMaxFit;
}


bool CTextDrawEngine_GDI::GetFontMetrics(FontMetrics *pMetrics)
{
	if (m_hdc==nullptr || pMetrics==nullptr)
		return false;

	TEXTMETRIC tm;

	if (!::GetTextMetrics(m_hdc,&tm))
		return false;

	pMetrics->Height=tm.tmHeight;
	pMetrics->LineGap=tm.tmExternalLeading;

	return true;
}


bool CTextDrawEngine_GDI::GetTextMetrics(LPCWSTR pText,int Length,TextMetrics *pMetrics)
{
	if (m_hdc==nullptr || pText==nullptr || pMetrics==nullptr)
		return false;

	SIZE sz;

	if (!::GetTextExtentPoint32W(m_hdc,pText,Length<0?::lstrlenW(pText):Length,&sz))
		return false;

	pMetrics->Width=sz.cx;
	pMetrics->Height=sz.cy;

	return true;
}


void CTextDrawEngine_GDI::UnbindDC()
{
	if (m_hdc!=nullptr) {
		::SelectObject(m_hdc,m_hfontOld);
		m_hfontOld=nullptr;
		::SetTextColor(m_hdc,m_OldTextColor);
		m_hdc=nullptr;
	}
}




CTextDrawEngine_DirectWrite::CTextDrawEngine_DirectWrite(CDirectWriteRenderer &Renderer)
	: m_Renderer(Renderer)
	, m_pFont(nullptr)
	, m_MaxFontCache(4)
{
}


CTextDrawEngine_DirectWrite::~CTextDrawEngine_DirectWrite()
{
	Finalize();
}


void CTextDrawEngine_DirectWrite::Finalize()
{
	ClearFontCache();
}


bool CTextDrawEngine_DirectWrite::BeginDraw(HDC hdc,const RECT &Rect)
{
	return m_Renderer.BeginDraw(hdc,Rect);
}


bool CTextDrawEngine_DirectWrite::EndDraw()
{
	bool fOK=m_Renderer.EndDraw();
	m_pFont=nullptr;
	m_Brush.Destroy();
	if (m_Renderer.IsNeedRecreate())
		ClearFontCache();
	return fOK;
}


bool CTextDrawEngine_DirectWrite::BindDC(HDC hdc,const RECT &Rect)
{
	return m_Renderer.BindDC(hdc,Rect);
}


bool CTextDrawEngine_DirectWrite::SetFont(HFONT hfont)
{
	if (hfont==nullptr)
		return false;

	LOGFONT lf;
	if (::GetObject(hfont,sizeof(LOGFONT),&lf)!=sizeof(LOGFONT))
		return false;

	for (auto it=m_FontCache.begin();it!=m_FontCache.end();++it) {
		LOGFONT lfCache;

		if ((*it)->GetLogFont(&lfCache) && CompareLogFont(&lf,&lfCache)) {
			m_pFont=*it;
			return true;
		}
	}

	CDirectWriteFont *pFont=new CDirectWriteFont;
	if (!pFont->Create(m_Renderer,lf)) {
		delete pFont;
		return false;
	}

	if (m_FontCache.size()>=m_MaxFontCache) {
		delete m_FontCache.back();
		m_FontCache.pop_back();
	}

	m_FontCache.push_front(pFont);
	m_pFont=pFont;

	return true;
}


bool CTextDrawEngine_DirectWrite::SetTextColor(COLORREF Color)
{
	if (m_Brush.IsCreated())
		return m_Brush.SetColor(GetRValue(Color),GetGValue(Color),GetBValue(Color));
	return m_Brush.Create(m_Renderer,GetRValue(Color),GetGValue(Color),GetBValue(Color));
}


bool CTextDrawEngine_DirectWrite::DrawText(LPCWSTR pText,int Length,const RECT &Rect,unsigned int Flags)
{
	if (m_pFont==nullptr)
		return false;

	return m_Renderer.DrawText(pText,Length,Rect,*m_pFont,m_Brush,Flags);
}


int CTextDrawEngine_DirectWrite::GetFitCharCount(LPCWSTR pText,int Length,int Width)
{
	if (m_pFont==nullptr)
		return false;

	return m_Renderer.GetFitCharCount(pText,Length,Width,*m_pFont);
}


bool CTextDrawEngine_DirectWrite::GetFontMetrics(FontMetrics *pMetrics)
{
	if (m_pFont==nullptr || pMetrics==nullptr)
		return false;

	CDirectWriteRenderer::FontMetrics Metrics;

	if (!m_Renderer.GetFontMetrics(*m_pFont,&Metrics))
		return false;

	pMetrics->Height=static_cast<int>(std::ceil(Metrics.Ascent+Metrics.Descent));
	pMetrics->LineGap=static_cast<int>(
#if _MSC_VER>=1700
		std::round(Metrics.LineGap)
#else
		(Metrics.LineGap>0.0f ? Metrics.LineGap+0.5f : Metrics.LineGap-0.5f)
#endif
		);

	return true;
}


bool CTextDrawEngine_DirectWrite::GetTextMetrics(LPCWSTR pText,int Length,TextMetrics *pMetrics)
{
	if (m_pFont==nullptr || pText==nullptr || pMetrics==nullptr)
		return false;

	CDirectWriteRenderer::TextMetrics Metrics;

	if (!m_Renderer.GetTextMetrics(pText,Length,*m_pFont,&Metrics))
		return false;

	pMetrics->Width=static_cast<int>(std::ceil(Metrics.WidthIncludingTrailingWhitespace));
	pMetrics->Height=static_cast<int>(std::ceil(Metrics.Height));

	return true;
}


bool CTextDrawEngine_DirectWrite::SetClippingRect(const RECT &Rect)
{
	return m_Renderer.SetClippingRect(Rect);
}


bool CTextDrawEngine_DirectWrite::ResetClipping()
{
	return m_Renderer.ResetClipping();
}


bool CTextDrawEngine_DirectWrite::OnWindowPosChanged()
{
	return m_Renderer.OnWindowPosChanged();
}


void CTextDrawEngine_DirectWrite::ClearFontCache()
{
	for (auto it=m_FontCache.begin();it!=m_FontCache.end();++it)
		delete *it;
	m_FontCache.clear();
}


bool CTextDrawEngine_DirectWrite::SetMaxFontCache(std::size_t MaxCache)
{
	if (MaxCache<1)
		return false;

	while (m_FontCache.size()>MaxCache) {
		delete m_FontCache.back();
		m_FontCache.pop_back();
	}

	m_MaxFontCache=MaxCache;

	return true;
}


}	// namespace TVTest
