// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Sound.cpp: implementation of the c_Sound class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Sound.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Sound::c_Sound()
{
	FileHandle = 0;
	WaveData = NULL;
	WaveHeader = NULL;
	isLoaded = FALSE;
	Buffer = NULL;
	isLoopingSound = FALSE;
	Buffer3D = NULL;
	NextInstanceSound = NULL;
	EntityNum = ENTITYNUM_NONE;
	isPlaying = FALSE;
}

//Only copy data about the sound file, not the DS buffer
c_Sound::c_Sound(c_Sound *Original)
{
	DQstrcpy( Filename, Original->Filename, MAX_QPATH );
	FileLength = Original->FileLength;
	WaveLength = Original->WaveLength;

	WaveHeader = Original->WaveHeader;
	WaveData = Original->WaveData;
	isLoaded = Original->isLoaded;
	EntityNum = Original->EntityNum;

	FileHandle = 0;
	Buffer = NULL;
	isLoopingSound = FALSE;
	Buffer3D = NULL;
	NextInstanceSound = NULL;
	EntityNum = ENTITYNUM_NONE;
	isPlaying = FALSE;
}

c_Sound::~c_Sound()
{
	Unload();
}

void c_Sound::Unload()
{
	if(!isLoaded) return;
	isLoaded = FALSE;

	if(FileHandle) DQFS.CloseFile(FileHandle);
	SafeRelease( Buffer );

	if(NextInstanceSound ) {
		DQDelete( NextInstanceSound );
	}
	else {
		//Make sure only the last instance deletes the original wav data
		DQDelete( WaveHeader );
		DQDeleteArray( WaveData );
	}
}

//Reference : www.gametutorials.com - DirectSound tutorial

BOOL c_Sound::Load2DSound(const char *SoundFilename)
{
	if( LoadSound( SoundFilename ) == FALSE) 
		return FALSE;

	return ReloadBuffer( FALSE, FALSE );
}

BOOL c_Sound::Load3DSound(const char *SoundFilename)
{
	if( LoadSound( SoundFilename ) == FALSE) 
		return FALSE;
	return ReloadBuffer( TRUE, FALSE );
}

BOOL c_Sound::LoadSound(const char *SoundFilename)
{
	char FOURCC[5];
	char WaveType[5];
	DWORD Length;
	char SectionType[5];
	int i;

	if(isLoaded) {
		return TRUE;
	}

	DQstrcpy(Filename, SoundFilename, MAX_QPATH);

	if(!FileHandle) {	//if not already open
		FileHandle = DQFS.OpenFile(SoundFilename, "rb");
		if(!FileHandle) return FALSE;
	}

	FileLength = DQFS.GetFileLength( FileHandle );

	//Check the 4CC
	DQFS.ReadFile( FOURCC, 1, 4, FileHandle );
	//(case sensitive)
	if(DQstrcmp(FOURCC, "RIFF", sizeof(FOURCC))) {
		Error(ERR_SOUND, va("%s is not a WAV file",SoundFilename));
		return FALSE;
	}

	//WAVE HEADER

	//Read Wave Length
	DQFS.ReadFile( &Length, sizeof(DWORD), 1, FileHandle );

	//Read Wave Type
	DQFS.ReadFile( WaveType, 1, 4, FileHandle );
	//(case sensitive)
	if(DQstrcmp(WaveType, "WAVE", sizeof(WaveType))) {
		Error(ERR_SOUND, va("%s is not a WAV file",SoundFilename));
		return FALSE;
	}

	//Now load format and data
	for(i=0;i<2;++i) {
		//Read Section header

		DQFS.ReadFile( SectionType, 1, 4, FileHandle );
		DQFS.ReadFile( &Length, sizeof(DWORD), 1, FileHandle );

		if(DQstrcmp(SectionType, "fmt ", sizeof(SectionType))==0) {
			//Wave data header
			if(WaveHeader) return FALSE;		//already got header!?

			WaveHeader = DQNew(WAVEFORMATEX);
			ZeroMemory( WaveHeader, sizeof(WAVEFORMATEX) );
			DQFS.ReadFile( WaveHeader, Length, 1, FileHandle );
		}
		else if(DQstrcmp(SectionType, "data", sizeof(SectionType))==0) {
			WaveLength = Length;

			if(WaveData) return FALSE;		//already got data!?

			WaveData = (BYTE*)DQNewVoid( BYTE[WaveLength] );
			DQFS.ReadFile( WaveData, sizeof(BYTE), WaveLength, FileHandle );
		}

	}
	if(!WaveData || !WaveHeader) return FALSE;		// something went wrong

	isLoaded = TRUE;
	return TRUE;
}

//Load into DXSound
BOOL c_Sound::ReloadBuffer( BOOL use3DBuffer, BOOL useHardware )
{
	if(!isLoaded) return FALSE;

	//Release buffer if already allocated
	SafeRelease( Buffer );

	//Create Sound buffer
	ZeroMemory(&BufferDesc, sizeof(DSBUFFERDESC) );
	BufferDesc.dwSize = sizeof(DSBUFFERDESC);
	BufferDesc.dwBufferBytes = WaveLength;
	BufferDesc.dwReserved = 0;
	BufferDesc.lpwfxFormat = WaveHeader;
	if(use3DBuffer) {
		BufferDesc.dwFlags = DSBCAPS_CTRL3D;
		BufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;
	}
	else {
		BufferDesc.dwFlags = DSBCAPS_CTRLVOLUME;					//TODO : check for 3d hardware and set LOC_HARDWARE
		BufferDesc.guid3DAlgorithm = GUID_NULL;
	}

  // Create buffer. 

	HRESULT hr = DQSound.DirectSound->CreateSoundBuffer( &BufferDesc, &Buffer, NULL );
	if(FAILED(hr)) {
		Error(ERR_SOUND, "Could not create DirectSound Buffer");
		Buffer = NULL;
		return FALSE;
	}

	BYTE *pSound;
	DWORD SoundLength;

	dxcheckOK( Buffer->Lock( 0, WaveLength, (void**)&pSound, &SoundLength, NULL, NULL, DSBLOCK_FROMWRITECURSOR ) );

	memcpy( pSound, WaveData, WaveLength );

	dxcheckOK( Buffer->Unlock( pSound, SoundLength, NULL, NULL ) );

	//Get 3D Buffer
	hr = Buffer->QueryInterface( IID_IDirectSound3DBuffer8, (void**)&Buffer3D );
	if(FAILED( hr ) || hr == DS_NO_VIRTUALIZATION) {
		Buffer3D = NULL;
	}
	else {
		Buffer3D->SetMinDistance( 30.0f, DS3D_IMMEDIATE );
	}

	is3DSound = use3DBuffer;

	return TRUE;
}

