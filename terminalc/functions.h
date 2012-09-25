//--------------------------------------------------------------------
// FILENAME:		Functions.h
//
// Copyright(c) 2008-2010 Kozlov Vasiliy. All rights reserved.
//
//
//--------------------------------------------------------------------


#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>




//подключаем хидер с библиотеками конкретного устройства
#include "hardware.h"

#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace std;


#define		ID_EXIT						1100
#define		ID_CLEAR					1101
#define		ID_EXCHANGE					1102
#define		ID_DESC						1103
#define		ID_CURROWDESC				1104
#define		ID_SCANLIST					1105
#define		ID_AMOUNT					1106

#define		ID_PWD_OK					1106
#define		ID_PWD_CANCEL				1107
#define		ID_PWD_STATIC				1108
#define		ID_PWD_EDIT					1109

//код цифровой клавиши 0
#define		ID_0_KEY					48


#define		NEGATIVE_BEEPER_FREQUENCY	500
#define		NEGATIVE_BEEPER_DURATION	500
#define		POSITIVE_BEEPER_FREQUENCY	1000
#define		POSITIVE_BEEPER_DURATION	100

//для обработки нажатия клавиши Alpha
#define		UM_KEYBOARDNOTIFICATION		WM_USER + 0x300


#define		MAX_LOADSTRING				400
#define		MAX_STRING					126


#define FONT_TAHOMA						TEXT("Tahoma")










BOOL			SwitchFullScreen(HWND, BOOL);
void			PlayAudio(AUDIO_INFO, int, int);
BOOL			MainWindow_NoMove(HWND, const LPWINDOWPOS);
HFONT			GetNewFont(BOOL, int, int);
void			BytesToHexStr(LPTSTR, LPBYTE, int);

char *			UNICODE2ANSI(LPCWSTR);

DWORD			ScanSetScanParams(HANDLE, DWORD, DWORD, BOOL, DWORD, DWORD, DWORD, LPTSTR);

BOOL			SetPrefix(HWND, std::wstring &, std::wstring, LPWSTR);//2.2.1
std::wstring	FromUtf8(const std::string&);						//возвращает строку wstring