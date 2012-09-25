//--------------------------------------------------------------------
// FILENAME:		Functions.cpp
//
// Copyright(c) 2008-2010 Kozlov Vasiliy. All rights reserved.
//
//
//--------------------------------------------------------------------


#include "resource.h"
#include "functions.h"



BOOL SwitchFullScreen(HWND hWnd, BOOL bTurnOnOff)
{
	BOOL ReturnValue = TRUE;
	RECT rect;
	HWND hHTaskBar;

	if (bTurnOnOff)
	{
		ReturnValue = SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
		ReturnValue = SetForegroundWindow(hWnd);
		
		hHTaskBar = FindWindow(TEXT("HHTaskBar"),NULL);
		ReturnValue = ShowWindow(hHTaskBar,SW_HIDE);

		
		ReturnValue = SetWindowPos(hWnd, HWND_TOPMOST,
						0, 
						0, 
						rect.right, 
						rect.bottom + 26, 
						SWP_SHOWWINDOW);


	}
	else
	{
		
		hHTaskBar=FindWindow(TEXT("HHTaskBar"),NULL);
		ReturnValue = ShowWindow(hHTaskBar,SW_SHOW);
	}
	return ReturnValue;
}

//----------------------------------------------------------------------------
//
//  FUNCTION:   PlayAudio(Aud, Freq, Dur)
//
//  PURPOSE:    Проигрывает звук
//  PARAMETERS:
//      Aud		- "аудио" переменная
//      Freq	- частота
//      Dur		- длительность
//
//  RETURN VALUE:
//
//----------------------------------------------------------------------------

void PlayAudio(AUDIO_INFO Aud, int Freq, int Dur)
{
	Aud.szSound[0] = 0;
	Aud.dwFrequency = Freq;
	Aud.dwDuration = Dur;
	AUDIO_PlayBeeper(&Aud);
}


//----------------------------------------------------------------------------
// MainWindow_NoMove
//----------------------------------------------------------------------------
BOOL MainWindow_NoMove(HWND hwnd, const LPWINDOWPOS lpwpos)
{
	
#ifndef WIN32_PLATFORM_VC5090
#ifndef WIN32_PLATFORM_POCKETPC

	static BOOL bFirstTime = TRUE;
	static int nLastX;
	static int nLastY;

	if ( bFirstTime )
	{
		nLastX = lpwpos->x;
		nLastY = lpwpos->y;

		bFirstTime = FALSE;
	}

	if ( ( lpwpos->x != nLastX ) ||
		 ( lpwpos->y != nLastY ) )
	{
		SetWindowPos(hwnd,
					 NULL,
					 0,
					 0,
					 0,
					 0,
					 SWP_NOSIZE|SWP_NOZORDER);

		return(FALSE);
	}

#endif
#endif

	return(DefWindowProc(hwnd,WM_WINDOWPOSCHANGED,0,(LPARAM)lpwpos));
}

/*Поле  Описание  
lfHeight  Высота шрифта, логических единиц  
lfWidth  Ширина шрифта, логических единиц  
lfEscapement  Угол нанесения текста - угол между базовой линией текста и горизонталью (десятые доли градуса)  
lfOrientation  Наклон символов (десятые доли градуса)  
lfWeight  Толщина линий начертания шрифта ("жирность")  
lfItalic  Ненулевое значение означает курсив  
lfUnderline  Ненулевое значение означает подчеркивание  
lfStrikeOut  Ненулевое значение означает перечеркнутый шрифт  
lfCharSet  Номер набора символов шрифта - таблицы кодировки  
lfOutPrecision  Параметр, определяющий соответствие запрашиваемого шрифта и имеющегося в наличии  
lfClipPrecision  Параметр, определяющий способ "обрезания" изображения литер при их выходе за пределы области ограничения вывода  
lfQuality  Качество воспроизведения шрифта  
lfPitchAndFamily  Это поле определяет, будет ли шрифт иметь фиксированную или переменную ширину литер, а также семейство, к которому принадлежит шрифт  
lfName  Имя шрифта */ 


HFONT GetNewFont(BOOL bForceNew, int ifHeight, int ifBold)
{
	static HFONT hNewFont = NULL;
	static LOGFONT logfont;
	static int nLastFontHeight;
	static int nLastFontWeight;


	if ( bForceNew )
	{
		hNewFont = NULL;
	}
	
	if ( hNewFont != NULL )
	{
		DeleteObject(hNewFont);

			hNewFont = NULL;
	}
	
	if ( hNewFont == NULL )
	{
		logfont.lfHeight = ifHeight;
		logfont.lfWidth = 0;
		logfont.lfOrientation = logfont.lfEscapement = 0;
		if (ifBold)
			logfont.lfWeight = FW_BOLD;
		else
			logfont.lfWeight = 0;
		logfont.lfItalic = FALSE;
		logfont.lfUnderline = FALSE;
		logfont.lfStrikeOut = FALSE;
		logfont.lfCharSet = DEFAULT_CHARSET;
		logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
		logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		logfont.lfQuality = DEFAULT_QUALITY;
		logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
		wcscpy(logfont.lfFaceName,FONT_TAHOMA);

		hNewFont = CreateFontIndirect(&logfont);

	}

	return(hNewFont);
}

//----------------------------------------------------------------------------
//
//  FUNCTION:   BytesToHexStr(LPTSTR, LPBYTE, int)
//
//  PURPOSE:    Convert a sequence of bytes to a string of its hex value 
//
//  PARAMETERS:
//      lpHexStr - pointer to the buffer receives the hex value string
//      lpBytes	 - pointer to the buffer contains the bytes to be converted
//      nSize	 - number of bytes to be converted
//
//  RETURN VALUE:
//      None.
//
//----------------------------------------------------------------------------

void BytesToHexStr(LPTSTR lpHexStr, LPBYTE lpBytes, int nSize)
{
	int		i;
	TCHAR	szByteStr[5];

	wsprintf(lpHexStr, TEXT(""));
	for (i=0; i<nSize; i++)
	{
		wsprintf(szByteStr, TEXT("%02X"), lpBytes[i]);
		_tcscat(lpHexStr, szByteStr);
	}
}


//для перобразования из юникода в Ansi, но текст выгружается в юникоде только посимвольно
char * UNICODE2ANSI(LPCWSTR WCharStr)
{
	int wbuflen = WideCharToMultiByte(CP_UTF8, 0, WCharStr, wcslen(WCharStr), NULL, 0, NULL, NULL);
	char * wbuf = new char[wbuflen + 1];
	WideCharToMultiByte(CP_UTF8, 0, WCharStr, wcslen(WCharStr), wbuf, wbuflen, NULL, NULL);
	wbuf[wbuflen] = '\0';
	return wbuf;
}



//----------------------------------------------------------------------------
// ScanSetScanParams
//----------------------------------------------------------------------------

DWORD ScanSetScanParams(HANDLE			hScanner,
						DWORD dwCodeIdType,
						DWORD dwScanType,
						BOOL bLocalFeedback,
						DWORD dwDecodeBeepTime,
						DWORD dwDecodeBeepFrequency,
						DWORD dwDecodeLedTime,
						LPTSTR lpszWaveFile)
{
	SCAN_PARAMS scan_params;
	DWORD dwResult;

	// Initialize the structure to all zeroes
	ZeroMemory(&scan_params,sizeof(scan_params));
	
	// Mark entire buffer allocated and not used
	SI_INIT(&scan_params);

	// Get current scan parameters
	dwResult = SCAN_GetScanParameters(hScanner,&scan_params);

	// If there was an error getting the parameters
	if ( dwResult != E_SCN_SUCCESS )
	{
		// Return the error
		return(dwResult);
	}

	// Set all parameters into structure if changed
	SI_SET_IF_CHANGED(&scan_params,dwCodeIdType,dwCodeIdType);
	SI_SET_IF_CHANGED(&scan_params,dwScanType,dwScanType);
	SI_SET_IF_CHANGED(&scan_params,bLocalFeedback,bLocalFeedback);
	SI_SET_IF_CHANGED(&scan_params,dwDecodeBeepTime,dwDecodeBeepTime);
	SI_SET_IF_CHANGED(&scan_params,dwDecodeBeepFrequency,dwDecodeBeepFrequency);
	SI_SET_IF_CHANGED(&scan_params,dwDecodeLedTime,dwDecodeLedTime);

	// If a wave file is supplied
	if ( lpszWaveFile != NULL )
	{
		// Set it into structure as the wave file name
		SI_SET_STRING(&scan_params,szWaveFile,lpszWaveFile);
	}


	// Return the result of setting parameters
	return(SCAN_SetScanParameters(hScanner,&scan_params));
}


//----------------------------------------------------------------------------
// ReturnParameter
//----------------------------------------------------------------------------
std::wstring ReturnParameter(std::wstring SrchStr)
{
	std::size_t ParBegin = SrchStr.find(L"=")+1;
	std::size_t ParEnd = SrchStr.find(L";")-ParBegin;

	if (ParEnd <= 0)
		return SrchStr.erase();

	std::wstring temp = SrchStr.substr(ParBegin, ParEnd);

	while( *temp.begin() == L' ' ) 
	{
		if (temp.size() == 1)//если строка длиной 1 и там есть пробел, возникало исключение
			return temp.erase();
		temp.erase( temp.begin() );
	}

	// откусываем конечные
	while( *--temp.end() == L' ' ) temp.erase( --temp.end() );

	return temp;
	
}

//----------------------------------------------------------------------------
// CompareParameter
//----------------------------------------------------------------------------
int CompareParameter(std::wstring SrchStr, std::wstring ParamStr)
{
	std::size_t ParBegin = SrchStr.find(L"=");

	if (ParBegin <= 0)
		return -1;

	std::wstring temp = SrchStr.substr(0, ParBegin);

	while( *temp.begin() == L' ' ) 
	{
		if (temp.size() == 1)//если строка длиной 1 и там есть пробел, возникало исключение
			return -1;
		temp.erase( temp.begin() );
	}

	// откусываем конечные
	while( *--temp.end() == L' ' ) temp.erase( --temp.end() );

	return temp.compare(0, ParamStr.length(), ParamStr);
}

//----------------------------------------------------------------------------
// GetStringBeforeChar
// возвращает строку из строки SourseStr, расположенную перед символом CharCode
//----------------------------------------------------------------------------
std::wstring GetStringBeforeChar(const std::wstring& SourseStr, std::wstring::value_type CharCode)
{
	std::size_t pos = SourseStr.find(CharCode);
	return pos != std::wstring::npos ? SourseStr.substr(0, pos) : SourseStr;
}

//----------------------------------------------------------------------------
// SetPrefix
// устанавливает префикс для первого параметра,
// проверяет, чтоб он не совпал со вторым
//----------------------------------------------------------------------------
//2.2.1
BOOL SetPrefix(HWND hwnd, std::wstring &par, std::wstring pardep, LPWSTR defval)
{
	BOOL bRes = TRUE;

	if (par.size() == 0)
		par.assign(defval);
	else
	{
		int tmpi = _wtoi(par.c_str()); //отсекаем пробелы и получаем число или 0
		if (tmpi == 0)//если это не число
			//тогда устанавливаем значение по умолчанию	
			par.assign(defval);
		else
		if (tmpi == _wtoi(pardep.c_str()))//если это префикс Code128,
		{
			par.assign(TEXT("-1"));
			bRes = FALSE;
		}
		else
		{
			_itow(tmpi, wcsdup(par.c_str()), 10);

			//если длина строки больше 1
			if (par.size() > 1)
				//копируем первый символ
				par = par.substr(0,1);
		}
	}
	return bRes;
}

std::wstring FromUtf8(const std::string& utf8string)
{
	int widesize = ::MultiByteToWideChar(CP_ACP, 0, utf8string.c_str(), -1, NULL, 0);
	if (widesize == ERROR_NO_UNICODE_TRANSLATION)
		return std::wstring(L" ");
	if (widesize == 0)
		return std::wstring(L" ");

	std::vector<wchar_t> resultstring(widesize);

	int convresult = ::MultiByteToWideChar(CP_ACP, 0, utf8string.c_str(), -1, &resultstring[0], widesize);

	if (convresult != widesize)
		return std::wstring(L" ");

	return std::wstring(&resultstring[0]);
}