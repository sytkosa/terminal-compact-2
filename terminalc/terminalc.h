//--------------------------------------------------------------------
// FILENAME:		Terminalc.h
//
// Copyright(c) 2008-2010 Kozlov Vasiliy. All rights reserved.
//
//
//--------------------------------------------------------------------

// Global variables
HINSTANCE		hInst					= NULL;
int				g_ScreenWidth;								//������ ������
int				g_ScreenHeight;								//������ ������
HANDLE			hScanner				= NULL;
DECODER_LIST	DecoderList;								//������ ���������
LPSCAN_BUFFER	lpScanBuffer			= NULL;
TCHAR			szScannerName[MAX_PATH] = TEXT("SCN1:");	// default scanner name
DWORD			dwScanSize				= 7095;				// default scan buffer size	
DWORD			dwScanTimeout			= 2000;				// default timeout value (0 means no timeout)	
BOOL			bUseText				= TRUE;
BOOL			bRequestPending			= FALSE;
BOOL			bStopScanning			= FALSE;

BOOL			bVersionPC;									//PocketPC - TRUE
AUDIO_INFO		g_AudioInfo;								//"�����" ����������
BOOL			g_WiFiAvailable = FALSE;					//���� �� Wi-Fi � ���������

//��������� �� ini-�����
std::wstring	g_RegistrationName;							//��� ���
std::wstring	g_ServerFolder;								//����� �� �������
std::wstring	g_ScannedFile;								//��� ������������ �����
int				g_LockBar = 0;								//���������� ������ �����
std::wstring	g_BarCodeWeightPrefix;						//������� �������� ��
int				g_QuantityEditing = 0;						//����� �������������� ����������
int				g_NewBarCodes = 0;							//����������� ����������� � �������� � �� ����� ��
std::wstring	g_Password;									//������ �� �����
std::wstring	g_DeviceName;								//��� �������
int				g_ExchButtonKey = 0;						//������� ������� ��� ������ "�����"
int				g_ClrButtonKey = 0;							//������� ������� ��� ������ "��������"
int				g_ExitButtonKey = 0;						//������� ������� ��� ������ "�����"
int				g_DeleteUpdate = 0;							//������� ���� ���������� � ������� ����� �������� ���������� 
std::wstring	g_BarCode128Prefix;							//������� �� ���� Code 128 //2.2.1
int				g_Debug = 0;								//����� ���������� ���������� � g_Debug.txt //2.2.2
int				g_RecoveryMode = 0;							//����� ������ �� ����� //2.2.2

TCHAR			g_szDebugName[MAX_PATH+1] = {0};			//2.2.2
TCHAR			g_szRecoverName[MAX_PATH+1] = {0};			//2.2.2

HFONT			g_hFont = NULL;

int				g_NewBCCount = 0;							//������� ����� ��


//��� RecoveryMode //2.2.2
enum
{
	BC_ADD = 0,
	BC_REMOVE
};


//����� ��� terminalc.db
class bar_code_data
{
	
	std::wstring	barcode,	//�����-���
					desc;		//������������ ������������
	double			weight;		//��� ��� �������� ��
	int				length;		//����� ��	//2.0.3			

	public:
		//�����������
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
					length = iChCount; //2.2.1 - ����� ���� �� 1 �������
				}
				else if (desc.empty())
					desc = tmpWStr.substr(0,iChCount);
				else
					weight = wcstod(tmpWStr.substr(0,iChCount).c_str(), NULL);
				
				tmpWStr = new_barcode_string;
				
			}
		}

		//�����������
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


//�����-��������� � ��������� ������ �� (�.�. �� �� ��)
//��� �������� ��������������� ��
class bar_code_data_new:	public bar_code_data
{
	DWORD	BarCodeType;	//��� ����� ���� 

	public:
		BOOL	IsNew;			//������� ������

		//����������� ��������� ����������� �� �������� ������
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



//����� ��� ������ �����-����� � ��:
//����� �� �������� ���� ��, ��� ��� �������� ���������� �� ����� ��.
//����� ����� ���� 5-�������� �� ���� EAN13, ������� � ������ �������� ���������� � g_BarCodeWeightPrefix.
//Code128 � �� ��������� ������, �� ������������ ����� � ������ �� ���� 5-�������� �����.
//������ ��� ������������ ����� �� ��������� ��������, ���� ��� ���������� � 9, ����� ��� �� ���������.
//������������� g_BarCodeWeightPrefix �� ������ ���� ����� 9!
class FindDBBC
{
	std::wstring l_scannedBC;

	public:
		//����������� - ��� �������� �� ��������� � 5-7 �����
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

//����� ��� ������ �����-����� � ��������� ���������������:
//����� �� ����� ���� 5-�������� �� ���� EAN13, �.�. �� �������� � ��������� ��� ���� ������� ��������,
//�� ����� ��� �������� EAN13, ������� ���������� � g_BarCodeWeightPrefix, �� �� � 9, ���� ����������� �� 5 ������.
//� ��� ��� �� �� ����� ����� ���� ��, �� �����, ��� ������� �� ���������� � g_BarCodeWeightPrefix, �� �� � 9, 
//� ����� ����� 13 ����, �� ����� �� ������� �������.
//� 9 ����� ���������� ����� ��, �� Code128 ������ � 9��. ������������� �� ������� �� ����� ��������� ���������� ��� �������.
class FindScannedBC
{
	std::wstring l_scannedBC;

	public:
		//����������� - ��� �������� �� ��������� � 5-7 �����
		FindScannedBC (std::wstring scannedBC, DWORD bcType)
		{
			DWORD l_bcType = bcType;

			l_bcType = ((bcType == LABELTYPE_UNKNOWN) && (g_BarCodeWeightPrefix.compare(scannedBC.substr(0,1)) == 0) && (scannedBC.length() == 13)) ? LABELTYPE_EAN13 : bcType; //��� ������������� �� �� ��������

			if ((g_BarCodeWeightPrefix == scannedBC.substr(0,1)) &&
				(l_bcType == LABELTYPE_EAN13)) //��������� ��� ������ ������ ��� ������� �� EAN13
				l_scannedBC = scannedBC.substr(2,5);
			else
				l_scannedBC = scannedBC;
		};



		bool operator ()(bar_code_data_new& scannedBCData)
		{
			if ((scannedBCData.GetBarCodeType()== LABELTYPE_EAN13) &&
			(scannedBCData.GetBC().compare(0, 1, g_BarCodeWeightPrefix) == 0))//��������� ��� ������ ������ ��� ������� �� EAN13
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

//����� ��� ������ ���������� �����-�����
class FindUniqueBC
{
	std::wstring l_scannedBC;

	public:
		//�����������
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




//��������� ��� �������� ���������� �������� �� terminalc.db
std::set <bar_code_data> bar_codes;							

//��������� ��� �������� ���������� �������� �� terminalc.new
std::set <bar_code_data> new_bar_codes;							

//��������� ��� �������� ��������������� ��
std::vector <bar_code_data_new> bar_codes_scanned;							

