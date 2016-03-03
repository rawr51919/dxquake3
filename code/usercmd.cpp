// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//  Delta-compression of usercmd_t's into UserCmdPacket
//

#include "stdafx.h"

//***UserCmd Packet Format***
// Compacts upto 3 usercmd_t's into 1 packet
// First byte is ePacketUserCmd (identity byte)
// Next 4 bytes (int) are last received snapshot num
// Next 24 bytes are the first usercmd_t
// Second usercmd :
// If there are still bytes left...
// Next byte is the diffence in serverTime (if 0xFF, the full int follows)
// Next byte is a bitfield : 1 if changed, 0 if unchanged
//  for each changed component, next x bytes are that component
// Third usercmd :
// same as second
// Max packet size = 81 bytes

#define WriteInt( bfKey, var ) \
	if(UserCmd[i].var != UserCmd[0].var ) { \
		*(bfChanged) |= bfKey; \
		*(int*)out = UserCmd[i].var; \
		out += sizeof(int); \
	}

#define WriteByte( bfKey, var ) \
	if(UserCmd[i].var != UserCmd[0].var ) { \
		*(bfChanged) |= bfKey; \
		*out = UserCmd[i].var; \
		++out; \
	}

BOOL c_Network::ClientSendUserCmd(usercmd_t *usercmd)
{
	HRESULT hr;
	BYTE UsercmdPacket[81];
	BYTE *out, *bfChanged;
	static usercmd_t UserCmd[3];
	static double LastSendTime = 0;
	double CurrentSendTime;
	int ServerTimeDiff;
	int i;

	++Client.LastUserCmd;
	if(Client.LastUserCmd>=CMD_BACKUP) Client.LastUserCmd = 0;

	//Make a copy for Q3 to reference
	memcpy( &Client.UserCmd[Client.LastUserCmd].UserCmd, usercmd, sizeof(usercmd_t) );
	++Client.CurrentUserCmdNum;
	Client.UserCmd[Client.LastUserCmd].UserCmdNum = Client.CurrentUserCmdNum;

	//Make a copy for delta-compression
	memcpy( &UserCmd[Client.UserCmdSendCount], usercmd, sizeof(usercmd_t) );
	++Client.UserCmdSendCount;

	//Only for remote bandwidth limited clients
	//Usercmd compressing causes slight jitter bugs
	if(!DQConsole.IsLocalServer() && Client.Ping>50) {
		//Send if we have 3 usercmd_t's, or 14ms have elapsed
		CurrentSendTime = DQTime.GetPerfMillisecs();

		if( (Client.UserCmdSendCount < 3) && (CurrentSendTime-LastSendTime<14) ) {
			LastSendTime = CurrentSendTime;
			return TRUE;
		}

		LastSendTime = CurrentSendTime;
	}

	//Send UserCmd Packet

	out = UsercmdPacket;
	*out = (U8)ePacketUserCmd;
	++out;

	EnterCriticalSection( &Client.csSnapshot );
	{
		*(int*)out = Client.CurrentSnapshotNum;

#if _DEBUG
		if(DQConsole.GetCVarInteger("cg_GetFullSnap", eAuthClient)) {
			*(int*)out = -1;
			DQConsole.SetCVarString("cg_GetFullSnap", "0", eAuthClient);
		}
#endif

		out += sizeof(int);
	}
	LeaveCriticalSection( &Client.csSnapshot );

	// Next 24 bytes are the first usercmd_t
	memcpy( out, usercmd, sizeof(usercmd_t) );
	out += sizeof(usercmd_t);

	for(i=1; i<3; ++i) {
		if(Client.UserCmdSendCount > i) {
			//Second & Third usercmd_t

			//Server Time
			ServerTimeDiff = UserCmd[i].serverTime-UserCmd[0].serverTime;
			if( ServerTimeDiff >= 255 || ServerTimeDiff < 0 ) {
				*out = 0xFF;
				++out;
				*(int*)out = ServerTimeDiff;
				out += sizeof(int);
			}
			else {
				*out = (U8)ServerTimeDiff;
				++out;
			}

			//Changed
			bfChanged = out;
			*bfChanged = 0;
			++out;
			WriteInt( 0x01, angles[0] );
			WriteInt( 0x02, angles[1] );
			WriteInt( 0x04, angles[2] );
			WriteInt( 0x08, buttons );
			WriteByte( 0x10, weapon );
			WriteByte( 0x20, forwardmove );
			WriteByte( 0x40, rightmove );
			WriteByte( 0x80, upmove );
		}
	}

	DPN_BUFFER_DESC BufferDesc;
	BufferDesc.dwBufferSize = (DWORD)out - (DWORD)UsercmdPacket;
	BufferDesc.pBufferData = (BYTE*)UsercmdPacket;

	hr = DirectPlayClient->Send( &BufferDesc, 1, 200, 
		NULL, &Client.ASyncSendUserCmd, 
		DPNSEND_NOCOMPLETE | DPNSEND_NONSEQUENTIAL | DPNSEND_PRIORITY_HIGH );

	if(FAILED(hr)) {
		DQPrint( "^1Disconnected from Server" );
		return FALSE;
	}

	Client.UserCmdSendCount = 0;		//reset

	return TRUE;
}

#undef WriteInt
#undef WriteByte

#define ReadInt( bfKey, var ) \
	if(*(bfChanged) & bfKey) { \
		UserCmd[i].var = *(int*)in; \
		in += sizeof(int); \
	}else { \
		UserCmd[i].var = UserCmd[0].var; \
	}

#define ReadByte( bfKey, var ) \
	if(*(bfChanged) & bfKey) { \
		UserCmd[i].var = *in; \
		++in; \
	}else { \
		UserCmd[i].var = UserCmd[0].var; \
	}

#define ReadSignedChar( bfKey, var ) \
	if(*(bfChanged) & bfKey) { \
		UserCmd[i].var = *(signed char*)in; \
		++in; \
	}else { \
		UserCmd[i].var = UserCmd[0].var; \
	}

void c_Network::ServerReceiveUserCmd( int ClientNum, void *UserCmdPacket, DWORD dwPacketSize )
{
	usercmd_t UserCmd[3];
	int i, SnapshotNum;
	BYTE *in,*bfChanged;
	U8 U8ServerTimeDiff;
	int ServerTimeDiff;

	in = (BYTE*)UserCmdPacket;

	if(dwPacketSize<sizeof(int)+sizeof(usercmd_t)) {
		Error( ERR_NETWORK, va("Corrupt usercmd packet from client %d",ClientNum) );
		return;
	}

	//Read in client's last received snapshot (used for delta-compression)
	SnapshotNum = *(int*)in;
	in += sizeof(int);

	memcpy( &UserCmd[0], in, sizeof(usercmd_t) );
	in += sizeof(usercmd_t);

	DQGame.ClientThink( ClientNum, &UserCmd[0], SnapshotNum );

	for(i=1; i<3; ++i) {
		
		//If there are no more bytes, return
		if((DWORD)in - (DWORD)UserCmdPacket>=dwPacketSize) {
			return;
		}

		//servertime
		U8ServerTimeDiff = *(U8*)in;
		++in;
		if(U8ServerTimeDiff == 0xFF) {
			ServerTimeDiff = *(int*)in;
			in += sizeof(int);
		}
		else {
			ServerTimeDiff = (int)U8ServerTimeDiff;
		}

		UserCmd[i].serverTime = UserCmd[0].serverTime + ServerTimeDiff;

		//everything else
		bfChanged = in;
		++in;
		ReadInt( 0x01, angles[0] );
		ReadInt( 0x02, angles[1] );
		ReadInt( 0x04, angles[2] );
		ReadInt( 0x08, buttons );
		ReadByte( 0x10, weapon );
		ReadSignedChar( 0x20, forwardmove );
		ReadSignedChar( 0x40, rightmove );
		ReadSignedChar( 0x80, upmove );

		DQGame.ClientThink( ClientNum, &UserCmd[i], SnapshotNum );
	}	
}

#undef ReadInt
#undef ReadByte
#undef ReadSignedChar