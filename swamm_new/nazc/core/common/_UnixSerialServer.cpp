//////////////////////////////////////////////////////////////////////
//
//	SerialServer.cpp: implementation of the CSerialServer class.
//
//	This file is a part of the AIMIR-GCP.
//	(c)Copyright 2004 NETCRA Co., Ltd. All Rights Reserved.
//
//	This source code can only be used under the terms and conditions 
//	outlined in the accompanying license agreement.
//
//	casir@com.ne.kr
//	http://www.netcra.com
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#ifndef _WIN32
#include <termios.h>
#include <sys/ioctl.h>
#endif
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "SerialServer.h"
#include "MemoryDebug.h"
#include "DebugUtil.h"
#include "CommonUtil.h"

//////////////////////////////////////////////////////////////////////
// CSerialServer Data Definitions
//////////////////////////////////////////////////////////////////////

#define DEFAULT_XOFF_TIMEOUT		10

//////////////////////////////////////////////////////////////////////
// CSerialServer Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSerialServer::CSerialServer()
{
	m_bStarted		= FALSE;
	m_bExitPending	= FALSE;
	m_bSendReady	= FALSE;
	m_bRecvReady	= FALSE;
	m_bPassiveMode	= FALSE;
	m_bDisableSendFail	= FALSE;
	m_nMaxSession	= 0;
	m_nFD			= -1;
	m_nTimeout		= -1;
	m_pSaveSession	= NULL;
	*m_szDevice		= '\0';
	
	sem_init(&m_sendSem, 0, 0);
}

CSerialServer::~CSerialServer()
{
	Shutdown();

	sem_destroy(&m_sendSem);
}

//////////////////////////////////////////////////////////////////////
// CSerialServer Attribute
//////////////////////////////////////////////////////////////////////

BOOL CSerialServer::IsCloseWait() const
{
	return m_bExitPending;
}

BOOL CSerialServer::IsStarted() const
{
	return m_bStarted;
}

int	CSerialServer::GetHandle()
{
	return m_nFD;
}

BOOL CSerialServer::IsSession()
{
	return (m_SessionList.GetCount() > 0) ? TRUE : FALSE;
}

int CSerialServer::GetMaxSession()
{
	return m_nMaxSession;
}

int CSerialServer::GetSessionCount()
{
	return m_SessionList.GetCount();
}

int CSerialServer::GetTimeout() const
{
	return m_nTimeout;
}

void CSerialServer::SetTimeout(int nTimeout)
{
	m_nTimeout = nTimeout;
}

void CSerialServer::DisableSendFail(BOOL bDisable)
{
	m_bDisableSendFail = bDisable;
}

BOOL CSerialServer::IsEmptyStream()
{
	WORKSESSION	*pSession;
	POSITION	pos;

	m_SessionLocker.Lock();
	pSession = m_SessionList.GetFirst(pos);
	m_SessionLocker.Unlock();

	return pSession ? FALSE : TRUE;
}

int CSerialServer::GetSendQSize()
{
	return m_StreamList.GetCount();
}

BOOL CSerialServer::GetPassiveMode()
{
	return m_bPassiveMode;
}

//////////////////////////////////////////////////////////////////////
// CSerialServer Operations
//////////////////////////////////////////////////////////////////////

BOOL CSerialServer::Startup(const char *pszDevice, int nMaxSession, int nTimeout, int nSpeed, int nParity, int nOptions)
{
	if (IsStarted())
		return FALSE;

	m_bStarted		= FALSE;
	m_bExitPending	= FALSE;
	m_bWatchExit	= FALSE;
	m_bSendReady	= FALSE;
	m_bRecvReady	= FALSE;
	m_nMaxSession	= nMaxSession;
	m_nTimeout		= nTimeout;
#ifndef _WIN32
	nSpeed			= (nSpeed == -1) ? B9600 : nSpeed;
#else
    nSpeed			= (nSpeed == -1) ? CBR_9600 : nSpeed;
#endif

	if (!CreateDaemonDevice(pszDevice, nMaxSession, nSpeed, nParity, nOptions))
		return FALSE;

	if (pthread_create(&m_watchThreadID, NULL, WatchThread, (void *)this) != 0)
	{
		XDEBUG("DELETEME: thread create fail\n");
		return FALSE;
	}

	if (pthread_create(&m_sendThreadID, NULL, SendThread, (void *)this) != 0)
	{
		XDEBUG("DELETEME: thread create fail\n");
		return FALSE;
	}

	pthread_detach(m_watchThreadID);
	while(!m_bSendReady || !m_bRecvReady) USLEEP(10000);

	m_bStarted = TRUE;
	return TRUE;
}

void CSerialServer::Shutdown()
{
	void *		nStatus;

	if (!IsStarted())
		return;

	m_bStarted	= FALSE;
	m_bDisableSendFail = FALSE;
	m_bExitPending = TRUE;

	while(!m_bWatchExit) USLEEP(100000);

	// 전송 쓰레드 종료
	sem_post(&m_sendSem);
 	pthread_join(m_sendThreadID, &nStatus);

	// 수신 쓰레드 종료
	CloseDaemonDevice();
}

BOOL CSerialServer::EnableFlowControl(BOOL bEnable)
{
#ifndef _WIN32
	struct	termios	tio;

	if (bEnable)
	{
		tcgetattr(m_nFD, &tio);
		tio.c_cflag |= CRTSCTS;
		tcsetattr(m_nFD, TCSANOW, &tio);
		tcflush(m_nFD, TCIFLUSH);
	}
	else
	{
		tcgetattr(m_nFD, &tio);
		tio.c_cflag &= ~CRTSCTS;
		tcsetattr(m_nFD, TCSANOW, &tio);
		tcflush(m_nFD, TCIFLUSH);
	}
#endif
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CSerialServer Member Functions
//////////////////////////////////////////////////////////////////////

BOOL CSerialServer::CreateDaemonDevice(const char *pszDevice, int nMaxSession, int nSpeed, int nParity, int nOptions)
{
#ifndef _WIN32
	struct	termios	tio;

	//XDEBUG("DELETEME %s %d \n", __FILE__, __LINE__);
	if (m_nFD != -1)
		return FALSE;
//
	strcpy(m_szDevice, pszDevice);
	m_nFD = open(m_szDevice, O_RDWR | O_NOCTTY);
	if (m_nFD < 0)
		return FALSE;

	// Save Terminal Attribute
	tcgetattr(m_nFD, &m_OldTIO);
	
	// Change Setting
	memset(&tio, 0, sizeof(tio));
	tio.c_cflag	= CS8 | CLOCAL | CREAD | nOptions | HUPCL;
    cfsetispeed(&tio, nSpeed);
    cfsetospeed(&tio, nSpeed);
	switch(nParity) {
	  case NONE_PARITY :
		   tio.c_iflag = IGNPAR;
		   break;
	  case EVEN_PARITY :
		   tio.c_cflag |= PARENB;
		   break;
	  case ODD_PARITY :
		   tio.c_cflag |= PARENB;
		   tio.c_cflag |= PARODD;
		   break;
	}
	tio.c_oflag		 = 0;
	tio.c_lflag		 = 0;
	tio.c_cc[VTIME]	 = 10;				// Timeout TIME*0.1
	tio.c_cc[VMIN]	 = 0;
	tio.c_cc[IGNBRK] = 1;
	tio.c_cc[BRKINT] = 0;

	tcflush(m_nFD, TCIFLUSH);
	tcsetattr(m_nFD, TCSANOW, &tio);
#endif
	return TRUE;
}

void CSerialServer::CloseDaemonDevice()
{
	if (m_nFD != -1)
	{
#ifndef _WIN32
		m_OldTIO.c_cflag |= HUPCL;
		tcsetattr(m_nFD, TCSANOW, &m_OldTIO);
#endif
		close(m_nFD);
		m_nFD = -1;
	}

	DeleteAllSession();
}

void CSerialServer::RestoreSerialSetting()
{
#ifndef _WIN32
	if (m_nFD != -1)
		tcsetattr(m_nFD, TCSANOW, &m_OldTIO);
#endif
}

WORKSESSION *CSerialServer::NewSession(int fd, LPSTR pszAddress)
{
	WORKSESSION	*pSession;

	pSession = (WORKSESSION *)MALLOC(sizeof(WORKSESSION));
	if (!pSession)
		return NULL;

	memset(pSession, 0, sizeof(WORKSESSION));
	time(&pSession->lLastInput);
	pSession->pThis		= (void *)this;
	pSession->sSocket	= fd;
	strcpy(pSession->szAddress, pszAddress);

	m_SessionLocker.Lock();
	pSession->nPosition = (int)m_SessionList.AddTail(pSession);
	m_SessionLocker.Unlock();

	return pSession;
}

void CSerialServer::DeleteSession(WORKSESSION *pSession)
{
	if (!pSession)
		return;

	m_SessionLocker.Lock();
	m_SessionList.RemoveAt((POSITION)pSession->nPosition);
	m_SessionLocker.Unlock();

	if (pSession->sSocket != -1)
	{
		OnCloseSession(pSession);
		pSession->sSocket = -1;
	}

	FREE(pSession);
}

void CSerialServer::DeleteAllSession()
{
	WORKSESSION	*pSession, *pDeleteIt;
	POSITION	pos;

	m_SessionLocker.Lock();
	pSession = m_SessionList.GetFirst(pos);
	while(pSession)
	{
		pDeleteIt = pSession;		
		pSession = m_SessionList.GetNext(pos);
		
		if (pDeleteIt->sSocket != -1)
		{
			OnCloseSession(pDeleteIt);
			pDeleteIt->sSocket = -1;
		}

		FREE(pDeleteIt);
	}
	m_SessionList.RemoveAll();
	m_SessionLocker.Unlock();
}

int CSerialServer::AddSendStream(WORKSESSION *pSession, const char *pszBuffer, int nLength)
{
	SIODATASTREAM	*pStream;

	if (!pszBuffer)
		return 0;

	nLength = (nLength == -1) ? strlen(pszBuffer) : nLength;
	if (nLength == 0)
		return 0;

	pStream = (SIODATASTREAM *)MALLOC(sizeof(SIODATASTREAM));
	if (pStream == NULL)
		return -1;

	memset(pStream, 0, sizeof(SIODATASTREAM));
	pStream->nLength	= nLength;
	pStream->pSession	= (void *)pSession;
	pStream->pszBuffer	= (char *)MALLOC(nLength);
	memcpy(pStream->pszBuffer, pszBuffer, nLength);

	m_StreamLocker.Lock();
	pStream->nPosition = (int)m_StreamList.AddTail(pStream);
	m_StreamLocker.Unlock();

	sem_post(&m_sendSem);
	return 0;
}

void CSerialServer::RemoveAllStream(WORKSESSION *pSession)
{
	SIODATASTREAM	*pStream, *pCurrent;
	POSITION		pos;

	m_StreamLocker.Lock();
	pStream = m_StreamList.GetFirst(pos);
	while(pStream)
	{
		pCurrent = pStream;
		pStream = m_StreamList.GetNext(pos);

		if (pCurrent->pszBuffer)
        {
			FREE(pCurrent->pszBuffer);
            pCurrent->pszBuffer = NULL;
        }
		FREE(pCurrent);
	}
	m_StreamList.RemoveAll();
	m_StreamLocker.Unlock();
}

int CSerialServer::WriteData(int fd, char *pszBuffer, int nLength)
{
	return write(fd, pszBuffer, nLength);
}

int CSerialServer::SendStreamToSession(int fd, const char *pszBuffer, int nLength)
{
	int		nCount, nSend=0;
    char * pszBp = const_cast<char *>(pszBuffer);

	for(;nSend<nLength;)
	{
		nCount = WriteData(fd, pszBp + nSend, nLength-nSend);
		if (IsCloseWait())
			return -1;

		if (nCount == 0)
		{
			XDEBUG("------------------ SERIAL BLOCKING (Total=%d, Current=%d) -------------\r\n", nLength, nSend);
			USLEEP(30000);
			continue;
		}
		else if (nCount < 0)
		{
			USLEEP(300000);
			if (!m_bDisableSendFail)
				return -1;
			continue;
		}

		nSend += nCount;
	}

	return nSend;
}

int CSerialServer::DirectSendToSession(WORKSESSION *pSession, const char *pszBuffer, int nLength)
{
	int		nSent;

	nLength = (nLength == -1) ? strlen(pszBuffer) : nLength;
	nSent = SendStreamToSession(pSession->sSocket, pszBuffer, nLength);
	if (nSent > 0) OnSendSession(pSession, const_cast<char *>(pszBuffer), nSent);
	return nSent;
}

int CSerialServer::SendToSession(WORKSESSION *pSession, const char *pszBuffer, int nLength)
{
	nLength = (nLength == -1) ? strlen(pszBuffer) : nLength;
	return AddSendStream(pSession, pszBuffer, nLength);
}

int CSerialServer::SendToSession(WORKSESSION *pSession, char c)
{
	char	szBuffer[1];

	szBuffer[0] = c;
	return AddSendStream(pSession, szBuffer, 1);
}

void CSerialServer::SetPassiveMode(BOOL bPassive)
{
	m_bPassiveMode = bPassive;
}

void CSerialServer::SessionError(WORKSESSION *pSession, int nType)
{
	OnErrorSession(pSession, nType);
}

//////////////////////////////////////////////////////////////////////
// CSerialServer Override Functions
//////////////////////////////////////////////////////////////////////

// CSerialServer Daemon Override Functions ------------------------------

BOOL CSerialServer::EnterDaemon()
{
	return OnEnterDaemon();
}

void CSerialServer::LeaveDaemon()
{
	DeleteAllSession();
	OnLeaveDaemon();
}

BOOL CSerialServer::RunDaemon()
{
	WORKSESSION		*pSession;
	char	szBuffer[2048+1];
	int		len;
	long	lCurTime;
	BOOL	bForever = TRUE;

	m_pSaveSession = pSession = NewSession(m_nFD, m_szDevice);
	if (pSession == NULL)
    {
		return FALSE;
    }

	if (!EnterSession(pSession))
	{
		LeaveSession(pSession);
		return FALSE;
	}

	for(;bForever && !m_bExitPending;)
	{
		// Passive mode에서는 수신을 하지 않는다.
		if (m_bPassiveMode)
		{
			USLEEP(1000000);
			continue;
		}

		len	= read(m_nFD, szBuffer, 2048);
		if (len == 0)
		{
			// Timeout Session 
			if (m_nTimeout != -1)
			{
				time(&lCurTime);
				if ((lCurTime-pSession->lLastInput) >= m_nTimeout)
				{
					pSession->lLastInput = lCurTime;
					if (!OnTimeoutSession(pSession))
						break;
				}
			}
			continue;
		}
		else if (len < 0)
		{
			// Terminate Session
			break;
		}
		else
		{
	 		// Receive Session
			time(&pSession->lLastInput);
			AddSessionRecvStream(pSession, szBuffer, len);
		}
	}
	LeaveSession(pSession);
	return FALSE;
}

// CSerialServer Daemon Session Functions -------------------------------

BOOL CSerialServer::EnterSession(WORKSESSION *pSession)
{
	return OnEnterSession(pSession);
}

void CSerialServer::LeaveSession(WORKSESSION *pSession)
{
	OnLeaveSession(pSession);
	DeleteSession(pSession);
}

WORKSESSION *CSerialServer::FindSession(int fd)
{
	WORKSESSION		*pSession;
	POSITION		pos=INITIAL_POSITION;

	m_SessionLocker.Lock();
	pSession = m_SessionList.GetFirst(pos);
	while(pSession)
	{
		if (pSession->sSocket == fd)
			break;
		pSession = m_SessionList.GetNext(pos);
	}
	m_SessionLocker.Unlock();

	return pSession;
}

BOOL CSerialServer::AddSessionRecvStream(WORKSESSION *pSession, char *pszBuffer, int nLength)
{
	return OnReceiveSession(pSession, pszBuffer, nLength);
}

SIODATASTREAM *CSerialServer::GetStream()
{
	SIODATASTREAM	*pStream;

	m_StreamLocker.Lock();
	pStream = m_StreamList.GetHead();
	if (pStream)
		m_StreamList.RemoveAt((POSITION)pStream->nPosition);
	m_StreamLocker.Unlock();

	return pStream;
}

BOOL CSerialServer::SessionSender()
{
	WORKSESSION		*pSession;
	SIODATASTREAM	*pStream;
	BOOL			bForever = TRUE;

    for(;bForever && !m_bExitPending;)
	{
		sem_wait(&m_sendSem);
		
		if (m_bPassiveMode)
		{
			USLEEP(1000000);
			continue;
		}

		if (m_bExitPending)
			return FALSE;

		while(bForever && !m_bExitPending)
		{
			pStream = GetStream();
			if (pStream == NULL)
				break;

			pSession = (WORKSESSION *)pStream->pSession;
			if (pSession != NULL)
			{
				OnPrevSendSession(pSession, pStream->pszBuffer, pStream->nLength);
				if (SendStreamToSession(pSession->sSocket, pStream->pszBuffer, pStream->nLength) == -1)
				{
					SessionError(pSession, 0);
					bForever = FALSE;
				}
				else OnSendSession(pSession, pStream->pszBuffer, pStream->nLength);
			}
			if (pStream->pszBuffer)
            {
				FREE(pStream->pszBuffer);
                pStream->pszBuffer = NULL;
            }
			FREE(pStream);
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CSerialServer Message Handlers
//////////////////////////////////////////////////////////////////////

// CSerialServer Daemon Thread Message Handlers -------------------------

BOOL CSerialServer::OnEnterDaemon()
{
	return TRUE;
}

void CSerialServer::OnLeaveDaemon()
{
}

// CSerialServer Session Thread Message Handlers ------------------------

BOOL CSerialServer::OnEnterSession(WORKSESSION *pSession)
{
	return TRUE;
}

void CSerialServer::OnLeaveSession(WORKSESSION *pSession)
{
}

BOOL CSerialServer::OnReceiveSession(WORKSESSION *pSession, char* pszBuffer, int nLength)
{
	return TRUE;
}

void CSerialServer::OnPrevSendSession(WORKSESSION *pSession, char* pszBuffer, int nLength)
{
}

void CSerialServer::OnSendSession(WORKSESSION *pSession, char* pszBuffer, int nLength)
{
}

void CSerialServer::OnErrorSession(WORKSESSION *pSession, int nType)
{
}

void CSerialServer::OnCloseSession(WORKSESSION *pSession)
{
}

BOOL CSerialServer::OnTimeoutSession(WORKSESSION *pSession)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////
// CSerialServer Work Thread
//////////////////////////////////////////////////////////////////////

void *CSerialServer::WatchThread(void *pParam)
{
	CSerialServer	*pThis;

	pThis = (CSerialServer *)pParam;
	pThis->m_bRecvReady	= TRUE;
	if (pThis->EnterDaemon())
		pThis->RunDaemon();
	pThis->LeaveDaemon();
	pThis->m_bWatchExit = TRUE;
	return 0;
}

void *CSerialServer::SendThread(void *pParam)
{
	CSerialServer	*pThis;

	pThis = (CSerialServer *)pParam;
	pThis->m_bSendReady	= TRUE;
	pThis->SessionSender();

	return 0;
}
