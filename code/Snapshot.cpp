// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
// c_Game::SendSnapshot
//

#include "stdafx.h"

#define GetGameEntity(i) ( (sharedEntity_t*)( (BYTE*)GameEntities+(i)*SizeofGameEntity ) )
#define GetPlayerState(i) ( (playerState_t*)( (BYTE*)PlayerStates+(i)*SizeofPlayerStates ) )

//Create a customised snapshot to send to each client
//Compress the snapshot_t struct as it's way too big to send every frame
void c_Game::SendSnapshot( int ClientNum )
{
	sharedEntity_t *ptrGameEnt;
	entityState_t *refEnt;
	playerState_t *ps;
	s_SnapshotPacket snap;
	s_BackupSnapshot *BackupSnapshot;
	s_BackupSnapshot *ReferenceSnapshot;
	int i,u,x,size, LastClientSnapNum;
	BYTE *pEntityPacket, *bfChangedComponents;
	short EntityNumber;
	
	snap.IdentityChar = ePacketSnapshot;
	snap.ThisSnapshotNum = LastSnapshotNum;			//already incremented by Update()

	snap.serverTime = ServerFrameTime;

	ps = GetPlayerState(ClientNum);

	//ServerCommands added by client to snapshot_t

	//Use Delta-Compression
	//We know the Client's last known received snapshot - delta-compress the snapshot_t against it
	EnterCriticalSection( &csLastSnapshotNum );
	{
		LastClientSnapNum = Client[ClientNum].LastReceivedSnapshotNum;
	}
	LeaveCriticalSection( &csLastSnapshotNum );

	if(LastClientSnapNum<0) { //no delta compress
		ReferenceSnapshot = NULL;
	}
	else {
		ReferenceSnapshot = &SnapshotArray[LastClientSnapNum%MAX_SNAPSHOTS];
		//double check this isn't an ultra-delayed snapshot
		if(ReferenceSnapshot->SnapshotNum!=LastClientSnapNum) {
			ReferenceSnapshot = NULL;					//send non-delta compressed snapshot
		}
	}

	BackupSnapshot = &SnapshotArray[LastSnapshotNum%MAX_SNAPSHOTS];
	if(BackupSnapshot == ReferenceSnapshot) ReferenceSnapshot = NULL;
	memcpy( &BackupSnapshot->PlayerState[ClientNum], ps, sizeof(playerState_t) );

	//DEBUG
//	ReferenceSnapshot = NULL;					//send non-delta compressed snapshot

	if(!ReferenceSnapshot) snap.ReferenceSnapshotNum = -1;	//no DeltaCompression
	else {
		snap.ReferenceSnapshotNum = LastClientSnapNum;
	}
	if(NumLinkedEntities<=0) return;			//don't send snapshot

	//PlayerStatePacket format
	//************************
	// 1st byte is 0x00 if full playerState_t struct is sent, 0xFF if delta compressed
	// if delta compressed :
	//	Next 13 bytes are a bitfield, 1 if changed, 0 if unchanged
	//	for each changed component, next x bytes are the new value (x=sizeof(component))

	pEntityPacket = (BYTE*)&snap.ps;
	if(ReferenceSnapshot) {
		*pEntityPacket = 0xFF;
		++pEntityPacket;
		MakePlayerStatePacket( &pEntityPacket, ps, &ReferenceSnapshot->PlayerState[ClientNum] );
	}
	else {
		*pEntityPacket = 0x00;
		++pEntityPacket;
		memcpy( pEntityPacket, ps, sizeof(playerState_t) );
		pEntityPacket += sizeof(playerState_t);
	}

	//EntityPacket format
	//*******************
	// If numEntities<=0, this isn't sent
	// for each entity :
	//	short EntityNumber
	//	if entity is not in ref snapshot, refent=ZeroEntity
	//  if entity is changed from ref snapshot, EntityNumber |= 0x4000
	//  if changed :
	//		4 byte bitfield : true if entityState_t component is changed, false if unchanged
	//		for each changed entityState_t component :
	//			next x bytes are the new value (x=sizeof(component))

	//SV Flags :
	//SVF_BROADCAST			- Send to all clients
	//SVF_NOCLIENT			- Send to no clients
	//SVF_SINGLECLIENT		- Only send to a single client (see entityShared_t->singleClient)
	//SVF_NOTSINGLECLIENT	- Send to everyone but single client (entityShared_t->singleClient)

	//SVF_CLIENTMASK		- Not implemented
	//SVF_PORTAL			- Merge a second PVS at origin2 into snapshot
	//SVF_USE_CURRENT_ORIGIN - IGNORE
	
	//Add modified linked entities to snapshot packet
	for(i=0,u=0;i<NumLinkedEntities;++i) {
		ptrGameEnt = GetGameEntity( LinkedEntities[i] );

		//Make Compressed Client-Customized snapshot

		//Find entity in ReferenceSnapshot
		refEnt = NULL;
		if(ReferenceSnapshot) {
			for(x=0;x<ReferenceSnapshot->NumEntities;++x) {
				if( LinkedEntities[i] == ReferenceSnapshot->Ent[x].s.number ) {

					//check svFlags
					if(ReferenceSnapshot->Ent[x].svFlags & SVF_NOCLIENT) continue;
					if(ReferenceSnapshot->Ent[x].svFlags & SVF_SINGLECLIENT) {
						if(ReferenceSnapshot->Ent[x].SingleClient != ClientNum) continue;
					}
					if(ReferenceSnapshot->Ent[x].svFlags & SVF_NOTSINGLECLIENT) {
						if(ReferenceSnapshot->Ent[x].SingleClient == ClientNum) continue;
					}

					refEnt = &ReferenceSnapshot->Ent[x].s;
					break;
				}
			}
		}

		if(ClientNum == LinkedEntities[i]) continue;	//Client Entity sent in playerState_t

		//SV FLAGS
		//**************

		//SVF_NOCLIENT			- Send to no clients
		if(ptrGameEnt->r.svFlags & SVF_NOCLIENT) {
			continue;
		}

		//SVF_SINGLECLIENT		- Only send to a single client (see entityShared_t->singleClient)
		if(ptrGameEnt->r.svFlags & SVF_SINGLECLIENT ) {
			if(ClientNum != ptrGameEnt->r.singleClient) {
				//Don't send this ent to current Client
				continue;
			}
		}

		//SVF_NOTSINGLECLIENT	- Send to everyone but single client (entityShared_t->singleClient)
		if(ptrGameEnt->r.svFlags & SVF_NOTSINGLECLIENT ) {
			if(ClientNum == ptrGameEnt->r.singleClient) {
				//Don't send to current Client
				continue;
			}
		}

		//Send this Entity
		//****************

		if(!refEnt) refEnt = &DQNetwork.ZeroEntity;		//if we're sending a new entity, delta-compress against zero entity

		//Check for modification
		EntityNumber = ptrGameEnt->s.number;
//		EntityNumber &= ~0x8000;				//make sure we've not marked as full entityState_t

		if(memcmp( &ptrGameEnt->s, refEnt, sizeof(entityState_t) )==0) {
			//identical
			*(short*)pEntityPacket = EntityNumber;
			pEntityPacket += sizeof(short);
			++u;
			continue;
		}

		EntityNumber |= 0x4000;				//mark as changed
		*(short*)pEntityPacket = EntityNumber;
		pEntityPacket += sizeof(short);

		//DELTA COMPRESS entityState_t
		//**********************************
		bfChangedComponents = pEntityPacket;
		ZeroMemory( bfChangedComponents, sizeof(BYTE)*4 );
		pEntityPacket += 4;

		//int number
/*			if( ptrGameEnt->s.number != refEnt->number ) {
			bfChangedComponents[0] |= 0x01;
			*(int*)pEntityPacket = ptrGameEnt->s.number;
			pEntityPacket += sizeof(int);
		}*/
		//int eType
		if( ptrGameEnt->s.eType != refEnt->eType ) {
			bfChangedComponents[0] |= 0x02;
			*(int*)pEntityPacket = ptrGameEnt->s.eType;
			pEntityPacket += sizeof(int);
		}
		//int eFlags
		if( ptrGameEnt->s.eFlags != refEnt->eFlags ) {
			bfChangedComponents[0] |= 0x04;
			*(int*)pEntityPacket = ptrGameEnt->s.eFlags;
			pEntityPacket += sizeof(int);
		}
		//trajectory_t	pos
		if( memcmp( &ptrGameEnt->s.pos, &refEnt->pos, sizeof(trajectory_t) )!=0) {
			bfChangedComponents[0] |= 0x08;

			*(BYTE*)pEntityPacket = ptrGameEnt->s.pos.trType;
			pEntityPacket += sizeof(BYTE);

			*(int*)pEntityPacket = ptrGameEnt->s.pos.trTime;
			pEntityPacket += sizeof(int);

			*(int*)pEntityPacket = ptrGameEnt->s.pos.trDuration;
			pEntityPacket += sizeof(int);

			((float*)pEntityPacket)[0] = ptrGameEnt->s.pos.trBase[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.pos.trBase[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.pos.trBase[2];
			pEntityPacket += 3*sizeof(float);

			((float*)pEntityPacket)[0] = ptrGameEnt->s.pos.trDelta[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.pos.trDelta[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.pos.trDelta[2];
			pEntityPacket += 3*sizeof(float);
		}
		//trajectory_t	apos
		if( memcmp( &ptrGameEnt->s.apos, &refEnt->apos, sizeof(trajectory_t) )!=0) {
			bfChangedComponents[0] |= 0x10;

			*(BYTE*)pEntityPacket = ptrGameEnt->s.apos.trType;
			pEntityPacket += sizeof(BYTE);

			*(int*)pEntityPacket = ptrGameEnt->s.apos.trTime;
			pEntityPacket += sizeof(int);

			*(int*)pEntityPacket = ptrGameEnt->s.apos.trDuration;
			pEntityPacket += sizeof(int);

			((float*)pEntityPacket)[0] = ptrGameEnt->s.apos.trBase[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.apos.trBase[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.apos.trBase[2];
			pEntityPacket += 3*sizeof(float);

			((float*)pEntityPacket)[0] = ptrGameEnt->s.apos.trDelta[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.apos.trDelta[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.apos.trDelta[2];
			pEntityPacket += 3*sizeof(float);
		}
		//int time
		if( ptrGameEnt->s.time != refEnt->time ) {
			bfChangedComponents[0] |= 0x20;
			*(int*)pEntityPacket = ptrGameEnt->s.time;
			pEntityPacket += sizeof(int);
		}
		//int time2
		if( ptrGameEnt->s.time2 != refEnt->time2 ) {
			bfChangedComponents[0] |= 0x40;
			*(int*)pEntityPacket = ptrGameEnt->s.time2;
			pEntityPacket += sizeof(int);
		}
		//vec3_t origin
		if( memcmp( ptrGameEnt->s.origin, refEnt->origin, sizeof(vec3_t) )!=0) {
			bfChangedComponents[0] |= 0x80;
			((float*)pEntityPacket)[0] = ptrGameEnt->s.origin[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.origin[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.origin[2];
			pEntityPacket += 3*sizeof(float);
		}
		//vec3_t origin2
		if( memcmp( ptrGameEnt->s.origin2, refEnt->origin2, sizeof(vec3_t) )!=0) {
			bfChangedComponents[1] |= 0x01;
			((float*)pEntityPacket)[0] = ptrGameEnt->s.origin2[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.origin2[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.origin2[2];
			pEntityPacket += 3*sizeof(float);
		}
		//vec3_t angles
		if( memcmp( ptrGameEnt->s.angles, refEnt->angles, sizeof(vec3_t) )!=0) {
			bfChangedComponents[1] |= 0x02;
			((float*)pEntityPacket)[0] = ptrGameEnt->s.angles[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.angles[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.angles[2];
			pEntityPacket += 3*sizeof(float);
		}
		//vec3_t angles2
		if( memcmp( ptrGameEnt->s.angles2, refEnt->angles2, sizeof(vec3_t) )!=0) {
			bfChangedComponents[1] |= 0x04;
			((float*)pEntityPacket)[0] = ptrGameEnt->s.angles2[0];
			((float*)pEntityPacket)[1] = ptrGameEnt->s.angles2[1];
			((float*)pEntityPacket)[2] = ptrGameEnt->s.angles2[2];
			pEntityPacket += 3*sizeof(float);
		}
		//int otherEntityNum
		if( ptrGameEnt->s.otherEntityNum != refEnt->otherEntityNum ) {
			bfChangedComponents[1] |= 0x08;
			*(int*)pEntityPacket = ptrGameEnt->s.otherEntityNum;
			pEntityPacket += sizeof(int);
		}
		//int otherEntityNum2
		if( ptrGameEnt->s.otherEntityNum2 != refEnt->otherEntityNum2 ) {
			bfChangedComponents[1] |= 0x10;
			*(int*)pEntityPacket = ptrGameEnt->s.otherEntityNum2;
			pEntityPacket += sizeof(int);
		}
		//int groundEntityNum
		if( ptrGameEnt->s.groundEntityNum != refEnt->groundEntityNum ) {
			bfChangedComponents[1] |= 0x20;
			*(int*)pEntityPacket = ptrGameEnt->s.groundEntityNum;
			pEntityPacket += sizeof(int);
		}
		//int constantLight
		if( ptrGameEnt->s.constantLight != refEnt->constantLight ) {
			bfChangedComponents[1] |= 0x40;
			*(int*)pEntityPacket = ptrGameEnt->s.constantLight;
			pEntityPacket += sizeof(int);
		}
		//int loopSound
		if( ptrGameEnt->s.loopSound != refEnt->loopSound ) {
			bfChangedComponents[1] |= 0x80;
			*(int*)pEntityPacket = ptrGameEnt->s.loopSound;
			pEntityPacket += sizeof(int);
		}
		//int modelindex
		if( ptrGameEnt->s.modelindex != refEnt->modelindex ) {
			bfChangedComponents[2] |= 0x01;
			*(int*)pEntityPacket = ptrGameEnt->s.modelindex;
			pEntityPacket += sizeof(int);
		}
		//int modelindex2
		if( ptrGameEnt->s.modelindex2 != refEnt->modelindex2 ) {
			bfChangedComponents[2] |= 0x02;
			*(int*)pEntityPacket = ptrGameEnt->s.modelindex2;
			pEntityPacket += sizeof(int);
		}
		//int clientNum
		if( ptrGameEnt->s.clientNum != refEnt->clientNum ) {
			bfChangedComponents[2] |= 0x04;
			*(int*)pEntityPacket = ptrGameEnt->s.clientNum;
			pEntityPacket += sizeof(int);
		}
		//int frame
		if( ptrGameEnt->s.frame != refEnt->frame ) {
			bfChangedComponents[2] |= 0x08;
			*(int*)pEntityPacket = ptrGameEnt->s.frame;
			pEntityPacket += sizeof(int);
		}
		//int solid
		if( ptrGameEnt->s.solid != refEnt->solid ) {
			bfChangedComponents[2] |= 0x10;
			*(int*)pEntityPacket = ptrGameEnt->s.solid;
			pEntityPacket += sizeof(int);
		}
		//int event
		if( ptrGameEnt->s.event != refEnt->event ) {
			bfChangedComponents[2] |= 0x20;
			*(int*)pEntityPacket = ptrGameEnt->s.event;
			pEntityPacket += sizeof(int);
		}
		//int eventParm
		if( ptrGameEnt->s.eventParm != refEnt->eventParm ) {
			bfChangedComponents[2] |= 0x40;
			*(int*)pEntityPacket = ptrGameEnt->s.eventParm;
			pEntityPacket += sizeof(int);
		}
		//int powerups
		if( ptrGameEnt->s.powerups != refEnt->powerups ) {
			bfChangedComponents[2] |= 0x80;
			*(int*)pEntityPacket = ptrGameEnt->s.powerups;
			pEntityPacket += sizeof(int);
		}
		//int weapon
		if( ptrGameEnt->s.weapon != refEnt->weapon ) {
			bfChangedComponents[3] |= 0x01;
			*(int*)pEntityPacket = ptrGameEnt->s.weapon;
			pEntityPacket += sizeof(int);
		}
		//int legsAnim
		if( ptrGameEnt->s.legsAnim != refEnt->legsAnim ) {
			bfChangedComponents[3] |= 0x02;
			*(int*)pEntityPacket = ptrGameEnt->s.legsAnim;
			pEntityPacket += sizeof(int);
		}
		//int torsoAnim
		if( ptrGameEnt->s.torsoAnim != refEnt->torsoAnim ) {
			bfChangedComponents[3] |= 0x04;
			*(int*)pEntityPacket = ptrGameEnt->s.torsoAnim;
			pEntityPacket += sizeof(int);
		}
		//int generic1
		if( ptrGameEnt->s.generic1 != refEnt->generic1 ) {
			bfChangedComponents[3] |= 0x08;
			*(int*)pEntityPacket = ptrGameEnt->s.generic1;
			pEntityPacket += sizeof(int);
		}

/*		} //if(refent)

		else {
			EntityNumber |= 0x8000; //Flag as full entityState_t

			*(short*)pEntityPacket = EntityNumber;
			pEntityPacket += sizeof(short);

			memcpy( pEntityPacket, &ptrGameEnt->s, sizeof(entityState_t) );
			pEntityPacket += sizeof(entityState_t);

			DQPrint( "Send New Entity" );
		}*/
		++u;
	} //for each linked entity

	snap.numEntities = u;

	size = (int)pEntityPacket-(int)&snap;

	if(g_NetworkInfo.integer) {
		DQPrint( va("Server Network Info : Client %d Snapshot %d  size : %d, Relative ref snapshot %d", ClientNum, LastSnapshotNum, size, LastSnapshotNum-LastClientSnapNum ) );
	}

//	DQNetwork.ServerSendTo( ClientNum, &snap, size, 90, DPNSEND_NONSEQUENTIAL | DPNSEND_PRIORITY_HIGH | DPNSEND_NOCOMPLETE);
	DQNetwork.ServerSendSnapshot( ClientNum, &snap, size );
}

//*****************************************************************

	//PlayerStatePacket format
	//************************
	// 1st byte is 0x00 if full playerState_t struct is sent, 0xFF if delta compressed
	// if delta compressed :
	//	Next 13 bytes are a bitfield, 1 if changed, 0 if unchanged
	//	for each changed component, next x bytes are the new value (x=sizeof(component))

#define WriteInt( bf, bfKey, var ) \
	if(psNew->var != psOld->var ) { \
		*(bf) |= bfKey; \
		*(int*)out = psNew->var; \
		out += sizeof(int); \
	}

#define WriteFloat3( bf, bfKey, var ) \
	if( (psNew->var[0] != psOld->var[0]) || (psNew->var[1] != psOld->var[1]) || (psNew->var[2] != psOld->var[2]) ) { \
		*(bf) |= bfKey; \
		((float*)out)[0] = psNew->var[0]; \
		((float*)out)[1] = psNew->var[1]; \
		((float*)out)[2] = psNew->var[2]; \
		out += 3*sizeof(float); \
	}

#define WriteInt3( bf, bfKey, var ) \
	if( (psNew->var[0] != psOld->var[0]) || (psNew->var[1] != psOld->var[1]) || (psNew->var[2] != psOld->var[2]) ) { \
		*(bf) |= bfKey; \
		((int*)out)[0] = psNew->var[0]; \
		((int*)out)[1] = psNew->var[1]; \
		((int*)out)[2] = psNew->var[2]; \
		out += 3*sizeof(int); \
	}

void c_Game::MakePlayerStatePacket( BYTE **pOut, playerState_t *psNew, playerState_t *psOld )
{
	BYTE *bfChanged, *out;

	if( !pOut || !psNew || !psOld ) {
		Error( ERR_MISC, "c_Game::MakePlayerStatePacket - Invalid parameters" );
		return;
	}

	out = *pOut;
	bfChanged = out;
	out += 13;
	ZeroMemory( bfChanged, 13 );

	WriteInt( bfChanged, 0x01, commandTime )
	WriteInt( bfChanged, 0x02, pm_type )
	WriteInt( bfChanged, 0x04, bobCycle )
	WriteInt( bfChanged, 0x08, pm_flags )
	WriteInt( bfChanged, 0x10, pm_time )

	WriteFloat3( bfChanged, 0x20, origin )
	WriteFloat3( bfChanged, 0x40, velocity )
	WriteInt( bfChanged, 0x80, weaponTime )
	WriteInt( bfChanged+1, 0x01, gravity )
	WriteInt( bfChanged+1, 0x02, speed )
	WriteInt3( bfChanged+1, 0x04, delta_angles )

	WriteInt( bfChanged+1, 0x08, groundEntityNum )
	WriteInt( bfChanged+1, 0x10, legsTimer )
	WriteInt( bfChanged+1, 0x20, legsAnim )
	WriteInt( bfChanged+1, 0x40, torsoTimer )
	WriteInt( bfChanged+1, 0x80, torsoAnim )

	WriteInt( bfChanged+2, 0x01, movementDir )
	WriteFloat3( bfChanged+2, 0x02, grapplePoint )
	WriteInt( bfChanged+2, 0x04, eFlags )
	WriteInt( bfChanged+2, 0x08, eventSequence )
	WriteInt( bfChanged+2, 0x10, events[0] )
	WriteInt( bfChanged+2, 0x20, events[1] )
	WriteInt( bfChanged+2, 0x40, eventParms[0] )
	WriteInt( bfChanged+2, 0x80, eventParms[1] )
	WriteInt( bfChanged+3, 0x01, externalEvent )
	WriteInt( bfChanged+3, 0x02, externalEventParm )
	WriteInt( bfChanged+3, 0x04, externalEventTime )

	WriteInt( bfChanged+3, 0x08, clientNum )
	WriteInt( bfChanged+3, 0x10, weapon )
	WriteInt( bfChanged+3, 0x20, weaponstate )
	WriteFloat3( bfChanged+3, 0x40, viewangles )
	WriteInt( bfChanged+3, 0x80, viewheight )

	WriteInt( bfChanged+4, 0x01, damageEvent )
	WriteInt( bfChanged+4, 0x02, damageYaw )
	WriteInt( bfChanged+4, 0x04, damagePitch )
	WriteInt( bfChanged+4, 0x08, damageCount )

	WriteInt( bfChanged+4, 0x10, stats[0] )
	WriteInt( bfChanged+4, 0x20, stats[1] )
	WriteInt( bfChanged+4, 0x40, stats[2] )
	WriteInt( bfChanged+4, 0x80, stats[3] )
	WriteInt( bfChanged+5, 0x01, stats[4] )
	WriteInt( bfChanged+5, 0x02, stats[5] )
	WriteInt( bfChanged+5, 0x04, stats[6] )
	WriteInt( bfChanged+5, 0x08, stats[7] )
	WriteInt( bfChanged+5, 0x10, stats[8] )
	WriteInt( bfChanged+5, 0x20, stats[9] )
	WriteInt( bfChanged+5, 0x40, stats[10] )
	WriteInt( bfChanged+5, 0x80, stats[11] )
	WriteInt( bfChanged+6, 0x01, stats[12] )
	WriteInt( bfChanged+6, 0x02, stats[13] )
	WriteInt( bfChanged+6, 0x04, stats[14] )
	WriteInt( bfChanged+6, 0x08, stats[15] )

	WriteInt( bfChanged+6, 0x10, persistant[0] )
	WriteInt( bfChanged+6, 0x20, persistant[1] )
	WriteInt( bfChanged+6, 0x40, persistant[2] )
	WriteInt( bfChanged+6, 0x80, persistant[3] )
	WriteInt( bfChanged+7, 0x01, persistant[4] )
	WriteInt( bfChanged+7, 0x02, persistant[5] )
	WriteInt( bfChanged+7, 0x04, persistant[6] )
	WriteInt( bfChanged+7, 0x08, persistant[7] )
	WriteInt( bfChanged+7, 0x10, persistant[8] )
	WriteInt( bfChanged+7, 0x20, persistant[9] )
	WriteInt( bfChanged+7, 0x40, persistant[10] )
	WriteInt( bfChanged+7, 0x80, persistant[11] )
	WriteInt( bfChanged+8, 0x01, persistant[12] )
	WriteInt( bfChanged+8, 0x02, persistant[13] )
	WriteInt( bfChanged+8, 0x04, persistant[14] )
	WriteInt( bfChanged+8, 0x08, persistant[15] )

	WriteInt( bfChanged+8, 0x10, powerups[0] )
	WriteInt( bfChanged+8, 0x20, powerups[1] )
	WriteInt( bfChanged+8, 0x40, powerups[2] )
	WriteInt( bfChanged+8, 0x80, powerups[3] )
	WriteInt( bfChanged+9, 0x01, powerups[4] )
	WriteInt( bfChanged+9, 0x02, powerups[5] )
	WriteInt( bfChanged+9, 0x04, powerups[6] )
	WriteInt( bfChanged+9, 0x08, powerups[7] )
	WriteInt( bfChanged+9, 0x10, powerups[8] )
	WriteInt( bfChanged+9, 0x20, powerups[9] )
	WriteInt( bfChanged+9, 0x40, powerups[10] )
	WriteInt( bfChanged+9, 0x80, powerups[11] )
	WriteInt( bfChanged+10, 0x01, powerups[12] )
	WriteInt( bfChanged+10, 0x02, powerups[13] )
	WriteInt( bfChanged+10, 0x04, powerups[14] )
	WriteInt( bfChanged+10, 0x08, powerups[15] )

	WriteInt( bfChanged+10, 0x10, ammo[0] )
	WriteInt( bfChanged+10, 0x20, ammo[1] )
	WriteInt( bfChanged+10, 0x40, ammo[2] )
	WriteInt( bfChanged+10, 0x80, ammo[3] )
	WriteInt( bfChanged+11, 0x01, ammo[4] )
	WriteInt( bfChanged+11, 0x02, ammo[5] )
	WriteInt( bfChanged+11, 0x04, ammo[6] )
	WriteInt( bfChanged+11, 0x08, ammo[7] )
	WriteInt( bfChanged+11, 0x10, ammo[8] )
	WriteInt( bfChanged+11, 0x20, ammo[9] )
	WriteInt( bfChanged+11, 0x40, ammo[10] )
	WriteInt( bfChanged+11, 0x80, ammo[11] )
	WriteInt( bfChanged+12, 0x01, ammo[12] )
	WriteInt( bfChanged+12, 0x02, ammo[13] )
	WriteInt( bfChanged+12, 0x04, ammo[14] )
	WriteInt( bfChanged+12, 0x08, ammo[15] )

	WriteInt( bfChanged+12, 0x10, generic1 )
	WriteInt( bfChanged+12, 0x20, loopSound )
	WriteInt( bfChanged+12, 0x40, jumppad_ent )

	*pOut = out;
}

#undef WriteInt
#undef WriteFloat3

//********************************************************************

#define ReadInt( bf, bfKey, var ) \
	if(*(bf) & bfKey) { \
		psNew->var = *(int*)in; \
		in += sizeof(int); \
	}else { \
		psNew->var = psRef->var; \
	}

#define ReadFloat3( bf, bfKey, var ) \
	if(*(bf) & bfKey) { \
		psNew->var[0] = ((float*)in)[0]; \
		psNew->var[1] = ((float*)in)[1]; \
		psNew->var[2] = ((float*)in)[2]; \
		in += 3*sizeof(float); \
	}else { \
		psNew->var[0] = psRef->var[0]; \
		psNew->var[1] = psRef->var[1]; \
		psNew->var[2] = psRef->var[2]; \
	}

#define ReadInt3( bf, bfKey, var ) \
	if(*(bf) & bfKey) { \
		psNew->var[0] = ((int*)in)[0]; \
		psNew->var[1] = ((int*)in)[1]; \
		psNew->var[2] = ((int*)in)[2]; \
		in += 3*sizeof(int); \
	}else { \
		psNew->var[0] = psRef->var[0]; \
		psNew->var[1] = psRef->var[1]; \
		psNew->var[2] = psRef->var[2]; \
	}

void c_Game::ReadPlayerStatePacket( BYTE **pIn, playerState_t *psNew, playerState_t *psRef )
{
	BYTE *bfChanged, *in;

	if( !pIn || !psNew || !psRef ) {
		Error( ERR_MISC, "c_Game::ReadPlayerStatePacket - Invalid parameters" );
		return;
	}

	in = *pIn;
	bfChanged = in;
	in += 13;

	ReadInt( bfChanged, 0x01, commandTime )
	ReadInt( bfChanged, 0x02, pm_type )
	ReadInt( bfChanged, 0x04, bobCycle )
	ReadInt( bfChanged, 0x08, pm_flags )
	ReadInt( bfChanged, 0x10, pm_time )

	ReadFloat3( bfChanged, 0x20, origin )
	ReadFloat3( bfChanged, 0x40, velocity )
	ReadInt( bfChanged, 0x80, weaponTime )
	ReadInt( bfChanged+1, 0x01, gravity )
	ReadInt( bfChanged+1, 0x02, speed )
	ReadInt3( bfChanged+1, 0x04, delta_angles )

	ReadInt( bfChanged+1, 0x08, groundEntityNum )
	ReadInt( bfChanged+1, 0x10, legsTimer )
	ReadInt( bfChanged+1, 0x20, legsAnim )
	ReadInt( bfChanged+1, 0x40, torsoTimer )
	ReadInt( bfChanged+1, 0x80, torsoAnim )

	ReadInt( bfChanged+2, 0x01, movementDir )
	ReadFloat3( bfChanged+2, 0x02, grapplePoint )
	ReadInt( bfChanged+2, 0x04, eFlags )
	ReadInt( bfChanged+2, 0x08, eventSequence )
	ReadInt( bfChanged+2, 0x10, events[0] )
	ReadInt( bfChanged+2, 0x20, events[1] )
	ReadInt( bfChanged+2, 0x40, eventParms[0] )
	ReadInt( bfChanged+2, 0x80, eventParms[1] )
	ReadInt( bfChanged+3, 0x01, externalEvent )
	ReadInt( bfChanged+3, 0x02, externalEventParm )
	ReadInt( bfChanged+3, 0x04, externalEventTime )

	ReadInt( bfChanged+3, 0x08, clientNum )
	ReadInt( bfChanged+3, 0x10, weapon )
	ReadInt( bfChanged+3, 0x20, weaponstate )
	ReadFloat3( bfChanged+3, 0x40, viewangles )
	ReadInt( bfChanged+3, 0x80, viewheight )

	ReadInt( bfChanged+4, 0x01, damageEvent )
	ReadInt( bfChanged+4, 0x02, damageYaw )
	ReadInt( bfChanged+4, 0x04, damagePitch )
	ReadInt( bfChanged+4, 0x08, damageCount )

	ReadInt( bfChanged+4, 0x10, stats[0] )
	ReadInt( bfChanged+4, 0x20, stats[1] )
	ReadInt( bfChanged+4, 0x40, stats[2] )
	ReadInt( bfChanged+4, 0x80, stats[3] )
	ReadInt( bfChanged+5, 0x01, stats[4] )
	ReadInt( bfChanged+5, 0x02, stats[5] )
	ReadInt( bfChanged+5, 0x04, stats[6] )
	ReadInt( bfChanged+5, 0x08, stats[7] )
	ReadInt( bfChanged+5, 0x10, stats[8] )
	ReadInt( bfChanged+5, 0x20, stats[9] )
	ReadInt( bfChanged+5, 0x40, stats[10] )
	ReadInt( bfChanged+5, 0x80, stats[11] )
	ReadInt( bfChanged+6, 0x01, stats[12] )
	ReadInt( bfChanged+6, 0x02, stats[13] )
	ReadInt( bfChanged+6, 0x04, stats[14] )
	ReadInt( bfChanged+6, 0x08, stats[15] )

	ReadInt( bfChanged+6, 0x10, persistant[0] )
	ReadInt( bfChanged+6, 0x20, persistant[1] )
	ReadInt( bfChanged+6, 0x40, persistant[2] )
	ReadInt( bfChanged+6, 0x80, persistant[3] )
	ReadInt( bfChanged+7, 0x01, persistant[4] )
	ReadInt( bfChanged+7, 0x02, persistant[5] )
	ReadInt( bfChanged+7, 0x04, persistant[6] )
	ReadInt( bfChanged+7, 0x08, persistant[7] )
	ReadInt( bfChanged+7, 0x10, persistant[8] )
	ReadInt( bfChanged+7, 0x20, persistant[9] )
	ReadInt( bfChanged+7, 0x40, persistant[10] )
	ReadInt( bfChanged+7, 0x80, persistant[11] )
	ReadInt( bfChanged+8, 0x01, persistant[12] )
	ReadInt( bfChanged+8, 0x02, persistant[13] )
	ReadInt( bfChanged+8, 0x04, persistant[14] )
	ReadInt( bfChanged+8, 0x08, persistant[15] )

	ReadInt( bfChanged+8, 0x10, powerups[0] )
	ReadInt( bfChanged+8, 0x20, powerups[1] )
	ReadInt( bfChanged+8, 0x40, powerups[2] )
	ReadInt( bfChanged+8, 0x80, powerups[3] )
	ReadInt( bfChanged+9, 0x01, powerups[4] )
	ReadInt( bfChanged+9, 0x02, powerups[5] )
	ReadInt( bfChanged+9, 0x04, powerups[6] )
	ReadInt( bfChanged+9, 0x08, powerups[7] )
	ReadInt( bfChanged+9, 0x10, powerups[8] )
	ReadInt( bfChanged+9, 0x20, powerups[9] )
	ReadInt( bfChanged+9, 0x40, powerups[10] )
	ReadInt( bfChanged+9, 0x80, powerups[11] )
	ReadInt( bfChanged+10, 0x01, powerups[12] )
	ReadInt( bfChanged+10, 0x02, powerups[13] )
	ReadInt( bfChanged+10, 0x04, powerups[14] )
	ReadInt( bfChanged+10, 0x08, powerups[15] )

	ReadInt( bfChanged+10, 0x10, ammo[0] )
	ReadInt( bfChanged+10, 0x20, ammo[1] )
	ReadInt( bfChanged+10, 0x40, ammo[2] )
	ReadInt( bfChanged+10, 0x80, ammo[3] )
	ReadInt( bfChanged+11, 0x01, ammo[4] )
	ReadInt( bfChanged+11, 0x02, ammo[5] )
	ReadInt( bfChanged+11, 0x04, ammo[6] )
	ReadInt( bfChanged+11, 0x08, ammo[7] )
	ReadInt( bfChanged+11, 0x10, ammo[8] )
	ReadInt( bfChanged+11, 0x20, ammo[9] )
	ReadInt( bfChanged+11, 0x40, ammo[10] )
	ReadInt( bfChanged+11, 0x80, ammo[11] )
	ReadInt( bfChanged+12, 0x01, ammo[12] )
	ReadInt( bfChanged+12, 0x02, ammo[13] )
	ReadInt( bfChanged+12, 0x04, ammo[14] )
	ReadInt( bfChanged+12, 0x08, ammo[15] )

	ReadInt( bfChanged+12, 0x10, generic1 )
	ReadInt( bfChanged+12, 0x20, loopSound )
	ReadInt( bfChanged+12, 0x40, jumppad_ent )

	*pIn = in;
}

#undef ReadInt
#undef ReadFloat3
#undef ReadInt3

#undef GetGameEntity
#undef GetPlayerState