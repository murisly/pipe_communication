
#pragma once

#include "PipeUtils.h"
#include <windows.h>

#define		THREAD_STATUS_RUN		0
#define		THREAD_STATUS_PAUSE		1
#define		THREAD_STATUS_STOP		2

#define		PIPESERVERNAME			L"\\\\.\\pipe\\server_{2F39AD4F-25C8-42C7-A66E-06DEEBC0E0B1}"

typedef DWORD (*PipeReadFunc)(void* lpBuffer, LPDWORD dwSize);
typedef DWORD (*PipeWriteFunc)(void** lpBuffer, LPDWORD dwSize);
typedef DWORD (*PipeReadAndWrite)(void* lpReadBuffer, LPDWORD dwReadSize, void** lpWriterBuffer, LPDWORD dwWriteSize);

class PipeCommuUtils;

typedef struct _CALLBACKFUNC
{
	PipeReadFunc readFunc;
	PipeWriteFunc writeFunc;
}CALLBACKFUNC, *PCALLBACKFUNC;

typedef struct _INSTANCEINFO
{
	PipeUtils* pipeUtils;
	PipeCommuUtils* pipeCommu;
}INSTANCEINFO, *PINSTANCEINFO;

class PipeCommuUtils
{
private:
	DWORD  m_dwStatus;	  //server当前状态
	HANDLE m_hInstance;   //pipe instance
	CALLBACKFUNC m_CallbackFunc;  //设置的回调函数
	HANDLE m_Handle[2];
	WCHAR  m_PipeName[MAX_PATH];

public:
	PipeCommuUtils();
	~PipeCommuUtils();
	DWORD GetStatus();
	DWORD SetStatus(DWORD dwStatus);
	DWORD GetPipeName(LPWSTR lpPipeName, DWORD dwSize);
	DWORD SetCallbackFunc(CALLBACKFUNC lpCallbackFunc);
	PCALLBACKFUNC GetCallbackFunc();

	DWORD StartPipeServer(LPCWSTR lpPipeName = NULL);
	DWORD StopPipeServer();

	DWORD StartClient(LPCWSTR lpPipeName = NULL);

public:
	static unsigned WINAPI PipeServerThread(void* lpParam);
	static unsigned WINAPI PipeWorkThread(void* lpParam);
};