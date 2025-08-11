// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Timer.cpp: implementation of the c_Timer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//Initialise static class members
BOOL c_Time::usePerformanceCounter = FALSE;
LONGLONG c_Time::PerfCountFreq = 0;

c_Time::c_Time() { 
	LARGE_INTEGER freq, count;

	//Check for a performance counter
	if(QueryPerformanceFrequency(&freq))
	{
		usePerformanceCounter = TRUE;
		PerfCountFreq = freq.QuadPart/(LONGLONG)1000.0;	//Count in millisecs

		//Init new timer
		QueryPerformanceCounter(&count);
		initPCTime = count.QuadPart;
		lastPCTime = initPCTime;
	}
	else
	{
		usePerformanceCounter = FALSE;

		initGTCTime = GetTickCount();
		lastGTCTime = initGTCTime;
	}
	LastFrameTime = 0.05f;		//Default : 20fps

	ProfileClearTimes();
	ProfileLogEveryFrame = FALSE;

	ZeroMemory( ProfileSectionName, sizeof(ProfileSectionName) );
	for(int i=0; i<MAX_SECTIONS;++i) ProfileTimeString[i][0] = '\0';
}

c_Time::~c_Time() {

#ifdef _PROFILE
	if(NumSections>0) WriteProfileTimes();
#endif

}

void c_Time::Initialise()
{
}

void c_Time::NextFrame()
{
	LARGE_INTEGER PerfCount;
	DWORD currentTickCount;

	if(usePerformanceCounter)
	{
		QueryPerformanceCounter(&PerfCount);
		LastFrameTime = (float)( (double)(PerfCount.QuadPart - lastPCTime)/ (double)PerfCountFreq);
		lastPCTime = PerfCount.QuadPart;
	}
	else
	{
		currentTickCount = GetTickCount();
		LastFrameTime = (float)(currentTickCount - lastGTCTime);
		lastGTCTime = currentTickCount;
	}

}

int c_Time::GetMillisecs() 
{
	LARGE_INTEGER PerfCount;
	DWORD currentTickCount;

	if(usePerformanceCounter)
	{
		QueryPerformanceCounter(&PerfCount);
		return (int) ((PerfCount.QuadPart-initPCTime)/(float)PerfCountFreq);
	}
	else
	{
		currentTickCount = GetTickCount();
		return (currentTickCount-initGTCTime);
	}
}

double c_Time::GetPerfMillisecs()
{
	LARGE_INTEGER PerfCount;

	if(!usePerformanceCounter) return GetMillisecs();
	QueryPerformanceCounter(&PerfCount);
	return (double)(PerfCount.QuadPart-initPCTime)/(double)PerfCountFreq;
}

void c_Time::ProfileSectionStart(int SectionNum, const char *SectionName)
{
	if(SectionNum>=MAX_SECTIONS) { 
		CriticalError(ERR_TIME, "Too many Profile Sections");
		return;
	}
	if(SectionNum>=NumSections) NumSections=SectionNum+1;

	//Copy name if 1st time
	if(ProfileSectionName[SectionNum][0]=='\0') {
		int i=DQstrcpy(ProfileSectionName[SectionNum], SectionName, 200);
		//fill the rest with spaces
		for( ; i<MAX_PROFILENAME-1; ++i) ProfileSectionName[SectionNum][i] = ' ';
		ProfileSectionName[SectionNum][MAX_PROFILENAME-1] = '\0';
	}

	LastSectionTime[SectionNum] = GetPerfMillisecs();
}

void c_Time::ProfileSectionEnd(int SectionNum)
{
	float SectionTime;

	//Check for errors
	if(SectionNum>=NumSections) { 
		CriticalError(ERR_TIME, "ProfileSectionEnd : Undefined Profile Section");
		return;
	}
	if(LastSectionTime[SectionNum]<0) {
		CriticalError(ERR_TIME, "ProfileSectionEnd : Section already ended");
		return;
	}

	//Update Profile Times
	SectionTime = GetPerfMillisecs()-LastSectionTime[SectionNum];
	ProfileSectionTotal[SectionNum] += SectionTime;
	ProfileSectionTimePerFrame[SectionNum] += SectionTime;

	//Update Profile Call Counts
	++ProfileNumCalls[SectionNum];
	++ProfileSectionCallsPerFrame[SectionNum];
	LastSectionTime[SectionNum] = -1;
	
	ProfileSectionMin[SectionNum] = min( ProfileSectionMin[SectionNum], SectionTime );
	ProfileSectionMax[SectionNum] = max( ProfileSectionMax[SectionNum], SectionTime );
}

void c_Time::ProfileMarkFrame()
{
	float SectionTime;
	char buffer[1000];
	FILE *file;
	double CurrentTime;
	double SectionTimeCount;
	static float AverageFrameTime = 50.0f;

	if(ProfileLogEveryFrame) {
		CurrentTime = GetPerfMillisecs();

		if(LastFrameTime>0) {
			//Calculate Frame Time
			CurrentTime = CurrentTime - LastFrameTime;
			file = fopen("Profile Times.txt", "a");
		}
		else {
			//First time
			CurrentTime = 0; //Frame Time
			file = fopen("Profile Times.txt", "w+");		//overwrite existing file
		}

		//Write header to file
		SectionTimeCount = 0;
		for(int i=0;i<NumSections;++i) SectionTimeCount += ProfileSectionTotal[i];

		sprintf(buffer, "-----------------------------------\nFrame %d - Time taken \t%f\t\tMissing Time %f (%.1f%%)\t\tNum D3D Calls %d\n", DQRender.FrameNum, CurrentTime, CurrentTime-SectionTimeCount, (CurrentTime-SectionTimeCount)/CurrentTime*100.0f, DQRender.NumD3DCalls);
		fwrite(buffer, strlen(buffer), 1, file);
	}

	sprintf( ProfileTimeString[0], "FPS : %.2f     Time ms %f", 1000.0f/LastFrameTime, LastFrameTime);

	for(int i=0;i<NumSections;++i) {
		if(ProfileSectionCallsPerFrame[i]==0) continue;

		SectionTime = ProfileSectionTimePerFrame[i]/(float)ProfileSectionCallsPerFrame[i];
		ProfileSectionMax[i] = max(ProfileSectionMax[i], SectionTime);
		ProfileSectionMin[i] = min(ProfileSectionMin[i], SectionTime);

		ProfileSectionTimePerFrame[i] = 0.0f;
		ProfileSectionCallsPerFrame[i] = 0;

		sprintf( ProfileTimeString[i+1], "%s \tSection %-d Calls : %d \tTotal : %-f Time per call : %-f Min : %-f Max : %-f\n", ProfileSectionName[i], i, ProfileNumCalls[i], ProfileSectionTotal[i], ProfileSectionTotal[i]/ProfileNumCalls[i], ProfileSectionMin[i], ProfileSectionMax[i] );

		if(ProfileLogEveryFrame) {
			fwrite(ProfileTimeString[i+1], strlen(ProfileTimeString[i+1]), 1, file);
		}

		ProfileSectionMax[i] = 0;
		ProfileSectionMin[i] = 99999999;
	}

	if(ProfileLogEveryFrame) {
		fclose(file);
	}

	//Print profile times if slow frame
	AverageFrameTime = AverageFrameTime*0.8f + LastFrameTime*0.2f;		//rough average fps

/*	if(LastFrameTime>AverageFrameTime*1.5f) {
		DQPrint( va("^6Slow Frame : Frame Time ms : %.3f Num D3D Calls : %d      Faces Drawn %d", LastFrameTime, DQRender.NumD3DCalls, DQBSP.FacesDrawn) );
		PrintProfileTimes();
	}*/

	ProfileClearTimes();
	LastFrameTime = GetPerfMillisecs();
}

void c_Time::PrintProfileTimes()
{
	for(int i=0; i<=NumSections; ++i ) {
		DQPrint(ProfileTimeString[i]);
	}
}

void c_Time::ProfileClearTimes()
{
	for(int i=0;i<MAX_SECTIONS;++i) {
		LastSectionTime[i] = -1;
		ProfileSectionTotal[i] = 0.0;
		ProfileSectionTimePerFrame[i] = 0.0;
		ProfileNumCalls[i] = 0;
		ProfileSectionCallsPerFrame[i] = 0;
		ProfileSectionMax[i] = 0.0f;
		ProfileSectionMin[i] = 99999999;
	}
	NumSections = 0;
}

void c_Time::WriteProfileTimes()
{
	if(ProfileLogEveryFrame) return;

#if _DEBUG

	char buffer[1000];
	FILE *file;
	
	file = fopen("Profile Times.txt", "w+");

	for(int i=0;i<NumSections;++i) {
		if(ProfileNumCalls[i]==0) continue;
		sprintf(buffer, "Section %d : Calls : %d \tTotal : %f \tTime per call : %f \tMin pf: %f \tMax pf: %f\n", i, ProfileNumCalls[i], ProfileSectionTotal[i], ProfileSectionTotal[i]/ProfileNumCalls[i], ProfileSectionMin[i], ProfileSectionMax[i]);
		fwrite(buffer, strlen(buffer), 1, file);
	}

	fclose(file);
#endif
}

void c_Time::SynchronizeTime( double SynchronizeTime )
{
	LARGE_INTEGER PerfCount;
	DWORD currentTickCount;

	if(usePerformanceCounter)
	{
		QueryPerformanceCounter(&PerfCount);
		initPCTime = (double)PerfCount.QuadPart - SynchronizeTime*(double)PerfCountFreq;
	}
	else
	{
		currentTickCount = GetTickCount();
		initGTCTime = currentTickCount - SynchronizeTime;
	}
}