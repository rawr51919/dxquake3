// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_SoundManager.cpp: implementation of the c_Sound class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_SoundManager::c_SoundManager():NumChannels(8)
{
	bClearLoopingSounds = TRUE;
	SoundFrameNum = 0;
	isInitialised = FALSE;
	DQConsole.RegisterCVar("s_initSound", "1", NULL, eAuthClient);

	//Initialise Member variables
	NumSounds = 0;
	for(int i=0;i<MAX_SOUNDS;++i) pSoundArray[i] = NULL;

	DirectSound = NULL;
	DSListener = NULL;
	PrimaryBuffer = NULL;
	SoundChannel = NULL;	
}

c_SoundManager::~c_SoundManager()
{
	Unload();
}

void c_SoundManager::Unload()
{
	for(int i=0;i<NumSounds;++i) DQDelete(pSoundArray[i]);
	NumSounds = 0;

	SafeRelease( DSListener );
	SafeRelease( PrimaryBuffer );
	SafeRelease( DirectSound );
	DQDeleteArray( SoundChannel );

	isInitialised = FALSE;
}

void c_SoundManager::Initialise()
{
	HRESULT hr;

	noSound = DQConsole.GetCVarInteger( "s_initsound", eAuthClient )?FALSE:TRUE;
	if(noSound) return;

	//Set in case of fail (reset at end if successful)
	DQConsole.SetCVarString( "s_initsound", "0", eAuthClient );
	noSound = TRUE;

	Unload();

	SoundChannel = (int*)DQNewVoid(int[NumChannels]);
	ZeroMemory( SoundChannel, sizeof(int)*NumChannels );
	ZeroMemory( pSoundEntityPos, sizeof(pSoundEntityPos) );

	//TODO: Enumerate sound devices

	//Create DirectSound device
	hr = DirectSoundCreate( NULL, &DirectSound, NULL );
	if(FAILED(hr)) return;

	//Set Cooperation level
	hr = DirectSound->SetCooperativeLevel( DQRender.GetHWnd(), DSSCL_PRIORITY );
	if(FAILED(hr)) return;

	//Create 3D Primary Buffer
	ZeroMemory( &PrimaryBufferDesc, sizeof(DSBUFFERDESC) );
	PrimaryBufferDesc.dwSize			= sizeof(DSBUFFERDESC);
	PrimaryBufferDesc.dwFlags			= DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
	PrimaryBufferDesc.guid3DAlgorithm	= DS3DALG_DEFAULT;

	hr = DirectSound->CreateSoundBuffer( &PrimaryBufferDesc, &PrimaryBuffer, NULL );
	if(FAILED(hr)) return;
	hr = PrimaryBuffer->QueryInterface( IID_IDirectSound3DListener8, (void**)&DSListener );
	if(FAILED(hr)) return;

	DSListener->SetRolloffFactor( 0.5f, DS3D_IMMEDIATE );
	DSListener->SetDistanceFactor( 0.1f, DS3D_IMMEDIATE );

	DQConsole.SetCVarString( "s_initsound", "1", eAuthClient );
	noSound = FALSE;
	isInitialised = TRUE;
}

//New sounds are also registered in c_SoundManager::GetNextInstance
int c_SoundManager::RegisterSound(char *SoundFilename, BOOL isCompressed)
{
	c_Sound *Sound;
	int i;

	if(noSound) return 0;
	if(!isInitialised) Initialise();

	if(NumSounds>=MAX_SOUNDS) {
		Error( ERR_SOUND, "RegisterSound : Max sounds reached" );
		return 0;
	}

	//Check if sound has been registered before
	for(i=0;i<NumSounds;++i) {
		if(!DQstrcmpi(pSoundArray[i]->Filename, SoundFilename, MAX_QPATH))
			return i+1;
	}

	Sound = DQNew( c_Sound );
	pSoundArray[NumSounds] = Sound;

	if( !Sound->Load2DSound(SoundFilename) ) {
		//LoadSound failed
		DQDelete( Sound );
		return 0;
	}

	++NumSounds;

	//check this func is finished
	return NumSounds;				//Handle is ArrayIndex+1
}

c_Sound *c_SoundManager::GetNextInstance(c_Sound *Sound)
{
	if(!Sound->NextInstanceSound) {

		Sound->NextInstanceSound = (c_Sound*)DQNewVoid( c_Sound(Sound) );
		Sound->NextInstanceSound->ReloadBuffer( TRUE, FALSE );

	}

	return Sound->NextInstanceSound;
}

void c_SoundManager::Play2DSound( int SoundHandle, int Channel, int EntityNum )
{
	if(noSound) return;
	if(!isInitialised) Initialise();
	if(SoundHandle<1 || SoundHandle>NumSounds) return;
	if(Channel<0 || Channel>NumChannels) return;

	DWORD Status;
	c_Sound *Sound = pSoundArray[SoundHandle-1];
	
	//Channel 0 should always play (uninterrupted)
	if(Channel==CHAN_AUTO) {
		while(1) {
			if(!Sound->Buffer) return;

			Sound->Buffer->GetStatus( &Status );
			if(Status & DSBSTATUS_PLAYING) {
				Sound->isPlaying = TRUE;
				Sound = GetNextInstance(Sound);
			}
			else break;
		}
	}
	else if(SoundChannel[Channel]!=0) {
		//Stop any sounds using this channel
		if(pSoundArray[SoundChannel[Channel]-1]->Buffer) {
			pSoundArray[SoundChannel[Channel]-1]->Buffer->Stop();
		}
		pSoundArray[SoundChannel[Channel]-1]->isPlaying = FALSE;
	}

	if(Sound->is3DSound) Sound->Load2DSound(Sound->Filename);
	Sound->isPlaying = TRUE;

	//Play sound
	dxcheckOK( Sound->Buffer->SetVolume( -2000 ) );
	dxcheckOK( Sound->Buffer->SetCurrentPosition( 0 ) );
	dxcheckOK( Sound->Buffer->Play( 0, 0, 0 ) );

	SoundChannel[Channel] = SoundHandle;
	Sound->EntityNum = EntityNum;
}

void c_SoundManager::Play3DSound( int SoundHandle, int Channel, int EntityNum, D3DXVECTOR3 *Position )
{
	if(noSound) return;
	if(SoundHandle<1 || SoundHandle>NumSounds) return;
	if(Channel<0 || Channel>NumChannels) return;

	DWORD Status;
	c_Sound *Sound;

	Sound = pSoundArray[SoundHandle-1];

	//Channel 0 should always play (uninterrupted)
	if(Channel==CHAN_AUTO) {
		while(1) {
			if(!Sound->Buffer) return;

			Sound->Buffer->GetStatus( &Status );
			if(Status & DSBSTATUS_PLAYING) {
				Sound->isPlaying = TRUE;
				Sound = GetNextInstance(Sound);
			}
			else break;
		}
	}
	else if(SoundChannel[Channel]!=0) {
		//Stop any sounds using this channel
		if(pSoundArray[SoundChannel[Channel]-1]->Buffer) {
			pSoundArray[SoundChannel[Channel]-1]->Buffer->Stop();
		}
		pSoundArray[SoundChannel[Channel]-1]->isPlaying = FALSE;
	}

	//Set 3D Position information
	if(!Sound->is3DSound) Sound->Load3DSound(Sound->Filename);

	dxcheckOK( Sound->Buffer3D->SetPosition( Position->x, Position->y, Position->z, DS3D_IMMEDIATE ) );

	//Play sound
	dxcheckOK( Sound->Buffer->SetCurrentPosition( 0 ) );
	dxcheckOK( Sound->Buffer->Play( 0, 0, 0 ) );
	Sound->isPlaying = TRUE;

	SoundChannel[Channel] = SoundHandle;

	Sound->EntityNum = EntityNum;

	if(EntityNum>=0 && EntityNum<ENTITYNUM_MAX_NORMAL) {
		pSoundEntityPos[EntityNum] = *Position;
	}
}

void c_SoundManager::Play3DLoopingSound( int SoundHandle, int EntityNum, D3DXVECTOR3 *Position, D3DXVECTOR3 *Velocity )
{
	if(noSound) return;
	if(SoundHandle<1 || SoundHandle>NumSounds) return;

	c_Sound *Sound = pSoundArray[SoundHandle-1];
	//check if this is the same sound as before
	while( (Sound->EntityNum != EntityNum) && (Sound->EntityNum != ENTITYNUM_NONE)) {
		Sound = GetNextInstance(Sound);
	}
	Sound->EntityNum = EntityNum;

	if(EntityNum>=0 && EntityNum<ENTITYNUM_MAX_NORMAL) pSoundEntityPos[EntityNum] = *Position;

	if(!Sound->is3DSound) Sound->Load3DSound(Sound->Filename);

	if(!Sound->Buffer || !Sound->Buffer3D) return;

	dxcheckOK( Sound->Buffer3D->SetPosition( Position->x, Position->y, Position->z, DS3D_IMMEDIATE ) );
	dxcheckOK( Sound->Buffer3D->SetVelocity( Velocity->x, Velocity->y, Velocity->z, DS3D_IMMEDIATE ) );

	dxcheckOK( Sound->Buffer->Play( 0, 0, DSBPLAY_LOOPING ) );
	Sound->isPlaying = TRUE;
	Sound->isLoopingSound = TRUE;
	Sound->CurrentFrame = SoundFrameNum;
}

void c_SoundManager::ClearLoopingSounds()
{
	bClearLoopingSounds = TRUE;
}

void c_SoundManager::Update()
{
	D3DXVECTOR3 Pos;
	c_Sound *Sound;

	if(noSound) return;
	if(!isInitialised) Initialise();

	//For each sound handle
	for(int i=0;i<NumSounds;++i) {

		//For each instance of this sound
		for( Sound = pSoundArray[i]; Sound; Sound = Sound->NextInstanceSound ) {

			//Update 3D Position
			if(Sound->is3DSound 
				&& Sound->CurrentFrame != SoundFrameNum
				&& Sound->isPlaying
				&& Sound->Buffer3D
				&& Sound->EntityNum>=0 
				&& Sound->EntityNum<ENTITYNUM_MAX_NORMAL) 
			{
				Pos = pSoundEntityPos[Sound->EntityNum];
				Sound->Buffer3D->SetPosition( Pos.x, Pos.y, Pos.z, DS3D_DEFERRED );
			}

			if(bClearLoopingSounds) {
				if(Sound->isLoopingSound && Sound->CurrentFrame != SoundFrameNum) {
					//Clear sound
					if(Sound->Buffer) {
						Sound->Buffer->Stop();
					}
					Sound->isPlaying = FALSE;
					Sound->isLoopingSound = FALSE;
					Sound->EntityNum = ENTITYNUM_NONE;
				}
			}
		}
	}

	bClearLoopingSounds = FALSE;
	++SoundFrameNum;
	dxcheckOK( DSListener->CommitDeferredSettings() );
}

void c_SoundManager::StopLoopingSound( int EntityNum )
{
	if(noSound) return;
	if(EntityNum<0 || EntityNum>=ENTITYNUM_MAX_NORMAL) return;

	for(int i=0;i<NumSounds;++i) {
		if(pSoundArray[i]->EntityNum == EntityNum) {
			if(pSoundArray[i]->Buffer) {
				pSoundArray[i]->Buffer->Stop();
			}
			pSoundArray[i]->isPlaying = FALSE;
			pSoundArray[i]->isLoopingSound = FALSE;
		}
	}
}

void c_SoundManager::UpdateEntityPosition( int EntityNum, D3DXVECTOR3 *Position )
{
	if(noSound) return;
	if(EntityNum<0 || EntityNum>=ENTITYNUM_MAX_NORMAL) return;

	pSoundEntityPos[EntityNum] = *Position;
}

void c_SoundManager::SetListenerOrientation( D3DXVECTOR3 *Position, float *axis, BOOL inWater )
{
	if(noSound) return;
	if(!DSListener) return;
	dxcheckOK( DSListener->SetPosition( Position->x, Position->y, Position->z, DS3D_DEFERRED ) );
	dxcheckOK( DSListener->SetOrientation( -axis[0], -axis[1], -axis[2], axis[6], axis[7], axis[8], DS3D_DEFERRED ) );
}

void c_SoundManager::UnloadSound( int SoundHandle )
{
	if(noSound) return;
	if(SoundHandle<1 || SoundHandle>NumSounds) return;

	DQDelete( pSoundArray[SoundHandle-1] );

	//Check OK: List not reshuffled
}