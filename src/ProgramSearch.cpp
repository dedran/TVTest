#include "stdafx.h"
#include "TVTest.h"
#include "AppMain.h"
#include "ProgramSearch.h"
#include "DialogUtil.h"
#include "EpgUtil.h"
#include "EventInfoUtil.h"
#include "GUIUtil.h"
#include "resource.h"
#include "Common/DebugDef.h"



void CEventSearchServiceList::Clear()
{
	m_ServiceList.clear();
}


bool CEventSearchServiceList::IsEmpty() const
{
	return m_ServiceList.empty();
}


size_t CEventSearchServiceList::GetServiceCount() const
{
	return m_ServiceList.size();
}


void CEventSearchServiceList::Add(ServiceKey Key)
{
	m_ServiceList.insert(Key);
}


void CEventSearchServiceList::Add(WORD NetworkID,WORD TSID,WORD ServiceID)
{
	m_ServiceList.insert(GetServiceKey(NetworkID,TSID,ServiceID));
}


bool CEventSearchServiceList::IsExists(ServiceKey Key) const
{
	return m_ServiceList.find(Key)!=m_ServiceList.end();
}


bool CEventSearchServiceList::IsExists(WORD NetworkID,WORD TSID,WORD ServiceID) const
{
	return m_ServiceList.find(GetServiceKey(NetworkID,TSID,ServiceID))!=m_ServiceList.end();
}


void CEventSearchServiceList::Combine(const CEventSearchServiceList &List)
{
	for (auto it=List.m_ServiceList.begin();it!=List.m_ServiceList.end();++it) {
		m_ServiceList.insert(*it);
	}
}


CEventSearchServiceList::Iterator CEventSearchServiceList::Begin() const
{
	return m_ServiceList.begin();
}


CEventSearchServiceList::Iterator CEventSearchServiceList::End() const
{
	return m_ServiceList.end();
}


bool CEventSearchServiceList::ToString(TVTest::String *pString) const
{
	if (pString==NULL)
		return false;

	pString->clear();

	ServiceKey PrevKey=0;

	for (auto it=m_ServiceList.begin();it!=m_ServiceList.end();++it) {
		ServiceKey Key=*it;
		TCHAR szKey[16];
		if (PrevKey!=0 && PrevKey>>16==Key>>16)
			Key&=0xFFFFULL;
		else if (ServiceKey_GetNetworkID(Key)==ServiceKey_GetTransportStreamID(Key))
			Key&=0xFFFFFFFFULL;
		int Length=EncodeServiceKey(Key,szKey);
		szKey[Length]=_T(':');
		szKey[Length+1]=_T('\0');
		*pString+=szKey;
		PrevKey=*it;
	}

	return true;
}


bool CEventSearchServiceList::FromString(LPCTSTR pszString)
{
	if (pszString==NULL)
		return false;

	m_ServiceList.clear();

	LPCTSTR p=pszString;
	ServiceKey PrevKey=0;
	while (*p!=_T('\0')) {
		size_t Length=0;
		for (Length=0;p[Length]!=_T(':') && p[Length]!=_T('\0');Length++);
		if (Length>0) {
			ServiceKey Key;
			if (!DecodeServiceKey(p,Length,&Key))
				break;
			if (Key<=0xFFFFULL)
				Key|=PrevKey&0xFFFFFFFF0000ULL;
			else if (ServiceKey_GetNetworkID(Key)==0)
				Key|=(ULONGLONG)ServiceKey_GetTransportStreamID(Key)<<32;
			m_ServiceList.insert(Key);
			PrevKey=Key;
			p+=Length;
		}
		if (*p==_T(':'))
			p++;
	}

	return true;
}


int CEventSearchServiceList::EncodeServiceKey(ServiceKey Key,LPTSTR pText)
{
	static const char EncodeChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	int Length=0;
	for (int i=0;i<8;i++) {
		unsigned int Char=(unsigned int)((Key>>(42-i*6))&0x3F);
		if (Char!=0 || Length>0) {
			pText[Length++]=EncodeChars[Char];
		}
	}

	return Length;
}


bool CEventSearchServiceList::DecodeServiceKey(LPCTSTR pText,size_t Length,ServiceKey *pKey)
{
	ServiceKey Key=0;

	for (size_t i=0;i<Length;i++) {
		TCHAR c=pText[i];
		unsigned int Value;

		if (c>=_T('A') && c<=_T('Z')) {
			Value=c-_T('A');
		} else if (c>=_T('a') && c<=_T('z')) {
			Value=c-_T('a')+26;
		} else if (c>=_T('0') && c<=_T('9')) {
			Value=c-_T('0')+52;
		} else if (c==_T('+')) {
			Value=62;
		} else if (c==_T('/')) {
			Value=63;
		} else {
			return false;
		}
		Key=(Key<<6) | Value;
	}

	*pKey=Key;

	return true;
}




CEventSearchSettings::CEventSearchSettings()
{
	Clear();
}


void CEventSearchSettings::Clear()
{
	fDisabled=false;
	Name.clear();
	Keyword.clear();
	fRegExp=false;
	fIgnoreCase=true;
	fIgnoreWidth=true;
	fEventName=true;
	fEventText=true;
	fGenre=false;
	Genre1=0x0000;
	::ZeroMemory(Genre2,sizeof(Genre2));
	fDayOfWeek=false;
	DayOfWeekFlags=0x00;
	fTime=false;
	StartTime.Hour=0;
	StartTime.Minute=0;
	EndTime.Hour=23;
	EndTime.Minute=59;
	fDuration=false;
	DurationShortest=10*60;
	DurationLongest=0;
	fCA=false;
	CA=CA_FREE;
	fVideo=false;
	Video=VIDEO_HD;
	fServiceList=false;
	ServiceList.Clear();
}


bool CEventSearchSettings::ToString(TVTest::String *pString) const
{
	TVTest::String Buffer;

	pString->clear();

	TVTest::StringUtility::Encode(Name.c_str(),&Buffer);
	*pString+=Buffer;
	*pString+=TEXT(",");
	TVTest::StringUtility::Encode(Keyword.c_str(),&Buffer);
	*pString+=Buffer;

	unsigned int Flags=0;
	if (fDisabled)
		Flags|=FLAG_DISABLED;
	if (fRegExp)
		Flags|=FLAG_REG_EXP;
	if (fIgnoreCase)
		Flags|=FLAG_IGNORE_CASE;
	if (fIgnoreWidth)
		Flags|=FLAG_IGNORE_WIDTH;
	if (fEventName)
		Flags|=FLAG_EVENT_NAME;
	if (fEventText)
		Flags|=FLAG_EVENT_TEXT;
	if (fGenre)
		Flags|=FLAG_GENRE;
	if (fDayOfWeek)
		Flags|=FLAG_DAY_OF_WEEK;
	if (fTime)
		Flags|=FLAG_TIME;
	if (fDuration)
		Flags|=FLAG_DURATION;
	if (fCA)
		Flags|=FLAG_CA;
	if (fVideo)
		Flags|=FLAG_VIDEO;
	if (fServiceList)
		Flags|=FLAG_SERVICE_LIST;

	bool fGenre2=false;
	for (int i=0;i<16;i++) {
		if (Genre2[i]!=0) {
			fGenre2=true;
			break;
		}
	}
	TCHAR szGenre2[16*4+1];
	if (fGenre2) {
		for (int i=0;i<16;i++) {
			::wsprintf(&szGenre2[i*4],TEXT("%04x"),Genre2[i]);
		}
	} else {
		szGenre2[0]=_T('\0');
	}

	TVTest::StringUtility::Format(Buffer,
		TEXT(",%u,%d,%s,%u,%d:%02d,%d:%02d,%u,%u,%d,%d"),
		Flags,
		Genre1,
		szGenre2,
		DayOfWeekFlags,
		StartTime.Hour,StartTime.Minute,
		EndTime.Hour,EndTime.Minute,
		DurationShortest,
		DurationLongest,
		(int)CA,
		(int)Video);
	*pString+=Buffer;

	if (!ServiceList.IsEmpty()) {
		*pString+=TEXT(",");
		ServiceList.ToString(&Buffer);
		*pString+=Buffer;
	}

	return true;
}


bool CEventSearchSettings::FromString(LPCTSTR pszString)
{
	std::vector<TVTest::String> Value;

	TVTest::StringUtility::Split(TVTest::String(pszString),L",",&Value);

	for (size_t i=0;i<Value.size();i++) {
		switch (i) {
		case 0:
			TVTest::StringUtility::Decode(Value[i].c_str(),&Name);
			break;

		case 1:
			TVTest::StringUtility::Decode(Value[i].c_str(),&Keyword);
			break;

		case 2:
			{
				unsigned int Flags=std::wcstoul(Value[i].c_str(),nullptr,0);
				fDisabled=(Flags&FLAG_DISABLED)!=0;
				fRegExp=(Flags&FLAG_REG_EXP)!=0;
				fIgnoreCase=(Flags&FLAG_IGNORE_CASE)!=0;
				fIgnoreWidth=(Flags&FLAG_IGNORE_WIDTH)!=0;
				fEventName=(Flags&FLAG_EVENT_NAME)!=0;
				fEventText=(Flags&FLAG_EVENT_TEXT)!=0;
				fGenre=(Flags&FLAG_GENRE)!=0;
				fDayOfWeek=(Flags&FLAG_DAY_OF_WEEK)!=0;
				fTime=(Flags&FLAG_TIME)!=0;
				fDuration=(Flags&FLAG_DURATION)!=0;
				fCA=(Flags&FLAG_CA)!=0;
				fVideo=(Flags&FLAG_VIDEO)!=0;
				fServiceList=(Flags&FLAG_SERVICE_LIST)!=0;
			}
			break;

		case 3:
			Genre1=(WORD)std::wcstoul(Value[i].c_str(),nullptr,0);
			break;

		case 4:
			if (Value[i].length()>=16*4) {
				auto it=Value[i].begin();
				for (int j=0;j<16;j++) {
					TCHAR Hex[4];
					Hex[0]=*it++;
					Hex[1]=*it++;
					Hex[2]=*it++;
					Hex[3]=*it++;
					Genre2[j]=(WORD)HexStringToUInt(Hex,4);
				}
			} else {
				::ZeroMemory(Genre2,sizeof(Genre2));
			}
			break;

		case 5:
			DayOfWeekFlags=std::wcstoul(Value[i].c_str(),nullptr,0);
			break;

		case 6:
			ParseTime(Value[i].c_str(),&StartTime);
			break;

		case 7:
			ParseTime(Value[i].c_str(),&EndTime);
			break;

		case 8:
			DurationShortest=std::wcstoul(Value[i].c_str(),nullptr,0);
			break;

		case 9:
			DurationLongest=std::wcstoul(Value[i].c_str(),nullptr,0);
			break;

		case 10:
			CA=(CAType)std::wcstoul(Value[i].c_str(),nullptr,0);
			break;

		case 11:
			Video=(VideoType)std::wcstoul(Value[i].c_str(),nullptr,0);
			break;

		case 12:
			ServiceList.FromString(Value[i].c_str());
			break;
		}
	}

	return true;
}


void CEventSearchSettings::ParseTime(LPCWSTR pszString,TimeInfo *pTime)
{
	wchar_t *p;

	pTime->Hour=std::wcstol(pszString,&p,10);
	if (*p==L':')
		pTime->Minute=std::wcstol(p+1,nullptr,10);
	else
		pTime->Minute=0;
}




CEventSearchSettingsList::CEventSearchSettingsList()
{
}


CEventSearchSettingsList::CEventSearchSettingsList(const CEventSearchSettingsList &Src)
{
	*this=Src;
}


CEventSearchSettingsList::~CEventSearchSettingsList()
{
	Clear();
}


CEventSearchSettingsList &CEventSearchSettingsList::operator=(const CEventSearchSettingsList &Src)
{
	if (&Src!=this) {
		Clear();

		if (!Src.m_List.empty()) {
			m_List.reserve(Src.m_List.size());

			for (auto it=Src.m_List.begin();it!=Src.m_List.end();++it) {
				m_List.push_back(new CEventSearchSettings(**it));
			}
		}
	}

	return *this;
}


void CEventSearchSettingsList::Clear()
{
	for (auto it=m_List.begin();it!=m_List.end();++it)
		delete *it;
	m_List.clear();
}


size_t CEventSearchSettingsList::GetCount() const
{
	return m_List.size();
}


size_t CEventSearchSettingsList::GetEnabledCount() const
{
	size_t Count=0;
	for (auto it=m_List.begin();it!=m_List.end();++it) {
		if (!(*it)->fDisabled)
			Count++;
	}
	return Count;
}


CEventSearchSettings *CEventSearchSettingsList::Get(size_t Index)
{
	if (Index>=m_List.size())
		return NULL;
	return m_List[Index];
}


const CEventSearchSettings *CEventSearchSettingsList::Get(size_t Index) const
{
	if (Index>=m_List.size())
		return NULL;
	return m_List[Index];
}


CEventSearchSettings *CEventSearchSettingsList::GetByName(LPCTSTR pszName)
{
	int Index=FindByName(pszName);
	if (Index<0)
		return NULL;
	return m_List[Index];
}


const CEventSearchSettings *CEventSearchSettingsList::GetByName(LPCTSTR pszName) const
{
	int Index=FindByName(pszName);
	if (Index<0)
		return NULL;
	return m_List[Index];
}


bool CEventSearchSettingsList::Add(const CEventSearchSettings &Settings)
{
	m_List.push_back(new CEventSearchSettings(Settings));
	return true;
}


bool CEventSearchSettingsList::Erase(size_t Index)
{
	if (Index>=m_List.size())
		return false;

	auto it=m_List.begin();
	std::advance(it,Index);
	delete *it;
	m_List.erase(it);

	return true;
}


int CEventSearchSettingsList::FindByName(LPCTSTR pszName) const
{
	if (pszName==NULL)
		return -1;

	for (size_t i=0;i<m_List.size();i++) {
		if (::lstrcmpi(m_List[i]->Name.c_str(),pszName)==0)
			return (int)i;
	}

	return -1;
}


bool CEventSearchSettingsList::Load(CSettings &Settings,LPCTSTR pszPrefix)
{
	Clear();

	TCHAR szKey[256];
	TVTest::String Value;

	for (int i=0;;i++) {
		StdUtil::snprintf(szKey,lengthof(szKey),TEXT("%s%d"),pszPrefix,i);
		if (!Settings.Read(szKey,&Value))
			break;
		CEventSearchSettings SearchSettings;
		if (SearchSettings.FromString(Value.c_str())) {
			m_List.push_back(new CEventSearchSettings(SearchSettings));
		}
	}

	return true;
}


bool CEventSearchSettingsList::Save(CSettings &Settings,LPCTSTR pszPrefix) const
{
	TCHAR szKey[256];
	TVTest::String Value;

	for (size_t i=0;i<m_List.size();i++) {
		StdUtil::snprintf(szKey,lengthof(szKey),TEXT("%s%d"),pszPrefix,(int)i);
		m_List[i]->ToString(&Value);
		Settings.Write(szKey,Value);
	}

	return true;
}




CEventSearcher::CEventSearcher()
#ifdef WIN_XP_SUPPORT
	: m_pFindNLSString(GET_MODULE_FUNCTION(TEXT("kernel32.dll"),FindNLSString))
#endif
{
}


bool CEventSearcher::InitializeRegExp()
{
	return m_RegExp.Initialize();
}


void CEventSearcher::Finalize()
{
	m_RegExp.Finalize();
}


bool CEventSearcher::BeginSearch(const CEventSearchSettings &Settings)
{
	m_Settings=Settings;

	if (Settings.fRegExp && !Settings.Keyword.empty()) {
		if (!m_RegExp.Initialize())
			return false;
		UINT Flags=0;
		if (Settings.fIgnoreCase)
			Flags|=TVTest::CRegExp::FLAG_IGNORE_CASE;
		if (Settings.fIgnoreWidth)
			Flags|=TVTest::CRegExp::FLAG_IGNORE_WIDTH;
		if (!m_RegExp.SetPattern(Settings.Keyword.c_str(),Flags))
			return false;
	}

	return true;
}


bool CEventSearcher::Match(const CEventInfoData *pEventInfo)
{
	if (m_Settings.fServiceList) {
		if (!m_Settings.ServiceList.IsExists(
				pEventInfo->m_NetworkID,
				pEventInfo->m_TransportStreamID,
				pEventInfo->m_ServiceID))
			return false;
	}

	if (m_Settings.fGenre) {
		bool fMatch=false;
		for (int i=0;i<pEventInfo->m_ContentNibble.NibbleCount;i++) {
			int Level1=pEventInfo->m_ContentNibble.NibbleList[i].ContentNibbleLevel1;
			if (Level1!=0xE) {
				if (Level1>15)
					return false;
				if ((m_Settings.Genre1&(1<<Level1))!=0
						&& m_Settings.Genre2[Level1]==0) {
					fMatch=true;
				} else {
					int Level2=pEventInfo->m_ContentNibble.NibbleList[i].ContentNibbleLevel2;
					if ((m_Settings.Genre2[Level1]&(1<<Level2))!=0)
						fMatch=true;
				}
				break;
			}
		}
		if (!fMatch)
			return false;
	}

	if (m_Settings.fDayOfWeek) {
		if ((m_Settings.DayOfWeekFlags&(1<<pEventInfo->m_StartTime.wDayOfWeek))==0)
			return false;
	}

	if (m_Settings.fTime) {
		int RangeStart=(m_Settings.StartTime.Hour*60+m_Settings.StartTime.Minute)%(24*60);
		int RangeEnd=(m_Settings.EndTime.Hour*60+m_Settings.EndTime.Minute)%(24*60);
		int EventStart=pEventInfo->m_StartTime.wHour*60+pEventInfo->m_StartTime.wMinute;
		int EventEnd=EventStart+pEventInfo->m_Duration/60;

		if (RangeStart<=RangeEnd) {
			if (EventEnd<=RangeStart || EventStart>RangeEnd)
				return false;
		} else {
			if (EventEnd<=RangeStart && EventStart>RangeEnd)
				return false;
		}
	}

	if (m_Settings.fDuration) {
		if (pEventInfo->m_Duration<m_Settings.DurationShortest)
			return false;
		if (m_Settings.DurationLongest>0
				&& pEventInfo->m_Duration>m_Settings.DurationLongest)
			return false;
	}

	if (m_Settings.fCA) {
		switch (m_Settings.CA) {
		case CEventSearchSettings::CA_FREE:
			if (pEventInfo->m_bFreeCaMode)
				return false;
			break;
		case CEventSearchSettings::CA_CHARGEABLE:
			if (!pEventInfo->m_bFreeCaMode)
				return false;
			break;
		}
	}

	if (m_Settings.fVideo
			&& !pEventInfo->m_VideoList.empty()) {
		switch (m_Settings.Video) {
		case CEventSearchSettings::VIDEO_HD:
			if (EpgUtil::GetVideoType(pEventInfo->m_VideoList[0].ComponentType)!=EpgUtil::VIDEO_TYPE_HD)
				return false;
			break;
		case CEventSearchSettings::VIDEO_SD:
			if (EpgUtil::GetVideoType(pEventInfo->m_VideoList[0].ComponentType)!=EpgUtil::VIDEO_TYPE_SD)
				return false;
			break;
		}
	}

	if (m_Settings.Keyword.empty())
		return true;

	if (m_Settings.fRegExp)
		return MatchRegExp(pEventInfo);

	return MatchKeyword(pEventInfo,m_Settings.Keyword.c_str());
}


int CEventSearcher::FindKeyword(LPCTSTR pszText,LPCTSTR pKeyword,int KeywordLength,int *pFoundLength) const
{
	if (IsStringEmpty(pszText))
		return -1;

	UINT Flags=0;
	if (m_Settings.fIgnoreCase)
		Flags|=NORM_IGNORECASE;
	if (m_Settings.fIgnoreWidth)
		Flags|=NORM_IGNOREWIDTH;

#ifdef WIN_XP_SUPPORT
	int Pos;

	if (m_pFindNLSString!=NULL) {
		// Vista以降
		Pos=m_pFindNLSString(LOCALE_USER_DEFAULT,FIND_FROMSTART | Flags,
							 pszText,-1,pKeyword,KeywordLength,pFoundLength);
	} else {
		const int StringLength=::lstrlen(pszText);

		if (StringLength<KeywordLength)
			return -1;

		Pos=-1;
		for (int i=0;i<=StringLength-KeywordLength;i++) {
			if (::CompareString(LOCALE_USER_DEFAULT,Flags,
					pszText+i,KeywordLength,pKeyword,KeywordLength)==CSTR_EQUAL) {
				Pos=i;
				if (pFoundLength!=NULL)
					*pFoundLength=KeywordLength;
				break;
			}
		}
	}

	return Pos;
#else	// WIN_XP_SUPPORT
	return ::FindNLSString(LOCALE_USER_DEFAULT,FIND_FROMSTART | Flags,
						   pszText,-1,pKeyword,KeywordLength,pFoundLength);
#endif
}


bool CEventSearcher::MatchKeyword(const CEventInfoData *pEventInfo,LPCTSTR pszKeyword) const
{
	bool fMatch=false,fMinusOnly=true;
	bool fOr=false,fPrevOr=false,fOrMatch;
	int WordCount=0;
	LPCTSTR p=pszKeyword;

	while (*p!='\0') {
		TCHAR szWord[CEventSearchSettings::MAX_KEYWORD_LENGTH],Delimiter;
		bool fMinus=false;

		while (*p==' ')
			p++;
		if (*p=='-') {
			fMinus=true;
			p++;
		}
		if (*p=='"') {
			p++;
			Delimiter='"';
		} else {
			Delimiter=' ';
		}
		int i;
		for (i=0;*p!=Delimiter && *p!='|' && *p!='\0';i++) {
			szWord[i]=*p++;
		}
		if (*p==Delimiter)
			p++;
		while (*p==' ')
			p++;
		if (*p=='|') {
			if (!fOr) {
				fOr=true;
				fOrMatch=false;
			}
			p++;
		} else {
			fOr=false;
		}
		if (i>0) {
			if ((m_Settings.fEventName
						&& FindKeyword(pEventInfo->m_EventName.c_str(),szWord,i)>=0)
					|| (m_Settings.fEventText
						&& FindKeyword(pEventInfo->m_EventText.c_str(),szWord,i)>=0)
					|| (m_Settings.fEventText
						&& FindKeyword(pEventInfo->m_EventExtendedText.c_str(),szWord,i)>=0)) {
				if (fMinus)
					return false;
				fMatch=true;
				if (fOr)
					fOrMatch=true;
			} else {
				if (!fMinus && !fOr && (!fPrevOr || !fOrMatch))
					return false;
			}
			if (!fMinus)
				fMinusOnly=false;
			WordCount++;
		}
		fPrevOr=fOr;
	}
	if (fMinusOnly && WordCount>0)
		return true;
	return fMatch;
}


bool CEventSearcher::MatchRegExp(const CEventInfoData *pEventInfo)
{
	return (m_Settings.fEventName
			&& !pEventInfo->m_EventName.empty()
			&& m_RegExp.Match(pEventInfo->m_EventName.c_str()))
		|| (m_Settings.fEventText
			&& !pEventInfo->m_EventText.empty()
			&& m_RegExp.Match(pEventInfo->m_EventText.c_str()))
		|| (m_Settings.fEventText
			&& !pEventInfo->m_EventExtendedText.empty()
			&& m_RegExp.Match(pEventInfo->m_EventExtendedText.c_str()));
}




CEventSearchOptions::CEventSearchOptions()
	: m_MaxKeywordHistory(40)
{
}


bool CEventSearchOptions::SetKeywordHistory(const LPTSTR *pKeywordList,int NumKeywords)
{
	if (pKeywordList==NULL)
		return false;
	m_KeywordHistory.clear();
	for (int i=0;i<NumKeywords;i++)
		m_KeywordHistory.push_back(TVTest::String(pKeywordList[i]));
	return true;
}


int CEventSearchOptions::GetKeywordHistoryCount() const
{
	return (int)m_KeywordHistory.size();
}


LPCTSTR CEventSearchOptions::GetKeywordHistory(int Index) const
{
	if (Index<0 || (size_t)Index>=m_KeywordHistory.size())
		return NULL;
	return m_KeywordHistory[Index].c_str();
}


bool CEventSearchOptions::AddKeywordHistory(LPCTSTR pszKeyword)
{
	if (IsStringEmpty(pszKeyword))
		return false;

	for (auto it=m_KeywordHistory.begin();it!=m_KeywordHistory.end();++it) {
		if (TVTest::StringUtility::CompareNoCase(*it,pszKeyword)==0) {
			if (it==m_KeywordHistory.begin()
					&& it->compare(pszKeyword)==0)
				return true;
			m_KeywordHistory.erase(it);
			break;
		}
	}

	m_KeywordHistory.push_front(TVTest::String(pszKeyword));

	if (m_KeywordHistory.size()>(size_t)m_MaxKeywordHistory) {
		m_KeywordHistory.erase(m_KeywordHistory.begin()+m_MaxKeywordHistory,m_KeywordHistory.end());
	}

	return true;
}


bool CEventSearchOptions::DeleteKeywordHistory(int Index)
{
	if (Index<0 || (size_t)Index>=m_KeywordHistory.size())
		return false;

	m_KeywordHistory.erase(m_KeywordHistory.begin()+Index);

	return true;
}


void CEventSearchOptions::ClearKeywordHistory()
{
	m_KeywordHistory.clear();
}


bool CEventSearchOptions::SetMaxKeywordHistory(int Max)
{
	if (Max<0)
		return false;

	m_MaxKeywordHistory=Max;

	if (m_KeywordHistory.size()>(size_t)Max) {
		m_KeywordHistory.erase(m_KeywordHistory.begin()+Max,m_KeywordHistory.end());
	}

	return true;
}


size_t CEventSearchOptions::GetSearchSettingsCount() const
{
	return m_SettingsList.GetCount();
}


CEventSearchSettings *CEventSearchOptions::GetSearchSettings(size_t Index)
{
	return m_SettingsList.Get(Index);
}


const CEventSearchSettings *CEventSearchOptions::GetSearchSettings(size_t Index) const
{
	return m_SettingsList.Get(Index);
}


CEventSearchSettings *CEventSearchOptions::GetSearchSettingsByName(LPCTSTR pszName)
{
	return m_SettingsList.GetByName(pszName);
}


const CEventSearchSettings *CEventSearchOptions::GetSearchSettingsByName(LPCTSTR pszName) const
{
	return m_SettingsList.GetByName(pszName);
}


void CEventSearchOptions::ClearSearchSettings()
{
	m_SettingsList.Clear();
}


bool CEventSearchOptions::AddSearchSettings(const CEventSearchSettings &Settings)
{
	return m_SettingsList.Add(Settings);
}


bool CEventSearchOptions::DeleteSearchSettings(size_t Index)
{
	return m_SettingsList.Erase(Index);
}


int CEventSearchOptions::FindSearchSettings(LPCTSTR pszName) const
{
	return m_SettingsList.FindByName(pszName);
}


bool CEventSearchOptions::LoadSearchSettings(CSettings &Settings,LPCTSTR pszPrefix)
{
	return m_SettingsList.Load(Settings,pszPrefix);
}


bool CEventSearchOptions::SaveSearchSettings(CSettings &Settings,LPCTSTR pszPrefix) const
{
	return m_SettingsList.Save(Settings,pszPrefix);
}




#ifndef LVN_GETEMPTYMARKUP
#include <pshpack1.h>
#define LVN_GETEMPTYMARKUP	(LVN_FIRST-87)
#define EMF_CENTERED		0x00000001
typedef struct tagNMLVEMPTYMARKUP {
	NMHDR hdr;
	DWORD dwFlags;
	WCHAR szMarkup[L_MAX_URL_LENGTH];
} NMLVEMPTYMARKUP;
#include <poppack.h>
#endif


#define WM_PROGRAM_SEARCH_GENRE_CHANGED WM_APP

#define GENRE_LPARAM_PACK(Level1,Level2) (((Level1)<<16) | (WORD)(SHORT)(Level2))
#define GENRE_LPARAM_LEVEL1(lParam) ((int)((lParam)>>16))
#define GENRE_LPARAM_LEVEL2(lParam) ((SHORT)(WORD)((lParam)&0xFFFF))


static ULONGLONG GetResultMapKey(const CEventInfoData *pEventInfo)
{
	return ((ULONGLONG)pEventInfo->m_NetworkID<<48)
		| ((ULONGLONG)pEventInfo->m_TransportStreamID<<32)
		| ((DWORD)pEventInfo->m_ServiceID<<16)
		| pEventInfo->m_EventID;
}




CEventSearchSettingsDialog::CEventSearchSettingsDialog(CEventSearchOptions &Options)
	: m_pEventHandler(NULL)
	, m_Options(Options)
	, m_SearchTarget(0)
{
	for (int i=0;i<lengthof(m_fGenreExpanded);i++)
		m_fGenreExpanded[i]=false;
}


CEventSearchSettingsDialog::~CEventSearchSettingsDialog()
{
}


bool CEventSearchSettingsDialog::Create(HWND hwndOwner)
{
	return CreateDialogWindow(hwndOwner,
							  GetAppClass().GetResourceInstance(),
							  MAKEINTRESOURCE(IDD_EVENTSEARCH));
}


bool CEventSearchSettingsDialog::GetSettings(CEventSearchSettings *pSettings) const
{
	if (m_hDlg!=NULL) {
		TCHAR szKeyword[CEventSearchSettings::MAX_KEYWORD_LENGTH];
		::GetDlgItemText(m_hDlg,IDC_EVENTSEARCH_KEYWORD,szKeyword,lengthof(szKeyword));
		RemoveTrailingWhitespace(szKeyword);
		pSettings->Keyword=szKeyword;
		pSettings->fIgnoreCase=
			!DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_CASESENSITIVE);
		pSettings->fRegExp=
			DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_REGEXP);

		switch (DlgComboBox_GetCurSel(m_hDlg,IDC_EVENTSEARCH_KEYWORDTARGET)) {
		case KEYWORDTARGET_EVENTNAME_AND_EVENTTEXT:
			pSettings->fEventName=true;
			pSettings->fEventText=true;
			break;
		case KEYWORDTARGET_EVENTNAME:
			pSettings->fEventName=true;
			pSettings->fEventText=false;
			break;
		case KEYWORDTARGET_EVENTTEXT:
			pSettings->fEventName=false;
			pSettings->fEventText=true;
			break;
		}

		GetGenreSettings(pSettings);

		pSettings->fDayOfWeek=
			DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_DAYOFWEEK);
		unsigned int DayOfWeekFlags=0;
		for (int i=0;i<7;i++) {
			if (DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_DAYOFWEEK_SUNDAY+i))
				DayOfWeekFlags|=1<<i;
		}
		pSettings->DayOfWeekFlags=DayOfWeekFlags;

		pSettings->fTime=
			DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_TIME);
		pSettings->StartTime.Hour=
			DlgEdit_GetInt(m_hDlg,IDC_EVENTSEARCH_TIME_START_HOUR);
		pSettings->StartTime.Minute=
			DlgEdit_GetInt(m_hDlg,IDC_EVENTSEARCH_TIME_START_MINUTE);
		pSettings->EndTime.Hour=
			DlgEdit_GetInt(m_hDlg,IDC_EVENTSEARCH_TIME_END_HOUR);
		pSettings->EndTime.Minute=
			DlgEdit_GetInt(m_hDlg,IDC_EVENTSEARCH_TIME_END_MINUTE);

		pSettings->fDuration=
			DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_DURATION);
		pSettings->DurationShortest=
			DlgEdit_GetInt(m_hDlg,IDC_EVENTSEARCH_DURATION_SHORT_INPUT)*60;

		pSettings->fCA=
			DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_CA);
		pSettings->CA=
			(CEventSearchSettings::CAType)DlgComboBox_GetCurSel(m_hDlg,IDC_EVENTSEARCH_CA_LIST);

		pSettings->fVideo=
			DlgCheckBox_IsChecked(m_hDlg,IDC_EVENTSEARCH_VIDEO);
		pSettings->Video=
			(CEventSearchSettings::VideoType)DlgComboBox_GetCurSel(m_hDlg,IDC_EVENTSEARCH_VIDEO_LIST);
	} else {
		*pSettings=m_SearchSettings;
	}

	return true;
}


void CEventSearchSettingsDialog::SetSettings(const CEventSearchSettings &Settings)
{
	if (m_hDlg!=NULL) {
		// キーワード
		::SetDlgItemText(m_hDlg,IDC_EVENTSEARCH_KEYWORD,Settings.Keyword.c_str());
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_CASESENSITIVE,!Settings.fIgnoreCase);
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_REGEXP,Settings.fRegExp);

		// キーワード検索対象
		int KeywordTarget=KEYWORDTARGET_EVENTNAME_AND_EVENTTEXT;
		if (Settings.fEventName!=Settings.fEventText) {
			if (Settings.fEventName)
				KeywordTarget=KEYWORDTARGET_EVENTNAME;
			else
				KeywordTarget=KEYWORDTARGET_EVENTTEXT;
		}
		DlgComboBox_SetCurSel(m_hDlg,IDC_EVENTSEARCH_KEYWORDTARGET,KeywordTarget);

		// ジャンル
		HWND hwndGenre=::GetDlgItem(m_hDlg,IDC_EVENTSEARCH_GENRE);
		HTREEITEM hItem=TreeView_GetChild(hwndGenre,TVI_ROOT);
		TVITEM tvi;
		tvi.mask=TVIF_PARAM;
		while (hItem!=NULL) {
			tvi.hItem=hItem;
			TreeView_GetItem(hwndGenre,&tvi);
			int Level1=GENRE_LPARAM_LEVEL1(tvi.lParam);
			TreeView_SetCheckState(hwndGenre,hItem,(Settings.Genre1&(1<<Level1))!=0);
			HTREEITEM hChild=TreeView_GetChild(hwndGenre,hItem);
			while (hChild!=NULL) {
				tvi.hItem=hChild;
				TreeView_GetItem(hwndGenre,&tvi);
				int Level2=GENRE_LPARAM_LEVEL2(tvi.lParam);
				TreeView_SetCheckState(hwndGenre,hChild,(Settings.Genre2[Level1]&(1<<Level2))!=0);
				hChild=TreeView_GetNextSibling(hwndGenre,hChild);
			}
			hItem=TreeView_GetNextSibling(hwndGenre,hItem);
		}
		SetGenreStatus();

		// 曜日
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_DAYOFWEEK,Settings.fDayOfWeek);
		for (int i=0;i<7;i++) {
			::CheckDlgButton(m_hDlg,IDC_EVENTSEARCH_DAYOFWEEK_SUNDAY+i,
							 (Settings.DayOfWeekFlags&(1<<i))!=0);
		}
		EnableDlgItems(m_hDlg,
					   IDC_EVENTSEARCH_DAYOFWEEK_SUNDAY,
					   IDC_EVENTSEARCH_DAYOFWEEK_SATURDAY,
					   Settings.fDayOfWeek);

		// 時間
		TCHAR szText[16];
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_TIME,Settings.fTime);
		DlgEdit_SetInt(m_hDlg,IDC_EVENTSEARCH_TIME_START_HOUR,Settings.StartTime.Hour);
		StdUtil::snprintf(szText,lengthof(szText),TEXT("%02d"),Settings.StartTime.Minute);
		DlgEdit_SetText(m_hDlg,IDC_EVENTSEARCH_TIME_START_MINUTE,szText);
		DlgEdit_SetInt(m_hDlg,IDC_EVENTSEARCH_TIME_END_HOUR,Settings.EndTime.Hour);
		StdUtil::snprintf(szText,lengthof(szText),TEXT("%02d"),Settings.EndTime.Minute);
		DlgEdit_SetText(m_hDlg,IDC_EVENTSEARCH_TIME_END_MINUTE,szText);
		EnableDlgItems(m_hDlg,IDC_EVENTSEARCH_TIME_START_HOUR,IDC_EVENTSEARCH_TIME_END_MINUTE,
					   Settings.fTime);

		// 長さ
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_DURATION,Settings.fDuration);
		DlgEdit_SetInt(m_hDlg,IDC_EVENTSEARCH_DURATION_SHORT_INPUT,Settings.DurationShortest/60);
		EnableDlgItems(m_hDlg,
					   IDC_EVENTSEARCH_DURATION_SHORT_INPUT,
					   IDC_EVENTSEARCH_DURATION_SHORT_UNIT,
					   Settings.fDuration);

		// CA
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_CA,Settings.fCA);
		DlgComboBox_SetCurSel(m_hDlg,IDC_EVENTSEARCH_CA_LIST,(int)Settings.CA);
		EnableDlgItem(m_hDlg,IDC_EVENTSEARCH_CA_LIST,Settings.fCA);

		// 映像
		DlgCheckBox_Check(m_hDlg,IDC_EVENTSEARCH_VIDEO,Settings.fVideo);
		DlgComboBox_SetCurSel(m_hDlg,IDC_EVENTSEARCH_VIDEO_LIST,(int)Settings.Video);
		EnableDlgItem(m_hDlg,IDC_EVENTSEARCH_VIDEO_LIST,Settings.fVideo);
	} else {
		m_SearchSettings=Settings;
	}
}


void CEventSearchSettingsDialog::SetEventHandler(CEventHandler *pEventHandler)
{
	m_pEventHandler=pEventHandler;
}


bool CEventSearchSettingsDialog::BeginSearch()
{
	if (m_hDlg==NULL)
		return false;
	::SendMessage(m_hDlg,WM_COMMAND,IDC_EVENTSEARCH_SEARCH,0);
	return true;
}


bool CEventSearchSettingsDialog::SetKeyword(LPCTSTR pszKeyword)
{
	if (m_hDlg==NULL)
		return false;
	::SetDlgItemText(m_hDlg,IDC_EVENTSEARCH_KEYWORD,pszKeyword==NULL?TEXT(""):pszKeyword);
	::UpdateWindow(::GetDlgItem(m_hDlg,IDC_EVENTSEARCH_KEYWORD));
	return true;
}


bool CEventSearchSettingsDialog::AddToKeywordHistory(LPCTSTR pszKeyword)
{
	if (m_hDlg==NULL || IsStringEmpty(pszKeyword))
		return false;

	HWND hwndComboBox=::GetDlgItem(m_hDlg,IDC_EVENTSEARCH_KEYWORD);
	int i;
	i=ComboBox_FindStringExact(hwndComboBox,-1,pszKeyword);
	if (i==CB_ERR) {
		ComboBox_InsertString(hwndComboBox,0,pszKeyword);
		int MaxHistory=m_Options.GetMaxKeywordHistory();
		if (ComboBox_GetCount(hwndComboBox)>MaxHistory)
			ComboBox_DeleteString(hwndComboBox,MaxHistory);
	} else if (i!=0) {
		ComboBox_DeleteString(hwndComboBox,i);
		ComboBox_InsertString(hwndComboBox,0,pszKeyword);
	}
	::SetWindowText(hwndComboBox,pszKeyword);

	m_Options.AddKeywordHistory(pszKeyword);

	return true;
}


void CEventSearchSettingsDialog::ShowButton(int ID,bool fShow)
{
	if (m_hDlg!=NULL)
		ShowDlgItem(m_hDlg,ID,fShow);
}


void CEventSearchSettingsDialog::CheckButton(int ID,bool fCheck)
{
	if (m_hDlg!=NULL)
		::CheckDlgButton(m_hDlg,ID,fCheck);
}


void CEventSearchSettingsDialog::SetFocus(int ID)
{
	if (m_hDlg!=NULL)
		::SetFocus(::GetDlgItem(m_hDlg,ID));
}


void CEventSearchSettingsDialog::SetSearchTargetList(const LPCTSTR *ppszList,int Count)
{
	if (ppszList!=NULL && Count>0) {
		m_SearchTargetList.resize(Count);
		for (int i=0;i<Count;i++)
			m_SearchTargetList[i]=ppszList[i];
	} else {
		m_SearchTargetList.clear();
	}
}


bool CEventSearchSettingsDialog::SetSearchTarget(int Target)
{
	if (Target<0 || (size_t)Target>=m_SearchTargetList.size())
		return false;
	m_SearchTarget=Target;
	return true;
}


INT_PTR CEventSearchSettingsDialog::DlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		AddControl(IDC_EVENTSEARCH_KEYWORD,ALIGN_HORZ);
		AddControl(IDC_EVENTSEARCH_KEYWORDMENU,ALIGN_RIGHT);
		AddControl(IDC_EVENTSEARCH_SETTINGSLIST,ALIGN_HORZ);
		AddControl(IDC_EVENTSEARCH_SETTINGSLIST_SAVE,ALIGN_RIGHT);
		AddControl(IDC_EVENTSEARCH_SETTINGSLIST_DELETE,ALIGN_RIGHT);
		AddControl(IDC_EVENTSEARCH_SEARCHTARGET,ALIGN_RIGHT);
		AddControl(IDC_EVENTSEARCH_SEARCH,ALIGN_RIGHT);

		// キーワード
		{
			DlgComboBox_LimitText(hDlg,IDC_EVENTSEARCH_KEYWORD,CEventSearchSettings::MAX_KEYWORD_LENGTH-1);

			LPCTSTR pszKeyword;
			for (int i=0;(pszKeyword=m_Options.GetKeywordHistory(i))!=NULL;i++)
				DlgComboBox_AddString(hDlg,IDC_EVENTSEARCH_KEYWORD,pszKeyword);

			COMBOBOXINFO cbi;
			cbi.cbSize=sizeof(cbi);
			::GetComboBoxInfo(::GetDlgItem(hDlg,IDC_EVENTSEARCH_KEYWORD),&cbi);
			m_KeywordEditSubclass.SetSubclass(cbi.hwndItem);

			InitDropDownButton(hDlg,IDC_EVENTSEARCH_KEYWORDMENU);
		}

		// キーワード検索対象
		{
			static const LPCTSTR KeywordTargetList[] = {
				TEXT("番組名と番組情報"),
				TEXT("番組名"),
				TEXT("番組情報"),
			};

			for (int i=0;i<lengthof(KeywordTargetList);i++)
				DlgComboBox_AddString(hDlg,IDC_EVENTSEARCH_KEYWORDTARGET,KeywordTargetList[i]);
		}

		// ジャンル
		{
			HWND hwndGenre=GetDlgItem(hDlg,IDC_EVENTSEARCH_GENRE);

			// 最初からTVS_CHECKBOXESを付けるとデフォルトでチェックできない
			::SetWindowLong(hwndGenre,GWL_STYLE,
				::GetWindowLong(hwndGenre,GWL_STYLE) | TVS_CHECKBOXES);

			TVINSERTSTRUCT tvis;
			CEpgGenre EpgGenre;
			TCHAR szText[256];
			tvis.hInsertAfter=TVI_LAST;
			tvis.item.mask=TVIF_STATE | TVIF_TEXT | TVIF_PARAM;
			tvis.item.stateMask=~0U;
			for (int i=0;i<CEpgGenre::NUM_GENRE;i++) {
				LPCTSTR pszText=EpgGenre.GetText(i,-1);
				if (pszText!=NULL) {
					::lstrcpyn(szText,pszText,lengthof(szText));
					tvis.hParent=TVI_ROOT;
					tvis.item.state=INDEXTOSTATEIMAGEMASK(1);
					if (m_fGenreExpanded[i])
						tvis.item.state|=TVIS_EXPANDED;
					tvis.item.pszText=szText;
					tvis.item.lParam=GENRE_LPARAM_PACK(i,-1);
					tvis.hParent=TreeView_InsertItem(hwndGenre,&tvis);
					for (int j=0;j<CEpgGenre::NUM_SUB_GENRE;j++) {
						pszText=EpgGenre.GetText(i,j);
						if (pszText!=NULL) {
							::lstrcpyn(szText,pszText,lengthof(szText));
							tvis.item.state=INDEXTOSTATEIMAGEMASK(1);
							tvis.item.lParam=GENRE_LPARAM_PACK(i,j);
							TreeView_InsertItem(hwndGenre,&tvis);
						}
					}
				}
			}
		}

		// 長さ
		DlgUpDown_SetRange(hDlg,IDC_EVENTSEARCH_DURATION_SHORT_SPIN,1,999);

		// CA
		{
			static const LPCTSTR pszCAList[] = {
				TEXT("無料"),
				TEXT("有料"),
			};

			for (int i=0;i<lengthof(pszCAList);i++)
				DlgComboBox_AddString(hDlg,IDC_EVENTSEARCH_CA_LIST,pszCAList[i]);
		}

		// 映像
		{
			static const LPCTSTR pszVideoList[] = {
				TEXT("HD"),
				TEXT("SD"),
			};

			for (int i=0;i<lengthof(pszVideoList);i++)
				DlgComboBox_AddString(hDlg,IDC_EVENTSEARCH_VIDEO_LIST,pszVideoList[i]);
		}

		// 設定保存
		{
			DlgComboBox_LimitText(hDlg,IDC_EVENTSEARCH_SETTINGSLIST,CEventSearchSettings::MAX_NAME_LENGTH-1);

			const size_t Count=m_Options.GetSearchSettingsCount();
			for (size_t i=0;i<Count;i++) {
				const CEventSearchSettings *pSettings=m_Options.GetSearchSettings(i);
				DlgComboBox_AddString(
					hDlg,IDC_EVENTSEARCH_SETTINGSLIST,
					pSettings->Name.c_str());
			}

			DlgComboBox_SetCueBanner(hDlg,IDC_EVENTSEARCH_SETTINGSLIST,TEXT("設定名"));
		}

		// 検索対象
		if (!m_SearchTargetList.empty()) {
			for (auto it=m_SearchTargetList.begin();it!=m_SearchTargetList.end();++it)
				DlgComboBox_AddString(hDlg,IDC_EVENTSEARCH_SEARCHTARGET,it->c_str());
			DlgComboBox_SetCurSel(hDlg,IDC_EVENTSEARCH_SEARCHTARGET,m_SearchTarget);
		} else {
			ShowDlgItem(hDlg,IDC_EVENTSEARCH_SEARCHTARGET,false);
		}

		SetSettings(m_SearchSettings);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_EVENTSEARCH_SEARCH:
			if (m_pEventHandler!=NULL) {
				m_pEventHandler->OnSearch();
			}
			return TRUE;

		case IDC_EVENTSEARCH_GENRE_CHECKALL:
		case IDC_EVENTSEARCH_GENRE_UNCHECKALL:
			{
				BOOL fCheck=LOWORD(wParam)==IDC_EVENTSEARCH_GENRE_CHECKALL;
				HWND hwndGenre=::GetDlgItem(hDlg,IDC_EVENTSEARCH_GENRE);
				HTREEITEM hItem=TreeView_GetChild(hwndGenre,TVI_ROOT);
				while (hItem!=NULL) {
					TreeView_SetCheckState(hwndGenre,hItem,fCheck);
					HTREEITEM hChild=TreeView_GetChild(hwndGenre,hItem);
					while (hChild!=NULL) {
						TreeView_SetCheckState(hwndGenre,hChild,fCheck);
						hChild=TreeView_GetNextSibling(hwndGenre,hChild);
					}
					hItem=TreeView_GetNextSibling(hwndGenre,hItem);
				}
				SetGenreStatus();
			}
			return TRUE;

		case IDC_EVENTSEARCH_DAYOFWEEK:
			EnableDlgItemsSyncCheckBox(hDlg,
				IDC_EVENTSEARCH_DAYOFWEEK_SUNDAY,
				IDC_EVENTSEARCH_DAYOFWEEK_SATURDAY,
				IDC_EVENTSEARCH_DAYOFWEEK);
			return TRUE;

		case IDC_EVENTSEARCH_TIME:
			EnableDlgItemsSyncCheckBox(hDlg,
				IDC_EVENTSEARCH_TIME_START_HOUR,
				IDC_EVENTSEARCH_TIME_END_MINUTE,
				IDC_EVENTSEARCH_TIME);

		case IDC_EVENTSEARCH_DURATION:
			EnableDlgItemsSyncCheckBox(hDlg,
				IDC_EVENTSEARCH_DURATION_SHORT_INPUT,
				IDC_EVENTSEARCH_DURATION_SHORT_UNIT,
				IDC_EVENTSEARCH_DURATION);
			return TRUE;

		case IDC_EVENTSEARCH_CA:
			EnableDlgItemSyncCheckBox(hDlg,
				IDC_EVENTSEARCH_CA_LIST,
				IDC_EVENTSEARCH_CA);
			return TRUE;

		case IDC_EVENTSEARCH_VIDEO:
			EnableDlgItemSyncCheckBox(hDlg,
				IDC_EVENTSEARCH_VIDEO_LIST,
				IDC_EVENTSEARCH_VIDEO);
			return TRUE;

		case IDC_EVENTSEARCH_HIGHLIGHT:
			if (m_pEventHandler!=NULL) {
				m_pEventHandler->OnHighlightResult(
					DlgCheckBox_IsChecked(hDlg,IDC_EVENTSEARCH_HIGHLIGHT));
			}
			return TRUE;

		case IDC_EVENTSEARCH_SETTINGSLIST:
			if (HIWORD(wParam)==CBN_SELCHANGE) {
				LRESULT Sel=DlgComboBox_GetCurSel(hDlg,IDC_EVENTSEARCH_SETTINGSLIST);

				if (Sel>=0) {
					TCHAR szName[CEventSearchSettings::MAX_NAME_LENGTH];

					if (DlgComboBox_GetLBString(hDlg,IDC_EVENTSEARCH_SETTINGSLIST,Sel,szName)>0) {
						const CEventSearchSettings *pSettings=m_Options.GetSearchSettingsByName(szName);

						if (pSettings!=NULL) {
							SetSettings(*pSettings);
						}
					}
				}
			}
			return TRUE;

		case IDC_EVENTSEARCH_SETTINGSLIST_SAVE:
			{
				HWND hwndComboBox=::GetDlgItem(hDlg,IDC_EVENTSEARCH_SETTINGSLIST);
				TCHAR szName[CEventSearchSettings::MAX_NAME_LENGTH];

				if (::GetWindowText(hwndComboBox,szName,lengthof(szName))<1) {
					::MessageBox(hDlg,TEXT("名前を入力してください。"),TEXT("設定の保存"),MB_OK | MB_ICONEXCLAMATION);
					::SetFocus(hwndComboBox);
					return TRUE;
				}

				int Index=ComboBox_FindStringExact(hwndComboBox,-1,szName);
				CEventSearchSettings *pSettings=m_Options.GetSearchSettingsByName(szName);
				if (pSettings!=NULL) {
					GetSettings(pSettings);
				} else {
					CEventSearchSettings Settings;

					GetSettings(&Settings);
					Settings.Name=szName;
					m_Options.AddSearchSettings(Settings);
				}
				if (Index==CB_ERR) {
					ComboBox_AddString(hwndComboBox,szName);
				}

				TCHAR szText[lengthof(szName)+64];
				StdUtil::snprintf(szText,lengthof(szText),TEXT("設定 \"%s\" を%sしました。"),
								  szName,pSettings==NULL?TEXT("保存"):TEXT("上書き"));
				::MessageBox(hDlg,szText,TEXT("設定の保存"),MB_OK | MB_ICONINFORMATION);
			}
			return TRUE;

		case IDC_EVENTSEARCH_SETTINGSLIST_DELETE:
			{
				HWND hwndComboBox=::GetDlgItem(hDlg,IDC_EVENTSEARCH_SETTINGSLIST);
				TCHAR szName[CEventSearchSettings::MAX_NAME_LENGTH];

				if (::GetWindowText(hwndComboBox,szName,lengthof(szName))>0) {
					int Index=ComboBox_FindStringExact(hwndComboBox,-1,szName);

					if (Index!=CB_ERR) {
						int i=m_Options.FindSearchSettings(szName);
						if (i>=0)
							m_Options.DeleteSearchSettings(i);

						ComboBox_DeleteString(hwndComboBox,Index);

						TCHAR szText[lengthof(szName)+64];
						StdUtil::snprintf(szText,lengthof(szText),TEXT("設定 \"%s\" を削除しました。"),szName);
						::MessageBox(hDlg,szText,TEXT("設定の削除"),MB_OK | MB_ICONINFORMATION);
					}
				}
			}
			return TRUE;

		case IDC_EVENTSEARCH_SEARCHTARGET:
			if (HIWORD(wParam)==CBN_SELCHANGE)
				m_SearchTarget=(int)DlgComboBox_GetCurSel(hDlg,IDC_EVENTSEARCH_SEARCHTARGET);
			return TRUE;

		case IDC_EVENTSEARCH_KEYWORDMENU:
			{
				CPopupMenu Menu(GetAppClass().GetResourceInstance(),
								MAKEINTRESOURCE(IDM_EVENTSEARCHKEYWORD));
				TCHAR szKeyword[CEventSearchSettings::MAX_KEYWORD_LENGTH];
				RECT rc;
				POINT pt;

				::GetDlgItemText(hDlg,IDC_EVENTSEARCH_KEYWORD,szKeyword,lengthof(szKeyword));
				Menu.EnableItem(IDC_EVENTSEARCH_DELETEKEYWORD,
					szKeyword[0]!=_T('\0') &&
						DlgComboBox_FindStringExact(hDlg,IDC_EVENTSEARCH_KEYWORD,-1,szKeyword)>=0);
				::GetWindowRect(::GetDlgItem(hDlg,IDC_EVENTSEARCH_KEYWORDMENU),&rc);
				pt.x=rc.right;
				pt.y=rc.bottom;
				Menu.Show(hDlg,&pt,TPM_RIGHTALIGN);
			}
			return TRUE;

		case IDC_EVENTSEARCH_DELETEKEYWORD:
			{
				int Index=-1;

				if (DlgComboBox_GetDroppedState(hDlg,IDC_EVENTSEARCH_KEYWORD)) {
					Index=(int)DlgComboBox_GetCurSel(hDlg,IDC_EVENTSEARCH_KEYWORD);
				} else {
					TCHAR szKeyword[CEventSearchSettings::MAX_KEYWORD_LENGTH];

					if (::GetDlgItemText(hDlg,IDC_EVENTSEARCH_KEYWORD,szKeyword,lengthof(szKeyword))>0) {
						Index=(int)DlgComboBox_FindStringExact(hDlg,IDC_EVENTSEARCH_KEYWORD,-1,szKeyword);
						if (Index>=0)
							::SetDlgItemText(hDlg,IDC_EVENTSEARCH_KEYWORD,TEXT(""));
					}
				}
				if (Index>=0) {
					DlgComboBox_DeleteItem(hDlg,IDC_EVENTSEARCH_KEYWORD,Index);
					m_Options.DeleteKeywordHistory(Index);
				}
			}
			return TRUE;

		case IDC_EVENTSEARCH_CLEARKEYWORDHISTORY:
			DlgComboBox_Clear(hDlg,IDC_EVENTSEARCH_KEYWORD);
			m_Options.ClearKeywordHistory();
			return TRUE;
		}
		return TRUE;

	case WM_CTLCOLORSTATIC:
		if (reinterpret_cast<HWND>(lParam)==::GetDlgItem(hDlg,IDC_EVENTSEARCH_GENRE_STATUS)) {
			HDC hdc=reinterpret_cast<HDC>(wParam);

			::SetTextColor(hdc,::GetSysColor(COLOR_GRAYTEXT));
			::SetBkColor(hdc,::GetSysColor(COLOR_3DFACE));
			return reinterpret_cast<INT_PTR>(::GetSysColorBrush(COLOR_3DFACE));
		}
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case NM_CLICK:
			{
				LPNMHDR pnmh=reinterpret_cast<LPNMHDR>(lParam);

				if (pnmh->idFrom==IDC_EVENTSEARCH_GENRE) {
					HWND hwndTree=pnmh->hwndFrom;
					TVHITTESTINFO tvhti;
					DWORD Pos=::GetMessagePos();
					tvhti.pt.x=GET_X_LPARAM(Pos);
					tvhti.pt.y=GET_Y_LPARAM(Pos);
					::ScreenToClient(hwndTree,&tvhti.pt);
					if (TreeView_HitTest(hwndTree,&tvhti)!=NULL
							&& (tvhti.flags&TVHT_ONITEMSTATEICON)!=0) {
						TreeView_SelectItem(hwndTree,tvhti.hItem);
						::PostMessage(hDlg,WM_PROGRAM_SEARCH_GENRE_CHANGED,0,0);
					}
					return TRUE;
				}
			}
			break;

		case TVN_KEYDOWN:
			{
				LPNMTVKEYDOWN pnmtvkd=reinterpret_cast<LPNMTVKEYDOWN>(lParam);

				if (pnmtvkd->wVKey==VK_SPACE)
					::PostMessage(hDlg,WM_PROGRAM_SEARCH_GENRE_CHANGED,0,0);
			}
			return TRUE;
		}
		break;

	case WM_PROGRAM_SEARCH_GENRE_CHANGED:
		SetGenreStatus();
		return TRUE;

	case WM_DESTROY:
		// ジャンルのツリー展開状態を保存
		{
			HWND hwndGenre=::GetDlgItem(hDlg,IDC_EVENTSEARCH_GENRE);
			HTREEITEM hItem=TreeView_GetChild(hwndGenre,TVI_ROOT);
			TVITEM tvi;
			tvi.mask=TVIF_STATE | TVIF_PARAM;
			tvi.stateMask=~0U;
			for (int i=0;hItem!=NULL;i++) {
				tvi.hItem=hItem;
				TreeView_GetItem(hwndGenre,&tvi);
				m_fGenreExpanded[GENRE_LPARAM_LEVEL1(tvi.lParam)]=(tvi.state&TVIS_EXPANDED)!=0;
				hItem=TreeView_GetNextSibling(hwndGenre,hItem);
			}
		}

		return TRUE;
	}

	return FALSE;
}


void CEventSearchSettingsDialog::GetGenreSettings(CEventSearchSettings *pSettings) const
{
	HWND hwndGenre=::GetDlgItem(m_hDlg,IDC_EVENTSEARCH_GENRE);
	HTREEITEM hItem=TreeView_GetChild(hwndGenre,TVI_ROOT);
	TVITEM tvi;
	tvi.mask=TVIF_PARAM | TVIF_STATE;
	tvi.stateMask=TVIS_STATEIMAGEMASK;
	while (hItem!=NULL) {
		tvi.hItem=hItem;
		TreeView_GetItem(hwndGenre,&tvi);
		int Level1=GENRE_LPARAM_LEVEL1(tvi.lParam);
		if ((tvi.state&TVIS_STATEIMAGEMASK)>>12>1) {
			pSettings->Genre1|=1<<Level1;
			pSettings->fGenre=true;
		}
		HTREEITEM hChild=TreeView_GetChild(hwndGenre,hItem);
		while (hChild!=NULL) {
			tvi.hItem=hChild;
			TreeView_GetItem(hwndGenre,&tvi);
			if ((tvi.state&TVIS_STATEIMAGEMASK)>>12>1) {
				int Level2=GENRE_LPARAM_LEVEL2(tvi.lParam);
				pSettings->Genre2[Level1]|=1<<Level2;
				pSettings->fGenre=true;
			}
			hChild=TreeView_GetNextSibling(hwndGenre,hChild);
		}
		hItem=TreeView_GetNextSibling(hwndGenre,hItem);
	}
}


void CEventSearchSettingsDialog::SetGenreStatus()
{
	HWND hwndGenre=::GetDlgItem(m_hDlg,IDC_EVENTSEARCH_GENRE);
	HTREEITEM hItem=TreeView_GetChild(hwndGenre,TVI_ROOT);
	int CheckCount=0;
	TVITEM tvi;
	tvi.mask=TVIF_PARAM | TVIF_STATE;
	tvi.stateMask=TVIS_STATEIMAGEMASK;
	while (hItem!=NULL) {
		tvi.hItem=hItem;
		TreeView_GetItem(hwndGenre,&tvi);
		if ((tvi.state&TVIS_STATEIMAGEMASK)>>12>1)
			CheckCount++;
		HTREEITEM hChild=TreeView_GetChild(hwndGenre,hItem);
		while (hChild!=NULL) {
			tvi.hItem=hChild;
			TreeView_GetItem(hwndGenre,&tvi);
			if ((tvi.state&TVIS_STATEIMAGEMASK)>>12>1)
				CheckCount++;
			hChild=TreeView_GetNextSibling(hwndGenre,hChild);
		}
		hItem=TreeView_GetNextSibling(hwndGenre,hItem);
	}

	TCHAR szText[256];
	if (CheckCount>0)
		StdUtil::snprintf(szText,lengthof(szText),TEXT("%d 個選択"),CheckCount);
	else
		::lstrcpy(szText,TEXT("指定なし"));
	::SetDlgItemText(m_hDlg,IDC_EVENTSEARCH_GENRE_STATUS,szText);
}


LRESULT CEventSearchSettingsDialog::CKeywordEditSubclass::OnMessage(
	HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg) {
	case WM_GETDLGCODE:
		if (wParam==VK_RETURN)
			return DLGC_WANTALLKEYS;
		break;

	case WM_KEYDOWN:
		if (wParam==VK_RETURN) {
			::SendMessage(::GetParent(::GetParent(hwnd)),WM_COMMAND,IDC_EVENTSEARCH_SEARCH,0);
			return 0;
		}
		if (wParam==VK_DELETE) {
			int Length=::GetWindowTextLength(hwnd);

			if (Length>0) {
				HWND hwndComboBox=::GetParent(hwnd);

				if (ComboBox_GetDroppedState(hwndComboBox)) {
					int Sel=ComboBox_GetCurSel(hwndComboBox);

					if (Sel>=0) {
						DWORD Start,End;

						::SendMessage(hwnd,EM_GETSEL,
									  reinterpret_cast<WPARAM>(&Start),
									  reinterpret_cast<LPARAM>(&End));
						if (Start==0 && End==(DWORD)Length) {
							TCHAR szKeyword[CEventSearchSettings::MAX_KEYWORD_LENGTH];

							if (::GetWindowText(hwnd,szKeyword,lengthof(szKeyword))==Length
									&& ComboBox_FindStringExact(hwndComboBox,Sel-1,szKeyword)==Sel) {
								::SendMessage(::GetParent(hwndComboBox),WM_COMMAND,
											  IDC_EVENTSEARCH_DELETEKEYWORD,0);
								return 0;
							}
						}
					}
				}
			}
			break;
		}
		break;

	case WM_CHAR:
		if (wParam=='\r' || wParam=='\n')
			return 0;
		break;
	}

	return CWindowSubclass::OnMessage(hwnd,uMsg,wParam,lParam);
}




CSearchEventInfo::CSearchEventInfo(
	const CEventInfoData &EventInfo,const CTunerChannelInfo &ChannelInfo)
	: CEventInfoData(EventInfo)
	, m_ChannelInfo(ChannelInfo)
{
}




CProgramSearchDialog::CProgramSearchDialog(CEventSearchOptions &Options)
	: m_pEventHandler(NULL)
	, m_Options(Options)
	, m_SearchSettingsDialog(Options)
	, m_fHighlightResult(true)
	, m_ResultListHeight(-1)
{
	for (int i=0;i<NUM_COLUMNS;i++)
		m_ColumnWidth[i]=-1;

	RegisterUIChild(&m_SearchSettingsDialog);
	SetStyleScaling(&m_StyleScaling);
}


CProgramSearchDialog::~CProgramSearchDialog()
{
	if (m_pEventHandler!=NULL)
		m_pEventHandler->m_pSearchDialog=NULL;
}


bool CProgramSearchDialog::Create(HWND hwndOwner)
{
	m_RichEditUtil.LoadRichEditLib();
	return CreateDialogWindow(hwndOwner,GetAppClass().GetResourceInstance(),MAKEINTRESOURCE(IDD_PROGRAMSEARCH));
}


bool CProgramSearchDialog::SetEventHandler(CEventHandler *pHandler)
{
	if (m_pEventHandler!=NULL)
		m_pEventHandler->m_pSearchDialog=NULL;
	if (pHandler!=NULL)
		pHandler->m_pSearchDialog=this;
	m_pEventHandler=pHandler;
	return true;
}


int CProgramSearchDialog::GetColumnWidth(int Index) const
{
	if (Index<0 || Index>=NUM_COLUMNS)
		return 0;
	return m_ColumnWidth[Index];
}


bool CProgramSearchDialog::SetColumnWidth(int Index,int Width)
{
	if (Index<0 || Index>=NUM_COLUMNS)
		return false;
	m_ColumnWidth[Index]=max(Width,0);
	return true;
}


bool CProgramSearchDialog::Search(LPTSTR pszKeyword)
{
	if (m_hDlg==NULL || pszKeyword==NULL)
		return false;
	m_SearchSettingsDialog.SetKeyword(pszKeyword);
	m_SearchSettingsDialog.BeginSearch();
	return true;
}


bool CProgramSearchDialog::SetHighlightResult(bool fHighlight)
{
	if (m_fHighlightResult!=fHighlight) {
		m_fHighlightResult=fHighlight;
		if (m_pEventHandler!=NULL && !m_ResultMap.empty())
			m_pEventHandler->OnHighlightChange(fHighlight);
	}
	return true;
}


bool CProgramSearchDialog::IsHitEvent(const CEventInfoData *pEventInfo) const
{
	if (pEventInfo==NULL)
		return false;
	return m_ResultMap.find(GetResultMapKey(pEventInfo))!=m_ResultMap.end();
}


void CProgramSearchDialog::SetResultListHeight(int Height)
{
	m_ResultListHeight=Height;
}


void CProgramSearchDialog::SetSearchTargetList(const LPCTSTR *ppszList,int Count)
{
	m_SearchSettingsDialog.SetSearchTargetList(ppszList,Count);
}


bool CProgramSearchDialog::SetSearchTarget(int Target)
{
	return m_SearchSettingsDialog.SetSearchTarget(Target);
}


int CProgramSearchDialog::GetSearchTarget() const
{
	return m_SearchSettingsDialog.GetSearchTarget();
}


INT_PTR CProgramSearchDialog::DlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		AddControl(IDC_PROGRAMSEARCH_SETTINGSPLACE,ALIGN_HORZ);
		AddControl(IDC_PROGRAMSEARCH_STATUS,ALIGN_HORZ);
		AddControl(IDC_PROGRAMSEARCH_RESULTPANE,ALIGN_ALL);

		m_SearchSettingsDialog.SetEventHandler(this);
		m_SearchSettings.Keyword.clear();
		m_SearchSettingsDialog.SetSettings(m_SearchSettings);
		m_SearchSettingsDialog.Create(hDlg);
		m_SearchSettingsDialog.CheckButton(IDC_EVENTSEARCH_HIGHLIGHT,m_fHighlightResult);

		// 検索結果一覧
		{
			HWND hwndList=::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_RESULT);
			ListView_SetExtendedListViewStyle(hwndList,LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

			int FontSize;
			for (int i=0;i<NUM_COLUMNS;i++) {
				if (m_ColumnWidth[i]<0) {
					HDC hdc=::GetDC(hwndList);
					HFONT hfont=GetWindowFont(hwndList);
					HFONT hfontOld=SelectFont(hdc,hfont);
					TEXTMETRIC tm;
					::GetTextMetrics(hdc,&tm);
					FontSize=tm.tmHeight-tm.tmInternalLeading;
					SelectFont(hdc,hfontOld);
					::ReleaseDC(hwndList,hdc);
					break;
				}
			}

			LVCOLUMN lvc;
			lvc.mask=LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
			lvc.fmt=LVCFMT_LEFT;
			lvc.cx=m_ColumnWidth[COLUMN_CHANNEL]>=0?m_ColumnWidth[COLUMN_CHANNEL]:8*FontSize;
			lvc.pszText=TEXT("チャンネル");
			ListView_InsertColumn(hwndList,COLUMN_CHANNEL,&lvc);
			lvc.cx=m_ColumnWidth[COLUMN_TIME]>=0?m_ColumnWidth[COLUMN_TIME]:12*FontSize;
			lvc.pszText=TEXT("日時");
			ListView_InsertColumn(hwndList,COLUMN_TIME,&lvc);
			lvc.cx=m_ColumnWidth[COLUMN_EVENTNAME]>=0?m_ColumnWidth[COLUMN_EVENTNAME]:20*FontSize;
			lvc.pszText=TEXT("番組名");
			ListView_InsertColumn(hwndList,COLUMN_EVENTNAME,&lvc);

			m_SortColumn=-1;
			m_fSortDescending=false;
		}

		// 番組情報
		{
			LOGFONT lf;
			HDC hdc;

			::GetObject(GetWindowFont(hDlg),sizeof(LOGFONT),&lf);
			hdc=::GetDC(hDlg);
			CRichEditUtil::LogFontToCharFormat(hdc,&lf,&m_InfoTextFormat);
			::ReleaseDC(hDlg,hdc);
			::SendDlgItemMessage(hDlg,IDC_PROGRAMSEARCH_INFO,EM_SETEVENTMASK,0,ENM_MOUSEEVENTS | ENM_LINK);
		}

		TVTest::SetWindowIcon(hDlg,GetAppClass().GetResourceInstance(),MAKEINTRESOURCE(IDI_SEARCH));

		ApplyPosition();

		if (m_ResultListHeight>=0)
			AdjustResultListHeight(m_ResultListHeight);

		m_fSplitterCursor=false;

		m_SearchSettingsDialog.SetFocus(IDC_EVENTSEARCH_KEYWORD);

		return FALSE;

	case WM_SIZE:
		{
			RECT rc;
			GetDlgItemRect(hDlg,IDC_PROGRAMSEARCH_SETTINGSPLACE,&rc);
			m_SearchSettingsDialog.SetPosition(&rc);

			InvalidateDlgItem(hDlg,IDC_PROGRAMSEARCH_STATUS);

			AdjustResultListHeight(-1);
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			OnSearch();
			return TRUE;

		case IDCANCEL:
			if (m_pEventHandler==NULL || m_pEventHandler->OnClose())
				::DestroyWindow(hDlg);
			return TRUE;
		}
		return TRUE;

	case WM_CTLCOLORSTATIC:
		if (reinterpret_cast<HWND>(lParam)==::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_STATUS)) {
			HDC hdc=reinterpret_cast<HDC>(wParam);

			::SetTextColor(hdc,::GetSysColor(COLOR_WINDOWTEXT));
			::SetBkColor(hdc,::GetSysColor(COLOR_WINDOW));
			return reinterpret_cast<INT_PTR>(::GetSysColorBrush(COLOR_WINDOW));
		}
		break;

	case WM_SETCURSOR:
		if ((HWND)wParam==hDlg && LOWORD(lParam)==HTCLIENT && m_fSplitterCursor) {
			::SetCursor(::LoadCursor(NULL,IDC_SIZENS));
			::SetWindowLongPtr(hDlg,DWLP_MSGRESULT,TRUE);
			return TRUE;
		}
		break;

	case WM_LBUTTONDOWN:
		{
			int x=GET_X_LPARAM(lParam),y=GET_Y_LPARAM(lParam);

			if (IsSplitterPos(x,y)) {
				RECT rc;

				::GetWindowRect(::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_RESULT),&rc);
				m_ResultListHeight=rc.bottom-rc.top;
				m_fSplitterCursor=true;
				m_SplitterDragPos=y;
				::SetCursor(::LoadCursor(NULL,IDC_SIZENS));
				::SetCapture(hDlg);
			}
		}
		return TRUE;

	case WM_LBUTTONUP:
		if (::GetCapture()==hDlg)
			::ReleaseCapture();
		return TRUE;

	case WM_MOUSEMOVE:
		{
			int x=GET_X_LPARAM(lParam),y=GET_Y_LPARAM(lParam);

			if (::GetCapture()==hDlg)
				AdjustResultListHeight(m_ResultListHeight+(y-m_SplitterDragPos));
			else
				m_fSplitterCursor=IsSplitterPos(x,y);
		}
		return TRUE;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case NM_RCLICK:
		case NM_DBLCLK:
			{
				LPNMITEMACTIVATE pnmia=reinterpret_cast<LPNMITEMACTIVATE>(lParam);

				if (pnmia->hdr.idFrom==IDC_PROGRAMSEARCH_RESULT
						&& m_pEventHandler!=NULL
						&& pnmia->iItem>=0) {
					LVITEM lvi;

					lvi.mask=LVIF_PARAM;
					lvi.iItem=pnmia->iItem;
					lvi.iSubItem=0;
					if (ListView_GetItem(pnmia->hdr.hwndFrom,&lvi)) {
						const CSearchEventInfo *pEventInfo=
							reinterpret_cast<const CSearchEventInfo*>(lvi.lParam);
						if (pnmia->hdr.code==NM_DBLCLK)
							m_pEventHandler->OnLDoubleClick(pEventInfo);
						else
							m_pEventHandler->OnRButtonClick(pEventInfo);
						return TRUE;
					}
				}
			}
			break;

		case LVN_COLUMNCLICK:
			{
				LPNMLISTVIEW pnmlv=reinterpret_cast<LPNMLISTVIEW>(lParam);

				if (pnmlv->iSubItem==m_SortColumn) {
					m_fSortDescending=!m_fSortDescending;
				} else {
					m_SortColumn=pnmlv->iSubItem;
					m_fSortDescending=false;
				}
				SortSearchResult();
			}
			return TRUE;

		case LVN_ITEMCHANGED:
			{
				LPNMLISTVIEW pnmlv=reinterpret_cast<LPNMLISTVIEW>(lParam);
				int Sel=ListView_GetNextItem(pnmlv->hdr.hwndFrom,-1,LVNI_SELECTED);
				HWND hwndInfo=::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_INFO);

				::SetWindowText(hwndInfo,TEXT(""));
				if (Sel>=0) {
					LVITEM lvi;
					TCHAR szText[256];
					TVTest::String Text;

					lvi.mask=LVIF_PARAM;
					lvi.iItem=Sel;
					lvi.iSubItem=0;
					ListView_GetItem(pnmlv->hdr.hwndFrom,&lvi);
					::SendMessage(hwndInfo,WM_SETREDRAW,FALSE,0);
					const CSearchEventInfo *pEventInfo=reinterpret_cast<const CSearchEventInfo*>(lvi.lParam);
					FormatEventTimeText(pEventInfo,szText,lengthof(szText));
					CRichEditUtil::AppendText(hwndInfo,szText,&m_InfoTextFormat);
					FormatEventInfoText(pEventInfo,&Text);
					CRichEditUtil::AppendText(hwndInfo,Text.c_str(),&m_InfoTextFormat);
					HighlightKeyword();
					CRichEditUtil::DetectURL(hwndInfo,&m_InfoTextFormat,1);
					POINT pt={0,0};
					::SendMessage(hwndInfo,EM_SETSCROLLPOS,0,reinterpret_cast<LPARAM>(&pt));
					::SendMessage(hwndInfo,WM_SETREDRAW,TRUE,0);
					::InvalidateRect(hwndInfo,NULL,TRUE);
				}
			}
			return TRUE;

		case LVN_GETEMPTYMARKUP:
			{
				NMLVEMPTYMARKUP *pnmMarkup=reinterpret_cast<NMLVEMPTYMARKUP*>(lParam);

				pnmMarkup->dwFlags=EMF_CENTERED;
				::lstrcpyW(pnmMarkup->szMarkup,L"検索された番組はありません");
				::SetWindowLongPtr(hDlg,DWLP_MSGRESULT,TRUE);
			}
			return TRUE;

		case EN_MSGFILTER:
			if (reinterpret_cast<MSGFILTER*>(lParam)->msg==WM_RBUTTONUP) {
				TVTest::EventInfoUtil::EventInfoContextMenu(hDlg,::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_INFO));
			}
			return TRUE;

		case EN_LINK:
			{
				ENLINK *penl=reinterpret_cast<ENLINK*>(lParam);

				if (penl->msg==WM_LBUTTONUP) {
					CRichEditUtil::HandleLinkClick(penl);
				}
			}
			return TRUE;
		}
		break;

	case WM_DESTROY:
		// 検索結果のカラムの幅を保存
		{
			HWND hwndList=::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_RESULT);
			for (int i=0;i<lengthof(m_ColumnWidth);i++)
				m_ColumnWidth[i]=ListView_GetColumnWidth(hwndList,i);
		}

		{
			RECT rc;
			::GetWindowRect(::GetDlgItem(hDlg,IDC_PROGRAMSEARCH_RESULT),&rc);
			m_ResultListHeight=rc.bottom-rc.top;
		}

		ClearSearchResult();

		m_Searcher.Finalize();

		return TRUE;
	}

	return FALSE;
}


void CProgramSearchDialog::ApplyStyle()
{
	CResizableDialog::ApplyStyle();

	if (m_hDlg!=NULL) {
		HWND hwnd=::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_INFO);
		HDC hdc=::GetDC(hwnd);
		LOGFONT lf;
		m_Font.GetLogFont(&lf);
		CRichEditUtil::LogFontToCharFormat(hdc,&lf,&m_InfoTextFormat);
		::ReleaseDC(hwnd,hdc);
	}
}


int CALLBACK CProgramSearchDialog::ResultCompareFunc(LPARAM lParam1,LPARAM lParam2,LPARAM lParamSort)
{
	const CSearchEventInfo *pInfo1=reinterpret_cast<const CSearchEventInfo*>(lParam1);
	const CSearchEventInfo *pInfo2=reinterpret_cast<const CSearchEventInfo*>(lParam2);
	int Cmp;

	switch (LOWORD(lParamSort)) {
	case COLUMN_CHANNEL:	// チャンネル
		Cmp=::lstrcmpi(pInfo1->GetChannelInfo().GetName(),pInfo2->GetChannelInfo().GetName());
		break;
	case COLUMN_TIME:		// 日時
		Cmp=CompareSystemTime(&pInfo1->m_StartTime,&pInfo2->m_StartTime);
		break;
	case COLUMN_EVENTNAME:	// 番組名
		if (pInfo1->m_EventName.empty()) {
			if (pInfo2->m_EventName.empty())
				Cmp=0;
			else
				Cmp=-1;
		} else {
			if (pInfo2->m_EventName.empty())
				Cmp=1;
			else
				Cmp=::lstrcmpi(pInfo1->m_EventName.c_str(),pInfo2->m_EventName.c_str());
		}
		break;
	}
	return HIWORD(lParamSort)==0?Cmp:-Cmp;
}


bool CProgramSearchDialog::AddSearchResult(CSearchEventInfo *pEventInfo)
{
	if (m_hDlg==NULL || pEventInfo==NULL)
		return false;

	HWND hwndList=::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_RESULT);
	LV_ITEM lvi;
	TCHAR szText[256];

	lvi.mask=LVIF_TEXT | LVIF_PARAM;
	lvi.iItem=ListView_GetItemCount(hwndList);
	lvi.iSubItem=COLUMN_CHANNEL;
	::lstrcpyn(szText,pEventInfo->GetChannelInfo().GetName(),lengthof(szText));
	lvi.pszText=szText;
	lvi.lParam=reinterpret_cast<LPARAM>(pEventInfo);
	ListView_InsertItem(hwndList,&lvi);
	SYSTEMTIME stStart,stEnd;
	EpgUtil::EpgTimeToDisplayTime(pEventInfo->m_StartTime,&stStart);
	pEventInfo->GetEndTime(&stEnd);
	EpgUtil::EpgTimeToDisplayTime(&stEnd);
	StdUtil::snprintf(szText,lengthof(szText),
					  TEXT("%02d/%02d(%s) %02d:%02d〜%02d:%02d"),
					  stStart.wMonth,stStart.wDay,
					  GetDayOfWeekText(stStart.wDayOfWeek),
					  stStart.wHour,stStart.wMinute,
					  stEnd.wHour,stEnd.wMinute);
	lvi.mask=LVIF_TEXT;
	lvi.iSubItem=COLUMN_TIME;
	//lvi.pszText=szText;
	ListView_SetItem(hwndList,&lvi);
	//lvi.mask=LVIF_TEXT;
	lvi.iSubItem=COLUMN_EVENTNAME;
	::lstrcpyn(szText,pEventInfo->m_EventName.c_str(),lengthof(szText));
	//lvi.pszText=szText;
	ListView_SetItem(hwndList,&lvi);

	m_ResultMap.insert(std::pair<ULONGLONG,CSearchEventInfo*>(GetResultMapKey(pEventInfo),pEventInfo));

	return true;
}


void CProgramSearchDialog::ClearSearchResult()
{
	m_ResultMap.clear();

	HWND hwndList=::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_RESULT);
	int Items=ListView_GetItemCount(hwndList);
	LVITEM lvi;

	lvi.mask=LVIF_PARAM;
	lvi.iSubItem=0;
	for (int i=0;i<Items;i++) {
		lvi.iItem=i;
		ListView_GetItem(hwndList,&lvi);
		delete reinterpret_cast<CSearchEventInfo*>(lvi.lParam);
	}
	ListView_DeleteAllItems(hwndList);

	::SetDlgItemText(m_hDlg,IDC_PROGRAMSEARCH_STATUS,TEXT(""));
	::SetDlgItemText(m_hDlg,IDC_PROGRAMSEARCH_INFO,TEXT(""));
}


void CProgramSearchDialog::SortSearchResult()
{
	HWND hwndList=::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_RESULT);

	ListView_SortItems(hwndList,ResultCompareFunc,
					   MAKELPARAM(m_SortColumn,m_fSortDescending));
	SetListViewSortMark(hwndList,m_SortColumn,!m_fSortDescending);
}


int CProgramSearchDialog::FormatEventTimeText(const CEventInfoData *pEventInfo,LPTSTR pszText,int MaxLength) const
{
	if (pEventInfo==NULL) {
		pszText[0]='\0';
		return 0;
	}

	TCHAR szEndTime[16];
	SYSTEMTIME stEnd;
	if (pEventInfo->m_Duration>0 && pEventInfo->GetEndTime(&stEnd))
		StdUtil::snprintf(szEndTime,lengthof(szEndTime),
						  TEXT("〜%d:%02d"),stEnd.wHour,stEnd.wMinute);
	else
		szEndTime[0]='\0';
	return StdUtil::snprintf(pszText,MaxLength,TEXT("%d/%d/%d(%s) %d:%02d%s\r\n"),
							 pEventInfo->m_StartTime.wYear,
							 pEventInfo->m_StartTime.wMonth,
							 pEventInfo->m_StartTime.wDay,
							 GetDayOfWeekText(pEventInfo->m_StartTime.wDayOfWeek),
							 pEventInfo->m_StartTime.wHour,
							 pEventInfo->m_StartTime.wMinute,
							 szEndTime);
}


void CProgramSearchDialog::FormatEventInfoText(const CEventInfoData *pEventInfo,TVTest::String *pText) const
{
	pText->clear();

	if (pEventInfo==NULL)
		return;

	*pText=pEventInfo->m_EventName;
	*pText+=TEXT("\r\n\r\n");
	if (!pEventInfo->m_EventText.empty()) {
		*pText+=pEventInfo->m_EventText;
		*pText+=TEXT("\r\n");
	}
	if (!pEventInfo->m_EventExtendedText.empty()) {
		if (!pEventInfo->m_EventText.empty())
			*pText+=TEXT("\r\n");
		*pText+=pEventInfo->m_EventExtendedText;
	}

	TVTest::String::size_type Pos=pText->find_last_not_of(TEXT("\r\n"));
	if (Pos!=TVTest::String::npos && pText->length()>Pos+2)
		pText->resize(Pos+2);
}


void CProgramSearchDialog::HighlightKeyword()
{
	if (m_SearchSettings.Keyword.empty()
			|| (!m_SearchSettings.fEventName && !m_SearchSettings.fEventText))
		return;

	const HWND hwndInfo=::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_INFO);

	int FirstLine,LastLine;
	if (m_SearchSettings.fEventName)
		FirstLine=1;
	else
		FirstLine=3;
	if (m_SearchSettings.fEventText)
		LastLine=(int)::SendMessage(hwndInfo,EM_GETLINECOUNT,0,0)-1;
	else
		LastLine=1;

	CHARFORMAT2 cfHighlight;
	CRichEditUtil::CharFormatToCharFormat2(&m_InfoTextFormat,&cfHighlight);
	cfHighlight.dwMask|=CFM_BOLD | CFM_BACKCOLOR;
	cfHighlight.dwEffects|=CFE_BOLD;
	cfHighlight.crBackColor=RGB(255,255,0);
	CHARRANGE cr,crOld;

	::SendMessage(hwndInfo,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&crOld));

	for (int i=FirstLine;i<=LastLine;) {
		const int LineIndex=(int)::SendMessage(hwndInfo,EM_LINEINDEX,i,0);
		TCHAR szText[2048],*q;
		int TotalLength=0,Length;

		q=szText;
		while (i<=LastLine) {
#ifdef UNICODE
			q[0]=(WORD)(lengthof(szText)-2-TotalLength);
#else
			*(WORD*)q=(WORD)(sizeof(szText)-sizeof(WORD)-1-TotalLength);
#endif
			Length=(int)::SendMessage(hwndInfo,EM_GETLINE,i,reinterpret_cast<LPARAM>(q));
			i++;
			if (Length<1)
				break;
			q+=Length;
			TotalLength+=Length;
			if (*(q-1)==_T('\r') || *(q-1)==_T('\n'))
				break;
		}
		if (TotalLength>0) {
			szText[TotalLength]=_T('\0');

			if (m_SearchSettings.fRegExp) {
				LPCTSTR q=szText;
				TVTest::CRegExp::TextRange Range;

				while (*q!=_T('\0') && m_Searcher.GetRegExp().Match(q,&Range)) {
					q+=Range.Start;
					cr.cpMin=LineIndex+(LONG)(q-szText);
					cr.cpMax=cr.cpMin+(LONG)Range.Length;
					::SendMessage(hwndInfo,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&cr));
					::SendMessage(hwndInfo,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&cfHighlight));
					q+=Range.Length;
				}
			} else {
				LPCTSTR p=m_SearchSettings.Keyword.c_str();
				while (*p!=_T('\0')) {
					TCHAR szWord[CEventSearchSettings::MAX_KEYWORD_LENGTH],Delimiter;
					bool fMinus=false;

					while (*p==_T(' '))
						p++;
					if (*p==_T('-')) {
						fMinus=true;
						p++;
					}
					if (*p==_T('"')) {
						p++;
						Delimiter=_T('"');
					} else {
						Delimiter=_T(' ');
					}
					int KeywordLength;
					for (KeywordLength=0;*p!=Delimiter && *p!=_T('|') && *p!=_T('\0');KeywordLength++)
						szWord[KeywordLength]=*p++;
					if (*p==Delimiter)
						p++;
					if (!fMinus && KeywordLength>0) {
						LPCTSTR q=szText;
						while (SearchNextKeyword(&q,szWord,KeywordLength,&Length)) {
							cr.cpMin=LineIndex+(LONG)(q-szText);
							cr.cpMax=cr.cpMin+Length;
							::SendMessage(hwndInfo,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&cr));
							::SendMessage(hwndInfo,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&cfHighlight));
							q+=Length;
						}
					}
					while (*p==_T(' '))
						p++;
					if (*p==_T('|'))
						p++;
				}
			}
		}
	}

	::SendMessage(hwndInfo,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&crOld));
}


bool CProgramSearchDialog::SearchNextKeyword(LPCTSTR *ppszText,LPCTSTR pKeyword,int KeywordLength,int *pLength) const
{
	int Pos=m_Searcher.FindKeyword(*ppszText,pKeyword,KeywordLength,pLength);
	if (Pos<0)
		return false;
	*ppszText+=Pos;
	return true;
}


bool CProgramSearchDialog::IsSplitterPos(int x,int y) const
{
	RECT rcList,rcInfo;

	GetDlgItemRect(m_hDlg,IDC_PROGRAMSEARCH_RESULT,&rcList);
	GetDlgItemRect(m_hDlg,IDC_PROGRAMSEARCH_INFO,&rcInfo);
	return x>=rcInfo.left && x<rcInfo.right && y>=rcList.bottom && y<rcInfo.top;
}


void CProgramSearchDialog::AdjustResultListHeight(int Height)
{
	RECT rcPane,rcList,rcInfo;

	GetDlgItemRect(m_hDlg,IDC_PROGRAMSEARCH_RESULTPANE,&rcPane);
	GetDlgItemRect(m_hDlg,IDC_PROGRAMSEARCH_RESULT,&rcList);
	GetDlgItemRect(m_hDlg,IDC_PROGRAMSEARCH_INFO,&rcInfo);
	const int SplitterHeight=rcInfo.top-rcList.bottom;
	if (Height<0)
		Height=(rcPane.bottom-rcPane.top)-(rcInfo.bottom-rcInfo.top)-SplitterHeight;
	if (rcPane.top+Height>rcPane.bottom-MIN_PANE_HEIGHT-SplitterHeight)
		Height=(rcPane.bottom-rcPane.top)-MIN_PANE_HEIGHT-SplitterHeight;
	if (Height<MIN_PANE_HEIGHT)
		Height=MIN_PANE_HEIGHT;
	::MoveWindow(::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_RESULT),
				 rcPane.left,rcPane.top,rcPane.right-rcPane.left,Height,TRUE);
	rcInfo.top=rcPane.top+Height+SplitterHeight;
	::MoveWindow(::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_INFO),
				 rcPane.left,rcInfo.top,rcPane.right-rcPane.left,max(rcPane.bottom-rcInfo.top,0),TRUE);
}


void CProgramSearchDialog::OnSearch()
{
	CEventSearchSettings Settings;

	if (!m_SearchSettingsDialog.GetSettings(&Settings))
		return;

	ClearSearchResult();

	if (!Settings.Keyword.empty()) {
		m_SearchSettingsDialog.AddToKeywordHistory(Settings.Keyword.c_str());

		if (Settings.fRegExp) {
			if (!m_Searcher.InitializeRegExp()) {
				::MessageBox(m_hDlg,TEXT("正規表現が利用できません。"),NULL,MB_OK | MB_ICONEXCLAMATION);
				return;
			}
		}
	}

	m_SearchSettings=Settings;
	if (!m_Searcher.BeginSearch(m_SearchSettings))
		return;

	HCURSOR hcurOld=::SetCursor(::LoadCursor(NULL,IDC_WAIT));
	DWORD StartTime=::GetTickCount();
	m_pEventHandler->Search(&m_Searcher);
	DWORD SearchTime=TickTimeSpan(StartTime,::GetTickCount());
	if (m_SortColumn>=0)
		SortSearchResult();
	::SetCursor(hcurOld);

	TCHAR szStatus[CEventSearchSettings::MAX_KEYWORD_LENGTH+64];
	CStaticStringFormatter StatusFormat(szStatus,lengthof(szStatus));
	if (!m_SearchSettings.Keyword.empty()) {
		if (m_SearchSettings.fGenre)
			StatusFormat.Append(TEXT("指定ジャンルから "));
		StatusFormat.AppendFormat(TEXT("%s に一致する番組"),m_SearchSettings.Keyword.c_str());
	} else {
		if (m_SearchSettings.fGenre)
			StatusFormat.Append(TEXT("指定ジャンルの番組"));
		else
			StatusFormat.Append(TEXT("すべての番組"));
	}
	StatusFormat.AppendFormat(
		TEXT(" %d 件 (%d.%02d 秒)"),
		ListView_GetItemCount(::GetDlgItem(m_hDlg,IDC_PROGRAMSEARCH_RESULT)),
		SearchTime/1000,SearchTime/10%100);
	::SetDlgItemText(m_hDlg,IDC_PROGRAMSEARCH_STATUS,StatusFormat.GetString());

	m_pEventHandler->OnEndSearch();
}


void CProgramSearchDialog::OnHighlightResult(bool fHighlight)
{
	SetHighlightResult(fHighlight);
}




CProgramSearchDialog::CEventHandler::CEventHandler()
	: m_pSearchDialog(NULL)
	, m_pSearcher(NULL)
{
}


CProgramSearchDialog::CEventHandler::~CEventHandler()
{
	if (m_pSearchDialog!=NULL)
		m_pSearchDialog->m_pEventHandler=NULL;
}


bool CProgramSearchDialog::CEventHandler::Search(CEventSearcher *pSearcher)
{
	m_pSearcher=pSearcher;

	return OnSearch();
}


bool CProgramSearchDialog::CEventHandler::AddSearchResult(CSearchEventInfo *pEventInfo)
{
	if (pEventInfo==NULL || m_pSearchDialog==NULL)
		return false;
	return m_pSearchDialog->AddSearchResult(pEventInfo);
}


bool CProgramSearchDialog::CEventHandler::Match(const CEventInfoData *pEventInfo) const
{
	return m_pSearcher->Match(pEventInfo);
}
