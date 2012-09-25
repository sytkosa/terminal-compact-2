//--------------------------------------------------------------------
// FILENAME:		Terminalc.cpp
//
// Copyright(c) 2008-2010 Kozlov Vasiliy. All rights reserved.
//
//
//--------------------------------------------------------------------

#include "resource.h"
#include "functions.h"
#include "terminalc.h"


// Define 
#define		countof(x)		sizeof(x)/sizeof(x[0])

//2.2
#define ENCRYPT_ALGORITHM CALG_RC4 

// определения для работы с Wi-Fi
#define	S24_GET_CHANNEL_QUALITY		67	
#define	S24_GET_ADAPTER_MAC_ADDRESS	10
#define	S24_MAC_ADDR_LENGTH			6
#define	LOAD_WIFI_LIBRARY			0
#define	RETURN_SIGNAL_STRENGTH		1

#define	DOWNLOAD_UPDATE_SERVER		0
#define	DOWNLOAD_UPDATE_FILE		1

typedef BOOL (WINAPI *FPS24DRIVEREXTENSION)(PBYTE,		// MAC Address
								     DWORD,				// Function
								     HWND,				// Client Window Handle
								     DWORD,				// Client Token
								     DWORD,				// Synchronous timeout
								     LPVOID,			// Pointer to Input Buffer
								     DWORD,				// Length of Input Buffer
								     LPVOID,			// Pointer to Output Buffer
								     DWORD,				// Length of Output Buffer
								     LPDWORD,			// Returned Length
								     LPDWORD);			// Return Code	
typedef BOOL (WINAPI *FPS24DRIVERINIT) (void);
FPS24DRIVEREXTENSION	fpS24DriverExtension = NULL;
FPS24DRIVERINIT			fpS24DriverInit	= NULL;
HINSTANCE				hWiFiLib = NULL;				//хэндл библиотеки Wi-Fi
BYTE					byAdapterMacAddr[S24_MAC_ADDR_LENGTH];  
														//MAC address

typedef	struct  S24ChannelQuality {						// Current channel quality
	USHORT	usPercentMissedBeacons;						// percent missed beacons in last 5 sec
	USHORT	usPercentTxRetries;							// percent Tx retries in last 2.5 sec
	USHORT	usPercentRxCRCErrors;						// percent CRC errors in last 2.5 sec
	USHORT	usRxRSSI;									// last Rx RSSI
} S24_CHANNEL_QUALITY, *PS24_CHANNEL_QUALITY;

enum tagUSERMSGS
{
	UM_SCAN	= WM_USER + 0x200,
	UM_STARTSCANNING,
	UM_STOPSCANNING
};


// Global variables
HWND			hBtnExit,									//кнопка выхода
				hBtnClr,									//кнопка очистки
				hBtnExch,									//кнопка обмена
				hEditDesc,									//описание
				hStatCur,									//текущая позиция
				hListBox,									//таблица со штрих-кодами
				hEditAmount;								//редактирование количества текущей позиции


WNDPROC				oldEditPwdProc = NULL;								//старая процедура для обработки контролов диалога выхода	

WNDPROC				oldEditDescProc = NULL;								//старая процедура для обработки контрола для редактирования наименования 
WNDPROC				oldEditAmountProc = NULL;							//старая процедура для обработки контрола для редактирования количества

TCHAR				szMsgBuf[MAX_PATH+1];								//текстовая переменная для сообщений
TCHAR				szMsgCaptionBuf[25];								//текстовая переменная для сообщений

int					nResult;											// возвращаемое значение

TCHAR				szAppName[MAX_PATH+1];								//путь с именем exe - файла
TCHAR				szAppPath[MAX_PATH+1];								//путь до exe - файла, без имени
TCHAR				szFileVersion[MAX_STRING];							//номер версии

TCHAR				g_szTermNum[MAX_PATH+1];							//номер терминала //2.2.2
std::wstring		g_StrFromDevId;									//строка из файла devicec.id	//2.2

BOOL				ScannerDeactivate = FALSE;							//флаг выключенного сканера

//для обработки нажатия клавиши Alpha
LPFNREGKEYBOARDNOTIFY 
					lpfnRegisterKeyboard;
LPFNUNREGKEYBOARDNOTIFY		
					lpfnUnregisterKeyboard;
LPFNGETALPHAMODE	lpfnGetAlphaMode;
LPFNSETALPHAMODE	lpfnSetAlphaMode;
HINSTANCE			hKeyLib;


// Forward declarations
ATOM				MyRegisterClass	(HINSTANCE, LPTSTR, WNDPROC);
BOOL				InitInstance	(HINSTANCE, int);
LRESULT				CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);			//оконнная процедура главного окна

void				ErrorExit(HWND, UINT, UINT);
LPTSTR				LoadMsg(UINT, LPTSTR, int);
bool				OnCharPressed(HWND, WPARAM);					//Обработчик горячих клавиш
bool				OnKeyPressed(HWND, WPARAM);							//обработчик обхода конролов
bool				InitTerminalWindow(HWND, LPCREATESTRUCT);			//рисует главное окно программы
void				MainWindow_OnCommand(HWND, int, HWND, UINT);
LRESULT				MainWindow_OnNotify(HWND, int, LPNMHDR);
void				OnDestroy(HWND);
LPTSTR				GetVersion();										//получить версию exeшника
void				SetInitState();										// Строка экрана инициализации
int					LoadIniFile(HWND);									// загрузка terminalc.ini
BOOL				LoadDBFile(HWND, BOOL, WCHAR *);					//в параметрах: TRUE - добавлять, имя файла откуда загружать
//ВОЗВРАЩАЕТ СТРОКУ ПЕРЕД СИМВОЛОМ
std::wstring		GetStringBeforeChar(const std::wstring&, std::wstring::value_type);
std::wstring		ReturnParameter(std::wstring);						//возвращает строку с параметром между = и ; или до конца
int					CompareParameter(std::wstring, std::wstring);		//сравнивает строку и название папарметра //2.2.3.
void				SetButtonText(HWND, std::wstring);					//устанавливает текст кнопки

DWORD				ScanEnableDecoder(HWND, LPDECODER, int );			//подключить декодер, № по порядку
double				GetWeightFromBarCode(WCHAR *, DWORD, double);
void				SetSum();											//Выводит строку итого
bool				SaveBC(HWND);										//true - если успешное сохранение
bool				SaveNewBC(HWND);									//сохранение terminal.new
int					SendBC(HWND);										//-1 - если не успешное сохранение на сервер, 0 и более - количество файлов
int					UpdateBC(HWND, BOOL);								//проверяет обновления на сервере и загружает, -1 - не успешная загрузка с сервера, 1 - успешно //2.1 - второй параметр - откуда загружать обновление
bool				GetTermNum(HWND);									//получить серийный номер терминала
std::wstring		GetEncTermNum(HWND);								//закодировать серийный номер терминала
BOOL				EncryptString(TCHAR*, TCHAR*, TCHAR *);				//2.2
bool				DevId(HWND, int);									//проверка на существование Device.Id
WCHAR *				BarCodeFormat(WCHAR *, DWORD);						// возвращает весовой штрих-код, отформатированный с xxxxx всето веса
void				UpdateBCDesc(BOOL);									//обновить наименование,если можно редактировать ШК
void				UpdateBCAmount(HWND);								//обновить количество,если можно редактировать количество

int					IfWiFiAvailable(int TypeOfAction);					//проверка Wi-Fi


LRESULT	CALLBACK	EditDescProc(HWND, UINT, WPARAM, LPARAM);			//новая процедура для обработки контрола для редактирования наименования 
LRESULT	CALLBACK	EditAmountProc(HWND, UINT, WPARAM, LPARAM);			//новая процедура для обработки контрола для редактирования количества

//процедуры для обработки диалога с параметрами ШК
//LPWORD				lpwAlign (LPWORD);
LRESULT				TCExitDialog(HINSTANCE, HWND, LPWSTR);
LRESULT	CALLBACK	TCExitWndProc(HWND,UINT,WPARAM,LPARAM);				//оконнная процедура диалога выхода
LRESULT	CALLBACK	BCDataEditQuantityProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	TCExitEditPwdProc(HWND, UINT, WPARAM, LPARAM);


//для обработки нажатия клавиши Alpha
void				LoadKeyBoardLib(HWND);
void				UnLoadKeyBoardLib(HWND);
void				SetKeyCode(HWND, HWND, std::wstring, int&);			//2.2.2

//для отладки и восстановления
void				_TRACE(TCHAR *, ...);								//2.2.2
void				RecoveryModeEntry(LPCWSTR, int, double, DWORD, int);//2.2.2
void				RecoveryModeDelete(HWND);							//2.2.2
bool				RecoveryModeRecover(HWND);							//2.2.2

//Обработчик горячих клавиш
bool OnCharPressed(HWND hwnd, WPARAM PressedKey)
{
	HWND FocusedControl = GetFocus();		//получаем активный контрол

	if ((FocusedControl == hEditDesc) ||	//в описании цифровые клавиши должны работать как обычно
		(FocusedControl == hEditAmount))	//в количестве цифровые клавиши должны работать как обычно
		return false;

	if (PressedKey == g_ExitButtonKey)
	{
		SendMessage(hBtnExit,WM_LBUTTONDOWN,NULL,MK_LBUTTON);
		SendMessage(hBtnExit,WM_LBUTTONUP,NULL,MK_LBUTTON);
		SetFocus(hBtnExit);
	}
	else
		if (PressedKey == g_ClrButtonKey)
		{
			SendMessage(hBtnClr,WM_LBUTTONDOWN,NULL,MK_LBUTTON);
			SendMessage(hBtnClr,WM_LBUTTONUP,NULL,MK_LBUTTON);
			SetFocus(hBtnClr);
		}
		else
			if (PressedKey == g_ExchButtonKey)
			{
				SendMessage(hBtnExch,WM_LBUTTONDOWN,NULL,MK_LBUTTON);
				SendMessage(hBtnExch,WM_LBUTTONUP,NULL,MK_LBUTTON);
				SetFocus(hBtnExch);
			}
					
	return true;
}


//Обработчик обхода конролов
//
// - возвращает false, если не надо перехватывать нажатие клавиш
//
bool OnKeyPressed(HWND hwnd, WPARAM PressedKey)
{
	int FocusedItem = 0,					//выделенный элемент списка
		ItemsCount = 0;
	HWND FocusedControl = GetFocus();		//получаем активный контрол

    if (FocusedControl == hListBox)			//если это список
	{
		ItemsCount = SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL);
		if (ItemsCount == 0)	//если в списке нет элементов
		{
			if (PressedKey == VK_DOWN)			
				SetFocus(hBtnExch);
			if (PressedKey == VK_UP)
				SetFocus(hBtnExit);
			if (PressedKey == VK_LEFT)			
				SetFocus(hBtnExit);
			if (PressedKey == VK_RIGHT)
				SetFocus(hBtnExch);
		} 
		else
		{
			FocusedItem = SendMessage(hListBox,LVM_GETNEXTITEM,-1,LVNI_FOCUSED) + 1;
			if ((FocusedItem == ItemsCount) && (PressedKey == VK_DOWN))		//если курсор стоит на последнем элементе
			{
				if (IsWindowEnabled(hEditDesc))
				{
					int ndx = GetWindowTextLength (hEditDesc);
					SetFocus(hEditDesc);
					SendMessage (hEditDesc, EM_SETSEL, 0, ndx);
				}
				else
					SetFocus(hBtnExch);
			}
			else

				if ((FocusedItem == 1) && (PressedKey == VK_UP))			//если курсор стоит на первом элементе
				{
					SetFocus(hBtnExit);
				}
				else
				
					if (PressedKey == VK_RIGHT)
					{
						if (IsWindowEnabled(hEditDesc))
						{
							int ndx = GetWindowTextLength (hEditDesc);
							SetFocus(hEditDesc);
							SendMessage (hEditDesc, EM_SETSEL, 0, ndx);
						}
						else
							SetFocus(hBtnExch);
					}
					else

						if (PressedKey == VK_LEFT)
						{
							if (IsWindowEnabled(hEditDesc))
							{
								int ndx = GetWindowTextLength (hEditDesc);
								SetFocus(hEditDesc);
								SendMessage (hEditDesc, EM_SETSEL, 0, ndx);
							}
							else
								SetFocus(hBtnExit);
						}
						else
							return true;
		}
	}

	if (FocusedControl == hBtnExch)			//если это кнопка Обмен		
	{
		SetInitState();
		if ((PressedKey == VK_DOWN) || (PressedKey == VK_RIGHT))
			SetFocus(hBtnClr);
		if ((PressedKey == VK_UP) || (PressedKey == VK_LEFT))
			//if (IsWindowEnabled(hEditDesc))	//не будем позволять редактировать описание, пусть выберут элемент из списка
			//{
			//	int ndx = GetWindowTextLength (hEditDesc);
			//	SetFocus(hEditDesc);
			//	SendMessage (hEditDesc, EM_SETSEL, 0, ndx);
			//}
			//else
				SetFocus(hListBox);

		if (PressedKey == VK_RETURN)			//если нажат Enter, эмулируем нажатие мышкой
		{
			SendMessage(hBtnExch,WM_LBUTTONDOWN,NULL,MK_LBUTTON);
			SendMessage(hBtnExch,WM_LBUTTONUP,NULL,MK_LBUTTON);
			SetFocus(hBtnExch);
		}
	}
	if (FocusedControl == hBtnClr)			//если это кнопка Очистить
	{
		SetInitState();
		if ((PressedKey == VK_DOWN) || (PressedKey == VK_RIGHT))
			SetFocus(hBtnExit);
		if ((PressedKey == VK_UP) || (PressedKey == VK_LEFT))
			SetFocus(hBtnExch);

		if (PressedKey == VK_RETURN)			//если нажат Enter, эмулируем нажатие мышкой
		{
			SendMessage(hBtnClr,WM_LBUTTONDOWN,NULL,MK_LBUTTON);
			SendMessage(hBtnClr,WM_LBUTTONUP,NULL,MK_LBUTTON);
			SetFocus(hBtnClr);
		}
	}
	if (FocusedControl == hBtnExit)			//если это кнопка Выход
	{
		SetInitState();
		if ((PressedKey == VK_DOWN) || (PressedKey == VK_RIGHT))
			SetFocus(hListBox);
		if ((PressedKey == VK_UP) || (PressedKey == VK_LEFT))
			SetFocus(hBtnClr);

		if (PressedKey == VK_RETURN)			//если нажат Enter, эмулируем нажатие мышкой
		{
			SendMessage(hBtnExit,WM_LBUTTONDOWN,NULL,MK_LBUTTON);
			SendMessage(hBtnExit,WM_LBUTTONUP,NULL,MK_LBUTTON);
			SetFocus(hBtnExit);
		}
	}
	return true;
}


//----------------------------------------------------------------------------
//
//  FUNCTION:   WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
//
//  PURPOSE:    Entry point function, initializes the application, instance,
//              and then launches the message loop.
//
//  PARAMETERS:
//      hInstance     - handle that uniquely identifies this instance of the
//                      application
//      hPrevInstance - always zero in Win32
//      lpszCmdLine   - any command line arguements pass to the program
//      nCmdShow      - the state which the application shows itself on
//                      startup
//
//  RETURN VALUE:
//      (int) Returns the value from PostQuitMessage().
//
//----------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance,
				   HINSTANCE hPrevInstance,
				   LPWSTR lpszCmdLine,
				   int nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;

	//Определяем путь до exe-файла
    ZeroMemory(szAppName, sizeof(szAppName));
    GetModuleFileName(NULL, szAppName, MAX_PATH);
	wcsncpy(szAppPath, szAppName, wcslen(szAppName) - 13);	//копируем путь в szAppPath

	//получаем серийный номер в g_szTermNum
	if (!GetTermNum(NULL))
		return FALSE;
	
	//если обнаружен параметр r
	LPTSTR CmdLine = new WCHAR[MAX_LOADSTRING];
	wcscpy_s(CmdLine, wcslen(lpszCmdLine)+1, lpszCmdLine);
	if (wcslen(CmdLine) != 0)
		if (wcscmp(wcslwr(CmdLine), TEXT("r")) == 0)
		{
			DevId(NULL, 1); //1 - создаем devicec.id
			return FALSE;		 
		}

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)ID_TERMINAL_ACCELERATOR);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if(WM_CHAR == msg.message) 
			OnCharPressed(msg.hwnd, msg.wParam);			//обработчик горячих клавиш

		if((WM_KEYDOWN == msg.message) && ((VK_DOWN == msg.wParam) || (VK_UP == msg.wParam)
			|| (VK_RIGHT == msg.wParam) || (VK_LEFT == msg.wParam) || (VK_RETURN == msg.wParam) ))
			OnKeyPressed(msg.hwnd, msg.wParam);				//обработчик обхода конролов
    
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    It is important to call this function so that the application 
//    will get 'well formed' small icons associated with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass, WNDPROC WindowProcedure)
{
	WNDCLASS	wc;

    wc.style			= CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc		= (WNDPROC) WindowProcedure;
    wc.cbClsExtra		= 0;
    wc.cbWndExtra		= 0;
    wc.hInstance		= hInstance;
    wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor			= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName		= 0;
    wc.lpszClassName	= szWindowClass;

	return RegisterClass(&wc);
}

//
//  FUNCTION: InitInstance(HANDLE, int)
//
//  PURPOSE: Saves instance handle and creates main window
//
//  COMMENTS:
//
//    In this function, we save the instance handle in a global variable and
//    create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND	hWnd = NULL;
	TCHAR	szTitle[MAX_LOADSTRING];			// The title bar text
	TCHAR	szWindowClass[MAX_LOADSTRING];		// The window class name
	
	DWORD dwStyle = WS_VISIBLE; //| WS_CAPTION | WS_SYSMENU

	//размер экрана
	g_ScreenWidth = GetSystemMetrics(SM_CXSCREEN); 
	g_ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	hInst = hInstance;		// Store instance handle in our global variable
			
	// Initialize global strings
	LoadString(hInstance, IDS_TERMINALC, szWindowClass, MAX_LOADSTRING);
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	//TCHAR buffer[MAX_STRING] = {0}; // На будущее определяем тип ОС
	//SystemParametersInfo( SPI_GETPLATFORMTYPE, MAX_STRING, buffer,NULL);
	//if((wcscmp(buffer, L"PocketPC")==0)) // Pocket PC
	//	bVersionPC = TRUE; // Windows CE
	//else
	//	bVersionPC	  = FALSE;	

	//If it is already running, then focus on the window
	hWnd = FindWindow(szWindowClass, szTitle);	
	if (hWnd) 
	{
		// set focus to foremost child window
		// The "| 0x01" is used to bring any owned windows to the foreground and
		// activate them.
		SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
		return 0;
	} 
	
	//регистрируем класс главного окна
	MyRegisterClass(hInstance, szWindowClass, WndProc);

	hWnd = CreateWindowEx(WS_EX_TOPMOST, szWindowClass, szTitle, dwStyle,
		0, 0, g_ScreenWidth, g_ScreenHeight, HWND_DESKTOP, NULL, hInstance, NULL);
	
	if (!hWnd)	
		return FALSE;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//----------------------------------------------------------------------------
//
//  FUNCTION:   WndProc(HINSTANCE, HINSTANCE, LPSTR, int)
//
//  PURPOSE:    Application-defined callback function that processes messages 
//				sent to BasicScan dialog. 
//
//  PARAMETERS:
//      hwnd     - handle to the dialog box. 
//      uMsg	 - specifies the message. 
//      wParam   - specifies additional message-specific information. 
//      lParam   - specifies additional message-specific information. 
//
//  RETURN VALUE:
//      (BOOL) return TRUE if it processed the message, and FALSE if it did not. 
//
//----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD			dwResult;
	TCHAR			szMsgBuf[256];
	LPSCAN_BUFFER	lpScanBuf;
	LVITEM			LvItem;		//новая строка в таблице
	

	switch(uMsg)
    {
		HANDLE_MSG(hwnd, WM_NOTIFY,				MainWindow_OnNotify);
		HANDLE_MSG(hwnd, WM_WINDOWPOSCHANGED,	MainWindow_NoMove);		//обработка запрета передвижения окна
		HANDLE_MSG(hwnd, WM_CREATE,				InitTerminalWindow);	//обработка создания главного окна
		HANDLE_MSG(hwnd, WM_COMMAND,			MainWindow_OnCommand);
		HANDLE_MSG(hwnd, WM_DESTROY,			OnDestroy);

		//case UM_SCAN:
		//	OnScan(hwnd, wParam, lParam);
		//	break;

		case WM_SETFOCUS:
			//фокусируемся на кнопке обмена
			SetFocus(hBtnExch);
			return 0;
			break;


		case UM_STARTSCANNING:
			_TRACE(L"UM_STARTSCANNING: Open scanner, prepare for scanning");
			// Open scanner, prepare for scanning, 
			// and submit the first read request
			dwResult = SCAN_Open(szScannerName, &hScanner);
			if ( dwResult != E_SCN_SUCCESS )
			{	
				ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_OPEN);
				break;
			}
			_TRACE(L"SCAN_Enable");
			dwResult = SCAN_Enable(hScanner);
			if ( dwResult != E_SCN_SUCCESS )
			{
				ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_ENBL);
				break;
			}
			_TRACE(L"SCAN_AllocateBuffer");
			lpScanBuffer = SCAN_AllocateBuffer(bUseText, dwScanSize);
			if (lpScanBuffer == NULL)
			{
				ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_BUF);
				return TRUE;
			}
			_TRACE(L"Turn off sound - ScanSetScanParams");
			//отключаем встроеное пищание сканера
			dwResult = ScanSetScanParams(hScanner,PARAMETER_NO_CHANGE,PARAMETER_NO_CHANGE,PARAMETER_NO_CHANGE,0,PARAMETER_NO_CHANGE,PARAMETER_NO_CHANGE,NULL);
			if ( dwResult != E_SCN_SUCCESS )
				ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_PARAM);

			_TRACE(L"SCAN_ReadLabelMsg");
			dwResult = SCAN_ReadLabelMsg(hScanner, lpScanBuffer, hwnd, UM_SCAN, dwScanTimeout, NULL);
			if ( dwResult != E_SCN_SUCCESS )
				ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_MSG);
			else
				bRequestPending = TRUE;
			
			_TRACE(L"Turn on decoders list");
			// получаем пустой список декодеров
			SI_INIT(&DecoderList);
			//включаем поддержку EAN13
			ScanEnableDecoder(hwnd, DECODER_EAN13,	1);
			//включаем поддержку CODE128
			ScanEnableDecoder(hwnd, DECODER_CODE128,2);
			//2.1:
			//включаем поддержку EAN8
			ScanEnableDecoder(hwnd, DECODER_EAN8,	3);
			//включаем поддержку UPCA
			ScanEnableDecoder(hwnd, DECODER_UPCA,	4);
			//включаем поддержку UPCE0
			ScanEnableDecoder(hwnd, DECODER_UPCE0,	5);
			//включаем поддержку UPCE1
			ScanEnableDecoder(hwnd, DECODER_UPCE1,	6);
			_TRACE(L"UM_STARTSCANNING - OK");
			break;

			return TRUE;

		case UM_STOPSCANNING:

			// We stop scanning in two steps: first, cancel any pending read 
			// request; second, after there is no more pending request, disable 
			// and close the scanner. We may need to do the second step after 
			// a UM_SCAN message told us that the cancellation was completed.
			if (!bStopScanning && bRequestPending)													
				SCAN_Flush(hScanner);

			if (!bRequestPending)			
			{							 
				SCAN_Disable(hScanner);

				if (lpScanBuffer)
					SCAN_DeallocateBuffer(lpScanBuffer);

				SCAN_Close(hScanner);
			}
			bStopScanning = TRUE;

			return TRUE;

		case WM_ACTIVATE:

			// In foreground scanning mode, we need to cancel read requests
			// when the application is deactivated, and re-submit read request
			// when the application is activated again.
			switch(LOWORD(wParam))
			{
				case WA_INACTIVE:
					// Cancel any pending request since we are going to lose focus
					if (bRequestPending)
						dwResult = SCAN_Flush(hScanner);
						// Do not set bRequestPending to FALSE until
						// we get the UM_SCAN message
					break;
				
				default:	// activating
					if (!bRequestPending && lpScanBuffer != NULL && !bStopScanning)
					{	
						// Submit a read request if no request pending
						dwResult = SCAN_ReadLabelMsg(hScanner, lpScanBuffer, hwnd, UM_SCAN, dwScanTimeout, NULL);
						
						if ( dwResult != E_SCN_SUCCESS )
							ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_MSG);
						else
							bRequestPending = TRUE;
					}
					break;
			}
			break;



			// Submit next read request if we are foreground
			if (GetFocus())
			{
				dwResult = SCAN_ReadLabelMsg(hScanner,
											 lpScanBuffer,
											 hwnd,
											 uMsg,
											 dwScanTimeout,
											 NULL);
				
				if ( dwResult != E_SCN_SUCCESS )
					ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_MSG);
				else
					bRequestPending = TRUE;
			}

			return TRUE;

		case UM_SCAN:

			bRequestPending = FALSE;

			//проверка на активность сканера
			if (ScannerDeactivate)
				break;

			// Get scan result from the scan buffer, and display it
			lpScanBuf = (LPSCAN_BUFFER)lParam;
			if ( lpScanBuf == NULL )
				ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_BUF);
			
			switch (SCNBUF_GETSTAT(lpScanBuf))
			{ 
				case E_SCN_DEVICEFAILURE:

					MessageBox(hwnd,LoadMsg(IDS_DEVICE_FAILURE, szMsgBuf, countof(szMsgBuf)), 
									LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
									MB_OK);
					break;

				//case E_SCN_READPENDING:
				//сообщает о том, что ожидаются данные для сканера

				//сообщает о том, что действие было отменено
				case E_SCN_READCANCELLED:

					if (bStopScanning)
					{	// complete the second step of UM_STOPSCANNING
						SendMessage(hwnd,UM_STOPSCANNING,0,0L);	
						return TRUE;
					}
					if (!GetFocus())	
						break;	// Do nothing if read was cancelled while deactivation
					
					break;
			
				case E_SCN_SUCCESS:
				
					int					ItemCount = 0;		//количество строк в таблице
					TCHAR				strTemp[MAX_LOADSTRING]={0};	//для преобразования числа в строку
					int					iTemp = 0;
					LPTSTR				GotBarCode = (LPTSTR)SCNBUF_GETDATA(lpScanBuffer);
					DWORD				BarCodeType = SCNBUF_GETLBLTYP(lpScanBuffer);
					std::wstring 		scannedBarCode;
					bar_code_data_new	*bcd_scanned = NULL;


					scannedBarCode.assign(GotBarCode);

					//если Code128 начинается не g_BarCode128Prefix, то сообщаем об ошибке
					//2.2.1
					if ((BarCodeType == LABELTYPE_CODE128)
						&& scannedBarCode.compare(0,1,g_BarCode128Prefix) != 0)
					{
						SetWindowText(hEditDesc, LoadMsg(IDS_BC_WRONG_CODE128, szMsgBuf, countof(szMsgBuf)));
						PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
						break;
					}
					
					//=============================[на этом этапе имеем любой ШК, кроме Code128 с 9 в начале: ШК с 2 в начале исключены]=============================

					//поиск в db
					std::set<bar_code_data>::iterator found_bar_code
					= find_if (bar_codes.begin(), bar_codes.end(), FindDBBC(scannedBarCode));//БД
					
					//ШК не найден в db
					if (found_bar_code == bar_codes.end())
					{
						if ((g_NewBarCodes == 0) //нельзя сканировать новые ШК
							&& (BarCodeType != LABELTYPE_CODE128))//и тип ШК не Code128
						{
							SetWindowText(hEditDesc, LoadMsg(IDS_BC_NOT_FOUND, szMsgBuf, countof(szMsgBuf)));
							PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
							break;
						}
						else//ввод нового ШК
						{
							//поиск в коллекции отсканированных ШК, чтоб проверить - надо ли увеличивать счетчик новых ШК
							std::vector<bar_code_data_new>::iterator counts_bar_codes
							= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindScannedBC(scannedBarCode, BarCodeType));
					
							//ШК не найден в коллекции отсканированных ШК
							if (counts_bar_codes == bar_codes_scanned.end())
							{
								g_NewBCCount++; 
								if (BarCodeType == LABELTYPE_CODE128)
									wsprintf(szMsgBuf, LoadMsg(IDS_NEW_BARCODE128, szMsgBuf, countof(szMsgBuf)),g_NewBCCount);
								else
									wsprintf(szMsgBuf, LoadMsg(IDS_NEW_BARCODE, szMsgBuf, countof(szMsgBuf)),g_NewBCCount);
							}
							else
							{
								if (BarCodeType == LABELTYPE_CODE128) // если такой CODE128 уже был отсканирован, добавлять его не надо
								{
									//2.0.4
									//обозначаем удачное сканирование
									SetWindowText(hEditDesc, LoadMsg(IDS_SCAN_CODE128_AGAIN, szMsgBuf, countof(szMsgBuf))); 
									PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
									break;
								}
								wcscpy(szMsgBuf,counts_bar_codes->GetDesc());
							}
							
							
							//заполняем локальную переменную - объект класса bar_code_data
							bcd_scanned = new bar_code_data_new(scannedBarCode, szMsgBuf, GetWeightFromBarCode(wcsdup(scannedBarCode.c_str()), BarCodeType, 0.0f), TRUE, BarCodeType); 
							//помещаем данные в глобальный вектор - список отсканированных ШК
							bar_codes_scanned.push_back(*bcd_scanned);
						}
					}
					else //ШК найден в db
					{
						//заполняем локальную переменную - объект класса bar_code_data
						bcd_scanned = new bar_code_data_new(scannedBarCode, found_bar_code->GetDesc(), GetWeightFromBarCode(wcsdup(scannedBarCode.c_str()), BarCodeType, found_bar_code->GetWeight()), FALSE, BarCodeType); 
						//помещаем данные в глобальный вектор - список отсканированных ШК
						bar_codes_scanned.push_back(*bcd_scanned);
					}
					
					//-> поместили отсканированный ШК в bcd_scanned

					//если вкл. режим восстановления
					RecoveryModeEntry(bcd_scanned->GetBC().c_str(), 1, bcd_scanned->GetWeight(), bcd_scanned->GetBarCodeType(), BC_ADD);

					//Будем искать этот ШК в уже остканированном (в DataGrid)
					LVFINDINFO info = {0};
					info.flags=LVFI_STRING;
					info.psz=BarCodeFormat(wcsdup(scannedBarCode.c_str()), BarCodeType);//если весовой, то в формате x xxxxxx
					//info.psz=scannedBarCode;

					memset(&LvItem,0,sizeof(LvItem)); //Обнуление памяти
					LvItem.mask=LVIF_TEXT;   //стиль текст
					//поиск ШК в уже остканированном
					ItemCount = SendMessage(hListBox, LVM_FINDITEM, -1, (LPARAM) (const LVFINDINFO *)&info);
					if (ItemCount == -1)//если не нашли
					{
						//Получаем количество строк
						ItemCount=SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL);

						
						//Добавляем в первую колонку ШК
						LvItem.iItem=ItemCount;  //Позиция строки 
						LvItem.iSubItem=0;       //Позиция колонки
						LvItem.cchTextMax = scannedBarCode.length(); //Длина текста
						LvItem.pszText=BarCodeFormat(wcsdup(scannedBarCode.c_str()), BarCodeType); 
						//LvItem.pszText=scannedBarCode; //Текст для отображения
						SendMessage(hListBox,LVM_INSERTITEM,0,(LPARAM)&LvItem); //Устанавливаем значение во второй колонке уже существующей строки

						//Добавляем во вторую колонку вес
						LvItem.iSubItem=1;       //Позиция колонки
						//преобразовываем double в текст
						swprintf(strTemp, L"%.3f", GetWeightFromBarCode(wcsdup(scannedBarCode.c_str()), BarCodeType, bcd_scanned->GetWeight()));
						LvItem.pszText = strTemp;
						LvItem.cchTextMax = wcslen((LPTSTR)"1"); //Длина текста
						SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 

						//Добавляем в третью колонку штуки
						LvItem.iSubItem=2;       //Позиция колонки
						if (BarCodeType == LABELTYPE_CODE128)
							LvItem.pszText=(LPTSTR)"0"; //Текст для отображения
						else
							LvItem.pszText=(LPTSTR)"1"; //Текст для отображения
						LvItem.cchTextMax = wcslen((LPTSTR)"1"); //Длина текста
						SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 

						//помещаем в количество отсканированных ШК в поле для редактирования
						iTemp = 1;
					}
					else //если нашли
					if (BarCodeType != LABELTYPE_CODE128)//05.05.09
					{
						//получаем элемент (вес) из списка
						LvItem.iItem=ItemCount;  //Позиция строки 
						LvItem.iSubItem=1;       //Позиция колонки
						LvItem.pszText=strTemp; //Текст для отображения
						LvItem.cchTextMax = 10; //Длина текста
						SendMessage(hListBox,LVM_GETITEMTEXT, ItemCount, (LPARAM)&LvItem);
						
						//увеличиваем значение
						swprintf(strTemp, L"%.3f", wcstod(strTemp, NULL) + GetWeightFromBarCode(wcsdup(scannedBarCode.c_str()), bcd_scanned->GetBarCodeType(), bcd_scanned->GetWeight()));
						LvItem.pszText=strTemp; //Текст для отображения
						LvItem.cchTextMax = wcslen(strTemp); //Длина текста
						SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 


						//получаем элемент (количество) из списка
						LvItem.iItem=ItemCount;  //Позиция строки 
						LvItem.iSubItem=2;       //Позиция колонки
						LvItem.pszText=strTemp; //Текст для отображения
						LvItem.cchTextMax = 10; //Длина текста
						SendMessage(hListBox,LVM_GETITEMTEXT, ItemCount, (LPARAM)&LvItem);
						
						//увеличиваем значение
						iTemp = _wtoi(strTemp)+1;
						_itow(iTemp, strTemp, 10);
						LvItem.pszText=strTemp; //Текст для отображения
						LvItem.cchTextMax = wcslen(strTemp); //Длина текста
						SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 
					}

					//Фокусируемся на добавленой строке
					ListView_SetItemState(hListBox, -1, 0, LVIS_SELECTED); //Снять выделение
					SendMessage(hListBox,LVM_ENSUREVISIBLE,(WPARAM)ItemCount,FALSE); //Становимся на добавленую строку
					ListView_SetItemState(hListBox,ItemCount,LVIS_SELECTED ,LVIS_SELECTED); //Выделяем
                    ListView_SetItemState(hListBox,ItemCount,LVIS_FOCUSED ,LVIS_FOCUSED); //Фокусируемся
					
					SetWindowText(hEditDesc, bcd_scanned->GetDesc());

					//помещаем в количество отсканированных ШК в поле для редактирования
					SetWindowText(hEditAmount,(LPWSTR)iTemp);
					UpdateWindow(hEditAmount);
					
					SetFocus(hListBox);
					
					SetSum();

					//обозначаем удачное сканирование
					PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);

					break;
			}

			// Submit next read request if we are foreground
			if (GetFocus())
			{
				dwResult = SCAN_ReadLabelMsg(hScanner,
											 lpScanBuffer,
											 hwnd,
											 uMsg,
											 dwScanTimeout,
											 NULL);
				
				if ( dwResult != E_SCN_SUCCESS )
					ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_MSG);
				else
					bRequestPending = TRUE;
			}

			return TRUE;

		//обработка нажатия клавиши Alpha
		case UM_KEYBOARDNOTIFICATION:
		{
			HWND FocusedControl = GetFocus();		//получаем активный контрол
			if (FocusedControl == hEditDesc)
				return TRUE;

			if (wParam & 0x40)	// Alpha вкл.
			{
				SetWindowText(hEditDesc, LoadMsg(IDS_ALPHA_MODE, szMsgBuf, countof(szMsgBuf)));
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
			}
		}
		break;
	
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

void OnDestroy(HWND hwnd)
{
	SendMessage(hwnd,UM_STOPSCANNING,0,0L);

	if (g_LockBar == 1)
	{
		HWND hHTaskBar = FindWindow(TEXT("HHTaskBar"),NULL);
		if (hHTaskBar != NULL)
			EnableWindow(hHTaskBar,true);
	}
	SwitchFullScreen(hwnd, FALSE);

	UnLoadKeyBoardLib(hwnd); //освобождаем память из-под драйвера клавиатуры

	PostQuitMessage(0);
}


//обработка события от контролов
LRESULT MainWindow_OnNotify(HWND hwnd, int idCtl, LPNMHDR pnmh)
{
	LVITEM			LvItem;		//новая строка в таблице	
		
	switch(idCtl)
	{
	    case ID_SCANLIST:	//событие от списка
			{
				if (pnmh->code == LVN_DELETEALLITEMS)	//уведомление об удалении всех элементов
					SetInitState();
				else
				if((pnmh->code == NM_CLICK)				//уведомление о клике на элементе списка
				|| (pnmh->code == NM_DBLCLK)
				|| (pnmh->code == NM_SETFOCUS)			//уведомление о получении фокуса списком (при добавлении нового элемента)
				|| (pnmh->code == LVN_KEYDOWN)			//уведомление о перемещении по списку при помощи клавы
				|| (pnmh->code == LVN_ITEMCHANGED )
				//|| (pnmh->code == LVN_INSERTITEM)		//уведомление об добавлении элемента
				//|| (pnmh->code == LVN_ITEMACTIVATE))		//уведомление об активировании элемента
				){
					int iSelected = 0;
					char scannedBarCode[255]={0};
					char amountBarCodes[9]={0};
					
					iSelected = SendMessage(hListBox,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
					if(iSelected == -1)
						break;

					LPNMKEY  NMKEYStruct = ((LPNMKEY)pnmh);
					if (pnmh->code == LVN_KEYDOWN)
					{
						//Получаем количество строк, чтоб знать за какие пределы выходить нельзя
						int ItemCount=SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL)-1;
						//получаем структуру, где содержится код нажатой клавиши
						//LPNMKEY  NMKEYStruct = ((LPNMKEY)lParam);
						 if ((NMKEYStruct->wVKey == VK_UP) &&(iSelected-1 >= 0))
							 iSelected -= 1;
						 if ((NMKEYStruct->wVKey == VK_DOWN) && (iSelected+1 <= ItemCount))
							 iSelected += 1;
					}
								
					//получаем в scannedBarCode ШК из строки iSelected
					memset(&LvItem,0,sizeof(LvItem));
					LvItem.mask=LVIF_TEXT;
					LvItem.iSubItem=0;
					LvItem.pszText=(LPWSTR)scannedBarCode;
					LvItem.cchTextMax=255;
					LvItem.iItem=iSelected;
	                  
					SendMessage(hListBox,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);

					//помещаем в amountBarCodes количество отсканированных ШК
					LvItem.pszText=(LPWSTR)amountBarCodes;
					LvItem.iSubItem=2;
					SendMessage(hListBox,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);
					SetWindowText(hEditAmount,(LPWSTR)amountBarCodes);

					//поиск первого подобного ШК среди отсканированных ШК
					std::wstring ws_scannedBarCode((LPWSTR)scannedBarCode);
					std::vector<bar_code_data_new>::iterator found_bar_code
					= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindScannedBC(ws_scannedBarCode, LABELTYPE_UNKNOWN));//не знаем тип ШК, поэтому попытаемся внутри FindScannedBC определить по длине ШК и первой цифре

					//ШК не найден 
					if (found_bar_code == bar_codes_scanned.end())
						break;
					bar_code_data_new bcd_found = *found_bar_code;
					SetWindowText(hEditDesc, bcd_found.GetDesc());

					if (bcd_found.IsNew == TRUE)
					{
						EnableWindow(hEditDesc,true);
						int ndx = GetWindowTextLength (hEditDesc);
						SendMessage (hEditDesc, EM_SETSEL, 0, ndx);
					}
					else
						EnableWindow(hEditDesc,false);
																	
					//вот здесь надо обработать нажатие Enter или двойной клик
					//для вывода окна для редактирования кол-ва, если разрешено
					if (((NMKEYStruct->wVKey == VK_RETURN) || (pnmh->code == NM_DBLCLK)) && (g_QuantityEditing == 1))
					{
						SetFocus(hEditAmount);
						int ndx = GetWindowTextLength (hEditAmount);
						SendMessage (hEditAmount, EM_SETSEL, 0, ndx);
					}
				}
			}
		break;
	}
	// Return success
	return(FALSE);
}

//----------------------------------------------------------------------------
// Main_OnCommand
//----------------------------------------------------------------------------
void MainWindow_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	BOOL bIsThereAnyData = FALSE;
	TCHAR tcTemp[MAX_LOADSTRING], tcExchangeResult[MAX_LOADSTRING];
	BOOL bServerAccess = FALSE;
	int iSendRes, iUpdRes;


	switch (id)
    {
		case ID_EXCHANGE:	//Сохранить

			EnableWindow(hBtnExch,false);	EnableWindow(hBtnClr,false);	EnableWindow(hBtnExit,false);	

			wcscpy(tcExchangeResult, TEXT(" "));//обнуляем текст

			ScannerDeactivate = TRUE;

			if (SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL) > 0)
			{
				nResult = MessageBox(hwnd, 
							LoadMsg(IDS_SAVE_LIST, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_YESNO | MB_ICONQUESTION);
				if (nResult == IDYES) 
				{
					UpdateWindow(hwnd); //2.0.4

					if (SaveBC(hwnd)) //сохранение
					{
						//после - очищаем
						SendMessage(hListBox, LVM_DELETEALLITEMS, 0, 0);
						bar_codes_scanned.clear();
						//SetInitState();
						//сообщаем о сохранении в память терминала
						PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);
						SetWindowText(hEditDesc, LoadMsg(IDS_SAVED_LOCAL, szMsgBuf, countof(szMsgBuf)));
						UpdateWindow(hEditDesc);

						SetSum();
						bIsThereAnyData = TRUE;
					}
				}
				else
				{
					EnableWindow(hBtnExch,true);	EnableWindow(hBtnClr,true);	EnableWindow(hBtnExit,true);

					ScannerDeactivate = FALSE;
					break;
				}
			}
			UpdateWindow(hwnd);
			//если переменная "путь на сервере" указана и был проинициализирован Wi-Fi
			if ((!g_ServerFolder.empty()) && (g_WiFiAvailable))
			{
				//если сигнал есть
				if (IfWiFiAvailable(RETURN_SIGNAL_STRENGTH) > 0)
				{
					bServerAccess = TRUE;

					SetWindowText(hEditDesc, LoadMsg(IDS_SERVER_CHECK, szMsgBuf, countof(szMsgBuf)));
					UpdateWindow(hEditDesc);

					//проверка доступа к каталогу на сервере
					if(GetFileAttributes(g_ServerFolder.c_str()) == 0xFFFFFFFF)
                    {
						PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
						wsprintf(tcTemp, LoadMsg(IDS_ERR_ACCESS_SERVER, szMsgBuf, countof(szMsgBuf)),g_ServerFolder.c_str());
						if (bIsThereAnyData)
							wcscat(tcTemp,LoadMsg(IDS_SAVED_LOCAL, szMsgBuf, countof(szMsgBuf)));
						MessageBox(hwnd,tcTemp, 
									LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
									MB_OK | MB_ICONERROR);
						bServerAccess = FALSE;
					}
				//сигнала нет
				}
				else
				{
					PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
					wcscpy(tcTemp, LoadMsg(IDS_ERR_WIFI, szMsgBuf, countof(szMsgBuf)));
					if (bIsThereAnyData)
						wcscat(tcTemp,LoadMsg(IDS_SAVED_LOCAL, szMsgBuf, countof(szMsgBuf)));
					MessageBox(hwnd,tcTemp, 
								LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
								MB_OK | MB_ICONERROR);
				}

				//доступ к серверу есть
				if (bServerAccess)
				{
					if (g_StrFromDevId.compare(GetEncTermNum(hwnd)) != 0)
					{
						PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
						MessageBox(hwnd,LoadMsg(IDS_ERR_REG_UPDATE, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_OK | MB_ICONWARNING);
						SetInitState();
						
						ScannerDeactivate = FALSE;

						EnableWindow(hBtnExch,true);	EnableWindow(hBtnClr,true);	EnableWindow(hBtnExit,true);
						SetFocus(hBtnExch);
						break;
					}

					//копирование данных на сервер
					iSendRes = SendBC(hwnd); 
					
					if (iSendRes >= 0)
					{
						wsprintf(tcTemp, LoadMsg(IDS_AMOUNT_FILES, szMsgBuf, countof(szMsgBuf)),iSendRes);
						wcscat(tcExchangeResult, tcTemp);
						//wcscat(tcExchangeResult, TEXT("\r\n")); //добавляем перенос
					}
					
					//загрузка DB с сервера и обновление в любом случае
					iUpdRes = UpdateBC(hwnd, DOWNLOAD_UPDATE_SERVER);
					
					if (iUpdRes > 0)
					{
						//в режиме редактирования новых ШК загружаем БД новых ШК
						if (g_NewBarCodes == 1) 
							swprintf(tcTemp, LoadMsg(IDS_DB_UPDATED_NEW, szMsgBuf, countof(szMsgBuf)), bar_codes.size(), new_bar_codes.size());
						else
							swprintf(tcTemp, LoadMsg(IDS_DB_UPDATED, szMsgBuf, countof(szMsgBuf)), bar_codes.size());
						wcscat(tcExchangeResult, tcTemp);
					}
					else
						if (iUpdRes == 0)
							wcscat(tcExchangeResult,LoadMsg(IDS_NO_UPDATE, szMsgBuf, countof(szMsgBuf)));

					SetWindowText(hEditDesc, tcExchangeResult);
					UpdateWindow(hEditDesc);
				}
				else
					SetInitState();
			}
			else
			{
				//обмен не через Wi-Fi //2.1
				if (g_StrFromDevId.compare(GetEncTermNum(hwnd)) != 0) //возвращает закодированную строку
				{
					PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
					MessageBox(hwnd,LoadMsg(IDS_ERR_REG_UPDATE, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONWARNING);
					SetInitState();
					
					ScannerDeactivate = FALSE;

					EnableWindow(hBtnExch,true);	EnableWindow(hBtnClr,true);	EnableWindow(hBtnExit,true);
					SetFocus(hBtnExch);
					break;
				}

				if(bIsThereAnyData)
					wcscat(tcExchangeResult,LoadMsg(IDS_SAVED_LOCAL, szMsgBuf, countof(szMsgBuf)));
			
				//обновление из файла
				iUpdRes = UpdateBC(hwnd, DOWNLOAD_UPDATE_FILE);
				
				if (iUpdRes > 0)
				{
					//в режиме редактирования новых ШК загружаем БД новых ШК
					if (g_NewBarCodes == 1) 
						swprintf(tcTemp, LoadMsg(IDS_DB_UPDATED_NEW, szMsgBuf, countof(szMsgBuf)), bar_codes.size(), new_bar_codes.size());
					else
						swprintf(tcTemp, LoadMsg(IDS_DB_UPDATED, szMsgBuf, countof(szMsgBuf)), bar_codes.size());
					wcscat(tcExchangeResult,tcTemp);
				}
				else
					if (iUpdRes == 0)
						wcscat(tcExchangeResult,LoadMsg(IDS_NO_UPDATE, szMsgBuf, countof(szMsgBuf)));
				
				SetWindowText(hEditDesc, tcExchangeResult);
				UpdateWindow(hEditDesc);
			}
		
			ScannerDeactivate = FALSE;

			PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);

			EnableWindow(hBtnExch,true);	EnableWindow(hBtnClr,true);	EnableWindow(hBtnExit,true);
			EnableWindow(hEditDesc, false); //а вдруг оно было активно?
			SetFocus(hBtnExch);
			break;

		case ID_CLEAR:	//Очистить	
		
			if (SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL) > 0)
			{
			
				nResult = MessageBox(hwnd, 
							LoadMsg(IDS_CLEAR_LIST, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_YESNO | MB_ICONQUESTION);
				if (nResult == IDYES) 
				{
					SendMessage(hListBox, LVM_DELETEALLITEMS, 0, 0);
					bar_codes_scanned.clear();
					RecoveryModeDelete(hwnd);//2.2.2
					SetSum();
				}
				SetFocus(hBtnClr);
			}
			else
				if ((g_NewBarCodes == 1) && (new_bar_codes.size() > 0))
				{
					nResult = MessageBox(hwnd, 
								LoadMsg(IDS_CLEAR_LIST_NEW, szMsgBuf, countof(szMsgBuf)), 
								LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
								MB_YESNO | MB_ICONQUESTION);
					if (nResult == IDYES) 
					{
						new_bar_codes.clear();
						bar_codes.clear();
						SetWindowText(hEditDesc, LoadMsg(IDS_LOADING_DB, szMsgBuf, countof(szMsgBuf)));
						UpdateWindow(hEditDesc);
						LoadDBFile(hwnd, FALSE, LoadMsg(IDS_TERMINALC_DB, szMsgBuf, countof(szMsgBuf)));
					}
					SetFocus(hBtnClr);
				}
			EnableWindow(hEditDesc, false); //а вдруг оно было активно?
			SetInitState();
			break;
		
		case ID_EXIT:	//Выход
			{
				ScannerDeactivate = TRUE;
				
				if (g_Password.size() > 0)
				{
					LoadMsg(IDS_BUTTON_EXIT, szMsgCaptionBuf, countof(szMsgCaptionBuf));
					if (TCExitDialog(hInst, hwnd, szMsgCaptionBuf) == ID_PWD_OK)
					{
						//сохраняем terminal.new
						if (g_NewBarCodes == 1)
							SaveNewBC(hwnd);

						UpdateWindow(hwnd);
						SetWindowText(hEditDesc, LoadMsg(IDS_TERMINAL_EXIT, szMsgBuf, countof(szMsgBuf)));
						UpdateWindow(hEditDesc);
						
						SendMessage(hwnd,WM_DESTROY,0,0L);
					}
				}			
				else
				{
					nResult = MessageBox(hwnd, 
							LoadMsg(IDS_EXIT, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_YESNO | MB_ICONQUESTION);
					if (nResult == IDYES) 
					{
						//сохраняем terminal.new
						if (g_NewBarCodes == 1)
							SaveNewBC(hwnd);

						UpdateWindow(hwnd);
						SetWindowText(hEditDesc, LoadMsg(IDS_TERMINAL_EXIT, szMsgBuf, countof(szMsgBuf)));
						UpdateWindow(hEditDesc);

						SendMessage(hwnd,WM_DESTROY,0,0L);
					}
					/*else
						PostMessage(hwnd,UM_STARTSCANNING,0,0L);*/
				}

				ScannerDeactivate = FALSE;
			}
			break;

		case ID_CURROWDESC: //Наименование
			if (codeNotify == EN_KILLFOCUS)//EN_UPDATE
				UpdateBCDesc(FALSE);
			break;
	}
}

//----------------------------------------------------------------------------
//
//	Рисует главное окно программы и выполняет загрузку
//
bool InitTerminalWindow(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	int				CurrentControlHeight = g_ScreenHeight;
	int				CurrentButtonsHeight, CurrentButtonsWidth;
	TCHAR			lpSums[MAX_LOADSTRING];	
	LVCOLUMN		LvCol;										//новая колонка в таблице
	int				iBCWidth = 0x85, iWghWidth = 0x55, iColWidth = 0x45;
	SYSTEMTIME		systime;

	SwitchFullScreen(hwnd, TRUE);	//переключаемся на полный экран

	InitCommonControls();

	//Определяем размер шрифта в зависимости от размера экрана
	if ((g_ScreenWidth >= 320) && (g_ScreenHeight >= 320))
		g_hFont = GetNewFont(TRUE, 18, TRUE);
	else
	{	iBCWidth = 0x70; iColWidth = 0x25; iWghWidth = 0x43; //2.0.4
		g_hFont = GetNewFont(TRUE, 14, TRUE);
	}

	CurrentButtonsHeight = CurrentControlHeight * 9 / 100;
	CurrentButtonsWidth = g_ScreenWidth / 3;
	CurrentControlHeight = CurrentControlHeight - CurrentButtonsHeight - 1;
	hBtnExch = CreateWindow(TEXT("BUTTON"), LoadMsg(IDS_BUTTON_EXCH, szMsgBuf, countof(szMsgBuf)), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 3, CurrentControlHeight, CurrentButtonsWidth, CurrentButtonsHeight, hwnd, (HMENU)ID_EXCHANGE, hInst, NULL);
	SetWindowFont(hBtnExch,g_hFont,TRUE);
	hBtnClr = CreateWindow(TEXT("BUTTON"), LoadMsg(IDS_BUTTON_CLEAR, szMsgBuf, countof(szMsgBuf)), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CurrentButtonsWidth + 2, CurrentControlHeight, CurrentButtonsWidth, CurrentButtonsHeight, hwnd, (HMENU)ID_CLEAR, hInst, NULL);
	SetWindowFont(hBtnClr,g_hFont,TRUE);
	hBtnExit = CreateWindow(TEXT("BUTTON"), LoadMsg(IDS_BUTTON_EXIT, szMsgBuf, countof(szMsgBuf)), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, g_ScreenWidth - CurrentButtonsWidth - 2, CurrentControlHeight, CurrentButtonsWidth, CurrentButtonsHeight, hwnd, (HMENU)ID_EXIT, hInst, NULL);
	SetWindowFont(hBtnExit,g_hFont,TRUE);


	CurrentControlHeight = CurrentControlHeight - int(g_ScreenHeight * 20 / 100);
	CurrentControlHeight = CurrentControlHeight - 3;
	hEditDesc = CreateWindow(TEXT("EDIT"), LoadMsg(IDS_TERMINAL_READY, szMsgBuf, countof(szMsgBuf)), 
		WS_CHILD | WS_VISIBLE | ES_LEFT | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_TABSTOP,  
		3, CurrentControlHeight, g_ScreenWidth - 5 , int(g_ScreenHeight * 20 / 100), hwnd, (HMENU)ID_CURROWDESC, hInst, NULL);
	SetWindowFont(hEditDesc,g_hFont,TRUE);
	//назначаем новую оконную процедуру для обработки нажатий клавиш
	oldEditDescProc = (WNDPROC) SetWindowLong(hEditDesc,GWL_WNDPROC, (LONG)EditDescProc);
	EnableWindow(hEditDesc,false);


	CurrentControlHeight = CurrentControlHeight - CurrentButtonsHeight - 6;
	wsprintf(lpSums, LoadMsg(IDS_SUMS, szMsgBuf, countof(szMsgBuf)), 0.000, 0); //2.2.1
	if ((g_ScreenWidth >= 320) && (g_ScreenHeight >= 320))
	{
		while (wcslen(lpSums) < 31)
			wcscat(lpSums, TEXT(" "));
	}
	else
	{
		while (wcslen(lpSums) < 25)
			wcscat(lpSums, TEXT(" "));
	}
	wcscat(lpSums, LoadMsg(IDS_AMOUNT, szMsgBuf, countof(szMsgBuf)));
	hStatCur = CreateWindow(TEXT("STATIC"), lpSums, WS_CHILD | WS_VISIBLE | SS_CENTER | WS_BORDER, 3, CurrentControlHeight + 4, (g_ScreenWidth * 6/7) - 5, CurrentButtonsHeight, hwnd, (HMENU)ID_DESC, hInst, NULL);//2.2 - с 8 частей на 7
	SetWindowFont(hStatCur,g_hFont,TRUE);

	hEditAmount = CreateWindow(TEXT("EDIT"), TEXT("0"), WS_CHILD | WS_VISIBLE | ES_RIGHT | WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_NUMBER, (g_ScreenWidth * 6/7), CurrentControlHeight + 4, (g_ScreenWidth * 1/7), CurrentButtonsHeight, hwnd, (HMENU)ID_AMOUNT, hInst, NULL);//2.2
	SetWindowFont(hEditAmount,g_hFont,TRUE);
	//назначаем новую оконную процедуру для обработки нажатий клавиш
	oldEditAmountProc = (WNDPROC) SetWindowLong(hEditAmount,GWL_WNDPROC, (LONG)EditAmountProc);
	EnableWindow(hEditAmount,false);

	hListBox = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_ALIGNLEFT | WS_BORDER | WS_TABSTOP | WS_VSCROLL, 3, 3, g_ScreenWidth - 5, CurrentControlHeight, hwnd, (HMENU)ID_SCANLIST, hInst, NULL);
	SendMessage(hListBox,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT); // Set style
	SetWindowFont(hListBox,g_hFont,TRUE);
		
    memset(&LvCol,0,sizeof(LvCol)); // Reset Column
	LvCol.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM|LVCF_FMT; // тип по маске

	// Добавить колонку
	LvCol.cx=iBCWidth;									// Ширина
	LvCol.pszText=L"Штрих-код";									//Заголовок
	LvCol.fmt=LVCFMT_LEFT;										//выравнивание
	SendMessage(hListBox,LVM_INSERTCOLUMN,0,(LPARAM)&LvCol);
	
	// Добавить колонку
	LvCol.cx=iWghWidth;												// Ширина
	LvCol.pszText=L"Вес";										//Заголовок
	LvCol.fmt=LVCFMT_RIGHT;										//выравнивание
	SendMessage(hListBox,LVM_INSERTCOLUMN,1,(LPARAM)&LvCol);


	// Добавить колонку
	LvCol.cx=iColWidth;												// Ширина
	LvCol.pszText=L"Шт";										//Заголовок
	LvCol.fmt=LVCFMT_RIGHT;										//выравнивание
	SendMessage(hListBox,LVM_INSERTCOLUMN,2,(LPARAM)&LvCol);

	//устанавливаем некоторые начальные значения
	//устанавливаем префикс для весового ШК, в LoadIniFile он будет переустановлен при необходимости
	g_BarCodeWeightPrefix.assign(TEXT("2"));
	g_DeviceName.assign(TEXT("Scanner"));
	g_ScannedFile.assign(TEXT("scanned.txt"));
	g_BarCode128Prefix.assign(TEXT("9"));//2.2.1
	
	//прорисовываем все на экране
	UpdateWindow(hwnd);
	EnableWindow(hBtnExch,false);
	EnableWindow(hBtnClr,false);
	EnableWindow(hBtnExit,false);

	//включаем подсветку
	DISPLAY_SetBacklightState(BACKLIGHT_ON);
	
	// загрузка terminalc.ini
	SetWindowText(hEditDesc, LoadMsg(IDS_LOADING_INI, szMsgBuf, countof(szMsgBuf)));
	UpdateWindow(hEditDesc);
	if (!LoadIniFile(hwnd))
		return 0;

	//обработка параметров: выключение панели задач
	if (g_LockBar == 1)
	{
		HWND hHTaskBar = FindWindow(TEXT("HHTaskBar"),NULL);
		if (hHTaskBar != NULL)
			EnableWindow(hHTaskBar,false);
	}

	//обработка параметров: делаем доступным окно для редактирования количества
	if (g_QuantityEditing == 1)
		EnableWindow(hEditAmount,true);

	GetLocalTime(&systime);
	_TRACE(L"================================ InitTerminal: %02d:%02d:%02d - %02d.%02d.%02d", systime.wHour, systime.wMinute, systime.wSecond, systime.wDay, systime.wMonth, systime.wYear);
	_TRACE(L"TermNum: %s",g_szTermNum);
	_TRACE(L"RegistrationName: %s",wcsdup(g_RegistrationName.c_str()));
	_TRACE(L"ServerFolder: %s",wcsdup(g_ServerFolder.c_str()));
	_TRACE(L"ScannedFile: %s",wcsdup(g_ScannedFile.c_str()));
	_TRACE(L"LockBar: %d",g_LockBar);
	_TRACE(L"BarCodeWeightPrefix: %s",wcsdup(g_BarCodeWeightPrefix.c_str()));
	_TRACE(L"QuantityEditing: %d",g_QuantityEditing);
	_TRACE(L"NewBarCodes: %d",g_NewBarCodes);
	_TRACE(L"Password: %s",wcsdup(g_Password.c_str()));
	_TRACE(L"DeviceName: %s",wcsdup(g_DeviceName.c_str()));
	_TRACE(L"ExchButtonKey: %d",g_ExchButtonKey);
	_TRACE(L"ClrButtonKey: %d",g_ClrButtonKey);
	_TRACE(L"ExitButtonKey: %d",g_ExitButtonKey);
	_TRACE(L"DeleteUpdate %d",g_DeleteUpdate);
	_TRACE(L"BarCode128Prefix: %s",wcsdup(g_BarCode128Prefix.c_str()));
	_TRACE(L"Debug: %d",g_Debug);

	//registration
	_TRACE(L"Registration...");
	//если devicec.id не существует
	if (!DevId(hwnd, 0))
	{
		_TRACE(L"devicec.id doesn't exist");
		if (!DevId(hwnd, 1)) //1 - создаем 
		{
			_TRACE(L"Unable to create devicec.id ");
			return 0;		 //не смогли создать - выходим
		}

		_TRACE(L"Creating devicec.id");
		//ZeroMemory(g_RegistrationName, sizeof(g_RegistrationName));
		g_RegistrationName.erase();
		PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
		MessageBox(hwnd,LoadMsg(IDS_REG_CREATE, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_OK | MB_ICONWARNING);
	}
	else //если существует, то DevId считает в g_StrFromDevId его содержимое
	{
		_TRACE(L"devicec.id already exists");
		if (g_StrFromDevId.compare(GetEncTermNum(hwnd)) != 0) //возвращает закодированную строку
		{
			_TRACE(L"Wrong ID");
			g_RegistrationName.erase();
			PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
			MessageBox(hwnd,LoadMsg(IDS_ERR_REG, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_OK | MB_ICONWARNING);
		}
	}
	UpdateWindow(hwnd);

	// загрузка terminalc.db
	_TRACE(L"Loading terminalc.db");
	SetWindowText(hEditDesc, LoadMsg(IDS_LOADING_DB, szMsgBuf, countof(szMsgBuf)));
	UpdateWindow(hEditDesc);
	if (!LoadDBFile(hwnd, FALSE, LoadMsg(IDS_TERMINALC_DB, szMsgBuf, countof(szMsgBuf))))
	{
		_TRACE(L"Error while loading terminalc.db");
		PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
		MessageBox(hwnd,LoadMsg(IDS_ERR_OPEN_DB, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
		return 0;
	}

	//в режиме редактирования новых ШК загружаем БД новых ШК
	if (g_NewBarCodes == 1) 
	{
		_TRACE(L"Loading new barcodes");
		SetWindowText(hEditDesc, LoadMsg(IDS_LOADING_NEW_DB, szMsgBuf, countof(szMsgBuf)));
		UpdateWindow(hEditDesc);
		//добавляем в основную БД
		_TRACE(L"Add new barcodes to db");
		LoadDBFile(hwnd, TRUE, LoadMsg(IDS_TERMINALC_NEW, szMsgBuf, countof(szMsgBuf)));
		g_NewBCCount = new_bar_codes.size();
	}

	if ((g_RecoveryMode == 1) && (g_StrFromDevId.compare(GetEncTermNum(hwnd)) == 0))
	{
		_TRACE(L"Entering recovery mode...");
		//определяем путь до файла
		wcsncpy(g_szRecoverName, szAppPath, wcslen(szAppPath));		
		wcscat(g_szRecoverName, L"recover.txt");

		if(GetFileAttributes(g_szRecoverName) != 0xFFFFFFFF)
		{
			//задать вопрос о восстановлении, восстановить
			nResult = MessageBox(hwnd, 
						LoadMsg(IDS_RECFILE_FOUND, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_YESNO | MB_ICONQUESTION);
			if (nResult == IDYES) 
			{
				UpdateWindow(hwnd);
				RecoveryModeRecover(hwnd);
			}
			//удалить файл
			RecoveryModeDelete(hwnd);
		}
	}

	//==========================[загрузка выполнена успешно]==========================
	PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);
	
	SetInitState();

	EnableWindow(hBtnExch,true);
	EnableWindow(hBtnClr,true);
	EnableWindow(hBtnExit,true);

	_TRACE(L"Checking Wi-Fi");
	//определение необходимости работы с Wi-Fi
	if (g_ServerFolder.size() > 0)
	{
		if (IfWiFiAvailable(LOAD_WIFI_LIBRARY) == 0)
			g_WiFiAvailable = TRUE;
	}
	
	//фокусируемся на кнопке обмена
	SetFocus(hBtnExch);
	PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);

	PostMessage(hwnd,UM_STARTSCANNING,0,0L);
	_TRACE(L"InitTerminal - OK");
	return(TRUE);
}

//----------------------------------------------------------------------------
//
//  FUNCTION:   ErrorExit(HWND, UINT, LPTSTR)
//
//  PURPOSE:    Handle critical errors and exit dialog. 
//
//  PARAMETERS:
//      hwnd     - handle to the dialog box. 
//      uID		 - ID of the message string to be displayed 
//      szFunc   - function name if it's an API function failure 
//
//  RETURN VALUE:
//      None.
//
//----------------------------------------------------------------------------
void ErrorExit(HWND hwnd, UINT uID, UINT msgID)
{
	PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
	LoadMsg(msgID, szMsgBuf, countof(szMsgBuf));
			if (wcslen(szMsgBuf) == 0)
				wcscpy(szMsgBuf, TEXT("Error"));
	MessageBox(hwnd,szMsgBuf, 
						LoadMsg(uID, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
	SendMessage(hwnd,WM_DESTROY,0,0L);
}

//----------------------------------------------------------------------------
//
//  FUNCTION:   LoadMsg(UINT, LPTSTR, int)
//
//  PURPOSE:    Load a message string for the string table
//
//  PARAMETERS:
//      uID		 - ID of the message string to be loaded
//      lpBuffer - buffer to hold the message string
//		nBufSize - size of lpBuffer
//
//  RETURN VALUE:
//      (LPTSTR) pointer to lpBuffer
//
//----------------------------------------------------------------------------
LPTSTR LoadMsg(UINT uID, LPTSTR lpBuffer, int nBufSize)
{
	if (!LoadString(hInst, uID, lpBuffer, nBufSize))
		wcscpy(lpBuffer, TEXT(""));

	return lpBuffer;
}


// **************************************************************************
// FUNCTION : GetVersion
//
// PURPOSE :  Read Version
//
// PARAMETERS : None
// 
// RETURN VALUE : Version of the DLL in format ##.##
//
// **************************************************************************

LPTSTR GetVersion()
{
	DWORD vSize;
    DWORD vLen;
    BOOL retVal;    
    VS_FIXEDFILEINFO* version=NULL;
    LPVOID versionInfo=NULL;
    
	vSize = GetFileVersionInfoSize(szAppName,&vLen);
    if (vSize) 
    {
        versionInfo = malloc(vSize+1);
        if (GetFileVersionInfo(szAppName,vLen,vSize,versionInfo))
        {            
            retVal = VerQueryValue(versionInfo,L"\\",(void**)&version,(UINT *)&vLen);
            if (retVal) 
            {
				wsprintf(szFileVersion, L"%d.%d.%d",version->dwFileVersionMS >> 16,
                version->dwFileVersionMS & 0xFFFF,
                version->dwFileVersionLS >> 16); 
            }
            else 
				wsprintf(szFileVersion, L"0.0.0");
        }
    }
	  return (LPTSTR) szFileVersion;
 }

// Строка экрана инициализации
void SetInitState()
{
	TCHAR	InitStr[MAX_LOADSTRING];	
	TCHAR	RegStr[MAX_LOADSTRING];	
	TCHAR	strLoadedTemp[255]={0};

	wsprintf(InitStr, LoadMsg(IDS_TERMINAL_READY, szMsgBuf, countof(szMsgBuf)),g_DeviceName.c_str());
	
	wcscat(InitStr,TEXT("\r\n"));
	wcscat(InitStr,LoadMsg(IDS_VERSION, szMsgBuf, countof(szMsgBuf)));
	wcscat(InitStr,GetVersion());
	wcscat(InitStr,TEXT(". "));
	if (g_RegistrationName.size() == 0)
	{
		LoadString(hInst, IDS_TERMINAL_UNREG, RegStr, MAX_LOADSTRING);
		wcscat(InitStr,RegStr);
	}
	else
	{
		LoadString(hInst, IDS_TERMINAL_REG, RegStr, MAX_LOADSTRING);
		wcscat(InitStr,RegStr);
		wcscat(InitStr,g_RegistrationName.c_str());
	}
	wcscat(InitStr,TEXT("\r\n"));
	
	if (g_NewBarCodes == 1) //загружаем информацию о количестве новых ШК
		wsprintf(strLoadedTemp, LoadMsg(IDS_TERMINAL_LOADED_BC_COUNT_NEW, szMsgBuf, countof(szMsgBuf)), bar_codes.size(), new_bar_codes.size());//g_NewBCCount);
	else 
		wsprintf(strLoadedTemp, LoadMsg(IDS_TERMINAL_LOADED_BC_COUNT, szMsgBuf, countof(szMsgBuf)), bar_codes.size());	
	wcscat(InitStr,strLoadedTemp);

	SetWindowText(hEditDesc, InitStr);

	//делаем поле недоступным для редактирования //2.2.1
	EnableWindow(hEditDesc, FALSE);

	//помещаем в количество отсканированных ШК - 0
	SetWindowText(hEditAmount,TEXT("0"));
}


// загрузка terminalc.db
//в параметрах - имя файла откуда загружать
BOOL LoadDBFile(HWND hwnd, BOOL AppendBCs, WCHAR * szdbFileName)
{
	TCHAR			szdbName[MAX_PATH+1] = {0};	//путь до db-файла
	
	//определяем путь до db-файла
	wcsncpy(szdbName, szAppPath, wcslen(szAppPath));		//копируем путь в szdbName
	wcsncat(szdbName, szdbFileName, wcslen(szdbFileName));//склеиваем путь и terminalc.db

	std::ifstream db_file(szdbName);

	if (db_file.fail())
		return false;

	//удаляем все элементы, если не сказано добавлять
	if (!AppendBCs)
		bar_codes.clear();
	
	for(std::string line; std::getline(db_file, line); )
	{
		//преобразуем в Unicode
		int wbuflen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, 0, 0);
		WCHAR * wbuf = new WCHAR[wbuflen];
		MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, wbuf, wbuflen);

		if (wbuf[0] == 65279)//первый символ в кодировке unicode 
			//wcsncpy(wbuf, &wbuf[1], wcslen(wbuf)-1);// надо удалить
			wcsncpy_s(wbuf, wbuflen-1, &wbuf[1], _TRUNCATE); //2.2.1 - предыдущее сдвигало строку, добавляя еще один символ

		if (wcslen(wbuf) <= 1)
			continue;

		if (!bar_codes.insert(bar_code_data(wbuf)).second)
		{
			//сообщаем о задвоении, если не сказано добавлять
			if (!AppendBCs)
			{
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
				MessageBox(hwnd,wbuf, 
						LoadMsg(IDS_ERR_DOUBLE_BC, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONWARNING);
			}
		}
		else
			//если добавляем новые ШК, надо увеличить счетчик новых ШК
			if (AppendBCs)
			{
				//заносим в БД новых ШК
				new_bar_codes.insert(bar_code_data(wbuf));
				//g_NewBCCount++;
			}
		delete [] wbuf;
	}
	db_file.close();
	return true; 
}



// загрузка terminalc.ini
int LoadIniFile(HWND hwnd)
{
	TCHAR			sziniName[MAX_PATH+1] = {0};	//путь до ini-файла
	
	//определяем путь до ini-файла: wcslen(szAppName)-13 - длина пути без имени файла
	wcsncpy(szAppPath, szAppName, wcslen(szAppName) - 13);	//копируем путь в szAppPath
	wcsncpy(sziniName, szAppPath, wcslen(szAppPath));		//копируем путь в sziniName
	wcsncat(sziniName, TEXT("terminalc.ini"), wcslen(TEXT("terminalc.ini")));//склеиваем путь и terminalc.ini

	std::ifstream ini_file(sziniName);

	if (ini_file.fail())
    {
		PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
		MessageBox(hwnd,LoadMsg(IDS_ERR_OPEN_INI, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
		return 0;
	}

	std::vector<WCHAR> wbuf;
	std::wstring wline;

	for(std::string line; std::getline(ini_file, line); )
	{
		if (line.empty())
			continue;

		if (line.find(";") == 0)
			continue;

		//преобразуем в Unicode
		size_t wbuflen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), line.size(), NULL, NULL);
		wbuf.resize(wbuflen);
		MultiByteToWideChar(CP_UTF8, 0, line.c_str(), line.size(), &wbuf.front(), wbuflen);
		wline.clear();
		wline.assign(wbuf.begin(), wbuf.end());

		//обработка строк
		//присваеваем переменным значения
		if (wline.find(TEXT("Password")) != std::wstring::npos)
			g_Password = ReturnParameter(wline);
    	else
		if (wline.find(TEXT("RegistrationName")) != std::wstring::npos)
			g_RegistrationName = ReturnParameter(wline);
		else
		if (wline.find(TEXT("ServerFolder")) != std::wstring::npos)
		{
			g_ServerFolder = ReturnParameter(wline);
			if (g_ServerFolder.size() > 0)
				if(g_ServerFolder.find(TEXT("\\"),g_ServerFolder.size()-1) == std::wstring::npos)
				g_ServerFolder.append(TEXT("\\"));
		}
		else
		if (wline.find(TEXT("ScannedFile")) != std::wstring::npos)
		{
			g_ScannedFile = ReturnParameter(wline);	
			if (g_ScannedFile.size() == 0)
				g_ScannedFile.assign(TEXT("scanned.txt"));
		}
		else
		if (wline.find(TEXT("Debug")) != std::wstring::npos)//2.2.2
		{
			g_Debug = _wtoi(ReturnParameter(wline).c_str());	
			if (g_Debug > 1)
				g_Debug = 0;
			//определяем путь до файла
			wcsncpy(g_szDebugName, szAppPath, wcslen(szAppPath));		
			wcscat(g_szDebugName, L"debug.txt");
			//удаляем файл при запуске
			if (g_Debug == 1)
				if(GetFileAttributes(g_szDebugName) != 0xFFFFFFFF)
					DeleteFile(g_szDebugName);
		}
		else
		if (wline.find(TEXT("LockBar")) != std::wstring::npos)
		{
			g_LockBar = _wtoi(ReturnParameter(wline).c_str());	
			if (g_LockBar > 1)
				g_LockBar = 0;
		}
		else //2.2.3
		//if (wline.find(TEXT("WeightBarCodePrefix")) != std::wstring::npos)
		if (CompareParameter(wline, std::wstring(TEXT("WeightBarCodePrefix"))) == 0)
		{
			//устанавливаем префикс для весового ШК
			g_BarCodeWeightPrefix = ReturnParameter(wline);
			BOOL bRes = SetPrefix(hwnd, g_BarCodeWeightPrefix, g_BarCode128Prefix, TEXT("2"));

			if (bRes == FALSE)
			{
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
				MessageBox(hwnd,LoadMsg(IDS_ERR_WEIGHTBC_PREFIX, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_FAILURE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
			}
		}
		else
		if (wline.find(TEXT("QuantityEditing")) != std::wstring::npos)
		{
			g_QuantityEditing = _wtoi(ReturnParameter(wline).c_str());	
			if (g_QuantityEditing > 1)
				g_QuantityEditing = 0;
		}
		else
		if (wline.find(TEXT("NewBarCodes")) != std::wstring::npos)
		{
			g_NewBarCodes = _wtoi(ReturnParameter(wline).c_str());	
			if (g_NewBarCodes > 1)
				g_NewBarCodes = 0;
		}
		else
		if (wline.find(TEXT("DeviceName")) != std::wstring::npos)
		{
			g_DeviceName = ReturnParameter(wline);	
			if (g_DeviceName.size() == 0)
				g_DeviceName.assign(TEXT("Scanner"));
		}
		else
		if (wline.find(TEXT("ExchButtonKey")) != std::wstring::npos)
		{
			std::wstring tmpn = ReturnParameter(wline);
			SetKeyCode(hwnd, hBtnExch, tmpn, g_ExchButtonKey);
		}
		else
		if (wline.find(TEXT("ClrButtonKey")) != std::wstring::npos)
		{
			std::wstring tmpn = ReturnParameter(wline);
			SetKeyCode(hwnd, hBtnClr, tmpn, g_ClrButtonKey);
		}
		else
		if (wline.find(TEXT("ExitButtonKey")) != std::wstring::npos)
		{
			std::wstring tmpn = ReturnParameter(wline);
			SetKeyCode(hwnd, hBtnExit, tmpn, g_ExitButtonKey);
		}
		else
		if (wline.find(TEXT("RecoveryMode")) != std::wstring::npos) //2.2.2
		{
			g_RecoveryMode = _wtoi(ReturnParameter(wline).c_str());	
			if (g_RecoveryMode > 1)
				g_RecoveryMode = 0;
		}
		else
		if (wline.find(TEXT("DeleteUpdate")) != std::wstring::npos)
		{
			g_DeleteUpdate = _wtoi(ReturnParameter(wline).c_str());	
			if (g_DeleteUpdate > 1)
				g_DeleteUpdate = 0;
		}
		//2.2.3
		else
		//if ((wline.find(TEXT("BarCode128Prefix")) != std::wstring::npos) 
		if (CompareParameter(wline, std::wstring(TEXT("BarCode128Prefix"))) == 0)
		{
			//устанавливаем префикс для ШК Code128
			g_BarCode128Prefix = ReturnParameter(wline);
			BOOL bRes = SetPrefix(hwnd, g_BarCode128Prefix, g_BarCodeWeightPrefix, TEXT("9"));

			if (bRes == FALSE)
			{
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
				MessageBox(hwnd,LoadMsg(IDS_ERR_CODE128_PREFIX, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_FAILURE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
			}
		}
	}

	ini_file.close();

	return 1; 
}

void SetButtonText(HWND Btn, std::wstring str2add)
{
	std::wstring tmps;
	if (Btn == hBtnExch)
		tmps.assign(LoadMsg(IDS_BUTTON_EXCH, szMsgBuf, countof(szMsgBuf)));
	else
	if (Btn == hBtnClr)
		tmps.assign(LoadMsg(IDS_BUTTON_CLEAR, szMsgBuf, countof(szMsgBuf)));
	else
	if (Btn == hBtnExit)
		tmps.assign(LoadMsg(IDS_BUTTON_EXIT, szMsgBuf, countof(szMsgBuf)));

	tmps.append(TEXT("["));
	tmps.append(str2add);
	tmps.append(TEXT("]"));
	SetWindowText(Btn, tmps.c_str());
}

//----------------------------------------------------------------------------
// ScanEnableDecoder
//----------------------------------------------------------------------------

DWORD ScanEnableDecoder(HWND hwnd, LPDECODER lpDecoder, int DecodersCount)
{
	DWORD dwResult;

	//устанавливаем следующее значение
	SI_SET_FIELD(&DecoderList, Decoders.byList[DecodersCount-1], (BYTE)*lpDecoder);
	DecoderList.Decoders.dwDecoderCount = DecodersCount;
	dwResult = SCAN_SetEnabledDecoders(hScanner,&DecoderList);

	if (dwResult != E_SCN_SUCCESS)
		ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_SCAN_PARAM);

	return(dwResult);
}

//Если передан не весовой штрих-код, то возвращаем WeightReturn
//Если - весовой
//и если тип EAN13, то вычисляем его
double GetWeightFromBarCode(WCHAR * BarCode, DWORD BCType, double WeightReturn)
{
	WCHAR * curWeight = new WCHAR[5];
	WCHAR * Weight2 = new WCHAR[2];
	WCHAR * Weight3 = new WCHAR[3];

	if (wcslen(BarCode) == 0)
		return WeightReturn;

	if (BCType != LABELTYPE_EAN13)
		return WeightReturn;
	
	if (wcsncmp(g_BarCodeWeightPrefix.c_str(), BarCode, 1) != 0)
		return WeightReturn;

	wcsncpy(Weight2, &BarCode[7], 2);
	wcsncpy(Weight3, &BarCode[9], 3);
	wcscpy(curWeight,Weight2);
	wcscat(curWeight, TEXT("."));
	wcscat(curWeight,Weight3);

	return wcstod(curWeight, NULL);
}

//Выводит строку итого
void SetSum()
{
	TCHAR		lpSums[MAX_LOADSTRING];	
	double		dSumWeight = 0;
	int			dSumCount = 0;
	size_t		iSpaces = 0;//2.0.4
	LVCOLUMN	LvCol; //2.2.1
	int			ItemCount; //2.2.1

	//Получаем количество строк
	ItemCount=SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL);

	memset(&LvCol,0,sizeof(LvCol)); // Reset Column
	LvCol.mask=LVCF_TEXT; // тип по маске
	
	if (ItemCount == 0)
		LvCol.pszText=L"Штрих-код";
	else
	{
		wsprintf(lpSums, L"Строк: %.0d", ItemCount);
		LvCol.pszText=lpSums;
	}
	//Устанавливаем новый заголвок
	SendMessage(hListBox,LVM_SETCOLUMN,0,(LPARAM)&LvCol); //2.2.1

	for (int x = 0; x < (int)bar_codes_scanned.size(); x++)
	{
		dSumWeight += bar_codes_scanned[x].GetWeight();

		if (bar_codes_scanned[x].GetBarCodeType() != LABELTYPE_CODE128) //кроме CODE128 //2.1
			dSumCount += 1;
	}

	wsprintf(lpSums, LoadMsg(IDS_SUMS, szMsgBuf, countof(szMsgBuf)), 
		dSumWeight, //общий вес
		dSumCount);//общее количество

	if ((g_ScreenWidth >= 320) && (g_ScreenHeight >= 320))//2.0.4
		iSpaces = 28;
	else
		iSpaces = 24;//25 //2.2.1

	while (wcslen(lpSums) < iSpaces)
		wcscat(lpSums, TEXT(" "));
	if (wcslen(lpSums) == iSpaces)
		wcscat(lpSums, LoadMsg(IDS_AMOUNT, szMsgBuf, countof(szMsgBuf)));//2.0.4
	SetWindowText(hStatCur, lpSums);
}

//проверяет обновления на сервере и загружает
//-1 - не успешная загрузка с сервера 
//1 - успешно 
//2.1 - второй параметр - откуда загружать обновление
int UpdateBC(HWND hwnd, BOOL UpdSource)
{
	TCHAR szToCopy[MAX_PATH+1] = {0},
		  szUpdBCFile[MAX_PATH+1] = {0},
		  szBCFile[MAX_PATH+1] = {0};
	
	BOOL UpdFileCopied = FALSE;

	wcscpy(szToCopy, szAppPath);
	wcscat(szToCopy, TEXT("terminalc.upd"));

	if (UpdSource == DOWNLOAD_UPDATE_SERVER)
	{
		SetWindowText(hEditDesc, LoadMsg(IDS_SERVER_LOAD, szMsgBuf, countof(szMsgBuf)));
		UpdateWindow(hEditDesc);

		wcscpy(szUpdBCFile, wcsdup(g_ServerFolder.c_str()));
		wcscat(szUpdBCFile, LoadMsg(IDS_TERMINALC_DB, szMsgBuf, countof(szMsgBuf)));

		//проверка существования файла с обновлениями на сервере
		if(GetFileAttributes(szUpdBCFile) == 0xFFFFFFFF)
		{
			return 0; //обновлений нет
		}
		else //файл на сервере есть
		{
			SetWindowText(hEditDesc, LoadMsg(IDS_COPYING_DB, szMsgBuf, countof(szMsgBuf)));
			UpdateWindow(hEditDesc);

			if (!CopyFile(szUpdBCFile,szToCopy,false))
			{
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);

				MessageBox(hwnd,LoadMsg(IDS_ERR_LOAD_SERVER, szMsgBuf, countof(szMsgBuf)), 
				LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
				MB_OK | MB_ICONERROR);
				return -1; //ошибка копирования
			}
			else
				UpdFileCopied = TRUE;
		}
	}
	else
	{
		wcscpy(szUpdBCFile, szAppPath);
		wcscat(szUpdBCFile, TEXT("terminalc.upd"));
		//проверка существования файла с обновлениями на терминале
		if(GetFileAttributes(szUpdBCFile) == 0xFFFFFFFF)
			return 0; //обновлений нет
	}
	
	//загружаем из terminalc.upd, если файл был скопирован по сети или при подсоединении к компьютеру
	SetWindowText(hEditDesc, LoadMsg(IDS_LOADING_DB, szMsgBuf, countof(szMsgBuf)));
	UpdateWindow(hEditDesc);
	//если скопировали файл, то удаляем данные из bar_codes и загружаем их вновь, 
	//все равно это же надо делать для сравнения
	if (LoadDBFile(hwnd, FALSE, TEXT("terminalc.upd")))
	{
		//если загрузили удачно, то переименовываем terminalc.upd
		wcscpy(szBCFile, szAppPath);
		wcscat(szBCFile, LoadMsg(IDS_TERMINALC_DB, szMsgBuf, countof(szMsgBuf)));
		DeleteFile(szBCFile);
		//MoveFile(szToCopy,szBCFile);
		if ((MoveFile(szToCopy,szBCFile) != 0) && (g_DeleteUpdate == 1) && (UpdSource == DOWNLOAD_UPDATE_SERVER))
			DeleteFile(szUpdBCFile);//если переименовали нормально, то удаляем terminalc.db на сервере

		//в режиме редактирования новых ШК загружаем БД новых ШК
		if (g_NewBarCodes == 1) 
		{
			SetWindowText(hEditDesc, LoadMsg(IDS_LOADING_NEW_DB, szMsgBuf, countof(szMsgBuf)));
			UpdateWindow(hEditDesc);
			
			g_NewBCCount = 0;
			new_bar_codes.clear();
			//добавляем в основную БД
			LoadDBFile(hwnd, TRUE, LoadMsg(IDS_TERMINALC_NEW, szMsgBuf, countof(szMsgBuf)));
			g_NewBCCount = new_bar_codes.size();
		}
	}
	else
	{
		if (UpdFileCopied)	//если файл был скопирован, а открыть не удалось
		{
			PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
			MessageBox(hwnd,LoadMsg(IDS_ERR_OPEN_UPD, szMsgBuf, countof(szMsgBuf)), 
				LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
				MB_OK | MB_ICONERROR);
		}
		return -2; //ошибка открытия
	}
	
	return 1; //обновлено
}

//копирование данных на сервер
int SendBC(HWND hwnd)
{
    HANDLE fileHandle;
    DWORD dwError = ERROR_SUCCESS;
	WIN32_FIND_DATA findData;
	TCHAR szSaveFileMask[MAX_PATH+1] = {0},
		  szFromCopy[MAX_PATH+1] = {0},
		  szToCopy[MAX_PATH+1] = {0},
		  tcTemp[MAX_LOADSTRING];
	int AmountFilesToCopy = 0;

	//формируем маску
	wcsncpy(szSaveFileMask, szAppName, wcslen(szAppName) - 13);	
	
	wcscat(szSaveFileMask, GetStringBeforeChar(g_ScannedFile, 37).c_str()); //%
	wcscat(szSaveFileMask, TEXT("*"));
	wcscat(szSaveFileMask, g_ScannedFile.substr(g_ScannedFile.find(TEXT("."))).c_str());

	fileHandle = FindFirstFile(szSaveFileMask, &findData);
	// If no file was found
	if ( fileHandle == INVALID_HANDLE_VALUE )
		return -1;

	do
	{	//директория
		if ( ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ))
			continue;

		wcscpy(szFromCopy, szAppPath);
		wcscat(szFromCopy, findData.cFileName);

		wcscpy(szToCopy, wcsdup(g_ServerFolder.c_str()));
		wcscat(szToCopy, findData.cFileName);

		wsprintf(tcTemp, LoadMsg(IDS_SAVING_SERVER, szMsgBuf, countof(szMsgBuf)),findData.cFileName);
		SetWindowText(hEditDesc, tcTemp);
		UpdateWindow(hEditDesc);

		if (CopyFile(szFromCopy,szToCopy,false))
		{
			AmountFilesToCopy++;
			if (DeleteFile(szFromCopy) == 0)
			{
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
				wsprintf(tcTemp, LoadMsg(IDS_ERR_DELETE, szMsgBuf, countof(szMsgBuf)),findData.cFileName);
				SetWindowText(hEditDesc, tcTemp);
				UpdateWindow(hEditDesc);
			}
		}
		else
		{
			PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
			MessageBox(hwnd,LoadMsg(IDS_ERR_SAVE_SERVER, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
			SetWindowText(hEditDesc, LoadMsg(IDS_ERR_SAVE_SERVER, szMsgBuf, countof(szMsgBuf)));
			UpdateWindow(hEditDesc);
			return -1;
		}
			
	} while ( FindNextFile(fileHandle, &findData) );
	

	return AmountFilesToCopy;
}

//сохранение данных
bool SaveBC(HWND hwnd)
{
	TCHAR			szSaveName[MAX_PATH+1] = {0},
					szSaveNewName[MAX_PATH+1] = {0},	//путь до файла на сервере
					tcTemp[MAX_LOADSTRING] = {0};
	size_t			posofper;							//позиция процента
	std::wstring	wsFileNameCopy;						//копия имени файла со временем
	SYSTEMTIME		systime;

	if (g_StrFromDevId.compare(GetEncTermNum(hwnd)) != 0) //возвращает закодированную строку
	{
		PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
		MessageBox(hwnd,LoadMsg(IDS_ERR_REG_SAVE, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONWARNING);
		return false;
	}

	//определяем путь до файла
	wcsncpy(szAppPath, szAppName, wcslen(szAppName) - 13);	//копируем путь в szAppPath
	wcsncpy(szSaveName, szAppPath, wcslen(szAppPath));		//копируем путь в szSaveName

	//присваиваем шаблон имени файла локальной переменной
	wsFileNameCopy.assign(g_ScannedFile);

	//получаем текущее время и дату
	GetLocalTime(&systime);
	
	posofper = wsFileNameCopy.find(TEXT("%t")); //2.2.1
	if (posofper != std::wstring::npos)
	{
		swprintf(tcTemp, TEXT("%02d%02d%02d"), systime.wHour, systime.wMinute, systime.wSecond);
		wsFileNameCopy.replace(posofper, 2, tcTemp); 
	}

	//вставляем дату
	posofper = wsFileNameCopy.find(TEXT("%d")); //2.2.1
	if (posofper != std::wstring::npos)
	{
		WCHAR * wpDt = new WCHAR[6];
		wcscpy(wpDt, TEXT("ddMMyy"));
		int sz = GetDateFormat(LOCALE_SYSTEM_DEFAULT, DATE_SHORTDATE, NULL, NULL, NULL, NULL);
		GetDateFormat(LOCALE_SYSTEM_DEFAULT, NULL, NULL, wpDt, tcTemp, sz);
		//swprintf(tcTemp, TEXT("%02d%02d%02d"), systime.wDay, systime.wMonth, systime.wYear-2000);
		wsFileNameCopy.replace(posofper, 2, tcTemp); 
		delete wpDt;
	}

	wcsncat(szSaveName, wsFileNameCopy.c_str(), wsFileNameCopy.length());//склеиваем путь и имя
	 
	std::ofstream save_file(szSaveName, ios_base::out | ios_base::trunc);

	if (save_file.fail())
    {
		PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
		MessageBox(hwnd,LoadMsg(IDS_ERR_SAVE, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
		
		return false;
	}
	//обновляем экран
	wsprintf(tcTemp, LoadMsg(IDS_SAVING, szMsgBuf, countof(szMsgBuf)),wsFileNameCopy.c_str());
	SetWindowText(hEditDesc, tcTemp);
	UpdateWindow(hEditDesc);
	
	//вставляем признак UNICODE
	save_file << UNICODE2ANSI(L"\xfeff");

	//копируем
	for (int x = 0; x < (int)bar_codes_scanned.size(); x++)
	{
		//преобразовываем double в текст
		swprintf(tcTemp, L"%.3f", bar_codes_scanned[x].GetWeight());

		if (g_NewBarCodes == 1) //если можно сканировать новые ШК, тогда выгружаем и наименование
		{
			save_file << UNICODE2ANSI(bar_codes_scanned[x].GetBC().c_str()) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcTemp) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(bar_codes_scanned[x].GetDesc())
					  << endl;
		}
		else
			save_file << UNICODE2ANSI(bar_codes_scanned[x].GetBC().c_str()) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcTemp) << UNICODE2ANSI(TEXT("\t"))
					  << endl;
	}
	save_file.close();

	//сохранение прошло успешно
	RecoveryModeDelete(hwnd);//2.2.2

	return true;
}

//сохранение terminal.new
bool SaveNewBC(HWND hwnd)
{
	TCHAR			szSaveName[MAX_PATH+1] = {0},
					tcTemp[MAX_LOADSTRING];
	

	if (g_StrFromDevId.compare(GetEncTermNum(hwnd)) != 0) //возвращает закодированную строку
		return false;
	
	////определяем путь до файла
	wcsncpy(szSaveName, szAppPath, wcslen(szAppPath));		//копируем путь в szSaveName
	wcscat(szSaveName, LoadMsg(IDS_TERMINALC_NEW, szMsgCaptionBuf, countof(szMsgCaptionBuf)));

	std::ofstream save_file(szSaveName, ios_base::out | ios_base::trunc);

	if (save_file.fail())
    {
		PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
		MessageBox(hwnd,LoadMsg(IDS_ERR_SAVE_NEW, szMsgBuf, countof(szMsgBuf)), 
						LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
						MB_OK | MB_ICONERROR);
		
		return false;
	}
	//обновляем экран
	wsprintf(tcTemp, LoadMsg(IDS_SAVING, szMsgBuf, countof(szMsgBuf)), LoadMsg(IDS_TERMINALC_NEW, szMsgCaptionBuf, countof(szMsgCaptionBuf)));
	SetWindowText(hEditDesc, tcTemp);
	UpdateWindow(hEditDesc);
	
	//вставляем признак UNICODE
	save_file << UNICODE2ANSI(L"\xfeff");

	std::set<bar_code_data>::iterator i;

	//копируем
	for (i = new_bar_codes.begin(); 
		i != new_bar_codes.end(); i++)
	//for (int x = 0; x < (int)new_bar_codes.size(); x++)
	{
		save_file << UNICODE2ANSI((*i).GetBC().c_str()) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI((*i).GetDesc()) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(TEXT("0"))
					  << endl;
	}

	save_file.close();

	return true;
}

//получить серийный номер терминала
bool GetTermNum(HWND hwnd)
{
	DWORD		dwResult;
	UNITID_EX	UnitId;
	
	SI_INIT(&UnitId);	// initialize the StructInfo field of UnitId
	
	wcscpy(g_szTermNum, TEXT(""));
		
	dwResult = RCM_Open();
	if (dwResult != E_RCM_SUCCESS)
	{
		ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_GET_SN_OPEN);
		return false;
	}
			
	dwResult = RCM_GetUniqueUnitIdEx(&UnitId);
	if (dwResult != E_RCM_SUCCESS)
	{
		ErrorExit(hwnd, IDS_FAILURE, IDS_ERR_GET_SN);
		return false;
	}
	//преобразование в строку
	BytesToHexStr(g_szTermNum, UnitId.byUUID, UnitId.StructInfo.dwUsed - sizeof(STRUCT_INFO)); 
	
	return true;
}

//закодировать серийный номер терминала
std::wstring GetEncTermNum(HWND hwnd)
{
	std::wstring sRet;
	HCRYPTPROV	hProv = NULL; 
	HCRYPTHASH	hHash = NULL; 
	HCRYPTKEY	hKey = NULL; 

	TCHAR szPasswordDecrypted[MAX_PATH+1] = _T("");
	TCHAR szKey[MAX_PATH+1] = _T("");

	std::wstring sPwdStr;

	std::wstring sRegName;
	sRegName.assign(g_RegistrationName);
	std::transform(sRegName.begin(), sRegName.end(), sRegName.begin(), (int(*)(int))toupper);
	sPwdStr.assign(sRegName);

	std::wstring sVer;
	sVer.assign(GetVersion());
	size_t pointPos = sVer.find_first_of(L".",0);
	sPwdStr.append(sVer.substr(0, pointPos));

	_TRACE(L"Encrypring %s", wcsdup(sPwdStr.c_str()));

	EncryptString(g_szTermNum, szPasswordDecrypted, _wcsdup(sPwdStr.c_str()));

	_TRACE(L"Ser num encrypted: %s",szPasswordDecrypted);

	return std::wstring(szPasswordDecrypted);
}

// проверка на существование devicec.id
// b_cr = 0 - проверить на существование, если есть, то считать в g_StrFromDevId
// 1 - создать и записать ID
bool DevId(HWND hwnd, int b_cr)
{
	TCHAR			szDeviceId[MAX_PATH+1] = {0};	//путь до id-файла
	
	_TRACE(L"Finding devicec.id");
	//определяем путь до файла
	wcsncpy(szAppPath, szAppName, wcslen(szAppName) - 13);	//копируем путь в szAppPath
	wcsncpy(szDeviceId, szAppPath, wcslen(szAppPath));		//копируем путь в sziniName
	wcsncat(szDeviceId, TEXT("devicec.id"), wcslen(TEXT("devicec.id")));//склеиваем путь и device.id

	if (b_cr == 0)
	{
		//2.2
		std::ifstream a_file(szDeviceId);

		if (a_file.fail())
		{
			_TRACE(L"Error opening devicec.id");
			return false;
		}

		//чтение
		_TRACE(L"Reading devicec.id");
		std::string line; 
		std::getline(a_file, line);
		if (line.empty())
		{
			_TRACE(L"devicec.id is empty");
			return false;
		}
		else
			g_StrFromDevId.assign(FromUtf8(line));
		_TRACE(L"ID from devicec.id: %s",wcsdup(g_StrFromDevId.c_str()));
		a_file.close();
		return true;

	}
	
	if(b_cr == 1)
	{
		std::wofstream a_file(szDeviceId, ios_base::out | ios_base::trunc);

		if (a_file.fail())
		{
			PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
			LoadMsg(IDS_ERR_SAVE_ID, szMsgBuf, countof(szMsgBuf));
			if (wcslen(szMsgBuf) == 0)
				wcscpy(szMsgBuf, TEXT("Error write devicec.id"));
			MessageBox(hwnd,szMsgBuf, 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_OK | MB_ICONERROR);
			_TRACE(szMsgBuf);
			return false;
		}

		a_file << g_szTermNum;//записали

		a_file.close();
		return true;
	}

	return false;
}


// возвращает весовой штрих-код, отформатированный с xxxxx вместо веса
WCHAR * BarCodeFormat(WCHAR * SourseStr, DWORD BCType)
{
	//запрещаем объединение ШК, если можно редактировать количество
	if (g_QuantityEditing == 1)
		return SourseStr;

	if (BCType != LABELTYPE_EAN13) //2.1
		return SourseStr;

	if (wcsncmp(g_BarCodeWeightPrefix.c_str(), SourseStr, 1) != 0)
		return SourseStr;


	WCHAR * xStr = new WCHAR[1];
	wcscpy(xStr, TEXT("x"));

	WCHAR * retStr = new WCHAR[13];
	wcscpy(retStr, TEXT(" "));
	wcsncpy(retStr, SourseStr, 1);
	wcscat(retStr, xStr);
	wcsncat(retStr, &SourseStr[2], 5);
	wcscat(retStr, xStr);
	wcscat(retStr, xStr);
	wcscat(retStr, xStr);
	wcscat(retStr, xStr);
	wcscat(retStr, xStr);
	wcscat(retStr, xStr);
	return retStr;
}


//----------------------------------------------------------------------------
//
//  FUNCTION:   TCExitWndProc(HINSTANCE, HINSTANCE, LPSTR, int)
//
//  PURPOSE:     оконнная процедура окна с информацией о штрих-коде
//
//  PARAMETERS:
//      hwnd     - handle to the dialog box. 
//      uMsg	 - specifies the message. 
//      wParam   - specifies additional message-specific information. 
//      lParam   - specifies additional message-specific information. 
//
//  RETURN VALUE:
//      (BOOL) return TRUE if it processed the message, and FALSE if it did not. 
//
//----------------------------------------------------------------------------

LRESULT CALLBACK TCExitWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//список контролов диалога редактирования ШК
	HWND		 hbtnOk,	
				 hbtnCancel,
				 hEditPwd,
				 hStatPwd;

	TCHAR		strTemp[MAX_LOADSTRING]={0};	//для преобразования числа в строку

    switch (uMsg)
    {

		case WM_INITDIALOG:
            {
				int	CurrentControlHeight, CurrentControlWidth;
				RECT cRect;
				GetClientRect(hwnd, &cRect);

				CurrentControlHeight = cRect.bottom;
				CurrentControlWidth = cRect.right;

				//Надпись Введите пароль для выхода
				hStatPwd = CreateWindow(TEXT("STATIC"), 
					LoadMsg(IDS_PWD_STATIC, szMsgBuf, countof(szMsgBuf)),
					WS_CHILD | WS_VISIBLE | SS_LEFT, 
					10, 10, CurrentControlWidth - 20 , int(CurrentControlHeight * 10 / 100), 
					hwnd, (HMENU)ID_PWD_STATIC, hInst, NULL);
				SetWindowFont(hStatPwd,g_hFont,TRUE);

				//поле для ввода пароля
				hEditPwd = CreateWindow(TEXT("EDIT"), 
					TEXT(""),
					//WS_TABSTOP - чтоб можно было перемещаться Tab, ES_MULTILINE - чтоб ловить нажатие на Enter
					WS_CHILD | WS_VISIBLE | ES_LEFT | WS_BORDER | ES_PASSWORD | WS_TABSTOP, //ES_READONLY 
					10, 16 + int(CurrentControlHeight * 10 / 100), CurrentControlWidth - 20 , int(CurrentControlHeight * 10 / 100), 
					hwnd, (HMENU)ID_PWD_EDIT, hInst, NULL);
				SetWindowFont(hEditPwd,g_hFont,TRUE);
				//назначаем новую оконную процедуру для обработки нажатий клавиш
				oldEditPwdProc = (WNDPROC) SetWindowLong(hEditPwd, GWL_WNDPROC, (LONG)TCExitEditPwdProc);

				//кнопки
				hbtnOk = CreateWindow(TEXT("BUTTON"), 
					LoadMsg(IDS_PWD_BTNOK, szMsgBuf, countof(szMsgBuf)), 
					WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON  | WS_TABSTOP, 
					//10, CurrentControlHeight - int(CurrentControlHeight * 20 / 100) - 10, int(CurrentControlWidth * 43 / 100), int(CurrentControlHeight * 20 / 100), hwnd, 
					10, CurrentControlHeight - int(CurrentControlHeight * 20 / 100) - 10, int((CurrentControlWidth - 26) / 2), int(CurrentControlHeight * 20 / 100), hwnd, 
					(HMENU)ID_PWD_OK, hInst, NULL);
				SetWindowFont(hbtnOk,g_hFont,TRUE);

				hbtnCancel = CreateWindow(TEXT("BUTTON"), 
					LoadMsg(IDS_PWD_BTNCANCEL, szMsgBuf, countof(szMsgBuf)), 
					WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON  | WS_TABSTOP, 
					//int(CurrentControlWidth * 43 / 100) + 30, CurrentControlHeight - int(CurrentControlHeight * 20 / 100) - 10, int(CurrentControlWidth * 43 / 100), int(CurrentControlHeight * 20 / 100), hwnd, 
					int((CurrentControlWidth - 26) / 2) + 16, CurrentControlHeight - int(CurrentControlHeight * 20 / 100) - 10, int((CurrentControlWidth - 20) / 2), int(CurrentControlHeight * 20 / 100), hwnd, 
					(HMENU)ID_PWD_CANCEL, hInst, NULL);
				SetWindowFont(hbtnCancel,g_hFont,TRUE);
            }
            return (INT_PTR)TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == ID_PWD_CANCEL)
            {
				
                EndDialog(hwnd, LOWORD(wParam));
                return TRUE;
            }
			if (LOWORD(wParam) == ID_PWD_OK)
            {
				HWND hEditPassword = GetDlgItem(hwnd, ID_PWD_EDIT);
				int cTxtLen = GetWindowTextLength(hEditPassword); 
				LPWSTR pszMem = (LPWSTR) VirtualAlloc((LPVOID) NULL, (DWORD) (cTxtLen + 1), MEM_COMMIT, PAGE_READWRITE); 
				GetWindowText(hEditPassword, pszMem, cTxtLen + 1); 

				if (wcscmp(pszMem, g_Password.c_str()) == 0)
				{
					EnableWindow(hEditPassword, false);
					UpdateWindow(hEditPassword);
					EndDialog(hwnd, LOWORD(wParam));
				}
				else
				{
					PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
					SetWindowText(hEditPassword, TEXT(""));
					SetFocus(hEditPassword);
				}
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, uMsg);
            return TRUE;
    }
    return FALSE;
}

//  доп. процедура для создания диалогового окна в памяти, по произвольным размерам и т.д.
//  Пример из http://msdn.microsoft.com/en-us/library/ms644996(VS.85).aspx#template_in_memory
//
//LPWORD lpwAlign ( LPWORD lpIn)
//{
//    ULONG ul;
//
//    ul = (ULONG) lpIn;
//    ul +=3;
//    ul >>=2;
//    ul <<=2;
//    return (LPWORD) ul;
//}

//  создание диалогового окна в памяти, по произвольным размерам и т.д.
//  Пример из http://msdn.microsoft.com/en-us/library/ms644996(VS.85).aspx#template_in_memory
//
LRESULT TCExitDialog(HINSTANCE hinst, HWND hwndOwner, LPWSTR lpcsCaption)
{
	HGLOBAL hgbl;
    LPDLGTEMPLATE lpdt;
    LPWORD lpw;
    LPWSTR lpwsz;
	LRESULT ret;
    
    hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
    if (!hgbl)
        return -1;
 
    lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);
 
    // Define a dialog box.
    lpdt->style = WS_POPUP | WS_BORDER | DS_MODALFRAME | WS_CAPTION;
    lpdt->cdit = 0;  // Количество элементов на форме, 
	//будем создавать их при инициализации диалога в оконной ф-ции,
	//потому что здесь не удлось задать им русские имена

	//размеры
    lpdt->x  = 10 * 4 / LOWORD(GetDialogBaseUnits ());//перевод из экранных единиц в пиксели по формуле
	lpdt->y  = 10 * 8 / HIWORD(GetDialogBaseUnits ());//
	lpdt->cx = (g_ScreenWidth - 20) * 4 / LOWORD(GetDialogBaseUnits ());//
	lpdt->cy = (g_ScreenHeight - 60) * 8 / HIWORD(GetDialogBaseUnits ());//

    lpw = (LPWORD) (lpdt + 1);
    *lpw++ = 0;   // no menu
    *lpw++ = 0;   // predefined dialog box class (by default)

    //заголовок окна
	for (lpwsz = (LPWSTR)lpw; *lpwsz++ = (WCHAR) *lpcsCaption++;);
    lpw = (LPWORD)lpwsz;

    GlobalUnlock(hgbl); 
    ret = DialogBoxIndirect(hinst, (LPDLGTEMPLATE) hgbl, 
        hwndOwner, (DLGPROC) TCExitWndProc); 
	
    DWORD ik = GetLastError();
    GlobalFree(hgbl); 
    return ret; 


}


//собственная оконная процедура для обработки нажатий клавиш на контроле Edit пароль
LRESULT CALLBACK TCExitEditPwdProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_KEYDOWN:
        {
            if ((VK_DOWN == wParam) || (VK_UP == wParam))
            {
                HWND hParent = GetParent(hEdit);
                SendMessage( hParent, msg, wParam, lParam);
				
				
				if (GetWindowTextLength(hEdit) > 0)
				{
					HWND hwndOk = GetDlgItem(hParent,ID_PWD_OK);
					SendMessage(hwndOk, BM_SETSTYLE, BS_DEFPUSHBUTTON , TRUE);  
					SendMessage(hwndOk,DM_SETDEFID, ID_PWD_OK, 0);  
					SetFocus(hwndOk);
				}
				else
				{
					HWND hwndCancel = GetDlgItem(hParent,ID_PWD_CANCEL);
					SendMessage(hwndCancel, BM_SETSTYLE, BS_DEFPUSHBUTTON , TRUE);  
					SendMessage(hwndCancel,DM_SETDEFID, ID_PWD_CANCEL, 0);  
					SetFocus(hwndCancel);
				}
				return 0;   // запрет обработки по-умолчанию
            }
        }
        break;
    //case WM_CHAR:
    //    if((VK_DOWN == wParam) || (VK_UP == wParam))
    //        return 0;       // запрет обработки по-умолчанию
    //    break;
    }
    return CallWindowProc(oldEditPwdProc, hEdit, msg, wParam, lParam);
}

//собственная оконная процедура для обработки нажатий клавиш на контроле Edit Наименование
LRESULT CALLBACK EditDescProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_KEYDOWN:
        {
            if (VK_RETURN == wParam)
            {
                HWND hParent = GetParent(hEdit);
                SendMessage( hParent, msg, wParam, lParam);
				UpdateBCDesc(TRUE);
                return 0;   // запрет обработки по-умолчанию
            }
			if (wParam == VK_UP)
			{
				SetFocus(hListBox);
				return 0;   // запрет обработки по-умолчанию
			}
			if (wParam == VK_DOWN)
			{
				SetFocus(hBtnExch);
				return 0;   // запрет обработки по-умолчанию
			}
	    }
        break;
    case WM_CHAR:
        if(VK_RETURN == wParam)
            return 0;       // запрет обработки по-умолчанию
        break;
    }
    return CallWindowProc(oldEditDescProc, hEdit, msg, wParam, lParam);
}

//собственная оконная процедура для обработки нажатий клавиш на контроле Edit Количество
LRESULT CALLBACK EditAmountProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_KEYDOWN:
        {
            if (VK_RETURN == wParam)
            {
                HWND hParent = GetParent(hEdit);
                SendMessage( hParent, msg, wParam, lParam);
				UpdateBCAmount(hParent);
                return 0;   // запрет обработки по-умолчанию
            }
			if (wParam == VK_UP)
			{
				SetFocus(hListBox);
				return 0;   // запрет обработки по-умолчанию
			}
			if (wParam == VK_DOWN)
			{
				if (IsWindowEnabled(hEditDesc))
					SetFocus(hEditDesc);
				else
					SetFocus(hBtnExch);
				return 0;   // запрет обработки по-умолчанию
			}
	    }
        break;
    case WM_CHAR:
        if(VK_RETURN == wParam)
            return 0;       // запрет обработки по-умолчанию
        break;
    }
    return CallWindowProc(oldEditAmountProc, hEdit, msg, wParam, lParam);
}


//bIsEnterPressed == TRUE, если нажат был Enter, то поле наименование сохраняется для этого ШК, и наименование редактировать нельзя
void UpdateBCDesc(BOOL bIsEnterPressed)
{
	if (IsWindowEnabled(hEditDesc))
	{
		//найти текущий ШК и проверить - новый ли он
		int				iSelected = 0;
		char			scannedBarCode[255]={0};
		LVITEM			LvItem;		//новая строка в таблице
		TCHAR			szMsgBuf[256];
									
		iSelected = SendMessage(hListBox,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
		if(iSelected == -1)
			return;

		//получаем в scannedBarCode ШК из строки iSelected
		memset(&LvItem,0,sizeof(LvItem));
		LvItem.mask=LVIF_TEXT;
		LvItem.iSubItem=0;
		LvItem.pszText=(LPWSTR)scannedBarCode;
		LvItem.cchTextMax=255;
		LvItem.iItem=iSelected;
        SendMessage(hListBox,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);

		//поиск первого подобного ШК среди отсканированных ШК
		std::wstring ws_scannedBarCode((LPWSTR)scannedBarCode);
		std::vector<bar_code_data_new>::iterator found_bar_code
		= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindScannedBC(ws_scannedBarCode, LABELTYPE_UNKNOWN));//не знаем тип ШК, поэтому попытаемся внутри FindScannedBC определить по длине ШК и первой цифре

		
		if (found_bar_code->IsNew == TRUE)
		{
			int cTxtLen = GetWindowTextLength(hEditDesc); 

			LPWSTR pszMem = (LPWSTR) VirtualAlloc((LPVOID) NULL, (DWORD) (cTxtLen + 1), MEM_COMMIT, PAGE_READWRITE); 
			GetWindowText(hEditDesc, pszMem, cTxtLen + 1); 
			
			//установим новое наименование и сборсим флаг "новый"
			bar_code_data_new bcd_found(found_bar_code->GetBC(),pszMem,found_bar_code->GetWeight(), !bIsEnterPressed, found_bar_code->GetBarCodeType());
			replace_if(bar_codes_scanned.begin(), bar_codes_scanned.end(), FindScannedBC(ws_scannedBarCode, found_bar_code->GetBarCodeType()), bcd_found);

			
			if (bIsEnterPressed)//сообщаем о сохранении, если нажали Enter
			{
				//в режиме редактирования новых ШК, 
				if (g_NewBarCodes == 1) 
				{	
					//заносим в БД только те, для которых было сохранено наименование
					bar_codes.insert(bar_code_data(bcd_found.GetBC(), bcd_found.GetDesc(), 0.0f));
					//заносим в БД новых ШК
					new_bar_codes.insert(bar_code_data(bcd_found.GetBC(), bcd_found.GetDesc(), 0.0f));
				}


				LoadMsg(IDS_DESC_SAVED, szMsgBuf, countof(szMsgBuf));
				wcscat(szMsgBuf,pszMem);
				SetWindowText(hEditDesc,szMsgBuf);
				//успешно пищим
				PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);
				EnableWindow(hEditDesc, false);
				SetFocus(hBtnExch);
			}
		}
		if ( hKeyLib != NULL ) //отключим при выходе alpha режим
			if (lpfnGetAlphaMode() == TRUE)
				lpfnSetAlphaMode(FALSE);
	}
}

//количество сохраняется для этого ШК
void UpdateBCAmount(HWND hwnd)
{
	//найти текущий ШК
	int				iSelected = 0;
	char			scannedBarCode[255]={0};
	LVITEM			LvItem;		//новая строка в таблице
	TCHAR			strTemp[MAX_LOADSTRING]={0};	//для преобразования числа в строку
	
	//Если нельзя редактировать кол-во, то выходим
	if (g_QuantityEditing == 0)
		return;

	iSelected = SendMessage(hListBox,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
	if(iSelected == -1)
		return;

	//получаем в scannedBarCode ШК из строки iSelected
	memset(&LvItem,0,sizeof(LvItem));
	LvItem.mask=LVIF_TEXT;
	LvItem.iSubItem=0;
	LvItem.pszText=(LPWSTR)scannedBarCode;
	LvItem.cchTextMax=255;
	LvItem.iItem=iSelected;
    SendMessage(hListBox,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);

	//поиск подобного ШК среди отсканированных ШК
	std::wstring ws_scannedBarCode((LPWSTR)scannedBarCode);

	//пропускаем редактирование количества для Code 128
	std::vector<bar_code_data_new>::iterator found_bar_code_for_update
		= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindUniqueBC(ws_scannedBarCode));

	//ШК не найден, но такого быть не должно //2.1
	if (found_bar_code_for_update == bar_codes_scanned.end())
		return;

	//запоминаем параметры найденного ШК //2.1
	DWORD foundBarCodeType = found_bar_code_for_update->GetBarCodeType();
	double foundBarCodeWeight = found_bar_code_for_update->GetWeight();

	//получаем введенное кол-во в cTxtLen
	int cTxtLen = GetWindowTextLength(hEditAmount); 
	LPWSTR pszMem = (LPWSTR) VirtualAlloc((LPVOID) NULL, (DWORD) (cTxtLen + 1), MEM_COMMIT, PAGE_READWRITE); 
	GetWindowText(hEditAmount, pszMem, cTxtLen + 1); 
	cTxtLen = _wtoi(pszMem);

	if(cTxtLen < 0)
		return;

	if ((foundBarCodeType == LABELTYPE_CODE128) && 
		(cTxtLen != 0)) //могли ввести 0, чтоб удалить ШК //2.1
		return;

	//количество таких штрих-кодов в коллекции
	int found_bar_code_count
	= (int)count_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindUniqueBC(ws_scannedBarCode));

	if(cTxtLen == 0)
	{
		nResult = MessageBox(hwnd, 
							LoadMsg(IDS_DELETE_BC, szMsgBuf, countof(szMsgBuf)), 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_YESNO | MB_ICONQUESTION);
		if (nResult == IDNO) 
			return;
	}
	SetWindowText(hEditAmount,(LPWSTR)cTxtLen);
	//установим новое количество
	//если меньше, чем найдено, то удаляем и наоборот	
	if (cTxtLen > found_bar_code_count)
	{
		std::vector<bar_code_data_new>::iterator found_bar_code
		= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindUniqueBC(ws_scannedBarCode));

		//заполняем локальную переменную - объект класса bar_code_data
		bar_code_data_new bcd_scanned(found_bar_code->GetBC(), found_bar_code->GetDesc(), found_bar_code->GetWeight(), FALSE, found_bar_code->GetBarCodeType()); 

		for (int icnt = 0; icnt < (cTxtLen - found_bar_code_count); icnt++)
		{
			//помещаем данные в глобальный вектор - список отсканированных ШК
			bar_codes_scanned.push_back(bcd_scanned);
		}
		RecoveryModeEntry(ws_scannedBarCode.c_str(), cTxtLen - found_bar_code_count, foundBarCodeWeight, foundBarCodeType, BC_ADD); //2.2.2
	}
	else
		if (cTxtLen < found_bar_code_count)
		{
			for (int icnt = 0; icnt < (found_bar_code_count - cTxtLen); icnt++)
			{
				std::vector<bar_code_data_new>::iterator found_bar_code
				= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindUniqueBC(ws_scannedBarCode));
				
				RecoveryModeEntry(found_bar_code->GetBC().c_str(), 1, found_bar_code->GetWeight(), found_bar_code->GetBarCodeType(), BC_REMOVE); //2.2.2
				
				bar_codes_scanned.erase(found_bar_code);
			}
			//RecoveryModeEntry(ws_scannedBarCode.c_str(), found_bar_code_count - cTxtLen, 0, 0, BC_REMOVE); //2.2.2
		}
		else
			return;

	//получаем элемент (количество) из списка
	LvItem.iSubItem=2;       //Позиция колонки

	if(cTxtLen == 0)//если обнуляем количество, надо удалить строку
	{
		SendMessage(hListBox,LVM_DELETEITEM,iSelected,0); 
				
		//Получаем количество строк
		if (SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL) == 0)
		{
			SetInitState();
			SetFocus(hBtnExch);
		}
	}
	else
	{
		//изменяем значение
		_itow(cTxtLen, strTemp, 10);
		LvItem.pszText=strTemp; //Текст для отображения
		LvItem.cchTextMax = wcslen(strTemp); //Длина текста
		SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 


		//получаем элемент (вес) из списка
		LvItem.iSubItem=1;       //Позиция колонки

		//изменяем значение
		swprintf(strTemp, L"%.3f", GetWeightFromBarCode((LPWSTR)scannedBarCode, foundBarCodeType, foundBarCodeWeight) * cTxtLen);
		LvItem.pszText=strTemp; //Текст для отображения
		LvItem.cchTextMax = wcslen(strTemp); //Длина текста
		SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 

		SetFocus(hListBox);
	}
	
	EnableWindow(hEditDesc, false); //а вдруг оно было активно? //2.1

	//успешно пищим
	PlayAudio(g_AudioInfo, POSITIVE_BEEPER_FREQUENCY, POSITIVE_BEEPER_DURATION);
	SetSum();
}

//проверка Wi-Fi
int IfWiFiAvailable(int TypeOfAction)
{
	BOOL					bStatus;
	S24_CHANNEL_QUALITY		ChannelQuality;
	DWORD					dwMaxWaitForComplete	= 5;
	DWORD					dwReturnedLength		= 0;
	DWORD					dwReturnCode			= 0xFFFF;

	if (TypeOfAction == LOAD_WIFI_LIBRARY)
	{
		// Load S24 DLL and initialize S24 Driver
		_TRACE(L"Loading S24 DLL and initialize S24 Driver");
		hWiFiLib = LoadLibrary(L"mudll.dll");
		if (hWiFiLib == NULL)
			hWiFiLib = LoadLibrary(TEXT("s24mudll.dll"));
		if (hWiFiLib == NULL)
		{
			_TRACE(L"Error load S24 DLL ");
			return -1;
		}

		fpS24DriverExtension = (FPS24DRIVEREXTENSION)GetProcAddress(hWiFiLib,L"S24DriverExtension");
		if(NULL == fpS24DriverExtension)
			return -4;
		fpS24DriverInit = (FPS24DRIVERINIT)GetProcAddress(hWiFiLib,L"S24DriverInit");
		if(NULL == fpS24DriverInit)
			return -2;
		bStatus = (fpS24DriverInit)();
		if (bStatus == FALSE)
			return -3;

		_TRACE(L"Get adapter MAC address");
		// Get adapter MAC address
		bStatus = (fpS24DriverExtension)(
						0,								// MAC Address
						S24_GET_ADAPTER_MAC_ADDRESS,	// Function code
						NULL,							// Client window
						0,								// User token
						dwMaxWaitForComplete,			// Synchronous timeout
						NULL,							// Input buffer
						0,								// Input buffer length
						byAdapterMacAddr,				// Output buffer
						S24_MAC_ADDR_LENGTH,			// Output buffer length
						&dwReturnedLength,				// Returned length
						&dwReturnCode);					// Return code
		if (dwReturnCode != 0)
			return -6;
	}

	if(TypeOfAction == RETURN_SIGNAL_STRENGTH && hWiFiLib != NULL) 
	{
		_TRACE(L"Get channel quality");
		bStatus = (fpS24DriverExtension)(
								  byAdapterMacAddr,					// MAC Address
								  S24_GET_CHANNEL_QUALITY,			// Function code
								  NULL,								// Client window
								  0,								// User token
								  dwMaxWaitForComplete,				// Synchronous timeout
								  NULL,								// Input buffer
								  0,								// Input buffer length
								  &ChannelQuality,					// Output buffer
								  sizeof(ChannelQuality),			// Output buffer length
								  &dwReturnedLength,				// Returned length
								  &dwReturnCode);
		if (dwReturnCode != 0)
			return -5;
		else
			return (100 - ChannelQuality.usPercentMissedBeacons);
	}
	return 0;
}

//для обработки нажатия клавиши Alpha
void LoadKeyBoardLib(HWND hwnd)
{
	if (hKeyLib != NULL)
		return;

	hKeyLib = LoadLibrary(TEXT("keybddr.dll"));
	if (hKeyLib != NULL)
	{
		lpfnRegisterKeyboard = (LPFNREGKEYBOARDNOTIFY)GetProcAddress(hKeyLib,REGKEYBOARDNOTIFY);
		lpfnUnregisterKeyboard = (LPFNUNREGKEYBOARDNOTIFY)GetProcAddress(hKeyLib,UNREGKEYBOARDNOTIFY);
		lpfnSetAlphaMode = (LPFNSETALPHAMODE)GetProcAddress(hKeyLib,SETALPHAMODE);
		lpfnGetAlphaMode = (LPFNGETALPHAMODE)GetProcAddress(hKeyLib,GETALPHAMODE);
		if ( ( lpfnRegisterKeyboard == NULL ) ||
		 ( lpfnUnregisterKeyboard == NULL ) ||
		 (lpfnSetAlphaMode == NULL) ||
		 (lpfnGetAlphaMode == NULL) )
		{
			FreeLibrary(hKeyLib);
			hKeyLib = NULL;
		}
		if ( hKeyLib != NULL )
		{
			// register for Keyboard Notification (Alpha mode)
			lpfnRegisterKeyboard(hwnd,UM_KEYBOARDNOTIFICATION);
	
			if (lpfnGetAlphaMode() == TRUE)
				lpfnSetAlphaMode(FALSE);
		}
	}
}


//для обработки нажатия клавиши Alpha
void UnLoadKeyBoardLib(HWND hwnd)
{
	if (hKeyLib != NULL)
	{
		lpfnUnregisterKeyboard(hwnd);
		FreeLibrary(hKeyLib);
		hKeyLib = NULL;
	}
}


//2.2
BOOL EncryptString(TCHAR* szPassword, TCHAR* szEncryptPwd, TCHAR *szKey)
{	
	BOOL bResult = TRUE;	
	HKEY hRegKey = NULL;	
	HCRYPTPROV hProv = NULL;	
	HCRYPTKEY hKey = NULL;
	HCRYPTKEY hXchgKey = NULL;	
	HCRYPTHASH hHash = NULL;	
	DWORD dwLength;

	_TRACE(L"Get handle to user default provider");
	if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))	
	{
		_TRACE(L"Create hash object");		
		if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))		
		{
			dwLength = sizeof(TCHAR)*_tcslen(szKey);
			_TRACE(L"Hash password string");		
			if (CryptHashData(hHash, (BYTE *)szKey, dwLength, 0))			
			{
				_TRACE(L"Create block cipher session key based on hash of the password");		
				if (CryptDeriveKey(hProv, ENCRYPT_ALGORITHM, hHash, CRYPT_EXPORTABLE, &hKey))				
				{
					_TRACE(L"Determine number of bytes to encrypt at a time");		
					dwLength = sizeof(TCHAR)*_tcslen(szPassword);					
					_TRACE(L"Allocate memory");		
					BYTE *pbBuffer = (BYTE *)malloc(dwLength);					
					if (pbBuffer != NULL)					
					{
						memcpy(pbBuffer, szPassword, dwLength);						
						_TRACE(L"Encrypt data");
						if (CryptEncrypt(hKey, 0, TRUE, 0, pbBuffer, &dwLength, dwLength)) 						
						{
							BytesToHexStr(szEncryptPwd, pbBuffer, dwLength);
						}	
						else						
						{							
							bResult = FALSE;						
						}						
						_TRACE(L"Free memory");
						free(pbBuffer);					
					}
					else					
					{						
						bResult = FALSE;					
					}
					_TRACE(L"Release provider handle");
					CryptDestroyKey(hKey);  				
				}				
				else				
				{
					_TRACE(L"Error during CryptDeriveKey");
					bResult = FALSE;				
				}			
			}			
			else			
			{
				_TRACE(L"Error during CryptHashData");				
				bResult = FALSE;			
			}
			_TRACE(L"Destroy session key");
			CryptDestroyHash(hHash); 
		
		}		
		else		
		{
			_TRACE(L"Error during CryptCreateHash");			
			bResult = FALSE;		
		}
		_TRACE(L"CryptReleaseContext");	
		CryptReleaseContext(hProv, 0);	
	}	
	return bResult;
}

void _TRACE(TCHAR * str, ...) //2.2.2
{
	if (g_Debug == 1)
	{
		TCHAR buf[MAX_PATH];
		
		if (!str)
			return;
			
		va_list args;
		va_start(args, str);
		_vsnwprintf(buf, (MAX_PATH-1), str, args);
		va_end(args); 
		#ifdef _DEBUG
			OutputDebugString(L"\n");
			OutputDebugString(buf);
		#endif 

		std::ofstream save_file(g_szDebugName, ios_base::out | ios_base::app);

		if (save_file.fail())
    		return;

		save_file << UNICODE2ANSI(buf)<< UNICODE2ANSI(L"\n");
		
		save_file.close();

		return;
	}
}

void SetKeyCode(HWND hwnd, HWND hwndBtn, std::wstring tmpn, int &iKeyCode)//2.2.2
{
	int tmpi  = _wtoi(tmpn.c_str());	
	if (tmpi > 0 && tmpi < 10)
	{
		LoadKeyBoardLib(hwnd); //выделяем память для драйвера клавиатуры
		iKeyCode = tmpi + ID_0_KEY;
		SetButtonText(hwndBtn, tmpn); 
	}
}

//
//LPCWSTR lpBC	- штрих-код
//int iQt		- количество
//double dWght	- вес
//DWORD dwBCType - тип
//int iAct		- добавить или удалить
//
void RecoveryModeEntry(LPCWSTR lpBC, int iQt, double dWght, DWORD dwBCType, int iAct) //2.2.2
{
	if (g_RecoveryMode == 1)
	{
		TCHAR	tcQt[MAX_STRING] = {0},
				tcType[MAX_STRING] = {0},
				tcWght[MAX_STRING] = {0};
		
		if (!lpBC)
			return;

		std::ofstream save_file(g_szRecoverName, ios_base::out | ios_base::app);

		if (save_file.fail())
    		return;

		//вставляем признак UNICODE
		//save_file << UNICODE2ANSI(L"\xfeff");

		//преобразовываем числа в текст
		swprintf(tcQt, L"%.0d", iQt);
		swprintf(tcType, L"%.0d", dwBCType);
		swprintf(tcWght, L"%.3f", dWght);


		if (iAct == BC_ADD)
			save_file << UNICODE2ANSI(lpBC) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcQt) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcType) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcWght)
					  << endl;
		else
			save_file << UNICODE2ANSI(lpBC) << UNICODE2ANSI(TEXT("\t-"))
					  << UNICODE2ANSI(tcQt) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcType) << UNICODE2ANSI(TEXT("\t"))
					  << UNICODE2ANSI(tcWght)
					  << endl;

		save_file.close();

		return;
	}
}

void RecoveryModeDelete(HWND hwnd) //2.2.2
{
	if (g_RecoveryMode == 0) 
		return;

	if(GetFileAttributes(g_szRecoverName) == 0xFFFFFFFF)
		return;

	if (DeleteFile(g_szRecoverName) == 0)
	{
		 DWORD dwError = GetLastError();
		  if(dwError == ERROR_FILE_NOT_FOUND)
			return;
		  else if(dwError == ERROR_PATH_NOT_FOUND)
			return;
		  else //if(dwError == ERROR_ACCESS_DENIED)
		  {
				PlayAudio(g_AudioInfo, NEGATIVE_BEEPER_FREQUENCY, NEGATIVE_BEEPER_DURATION);
				LoadMsg(IDS_ERR_DEL_RECFILE, szMsgBuf, countof(szMsgBuf));
				MessageBox(hwnd,szMsgBuf, 
							LoadMsg(IDS_APP_TITLE, szMsgCaptionBuf, countof(szMsgCaptionBuf)), 
							MB_OK | MB_ICONERROR);
		  }
	}
}

bool RecoveryModeRecover(HWND hwnd) //2.2.2
{
	int bcCount = 0;
	size_t iChCount;
	double dWeight;
	DWORD bcType;
	TCHAR strTemp[MAX_LOADSTRING]={0};
	LVITEM	LvItem;	
	int ItemCount;
	int iTemp = 0;

	std::ifstream rec_file(g_szRecoverName);

	if (rec_file.fail())
		return false;

	for(std::string line; std::getline(rec_file, line); )
	{
		SetWindowText(hEditDesc, LoadMsg(IDS_RECMODE, szMsgBuf, countof(szMsgBuf)));
		UpdateWindow(hEditDesc);

		//преобразуем в Unicode
		int wbuflen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, 0, 0);
		WCHAR * wbuf = new WCHAR[wbuflen];
		MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, wbuf, wbuflen);

		if (wbuf[0] == 65279)//первый символ в кодировке unicode 
			wcsncpy_s(wbuf, wbuflen-1, &wbuf[1], _TRUNCATE);

		if (wcslen(wbuf) <= 1)
			continue;

		//отделяем ШК
		std::wstring new_barcode_string(wbuf);
		iChCount = new_barcode_string.find(TEXT("\t"));
		if (iChCount == std::string::npos)
			continue;

		std::wstring new_barcode = new_barcode_string.substr(0,iChCount);
		new_barcode_string = new_barcode_string.substr(iChCount+1);

		//отделяем кол-во
		iChCount = new_barcode_string.find(TEXT("\t"));
		if (iChCount == std::string::npos)
			continue;

		bcCount = _wtoi(new_barcode_string.substr(0,iChCount).c_str());
		new_barcode_string = new_barcode_string.substr(iChCount+1);

		//отделяем тип и вес
		iChCount = new_barcode_string.find(TEXT("\t"));
		if (iChCount == std::string::npos)
			continue;

		bcType = _wtoi(new_barcode_string.substr(0,iChCount).c_str());
		dWeight = wcstod(new_barcode_string.substr(iChCount+1).c_str(), NULL);

		//поиск подобного ШК среди отсканированных ШК
		std::vector<bar_code_data_new>::iterator found_bar_code
		= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindUniqueBC(new_barcode));

		if (found_bar_code == bar_codes_scanned.end()) //не найдено
		{
			//заполняем локальную переменную - объект класса bar_code_data
			bar_code_data_new bcd_scanned(new_barcode, L"", GetWeightFromBarCode(wcsdup(new_barcode.c_str()), bcType, dWeight), TRUE, bcType); 
			//помещаем данные в глобальный вектор - список отсканированных ШК
			bar_codes_scanned.push_back(bcd_scanned);
		}
		else if (bcCount > 0) //добавить
		{
			//заполняем локальную переменную - объект класса bar_code_data
			bar_code_data_new bcd_scanned(found_bar_code->GetBC(), found_bar_code->GetDesc(), found_bar_code->GetWeight(), FALSE, found_bar_code->GetBarCodeType()); 
	
			for (int icnt = 0; icnt < bcCount; icnt++)
			{
				//помещаем данные в глобальный вектор - список отсканированных ШК
				bar_codes_scanned.push_back(bcd_scanned);
			}
		}
		else if (bcCount < 0) //удалить
		{
			for (int icnt = 0; icnt < -1*bcCount; icnt++)
			{
				std::vector<bar_code_data_new>::iterator found_bar_code
				= find_if (bar_codes_scanned.begin(), bar_codes_scanned.end(), FindUniqueBC(new_barcode));

				bar_codes_scanned.erase(found_bar_code);
			}
		}
		//заполнить LV
		
		//Будем искать этот ШК в LV
		LVFINDINFO info = {0};
		info.flags=LVFI_STRING;
		info.psz=BarCodeFormat(wcsdup(new_barcode.c_str()), bcType);//если весовой, то в формате x xxxxxx

		
		memset(&LvItem,0,sizeof(LvItem)); //Обнуление памяти
		LvItem.mask=LVIF_TEXT;   //стиль текст
		//поиск ШК в уже остканированном
		ItemCount = SendMessage(hListBox, LVM_FINDITEM, -1, (LPARAM) (const LVFINDINFO *)&info);
		if (ItemCount == -1)//если не нашли
		{
			//Получаем количество строк
			ItemCount=SendMessage(hListBox,LVM_GETITEMCOUNT,NULL,NULL);

			
			//Добавляем в первую колонку ШК
			LvItem.iItem=ItemCount;  //Позиция строки 
			LvItem.iSubItem=0;       //Позиция колонки
			LvItem.cchTextMax = new_barcode.length(); //Длина текста
			LvItem.pszText=BarCodeFormat(wcsdup(new_barcode.c_str()), bcType); 
			//LvItem.pszText=scannedBarCode; //Текст для отображения
			SendMessage(hListBox,LVM_INSERTITEM,0,(LPARAM)&LvItem); //Устанавливаем значение во второй колонке уже существующей строки

			//Добавляем во вторую колонку вес
			LvItem.iSubItem=1;       //Позиция колонки
			//преобразовываем double в текст
			swprintf(strTemp, L"%.3f", dWeight);
			LvItem.pszText = strTemp;
			LvItem.cchTextMax = wcslen((LPTSTR)"1"); //Длина текста
			SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 

			//Добавляем в третью колонку штуки
			LvItem.iSubItem=2;       //Позиция колонки
			if (bcType == LABELTYPE_CODE128)
				LvItem.pszText=(LPTSTR)"0"; //Текст для отображения
			else
				LvItem.pszText=(LPTSTR)"1"; //Текст для отображения
			LvItem.cchTextMax = wcslen((LPTSTR)"1"); //Длина текста
			SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 

			//помещаем в количество отсканированных ШК в поле для редактирования
			//iTemp = 1;
		}
		else //если нашли
		if (bcType != LABELTYPE_CODE128)
		{				
			//получаем элемент (количество) из списка
			LvItem.iItem=ItemCount;  //Позиция строки 
			LvItem.iSubItem=2;       //Позиция колонки
			LvItem.pszText=strTemp; //Текст для отображения
			LvItem.cchTextMax = 10; //Длина текста
			SendMessage(hListBox,LVM_GETITEMTEXT, ItemCount, (LPARAM)&LvItem);
			
			//увеличиваем значение
			iTemp = _wtoi(strTemp)+bcCount;
			if (iTemp == 0)
				SendMessage(hListBox,LVM_DELETEITEM,ItemCount,0); 
			else
			{
				_itow(iTemp, strTemp, 10);
				LvItem.pszText=strTemp; //Текст для отображения
				LvItem.cchTextMax = wcslen(strTemp); //Длина текста
				SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 
			

				//получаем элемент (вес) из списка
				LvItem.iItem=ItemCount;  //Позиция строки 
				LvItem.iSubItem=1;       //Позиция колонки
				//LvItem.pszText=strTemp; //Текст для отображения
				//LvItem.cchTextMax = 10; //Длина текста
				//SendMessage(hListBox,LVM_GETITEMTEXT, ItemCount, (LPARAM)&LvItem);
				
				//увеличиваем значение
				swprintf(strTemp, L"%.3f", iTemp*dWeight);
				LvItem.pszText=strTemp; //Текст для отображения
				LvItem.cchTextMax = wcslen(strTemp); //Длина текста
				SendMessage(hListBox,LVM_SETITEM,0,(LPARAM)&LvItem); 
			}
		}

		//Фокусируемся на добавленой строке
		ListView_SetItemState(hListBox, -1, 0, LVIS_SELECTED); //Снять выделение
		SendMessage(hListBox,LVM_ENSUREVISIBLE,(WPARAM)ItemCount,FALSE); //Становимся на добавленую строку
		ListView_SetItemState(hListBox,ItemCount,LVIS_SELECTED ,LVIS_SELECTED); //Выделяем
        ListView_SetItemState(hListBox,ItemCount,LVIS_FOCUSED ,LVIS_FOCUSED); //Фокусируемся
		
		//пример использования static_cast, когда надо привести экземпляр класса-наследника
		//к базовому классу. static_cast НЕ возвращает 0, если преобразоание завершилось неудачей,
		//в отличие от dynamic_cast
		//http://www.find-info.ru/doc/cpp/001/use-inheritance.htm
		/*bar_code_data	*bcd_scanned = new bar_code_data(wbuf);
		bar_code_data_new *bcdn = static_cast< bar_code_data_new* >(bcd_scanned);
		bcdn->IsNew = TRUE;
		//помещаем данные в глобальный вектор - список отсканированных ШК
		bar_codes_scanned.push_back(*bcdn);*/

		delete [] wbuf;
	}
	rec_file.close();

	SetSum();

	return true;
}