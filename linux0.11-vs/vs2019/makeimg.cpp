// makeimg.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "Shlwapi.h"

#pragma comment( lib , "Shlwapi.lib" )

#define BOOT_SEC_EXE		L"bootsec.exe"
#define SETUP_EXE			L"setup.exe"
#define SYSTEM_EXE			L"system.exe"

#define LINUX_0_11_IMAGE	L"linux.img"
#define LINUX_0_11_IMAGEA	 "linux.img"
#define S_1_44_M			(1440*1024)

#ifndef Add2Ptr
#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))
#endif

WCHAR WorkDir[MAX_PATH] = {0};

typedef struct _FILE_NAMES
{
	CONST CHAR* InputFile;
	CONST CHAR* OutputFile;
}FILE_NAMES;

FILE_NAMES FN_Map[] =
{
	{ "linux0.11-vs.map" , "linux0.11-map-for-bohcs.map" },
	{ "system.map"       , "system-map-for-bohcs.map"	 },
};


typedef enum _BINS
{
	BOOT_SEC,
	SETUP,
	SYSTEM
}BINS;

BOOLEAN
AppendDataToImg(
	PVOID	ImgBase ,
	BINS	ObjIndex
	)
{
	HANDLE					ObjHandle			= NULL;
	HANDLE					ObjFileMapping		= NULL;
	PVOID					ObjBase				= NULL;
	LPCWSTR					ObjName				= NULL;
	PIMAGE_DOS_HEADER		DosHeader			= NULL;
	PIMAGE_NT_HEADERS		NtHeader			= NULL;
	PIMAGE_SECTION_HEADER	SecHeader			= NULL;
	LONG					SecCnt				= 0;
	LONG					OffsetInImage		= 0;
	ULONG					RawDataOffset		= 0;
	WCHAR					ObjPath[MAX_PATH]	= {0};
	BOOLEAN					RetVal				= FALSE;

	if ( ImgBase == NULL )
	{
		printf( "AppendDataToImg.ImgBase is NULL , Inalid Parameter.\n" );
		return FALSE;
	}

	switch ( ObjIndex )
	{
	case BOOT_SEC: 

		SecCnt			= 1;
		OffsetInImage	= 0;
		ObjName			= BOOT_SEC_EXE;
		break;

	case SETUP:

		SecCnt			= 1;
		OffsetInImage	= 0x200;
		ObjName			= SETUP_EXE;
		break;

	case SYSTEM:

		SecCnt			= 3;
		OffsetInImage	= 0xA00;
		ObjName			= SYSTEM_EXE;
		break;

	default:
		printf( "AppendDataToImg Inalid Parameter.\n" );
		return FALSE;
	}

	if ( S_OK != StringCchPrintf( ObjPath , MAX_PATH-10 , L"%s\\%s" ,WorkDir,ObjName ) )
	{
		printf("StringCchPrintf Failed.ErrorCode %d.\n", GetLastError());
		return FALSE;
	}

	ObjHandle = CreateFile( ObjPath , 
							GENERIC_READ, 
							FILE_SHARE_READ , 
							NULL, 
							OPEN_EXISTING, 
							FILE_ATTRIBUTE_NORMAL, 
							NULL ); 

	if ( ObjHandle == INVALID_HANDLE_VALUE )
	{
		printf( "AppendDataToImg Can not CreateFile.ErrorCode %d.\n", GetLastError() );
		goto _Exit;
	}

	ObjFileMapping = CreateFileMapping( ObjHandle , NULL, PAGE_READONLY | SEC_COMMIT, 0, 0, NULL);

	if ( NULL == ObjFileMapping )
	{
		printf("CreateFileMapping Failed.ErrorCode %d.\n", GetLastError());
		goto _Exit;
	}

	ObjBase = MapViewOfFile( ObjFileMapping, FILE_MAP_READ, 0, 0, NULL );

	if ( ObjBase == NULL )
	{
		printf("ImgBase is NULL Failed.ErrorCode %d.\n", GetLastError());
		goto _Exit;
	}

	DosHeader = ( PIMAGE_DOS_HEADER ) ObjBase;

	if ( DosHeader->e_magic != 'ZM' )
	{
		printf("Invalid Input File.\n");
		goto _Exit;
	}

	NtHeader  = ( PIMAGE_NT_HEADERS )( (DWORD_PTR)ObjBase + DosHeader->e_lfanew );

	SecHeader = IMAGE_FIRST_SECTION( NtHeader );

	if ( NtHeader->FileHeader.NumberOfSections < SecCnt )
	{
		printf("AppendDataToImg Invalid Input File.\n" );
		goto _Exit;
	}

	if ( ObjIndex == SYSTEM )
	{
		for ( LONG Index = 0 ; Index < SecCnt ; Index++ )
		{
			memcpy(  Add2Ptr( ImgBase,OffsetInImage + ( SecHeader[Index].VirtualAddress & 0x3ffff ) ) ,
					 Add2Ptr( ObjBase,SecHeader[Index].PointerToRawData ),
									  SecHeader[Index].SizeOfRawData
				);
		}
	}
	else
	{
		memcpy(  Add2Ptr( ImgBase,OffsetInImage ) ,
				 Add2Ptr( ObjBase,SecHeader[0].PointerToRawData ),
								  SecHeader[0].SizeOfRawData
			);
	}

	RetVal = TRUE;

_Exit:

	if ( ObjBase )
	{
		UnmapViewOfFile( ObjBase );
		ObjBase = NULL;
	}

	if ( ObjFileMapping )
	{
		CloseHandle( ObjFileMapping );
		ObjFileMapping = NULL;
	}

	if ( (ObjHandle != NULL) && (ObjHandle != INVALID_HANDLE_VALUE) )
	{
		CloseHandle( ObjHandle );
		ObjHandle = NULL;
	}

	return RetVal;
}

int ConvertMapToBochsSym()
{
	FILE				*	InputFile;
	FILE				*	OutputFile;
	CHAR					LineBuf[1024];
	CHAR					CurWorkDir[MAX_PATH];
	CHAR					FileFullPath[MAX_PATH];
	BOOLEAN					BeginFlag = FALSE;
	LONG					LineCnt = 0;
	LONG					Result;
	CHAR					Address1[100];
	CHAR					Name[100];
	CHAR					Address2[100];
	CHAR					FileName[100];
	CHAR					OutBuf[200];

	if ( 0 == GetModuleFileNameA(NULL, CurWorkDir, MAX_PATH - 10) )
	{
		printf("GetModuleFileName Failed.ErrorCode %d.\n", GetLastError());
		return 0;
	}

	*strrchr(CurWorkDir,'\\') = 0;

	sprintf_s( FileFullPath , 150 , "%s\\%s" , CurWorkDir ,"system.map"  );

	if ( fopen_s( &InputFile , FileFullPath, "r" ) != 0 )
	{
		printf("无法打开文件\n");
		return 0;
	}

	sprintf_s( FileFullPath , 150 , "%s\\%s" , CurWorkDir , "system-map-for-bohcs.map" );

	if ( fopen_s( &OutputFile , FileFullPath, "w" ) != 0 ) 
	{
		fclose( InputFile );
		printf("无法打开输出文件\n");
		return 0;
	}

	while ( fgets( LineBuf, 1000 , InputFile ) ) 
	{
		LineCnt++;

		Result = sscanf_s( LineBuf, "%90s %90s %90s %90s", Address1,90, Name, 90,Address2, 90,FileName,90 );

		if ( Result != 4 )
		{
			continue;
		}

		if ( !BeginFlag )
		{
			if ( 0 == strcmp(Address1 ,"Address" ) && (0 == strcmp( Name , "Publics")) )
			{
				BeginFlag = TRUE;
			}
			continue;
		}

		if( ( Address1[0] != '0' ) || ( Name[0] == '?' ) || ( 0 == strcmp(Address2, "00000000" ) ) )
		{
			continue;
		}

		Address2[1] = 'x';

		sprintf_s( OutBuf , 150 , "%s %s" , Address2 , &Name[1] );

		fprintf( OutputFile, "%s\n",OutBuf );
	}

	// 关闭文件
	fclose( InputFile  );
	fclose( OutputFile );

	return 0;
}

int MakeImg()
{
	HANDLE		ImgFile				= NULL;
	HANDLE		ImgFileMapping		= NULL;
	PVOID		ImgBase				= NULL;
	LONG		Index				= BOOT_SEC;	
	WCHAR		ImgPath[MAX_PATH]	= {0};
	BOOLEAN		MakeSuccess			= FALSE;

	if ( PathFileExists( LINUX_0_11_IMAGE ) )
	{
		if ( !DeleteFile( LINUX_0_11_IMAGE ) )
		{
			printf( "%s Exists and Can not be delete.ErrorCode %d.\n", LINUX_0_11_IMAGEA, GetLastError() );
			return 0;
		}
	}

	if( 0 == GetModuleFileName( NULL, WorkDir, MAX_PATH-10 ) )
	{
		printf("GetModuleFileName Failed.ErrorCode %d.\n", GetLastError());
		return 0;
	}

	*wcsrchr(WorkDir,L'\\') = 0;

	if ( S_OK != StringCchPrintf( ImgPath , MAX_PATH-10 , L"%s\\%s" ,WorkDir,LINUX_0_11_IMAGE ) )
	{
		printf("StringCchPrintf Failed.ErrorCode %d.\n", GetLastError());
		return 0;
	}

	DeleteFile( ImgPath );

	ImgFile = CreateFile( ImgPath , 
						  GENERIC_READ|GENERIC_WRITE, 
						  FILE_SHARE_READ , 
						  NULL, 
						  CREATE_NEW, 
						  FILE_ATTRIBUTE_NORMAL, 
						  NULL ); 

	if ( ImgFile == INVALID_HANDLE_VALUE )
	{
		printf( "Can not CreateFile.ErrorCode %d.\n", GetLastError() );
		goto _Exit;
	}

	if ( SetFilePointer( ImgFile, S_1_44_M, NULL, FILE_BEGIN ) == INVALID_SET_FILE_POINTER )
	{
		printf("SetFilePointer Failed.ErrorCode %d.\n", GetLastError());
		goto _Exit;
	}

	if ( !SetEndOfFile( ImgFile ) )
	{
		printf( "SetEndOfFile Failed.ErrorCode %d.\n", GetLastError() );
		goto _Exit;
	}

	ImgFileMapping = CreateFileMapping( ImgFile, NULL, PAGE_READWRITE , 0, 0, NULL );
	
	if ( NULL == ImgFileMapping )
	{
		printf( "CreateFileMapping Failed.ErrorCode %d.\n", GetLastError() );
		goto _Exit;
	}

	ImgBase = MapViewOfFile( ImgFileMapping , FILE_MAP_ALL_ACCESS , 0, 0, NULL ); 

	if ( ImgBase == NULL)
	{
		printf("ImgBase is NULL Failed.ErrorCode %d.\n", GetLastError());
		goto _Exit;
	}

	memset( ImgBase , S_1_44_M , 0 );

	for ( ; Index <= SYSTEM ; Index++  )
	{
		if ( !AppendDataToImg( ImgBase , (BINS)Index ) )
		{
			printf( "AppendDataToImg Failed.Index %d.\n" , Index );
			goto _Exit;
		}
	}

	if ( !UnmapViewOfFile(ImgBase) ) 
	{
		printf("UnmapViewOfFile Failed.ErrorCode %d.\n", GetLastError());
	}

	printf( "Make linux.img Success.\n" );
	MakeSuccess = TRUE;

_Exit:

	if ( ImgFileMapping )
	{
		CloseHandle( ImgFileMapping );
		ImgFileMapping = NULL;
	}

	if( ( ImgFile != NULL ) && ( ImgFile != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( ImgFile );
		ImgFile = NULL;
	}

	if ( !MakeSuccess )
	{
		DeleteFile( ImgPath );
	}

	return 0;
}

int CopyFileToUpperDir()
{
	WCHAR		InputFile[MAX_PATH] = {0};
	WCHAR		OutputFile[MAX_PATH] = {0};

	CONST WCHAR * FileName[] =  {
								L"linux.img",
								L"system-map-for-bohcs.map",
								};

	for (LONG i=0; i < 2 ; i++ )
	{
		StringCchPrintf( InputFile ,  MAX_PATH-10 , L"%s\\%s" , WorkDir, FileName[i] );
		StringCchPrintf( OutputFile , MAX_PATH-10 , L"%s\\..\\..\\bins\\%s" , WorkDir, FileName[i] );

		DeleteFile(OutputFile);

		CopyFile(InputFile,OutputFile,TRUE);
	}

	return 0;
}

int main()
{
	MakeImg();
	ConvertMapToBochsSym();
	CopyFileToUpperDir();
	return 0;
}
