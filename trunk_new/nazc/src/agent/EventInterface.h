#ifndef __EVENT_INTERFACE_H__
#define __EVENT_INTERFACE_H__

#define PDAEVENT_ADDSENSOR			1

#include "BatchJob.h"

BOOL SendOamEvent(BYTE nAction, BYTE nEvent, EUI64 *id, char *pszMessage, BYTE nLength);
BOOL SendPdaEvent(BYTE nAction, BYTE nEvent, BYTE nGroup, BYTE nMember, EUI64 *id, char *pszSerial);

char *getFepServerAddress();
void setFepServerAddress(char *pszAddress);
void setFepServerPort(int nPort);
void setFepServerAlarmPort(int nPort);
void setLocalPort(int nPort);
int getLocalPort();

void mcuInstallEvent();
void mcuUninstallEvent();
void mcuStartupEvent();
void mcuShutdownEvent(BYTE nType, char *pszAddress);
void mcuChangeEvent(MIBValue *pValue, int nCount, BOOL *pResult);
void mcuOamLogin(char *pszAddress, char *pszUser);
void mcuOamLogout(char *pszAddress, char *pszUser);
void mcuClockChangeEvent();
void mcuPowerFailEvent();
void mcuPowerRestoreEvent();
void mcuLowBatteryEvent();
void mcuLowBatteryRestoreEvent();
void mcuBatteryFailEvent();
void mcuTempRangeEvent(double fCurrent, double fMin, double fMax);
void mcuTempRangeRestoreEvent(double fCurrent, double fMin, double fMax);
void mcuResetEvent(BYTE nType, const char *pszAddress);
void mcuFactoryDefaultEvent(BYTE nType, char *pszAddress);
void mcuFirmwareUpdateEvent(BYTE nType, const char *pszAddress, const char *pszFileName);
void mcuBatteryChargingStartEvent();
void mcuBatteryChargingEndEvent();
void mcuCoverOpenEvent();
void mcuCoverCloseEvent();
void mcuHeaterOnEvent();
void mcuHeaterOffEvent();
void mcuMeteringStartEvent();
void mcuMeteringEndEvent();
void mcuRecoveryStartEvent();
void mcuRecoveryEndEvent();
void mcuMeteringFailEvent(char *pszList);
void mcuMcuDiagnosisEvent(BYTE *pszData, int nLength);
void mcuAlert(BYTE alertType, BYTE * previous, int previousLen, BYTE * current, int currentLen );

void sinkResetEvent(EUI64 *id, BYTE nReason);

void mobileConnectEvent();
void mobileKeepaliveEvent();

void sensorUnknownEvent(EUI64 *id, int nType);
void sensorJoinEvent(EUI64 *id);
void sensorLeaveEvent(EUI64 *id);
//void sensorChangeEvent(EUI64 *id, BYTE nType, SENSORPAGE *pPage, int nCount);
void sensorConnectEvent(EUI64 *id, BYTE nConnectType);
void sensorDisconnectEvent(EUI64 *id);
void sensorTimeoutEvent(EUI64 *id);
void sensorBreakingEvent(EUI64 *id);
void sensorResetEvent(EUI64 *id);
void sensorAlarmEvent(EUI64 *id, GMTTIMESTAMP *pTime, BYTE nEvent, BYTE *pStatus);
void sensorPowerOutage(GMTTIMESTAMP *pTime, EUI64 *ids, int nCount, BYTE nEvent);
void sensorInstallEvent(EUI64 *id, int nType, int nService, char *meterid, char *vendor, char *model, BYTE networkType);
void sensorUninstallEvent(EUI64 *id);

void malfDiskErrorEvent(UINT nTotal, UINT nFree, UINT nUse);
void malfDiskErrorRestoreEvent(UINT nTotal, UINT nFree, UINT nUse);
void malfMemoryErrorEvent(UINT nTotal, UINT nFree, UINT nUse, UINT nCache, UINT nBuffer);
void malfMemoryErrorRestoreEvent(UINT nTotal, UINT nFree, UINT nUse, UINT nCache, UINT nBuffer);
void malfWatchdogResetEvent(char *pszName);
void malfGarbageCleaningEvent(BYTE eventType, BYTE *eventData, int dataLength);

void commZruErrorEvent(EUI64 *id, BYTE nState);
void commZmuErrorEvent(EUI64 *id, BYTE nState);
void commZeuErrorEvent(EUI64 *id, BYTE nState);

void meterErrorEvent(EUI64 *id, char *meterId, UINT errorCode, UINT errorStatus, BYTE * errorData, UINT errorDataLen);
void transactionEvent(WORD nTrID, BYTE nEvtType, BYTE nOption, 
        cetime_t nCTime, cetime_t nLTime, INT nErrCode, UINT nResultCnt, MIBValue * pResult);
void otaDownloadEvent(char *pszTriggerId, BYTE errorCode);
void otaStartEvent(char *pszTriggerId);
void otaEndEvent(char *pszTriggerId, BYTE reason, int nTotal, int nSuccess, int nError);
void otaResultEvent(char *pszTriggerId, EUI64 *id, BYTE nType, BYTE nOtaStep, BYTE errorCode);

void energyLevelChangeEvent(const EUI64 *pId, cetime_t nIssue, int nErrorCode, BYTE nPreviousLevel, BYTE nCurrentLevel, 
        BYTE *pszUserData, int nUserDataLen);

#endif	// __EVENT_INTERFACE_H__
