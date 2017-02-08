
#include "PipeCommuUtils.h"
#include "PipeUtils.h"
#include <process.h>
#include <iostream>
#include <string.h>
#include <memory>
#include <tchar.h>


PipeCommuUtils::PipeCommuUtils()
{
	m_dwStatus = THREAD_STATUS_RUN;
	m_hInstance = NULL;
	m_Handle[0] = 0;
	m_Handle[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
	memset(m_PipeName, 0, sizeof(WCHAR) * MAX_PATH);
}

PipeCommuUtils::~PipeCommuUtils()
{

}

DWORD PipeCommuUtils::GetStatus()
{
	return m_dwStatus;
}

DWORD PipeCommuUtils::SetStatus(DWORD dwStatus)
{
	m_dwStatus = dwStatus;
	return 0;
}

DWORD PipeCommuUtils::GetPipeName( LPWSTR lpPipeName, DWORD dwSize )
{
	if (lpPipeName == NULL && dwSize == 0)
	{
		return 1;
	}

	_tcscpy_s(lpPipeName, dwSize, m_PipeName);
	return 0;
}

DWORD PipeCommuUtils::SetCallbackFunc( CALLBACKFUNC lpCallbackFunc )
{
	m_CallbackFunc = lpCallbackFunc;
	return 0;
}

PCALLBACKFUNC PipeCommuUtils::GetCallbackFunc()
{
	return &m_CallbackFunc;
}

unsigned PipeCommuUtils::PipeWorkThread(void* lpParam)
{
	DWORD dwResult = 0;
	PINSTANCEINFO hInstance = (PINSTANCEINFO)lpParam;
	std::auto_ptr<PipeUtils> pipeUtils(hInstance->pipeUtils);
	PipeCommuUtils * commuUtils = hInstance->pipeCommu;

	char szReceive[MAX_PATH] = {0};
	DWORD dwSize = MAX_PATH;
	DWORD dwRet = pipeUtils->Receive(szReceive, &dwSize, 100000);
	if (dwRet != 0)
	{
		return 1;
	}

	if (NULL != commuUtils->GetCallbackFunc()->readFunc)
	{
		commuUtils->GetCallbackFunc()->readFunc(szReceive, &dwSize);
	}
	
	void* lpSendBuffer = NULL;
	DWORD dwSendSize = 0;
	if (NULL != commuUtils->GetCallbackFunc()->writeFunc)
	{
		commuUtils->GetCallbackFunc()->writeFunc(&lpSendBuffer, &dwSendSize);
	}

	if (dwSendSize == 0 && lpSendBuffer != NULL)
	{
		delete lpSendBuffer;
		lpSendBuffer = NULL;
	}

	if (lpSendBuffer == NULL)
	{
		lpSendBuffer = new char('e');
		dwSendSize = 1;
	}

	dwRet = pipeUtils->Send(lpSendBuffer, &dwSendSize, 100000);
	if (dwRet != 0)
	{
		dwResult = 2;
	}
	
	delete lpSendBuffer;

	_endthreadex(0);
	return dwResult;
}

unsigned PipeCommuUtils::PipeServerThread(void* lpParam)
{
	PipeCommuUtils* pipeCommuUtils = (PipeCommuUtils*)lpParam;
	WCHAR szPipeName[MAX_PATH] = {0};
	pipeCommuUtils->GetPipeName(szPipeName, MAX_PATH);

	while (1)
	{
		DWORD dwStatus = pipeCommuUtils->GetStatus();
		if (THREAD_STATUS_PAUSE == dwStatus)
		{
			Sleep(1000);
			continue;
		}

		if (THREAD_STATUS_STOP == dwStatus)
		{
			break;
		}

		PipeUtils* pipeServer = new PipeUtils();
		std::shared_ptr<PipeUtils>  a(new PipeUtils());
		
		if (a == NULL)
		{
			continue;
		}

		while (1)
		{
			HANDLE hTempHandle = pipeServer->CreateNewPipe(szPipeName);
			if (hTempHandle != INVALID_HANDLE_VALUE)
			{
				break;
			}

			WaitNamedPipe(PIPESERVERNAME, 100);
			Sleep(100);
		}

		DWORD dwRet = pipeServer->WaitforConnect();
		if (dwRet != 0)
		{
			delete pipeServer;
			Sleep(100);
			continue;
		}
		
		UINT nThreadID = 0;
		INSTANCEINFO instanceInfo = {0};
		instanceInfo.pipeUtils = pipeServer;
		instanceInfo.pipeCommu = pipeCommuUtils;
		HANDLE hThread = (HANDLE)_beginthreadex( NULL, 0, PipeWorkThread, (void*)&instanceInfo, CREATE_SUSPENDED, &nThreadID );

		if ( hThread )
		{
			ResumeThread(hThread);
		}

		CloseHandle(hThread); 
	}

	return 0;
}

DWORD PipeCommuUtils::StartPipeServer( LPCWSTR lpPipeName )
{
	if (lpPipeName == NULL)
	{
		_tcscpy_s(m_PipeName, MAX_PATH, PIPENAME);
	}
	else
	{
		DWORD dwLength = _tcslen(lpPipeName);
		if (dwLength > MAX_PATH - 1)
		{
			return 1;
		}

		_tcscpy_s(m_PipeName, MAX_PATH, PIPENAME);
	}


	UINT nThreadID = 0;
	m_hInstance = (HANDLE)_beginthreadex( NULL, 0, PipeServerThread, this, 0, &nThreadID );

	if ( NULL != m_hInstance )
	{
		return S_OK;
	}

	return E_FAIL;
}

DWORD PipeCommuUtils::StopPipeServer()
{
	SetStatus(THREAD_STATUS_STOP);
	CloseHandle(m_hInstance);
	m_hInstance = NULL;
	return 0;
}

DWORD PipeCommuUtils::StartClient( LPCWSTR lpPipeName )
{
	std::cout<<"client thread :"<<GetCurrentThreadId()<<std::endl;
	PipeUtils pipeClient;
	DWORD dwRet = pipeClient.Connect(lpPipeName);
	if (dwRet != 0)
	{
		std::cout<<"Connect fail"<<std::endl;
		return 1;
	}

	void* lpBuf = NULL;
	DWORD dwSize = 0;
	if (m_CallbackFunc.writeFunc != NULL)
	{
		m_CallbackFunc.writeFunc(&lpBuf, &dwSize);
	}

	if (lpBuf == NULL)
	{
		lpBuf = new char('n');
		dwSize = 1;
	}

	dwRet = pipeClient.Send(lpBuf, &dwSize, 100000);
	if (dwRet != 0)
	{
		std::cout<<"Send fail"<<std::endl;
	}

	char szReceive[MAX_PATH] = {0};
	dwSize = MAX_PATH;
	dwRet = pipeClient.Receive(szReceive, &dwSize, 100000);
	if (dwRet != 0)
	{
		std::cout<<"Send fail"<<std::endl;
	}
	
	if (m_CallbackFunc.readFunc != NULL)
	{
		m_CallbackFunc.readFunc(szReceive, &dwSize);
	}

	//这里需要断开连接
	pipeClient.Close();
	delete lpBuf;
	lpBuf = NULL;
	return 0;
}




