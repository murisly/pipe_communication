
#include "PipeUtils.h"
#include <tchar.h>

DWORD PipeUtils::ResetOverlapped()
{
	HANDLE hTmpEvent = m_Overlap.hEvent;
	memset(&m_Overlap, 0, sizeof(OVERLAPPED));
	m_Overlap.hEvent = hTmpEvent;
	return 0;
}

DWORD PipeUtils::ClearOverlapped()
{
	if ( m_Overlap.hEvent )
		CloseHandle(m_Overlap.hEvent);
	memset(&m_Overlap, 0, sizeof(OVERLAPPED));
	return 1;
}

DWORD PipeUtils::CreateOverlapped()
{
	int result = 0;

	if ( m_Overlap.hEvent )
	{
		ResetOverlapped();
	}
	else
	{
		memset(&m_Overlap, 0, sizeof(OVERLAPPED));
		m_Overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		result = ( m_Overlap.hEvent == NULL );
	}

	return result;
}

PipeUtils::PipeUtils()
{
	m_dwSendBufferLength = MAX_PATH;
	m_dwReceiveBufferLength = MAX_PATH;
	m_dwWaitTime = PIPEWAITTIME;
	_tcscpy_s(m_szPipeName, MAX_PATH, PIPENAME);
	memset(&m_Overlap, 0, sizeof(OVERLAPPED));
	CreateOverlapped();
}

PipeUtils::~PipeUtils()
{
	Close();
}

DWORD PipeUtils::Close()
{
	if (m_Overlap.hEvent)
	{
		SetEvent(m_Overlap.hEvent);
		CloseHandle(m_Overlap.hEvent);
		m_Overlap.hEvent = NULL;
	}

	if (m_hInstance != NULL)
	{
		CloseHandle(m_hInstance);
		m_hInstance = NULL;
	}
	
	return 0;
}

DWORD CreateSecurityAttributes(PSECURITY_ATTRIBUTES pSecurityAttribute, BOOL bNeedDescriptor = TRUE, BOOL bInheritHandle = FALSE)
{
	if ( !pSecurityAttribute )
		return 1;

	PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
	pSecurityAttribute->lpSecurityDescriptor = pSecurityDescriptor;
	pSecurityAttribute->bInheritHandle = bInheritHandle;
	pSecurityAttribute->nLength = sizeof(SECURITY_ATTRIBUTES);

	if ( !bNeedDescriptor )
	{
		return 0;
	}

	pSecurityDescriptor = LocalAlloc(LMEM_ZEROINIT, sizeof(SECURITY_DESCRIPTOR));
	if ( pSecurityDescriptor
		&& InitializeSecurityDescriptor(pSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION)
		&& SetSecurityDescriptorDacl(pSecurityDescriptor, TRUE, NULL, FALSE) )
	{
		pSecurityAttribute->lpSecurityDescriptor = pSecurityDescriptor;
		return 0;
	}
	return 2;
}

DWORD FreeSecurityAttributes(PSECURITY_ATTRIBUTES lpSecurityAttribute)
{
	if ( lpSecurityAttribute && lpSecurityAttribute->lpSecurityDescriptor )
	{
		LocalFree(lpSecurityAttribute->lpSecurityDescriptor);
		lpSecurityAttribute->lpSecurityDescriptor = 0;
	}
	return 0;
}

HANDLE PipeUtils::CreateNewPipe(LPCWSTR lpcPipeName)
{
	HANDLE hResult = NULL;
	SECURITY_ATTRIBUTES SecurityAttributes = {0};

	if ( 0 == CreateSecurityAttributes(&SecurityAttributes) )
	{
		m_hInstance = CreateNamedPipe(
			lpcPipeName,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,  
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			PIPE_UNLIMITED_INSTANCES,
			m_dwSendBufferLength,
			m_dwReceiveBufferLength,
			0,
			&SecurityAttributes);

		FreeSecurityAttributes(&SecurityAttributes);
		hResult = m_hInstance;
	}

	return hResult;
}

DWORD PipeUtils::WaitforConnect()
{
	DWORD dwResult = 0;
	ResetOverlapped();

	BOOL bConnected = ConnectNamedPipe(m_hInstance, &m_Overlap);
	if ( !bConnected )
	{
		DWORD dwError = GetLastError();
		if ( dwError == ERROR_IO_PENDING )
		{
			WaitForSingleObject(m_Overlap.hEvent, INFINITE);
		}
		else if ( dwError != ERROR_PIPE_CONNECTED )
		{
			dwResult = 1;
		}
	}

	return dwResult;
}

DWORD PipeUtils::Connect( LPCTSTR lpcPipeName )
{
	SECURITY_ATTRIBUTES SecurityAttributes = {0};

	if ( CreateSecurityAttributes(&SecurityAttributes) )
	{
		return 2;
	}

	int iWaitNum = m_dwWaitTime / 100;
	while ( iWaitNum )
	{
		m_hInstance = CreateFileW(lpcPipeName, GENERIC_READ | GENERIC_WRITE, 0, &SecurityAttributes, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
		if ( m_hInstance != INVALID_HANDLE_VALUE )
			break;
		WaitNamedPipeW(lpcPipeName, m_dwWaitTime);
		iWaitNum--;
		Sleep(100);
	}
	FreeSecurityAttributes(&SecurityAttributes);

	if (iWaitNum <= 0)
	{
		return 3;
	}

	DWORD dwMode = 2;
	if ( 0 == SetNamedPipeHandleState(m_hInstance, &dwMode, 0, 0) )
	{
		return 4;
	}

	return 0;
}

DWORD PipeUtils::Receive( void *lpBuffer, LPDWORD lpSize, DWORD dwWaitTime )
{
	if (lpBuffer == NULL || lpSize == NULL || *lpSize == 0 )
	{
		return 1;
	}

	if (!m_hInstance || !m_Overlap.hEvent)
	{
		return 2;
	}

	DWORD dwResult = 0;
	DWORD dwBufferSize = *lpSize;
	BOOL bRet = ReadFile(m_hInstance, lpBuffer, dwBufferSize, lpSize, &m_Overlap);
	if ( !bRet )
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			DWORD dwTempWait = dwWaitTime ? dwWaitTime : m_dwWaitTime;
			DWORD dwWaitResult = WaitForSingleObject(m_Overlap.hEvent, dwTempWait);
			if ( dwWaitResult == WAIT_OBJECT_0 )
			{
				if ( GetOverlappedResult(m_hInstance, &m_Overlap, lpSize, 0) )
					dwResult = 0;
				else
					dwResult = 2;
			}
			else
			{
				CancelIo(m_hInstance);
				dwResult = 3;
			}
		}
	}

	ResetOverlapped();
	return dwResult;
}

DWORD PipeUtils::Send( void* lpBuffer, LPDWORD lpSendSize, DWORD dwWaitTime )
{
	if (lpBuffer == NULL || lpSendSize == NULL || *lpSendSize == 0)
	{
		return 1;
	}

	if (!m_hInstance || !m_Overlap.hEvent)
	{
		return 2;
	}

	DWORD dwResult = 0;
	DWORD dwWriteBufferSize = *lpSendSize;
	BOOL bRet = WriteFile( m_hInstance, lpBuffer, dwWriteBufferSize, lpSendSize, &m_Overlap );
	if (!bRet)
	{
		DWORD dwTempWait = dwWaitTime ? dwWaitTime : m_dwWaitTime;
		DWORD dwWaitResult = WaitForSingleObject(m_Overlap.hEvent, dwTempWait);
		if ( dwWaitResult == WAIT_OBJECT_0 )
		{
			if ( GetOverlappedResult(m_hInstance, &m_Overlap, lpSendSize, 0) )
				dwResult = 0;
			else
				dwResult = 2;
		}
		else
		{
			CancelIo(m_hInstance);
			dwResult = 3;
		}
	}

	ResetOverlapped();
	return dwResult;
}

