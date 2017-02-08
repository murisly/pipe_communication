
#pragma once
#include <windows.h>

#define		PIPENAMESIZE	100
#define		PIPENAME		L"\\\\.\\pipe\\pipeutils_{D5C192C6-FD51-4EC6-B8F3-F06F34EE4000}"
#define		PIPEWAITTIME	10000		//Ĭ�ϵȴ�ʱ��10s

class PipeUtils
{
private:
	DWORD m_dwSendBufferLength;
	DWORD m_dwReceiveBufferLength;
	DWORD m_dwWaitTime;	  //�ȴ�ʱ��
	HANDLE m_hInstance;   //server�˱�ʾ�����Ĺܵ���client�˱�ʾ�򿪵��ļ�
	WCHAR m_szPipeName[MAX_PATH];  //�ܵ���
	OVERLAPPED m_Overlap; //

private:
	DWORD ResetOverlapped();
	DWORD ClearOverlapped();
	DWORD CreateOverlapped();

public:
	PipeUtils();
	~PipeUtils();
	DWORD Close();
	HANDLE CreateNewPipe(LPCWSTR lpName);
	DWORD WaitforConnect();
	DWORD Connect(LPCTSTR lpName);
	DWORD Receive(void* lpBuffer, LPDWORD lpSize, DWORD dwWaitTime = PIPEWAITTIME);
	DWORD Send(void* lpBuffer, LPDWORD lpSize, DWORD dwWaitTime = PIPEWAITTIME);
};