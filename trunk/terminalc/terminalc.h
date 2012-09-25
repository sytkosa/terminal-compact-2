//--------------------------------------------------------------------
// FILENAME:		Terminalc.h
//
// Copyright(c) 2008-2010 Kozlov Vasiliy. All rights reserved.
//
//
//--------------------------------------------------------------------

// Global variables
HINSTANCE		hInst					= NULL;
int				g_ScreenWidth;								//ширина экрана
int				g_ScreenHeight;								//высота экрана
HANDLE			hScanner				= NULL;
DECODER_LIST	DecoderList;								//список декодеров
LPSCAN_BUFFER	lpScanBuffer			= NULL;
TCHAR			szScannerName[MAX_PATH] = TEXT("SCN1:");	// default scanner name
DWORD			dwScanSize				= 7095;				// default scan buffer size	
DWORD			dwScanTimeout			= 2000;				// default timeout value (0 means no timeout)	
BOOL			bUseText				= TRUE;
BOOL			bRequestPending			= FALSE;
BOOL			bStopScanning			= FALSE;

BOOL			bVersionPC;									//PocketPC - TRUE
AUDIO_INFO		g_AudioInfo;								//"аудио" переменная
BOOL			g_WiFiAvailable = FALSE;					//есть ли Wi-Fi у терминала

//параметры из ini-файла
std::wstring	g_RegistrationName;							//рег имя
std::wstring	g_ServerFolder;								//папка на сервере
std::wstring	g_ScannedFile;								//имя сохраняемого файла
int				g_LockBar = 0;								//выключение панели задач
std::wstring	g_BarCodeWeightPrefix;						//префикс весового ШК
int				g_QuantityEditing = 0;						//режим редактирования количества
int				g_NewBarCodes = 0;							//возможность сканировать и заносить в БД новые ШК
std::wstring	g_Password;									//пароль на выход
std::wstring	g_DeviceName;								//имя сканера
int				g_ExchButtonKey = 0;						//горячая клавиша для кнопки "Обмен"
int				g_ClrButtonKey = 0;							//горячая клавиша для кнопки "Очистить"
int				g_ExitButtonKey = 0;						//горячая клавиша для кнопки "Выход"
int				g_DeleteUpdate = 0;							//удалять файл обновления с сервера после удачного обновления 
std::wstring	g_BarCode128Prefix;							//префикс ШК типа Code 128 //2.2.1
int				g_Debug = 0;								//вывод отладочной информации в g_Debug.txt //2.2.2
int				g_RecoveryMode = 0;							//режим защиты от сбоев //2.2.2

TCHAR			g_szDebugName[MAX_PATH+1] = {0};			//2.2.2
TCHAR			g_szRecoverName[MAX_PATH+1] = {0};			//2.2.2

HFONT			g_hFont = NULL;

int				g_NewBCCount = 0;							//счетчик новых ШК


//для RecoveryMode //2.2.2
enum
{
	BC_ADD = 0,
	BC_REMOVE
};


//класс для terminalc.db
class bar_code_data
{
	
	std::wstring	barcode,	//штрих-код
					desc;		//наименование номенклатуры
	double			weight;		//вес для весового ШК
	int				length;		//длина ШК	//2.0.3			

	public:
		//конструктор
		bar_code_data(std::wstring new_barcode_string)
		{
			size_t iChCount = 0; 
			std::wstring tmpWStr = new_barcode_string, wstab(TEXT("\t"));

			while (0 != new_barcode_string.length())
			{
				iChCount = new_barcode_string.find(wstab);
				if (iChCount == std::string::npos)
				{
					iChCount = new_barcode_string.length(); 
					new_barcode_string = new_barcode_string.substr(iChCount);
					
				}
				else
					new_barcode_string = new_barcode_string.substr(iChCount+1);
				

				if (barcode.empty())
				{
					barcode = tmpWStr.substr(0,iChCount);
					//length = iChCount + 1; //2.0.3
					length = iChCount; //2.2.1 - иначе было на 1 длиннее
				}
				else if (desc.empty())
					desc = tmpWStr.substr(0,iChCount);
				else
					weight = wcstod(tmpWStr.substr(0,iChCount).c_str(), NULL);
				
				tmpWStr = new_barcode_string;
				
			}
		}

		//конструктор
		bar_code_data(std::wstring ws_barcode, std::wstring ws_desc, double d_weight)
		{
			this->barcode = ws_barcode;
			this->length = ws_barcode.length();//2.0.3
			this->desc = ws_desc;
			this->weight = d_weight;
		}

		wstring GetBC()
		{
			return this->barcode;
		}
		LPCWSTR GetDesc()
		{
			return this->desc.c_str();
		}
		double GetWeight()
		{
			return this->weight;
		}
		int GetLength()//2.0.3
		{
			return this->length;
		}
};


bool operator < (bar_code_data first, bar_code_data second)
{
	return first.GetBC() < second.GetBC();
}


//класс-наследник с признаком нового ШК (т.е. не из БД)
//для хранения отсканированных ШК
class bar_code_data_new:	public bar_code_data
{
	DWORD	BarCodeType;	//тип штрих кода 

	public:
		BOOL	IsNew;			//признак нового

		//конструктор наследует конструктор из базового класса
		bar_code_data_new(std::wstring ws_barcode, std::wstring ws_desc, double d_weight, BOOL b_IsNew, DWORD d_BarCodeType) 
			: bar_code_data(ws_barcode, ws_desc, d_weight)
		{
			this->IsNew = b_IsNew;
			this->BarCodeType = d_BarCodeType;
		}
	
		DWORD GetBarCodeType()
		{
			return this->BarCodeType;
		}
};



//Класс для поиска штрих-кодов в БД:
//Здесь не известны типы ШК, так как загрузка происходит из файла БД.
//Здесь могут быть 5-изначные ШК типа EAN13, которые в полном варианте начинаются с g_BarCodeWeightPrefix.
//Code128 в БД указывать нельзя, но пользователь может и ввести их даже 5-изначной длины.
//Однако при сканировании таких ШК программа проверит, чтоб они начинались с 9, иначе она их пропустит.
//Следовательно g_BarCodeWeightPrefix не должен быть равен 9!
class FindDBBC
{
	std::wstring l_scannedBC;

	public:
		//конструктор - для весового ШК извлекает с 5-7 цифры
		FindDBBC (std::wstring scannedBC)
		{
			if ((scannedBC.length() == 13) &&  //2.1
				(g_BarCodeWeightPrefix == scannedBC.substr(0,1)))
				l_scannedBC = scannedBC.substr(2,5);
			else
				l_scannedBC = scannedBC;
		};



		bool operator ()(bar_code_data& scannedBCData)
		{
			if ((scannedBCData.GetBC().length() == 13) &&  //2.1
			(g_BarCodeWeightPrefix == scannedBCData.GetBC().substr(0,1)))
			{
				std::wstring substrcmp = scannedBCData.GetBC().substr(2,5);
				if (substrcmp.compare(l_scannedBC) == 0)
					return true;
				else
					return false;
			}
			else
			{
				if (scannedBCData.GetBC() == l_scannedBC)
					return true;
				else
					return false;
			}
		};
};

//Класс для поиска штрих-кодов в коллекции отсканированных:
//Здесь НЕ могут быть 5-изначные ШК типа EAN13, т.к. ШК попадают в коллекцию как были считаны сканером,
//но поиск для весового EAN13, который начинается с g_BarCodeWeightPrefix, но не с 9, надо производить по 5 цифрам.
//А так как мы не знаем здесь типа ШК, но знаем, что весовой ШК начинается с g_BarCodeWeightPrefix, но не с 9, 
//и имеет длину 13 цифр, то такой ШК считаем весовым.
//С 9 может начинаться любой ШК, но Code128 только с 9ки. Следовательно он никогда не будет трактован программой как весовой.
class FindScannedBC
{
	std::wstring l_scannedBC;

	public:
		//конструктор - для весового ШК извлекает с 5-7 цифры
		FindScannedBC (std::wstring scannedBC, DWORD bcType)
		{
			DWORD l_bcType = bcType;

			l_bcType = ((bcType == LABELTYPE_UNKNOWN) && (g_BarCodeWeightPrefix.compare(scannedBC.substr(0,1)) == 0) && (scannedBC.length() == 13)) ? LABELTYPE_EAN13 : bcType; //тип передаваемого ШК не известен

			if ((g_BarCodeWeightPrefix == scannedBC.substr(0,1)) &&
				(l_bcType == LABELTYPE_EAN13)) //извлекаем код товара только для весовых ШК EAN13
				l_scannedBC = scannedBC.substr(2,5);
			else
				l_scannedBC = scannedBC;
		};



		bool operator ()(bar_code_data_new& scannedBCData)
		{
			if ((scannedBCData.GetBarCodeType()== LABELTYPE_EAN13) &&
			(scannedBCData.GetBC().compare(0, 1, g_BarCodeWeightPrefix) == 0))//извлекаем код товара только для весовых ШК EAN13
			{
				if (scannedBCData.GetBC().compare(2,5,l_scannedBC) == 0)
					return true;
				else
					return false;
			}
			else
			{
				if (l_scannedBC.compare(scannedBCData.GetBC()) == 0)
					return true;
				else
					return false;
			}
		};
};

//класс для поиска уникальных штрих-кодов
class FindUniqueBC
{
	std::wstring l_scannedBC;

	public:
		//конструктор
		FindUniqueBC (std::wstring scannedBC)
		{
			l_scannedBC = scannedBC;
		};

		bool operator ()(bar_code_data& scannedBCData)
		{
			if (l_scannedBC.compare(scannedBCData.GetBC()) == 0)
				return true;
			else
				return false;
		};
};




//коллекция для хранения уникальных значений из terminalc.db
std::set <bar_code_data> bar_codes;							

//коллекция для хранения уникальных значений из terminalc.new
std::set <bar_code_data> new_bar_codes;							

//коллекция для хранения отсканированных ШК
std::vector <bar_code_data_new> bar_codes_scanned;							

