//--------------------------------------------------------------------
// FILENAME:		Hardware.h
//
// Copyright(c) 2010 Kozlov Vasiliy. All rights reserved.
//
//
//--------------------------------------------------------------------

#include <tchar.h>
#include <Wincrypt.h>
#include <ScanCAPI.h>
#include <audiocapi.h> //Symbol Audio Library
#include <RcmCAPI.h>
#include <dispcapi.h>
#include <kbdapi.h>

#pragma comment ( lib, "dispapi32.lib" )
#pragma comment ( lib, "rcm2api32.lib" )
//#pragma comment ( lib, "rcmapi32.lib" )// uncomment for MC1000 insted of "rcm2api32.lib"
#pragma comment ( lib, "ScnAPI32.lib" )
#pragma comment ( lib, "audioapi32.lib" )
#pragma comment ( lib, "commctrl.lib" )
#pragma comment ( lib, "coredll.lib" )
#pragma comment ( lib, "secchk.lib" )
#pragma comment ( lib, "ccrtrtti.lib" )