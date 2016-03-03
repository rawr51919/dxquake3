// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Timer.h: interface for the c_Timer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_TIMER_H__53137D10_10CF_4375_8439_6481CED4821A__INCLUDED_)
#define AFX_C_TIMER_H__53137D10_10CF_4375_8439_6481CED4821A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_SECTIONS 30
#define MAX_PROFILENAME 15

#define _PROFILE

//Time counted in milliseconds
class c_Time : public c_Singleton<c_Time>
{
public:
	c_Time::c_Time();
	virtual c_Time::~c_Time();
	int GetMillisecs();	//Return the number of millisecs since game started
	float GetSecs() { return (GetMillisecs() * 0.001f); }
	double GetPerfMillisecs();	//Return a high precision value of GetMillisecs()
	float GetLastFrameTime() { return LastFrameTime; }
	LONGLONG GetPerfCountFreq() { return PerfCountFreq; }
	virtual void c_Time::NextFrame();	//Called once, at the beginning of each client frame
	void ProfileClearTimes();
	void ProfileSectionStart(int LineNum, const char *SectionName);
	void ProfileSectionEnd(int LineNum);
	void ProfileMarkFrame();
	void WriteProfileTimes();
	void SynchronizeTime( double SynchronizeTime );
	void c_Time::Initialise();
	void PrintProfileTimes();

	BOOL	ProfileLogEveryFrame;
	char	 ProfileTimeString[MAX_SECTIONS][200];

protected:

	union {
		DWORD initGTCTime;				//for GetTickCount method
		LONGLONG initPCTime;			//for PerformanceCounter method
	};
	union {
		DWORD lastGTCTime;
		LONGLONG lastPCTime;
	};
	float LastFrameTime;

	static BOOL usePerformanceCounter;
	static LONGLONG PerfCountFreq;

	//Profiling
	double	LastSectionTime[MAX_SECTIONS];
	double	ProfileSectionTotal[MAX_SECTIONS];
	double	ProfileSectionTimePerFrame[MAX_SECTIONS];
	double	ProfileSectionMax[MAX_SECTIONS];
	double	ProfileSectionMin[MAX_SECTIONS];
	char	ProfileSectionName[MAX_SECTIONS][MAX_PROFILENAME];
	int		ProfileNumCalls[MAX_SECTIONS];
	int		ProfileSectionCallsPerFrame[MAX_SECTIONS];
	int		NumSections;
};

#define DQTime c_Time::GetSingleton()

#ifdef _PROFILE
	#define DQProfileSectionStart(Num, Name) DQTime.ProfileSectionStart(Num, Name)
	#define DQProfileSectionEnd(Num) DQTime.ProfileSectionEnd(Num)
#else
	#define DQProfileSectionStart(Num, Name) 
	#define DQProfileSectionEnd(Num) 
#endif

#endif // !defined(AFX_C_TIMER_H__53137D10_10CF_4375_8439_6481CED4821A__INCLUDED_)
