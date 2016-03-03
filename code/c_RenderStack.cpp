// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	Any items to be drawn are added onto the render stack
//	This allows all render object sorting to be done in one place
//	and allows optimisations like using only 2 vertex buffers

#include "stdafx.h"

//*****************************************************************************

c_RenderStack::c_RenderStack():CurrentFrame(DQRender.FrameNum),
	OpaqueSortStack(MAX_GROUPS_PER_SORTEDSTACK), BlendedSortStack(MAX_GROUPS_PER_SORTEDSTACK), ConsoleSortStack(10), 
	SpriteSortStack(MAX_GROUPS_PER_SORTEDSTACK), SkyboxSortStack(10)
{
	Decl_P0T1 = Decl_P0N1T2 = Decl_P0D1T2 = Decl_P0N1D2T3 = Decl_P0T1T2 = Decl_P0N1T2T3
		 = Decl_P0D1T2T3 = Decl_P0N1D2T3T4 = NULL;

	ZeroMemory( &StaticBuffer, sizeof(s_Buffer) );
	ZeroMemory( &DynamicBuffer, sizeof(s_Buffer) );

	bProcessingBillboards = FALSE;

	DQConsole.RegisterCVar( "r_Wireframe", "0", 0, eAuthClient );

	Unload();
}

c_RenderStack::~c_RenderStack()
{
	Unload();
}

void c_RenderStack::Unload()
{
	UnlockBuffers();

	SafeRelease( Decl_P0T1 );
	SafeRelease( Decl_P0N1T2 );
	SafeRelease( Decl_P0D1T2 );
	SafeRelease( Decl_P0N1D2T3 );
	SafeRelease( Decl_P0T1T2 );
	SafeRelease( Decl_P0N1T2T3 );
	SafeRelease( Decl_P0D1T2T3 );
	SafeRelease( Decl_P0N1D2T3T4 );

	SafeRelease( StaticBuffer.VB_P );
	SafeRelease( StaticBuffer.VB_N );
	SafeRelease( StaticBuffer.VB_D );
	SafeRelease( StaticBuffer.VB_T1 );
	SafeRelease( StaticBuffer.VB_T2 );
	SafeRelease( StaticBuffer.IndexBuffer );

	SafeRelease( DynamicBuffer.VB_P );
	SafeRelease( DynamicBuffer.VB_N );
	SafeRelease( DynamicBuffer.VB_D );
	SafeRelease( DynamicBuffer.VB_T1 );
	SafeRelease( DynamicBuffer.VB_T2 );
	SafeRelease( DynamicBuffer.IndexBuffer );

	ZeroMemory( &StaticBuffer, sizeof(s_Buffer) );
	ZeroMemory( &DynamicBuffer, sizeof(s_Buffer) );

	ZeroMemory( &VBHandles, sizeof(VBHandles) );
	ZeroMemory( &StaticIBHandles, sizeof(StaticIBHandles) );
//	ZeroMemory( &DynamicIBHandles, sizeof(DynamicIBHandles) );

	NextVBHandle = 1;
	NextStaticIBHandle = 1;
//	NextDynamicIBHandle = 0;		//DynamicIBhandle = ~IBhandle

	ResetSortStack( &OpaqueSortStack );
	ResetSortStack( &BlendedSortStack );
	ResetSortStack( &ConsoleSortStack );
	ResetSortStack( &SpriteSortStack );
	ResetSortStack( &SkyboxSortStack );

	NextTM = 1;
	D3DXMatrixIdentity( &TMArray[0] );	//0 is identity
	OverrideShaderHandle = 0;

	ZeroMemory( &NullRenderFeatures, sizeof(s_RenderFeatureInfo) );

	useStreamOffsets = FALSE;
}

void c_RenderStack::Clear()
{
	UnlockBuffers();

	ResetSortStack( &OpaqueSortStack );
	ResetSortStack( &BlendedSortStack );
	ResetSortStack( &ConsoleSortStack );
	ResetSortStack( &SpriteSortStack );
	ResetSortStack( &SkyboxSortStack );

	ShaderGroupHeap.Reset();
	ItemHeap.Reset();

	NextTM = 1;
}

//Save the handles so we can reset the stack state to unload the level
void c_RenderStack::SaveState()
{
	SavedVBHandles = NextVBHandle;
	SavedStaticIB = NextStaticIBHandle;
}
void c_RenderStack::ResetState()
{
	Clear();
	NextVBHandle = SavedVBHandles;
	NextStaticIBHandle = NextStaticIBHandle;
}

void inline c_RenderStack::LockStaticVB() { 
	if(!StaticBuffer.isVBLocked) { 
		d3dcheckOK( StaticBuffer.VB_P->Lock ( 0, 0, (void**)&StaticBuffer.VertexP, 0 ) );
		d3dcheckOK( StaticBuffer.VB_N->Lock	( 0, 0, (void**)&StaticBuffer.VertexN, 0 ) );
		d3dcheckOK( StaticBuffer.VB_D->Lock	( 0, 0, (void**)&StaticBuffer.VertexD, 0 ) );
		d3dcheckOK( StaticBuffer.VB_T1->Lock( 0, 0, (void**)&StaticBuffer.VertexT1, 0 ) );
		d3dcheckOK( StaticBuffer.VB_T2->Lock( 0, 0, (void**)&StaticBuffer.VertexT2, 0 ) );
		StaticBuffer.isVBLocked = TRUE;
	}
}
void inline c_RenderStack::LockDynamicVB() { 
	if(!DynamicBuffer.isVBLocked) { 
		d3dcheckOK( DynamicBuffer.VB_P->Lock	( 0, 0, (void**)&DynamicBuffer.VertexP, D3DLOCK_DISCARD ) );
		d3dcheckOK( DynamicBuffer.VB_N->Lock	( 0, 0, (void**)&DynamicBuffer.VertexN, D3DLOCK_DISCARD ) );
		d3dcheckOK( DynamicBuffer.VB_D->Lock	( 0, 0, (void**)&DynamicBuffer.VertexD, D3DLOCK_DISCARD ) );
		d3dcheckOK( DynamicBuffer.VB_T1->Lock	( 0, 0, (void**)&DynamicBuffer.VertexT1, D3DLOCK_DISCARD ) );
		d3dcheckOK( DynamicBuffer.VB_T2->Lock	( 0, 0, (void**)&DynamicBuffer.VertexT2, D3DLOCK_DISCARD ) );
		DynamicBuffer.isVBLocked = TRUE;
	}
}
void inline c_RenderStack::LockStaticIB() { 
	if(!StaticBuffer.isIBLocked) { 
		d3dcheckOK( StaticBuffer.IndexBuffer->Lock	( 0, 0, (void**)&StaticBuffer.Index, 0 ) );
		StaticBuffer.isIBLocked = TRUE;
	}
}
/*
void inline c_RenderStack::LockDynamicIB() { 
	if(!DynamicBuffer.isIBLocked) { 
		d3dcheckOK( DynamicBuffer.IndexBuffer->Lock	( 0, 0, (void**)&DynamicBuffer.Index, D3DLOCK_DISCARD ) );
		DynamicBuffer.NextIBPos = 0;
		DynamicBuffer.isIBLocked = TRUE;
	}
}*/
void inline c_RenderStack::UnlockBuffers()
{
	if(StaticBuffer.isVBLocked) {
		d3dcheckOK( StaticBuffer.VB_P->Unlock() );
		d3dcheckOK( StaticBuffer.VB_N->Unlock() );
		d3dcheckOK( StaticBuffer.VB_D->Unlock() );
		d3dcheckOK( StaticBuffer.VB_T1->Unlock() );
		d3dcheckOK( StaticBuffer.VB_T2->Unlock() );
		StaticBuffer.isVBLocked = FALSE;
	}
	if(StaticBuffer.isIBLocked) {
		d3dcheckOK( StaticBuffer.IndexBuffer->Unlock() );
		StaticBuffer.isIBLocked = FALSE;
	}
	if(DynamicBuffer.isVBLocked) {
		d3dcheckOK( DynamicBuffer.VB_P->Unlock() );
		d3dcheckOK( DynamicBuffer.VB_N->Unlock() );
		d3dcheckOK( DynamicBuffer.VB_D->Unlock() );
		d3dcheckOK( DynamicBuffer.VB_T1->Unlock() );
		d3dcheckOK( DynamicBuffer.VB_T2->Unlock() );
		DynamicBuffer.isVBLocked = FALSE;
	}
/*	if(DynamicBuffer.isIBLocked) {
		d3dcheckOK( DynamicBuffer.IndexBuffer->Unlock() );
		DynamicBuffer.isIBLocked = FALSE;
	}*/
}

c_RenderStack::eVertDecl inline c_RenderStack::GetVertexDeclaration( c_Shader *Shader )
{
	int Flags;

	//Set appropriate Vertex Declaration depending on Shader Flags
	//************************************************************
	Flags = Shader->GetFlags();

	if(Flags & ShaderFlag_UseNormals) {
		if(Flags & ShaderFlag_UseDiffuse) {
			if(Flags & ShaderFlag_UseLightmap) {
				return eVD_P0N1D2T3T4;
			}
			else {
				return eVD_P0N1D2T3;
			}
		}
		else {	//No diffuse
			if(Flags & ShaderFlag_UseLightmap) {
				return eVD_P0N1T2T3;
			}
			else {
				return eVD_P0N1T2;
			}
		}
	}
	else {	//no Normals
		if(Flags & ShaderFlag_UseDiffuse) {
			if(Flags & ShaderFlag_UseLightmap) {
				return eVD_P0D1T2T3;
			}
			else {
				return eVD_P0D1T2;
			}
		}
		else {	//No diffuse
			if(Flags & ShaderFlag_UseLightmap) {
				return eVD_P0T1T2;
			}
			else {
				return eVD_P0T1;
			}
		}
	}
}

LPDIRECT3DVERTEXDECLARATION9 inline c_RenderStack::GetVertexDeclaration( eVertDecl Decl )
{
	switch(Decl) {
	case eVD_P0T1:
		return Decl_P0T1;
	case eVD_P0N1T2:
		return Decl_P0N1T2;
	case eVD_P0D1T2:
		return Decl_P0D1T2;
	case eVD_P0N1D2T3:
		return Decl_P0N1D2T3;
	case eVD_P0T1T2:
		return Decl_P0T1T2;
	case eVD_P0N1T2T3:
		return Decl_P0N1T2T3;
	case eVD_P0D1T2T3:
		return Decl_P0D1T2T3;
	case eVD_P0N1D2T3T4:
		return Decl_P0N1D2T3T4;
	default:
		CriticalError( ERR_RENDER, "Unknown eVertDecl format" );
		return Decl_P0T1;
	};
}

//*********************************************************************************

void c_RenderStack::Initialise()
{
	int CurveSubdivisions;

	CurveSubdivisions = DQConsole.GetCVarInteger( "r_subdivisions", eAuthClient );
	if( CurveSubdivisions<=4 ) CurveSubdivisions = 4;
	else if( CurveSubdivisions<=8 ) CurveSubdivisions = 8;
	else CurveSubdivisions = 16;
	DQConsole.SetCVarValue( "r_subdivisions", CurveSubdivisions, eAuthClient );

	MaxDynamicVerts = 80000;

	switch(CurveSubdivisions) {
	case 4:
		MaxStaticVerts = 80000;
		MaxStaticIndices = 200000;
		break;
	case 8:
		MaxStaticVerts = 120000;
		MaxStaticIndices = 250000;
		break;
	case 16:
	default:
		MaxStaticVerts = 300000;
		MaxStaticIndices = 650000;
		break;
	};

	//******Init Vertex Declarations***********************************************
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0T1,		&Decl_P0T1 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0N1T2,		&Decl_P0N1T2 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0D1T2,		&Decl_P0D1T2 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0N1D2T3,	&Decl_P0N1D2T3 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0T1T2,		&Decl_P0T1T2 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0N1T2T3,	&Decl_P0N1T2T3 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0D1T2T3,	&Decl_P0D1T2T3 ) );
	d3dcheckOK( DQDevice->CreateVertexDeclaration( ElementDecl_P0N1D2T3T4,	&Decl_P0N1D2T3T4 ) );
	//******End Init Vertex Declarations*******************************************

	//******Init D3D Static & Dynamic Buffers**************************************

	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxStaticVerts*sizeof(s_VertexStreamP), D3DUSAGE_WRITEONLY, 0, 
		D3DPOOL_DEFAULT, &StaticBuffer.VB_P, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxStaticVerts*sizeof(s_VertexStreamN), D3DUSAGE_WRITEONLY, 0, 
		D3DPOOL_DEFAULT, &StaticBuffer.VB_N, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxStaticVerts*sizeof(s_VertexStreamD), D3DUSAGE_WRITEONLY, 0, 
		D3DPOOL_DEFAULT, &StaticBuffer.VB_D, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxStaticVerts*sizeof(s_VertexStreamT), D3DUSAGE_WRITEONLY, 0, 
		D3DPOOL_DEFAULT, &StaticBuffer.VB_T1, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxStaticVerts*sizeof(s_VertexStreamT), D3DUSAGE_WRITEONLY, 0, 
		D3DPOOL_DEFAULT, &StaticBuffer.VB_T2, NULL ) );
	d3dcheckOK( DQDevice->CreateIndexBuffer( MaxStaticIndices*sizeof(WORD), D3DUSAGE_WRITEONLY, 
		D3DFMT_INDEX16, D3DPOOL_DEFAULT, &StaticBuffer.IndexBuffer, NULL ) );

	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxDynamicVerts*sizeof(s_VertexStreamP), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, 
		D3DPOOL_DEFAULT, &DynamicBuffer.VB_P, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxDynamicVerts*sizeof(s_VertexStreamN), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, 
		D3DPOOL_DEFAULT, &DynamicBuffer.VB_N, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxDynamicVerts*sizeof(s_VertexStreamD), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, 
		D3DPOOL_DEFAULT, &DynamicBuffer.VB_D, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxDynamicVerts*sizeof(s_VertexStreamT), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, 
		D3DPOOL_DEFAULT, &DynamicBuffer.VB_T1, NULL ) );
	d3dcheckOK( DQDevice->CreateVertexBuffer( MaxDynamicVerts*sizeof(s_VertexStreamT), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, 
		D3DPOOL_DEFAULT, &DynamicBuffer.VB_T2, NULL ) );
/*	d3dcheckOK( DQDevice->CreateIndexBuffer( MAX_INDICES*sizeof(WORD), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 
		D3DFMT_INDEX16, D3DPOOL_DEFAULT, &DynamicBuffer.IndexBuffer, NULL ) );
*/
	//******End D3D Static & Dynamic Buffers***************************************

	StaticBuffer.isVBLocked = FALSE;
	StaticBuffer.isIBLocked = FALSE;
	DynamicBuffer.isVBLocked = FALSE;
	DynamicBuffer.isIBLocked = FALSE;

	bUseVertexLighting = (DQConsole.GetCVarInteger( "r_vertexlight", eAuthClient ) )? TRUE:FALSE;

	Clear();
}

void c_RenderStack::AllocateShaderHint( int ShaderHandle, int ItemCount )
{
	if( ShaderHandle<0 || ShaderHandle>=MAX_SHADERS ) return;

	ShaderGroupHeap.Groups[ShaderHandle].SortedArray.IncreaseSize( ItemCount );
}

//Return value is a handle to the verts in the static VB
int c_RenderStack::AddStaticVertices( D3DXVECTOR3 *PosArray, D3DXVECTOR2 *TexCoordArray1, 
	D3DXVECTOR3 *NormalArray, DWORD *DiffuseArray, D3DXVECTOR2 *TexCoordArray2, int NumVerts )
{
	int i, handle;
	s_VertexStreamP *VertexP;
	s_VertexStreamN *VertexN;
	s_VertexStreamD *VertexD;
	s_VertexStreamT *VertexT1, *VertexT2;
	
	if( NumVerts<1 ) return 0;
	//Parameter Validation
	if(!PosArray || !TexCoordArray1) {
		Error( ERR_RENDER, "AddStaticVerticies : Requires both Pos & Tex1");
		return 0;
	}
	if( (NormalArray && StaticBuffer.NextVBPos.N+NumVerts>MaxStaticVerts)
	   || (DiffuseArray && StaticBuffer.NextVBPos.D+NumVerts>MaxStaticVerts)
	   || (TexCoordArray2 && StaticBuffer.NextVBPos.T2+NumVerts>MaxStaticVerts) ) {
		Error( ERR_RENDER, "Insufficient Static Vertex space" );
		return 0;
	}

	if(NextVBHandle>=MAX_VBHANDLES) {
		Error( ERR_RENDER, "Insufficient VB Handles" );
		return 0;
	}
	handle = NextVBHandle++;

	//Create a new handle
	VBHandles[handle-1].FirstIndex = StaticBuffer.NextVBPos;
	VBHandles[handle-1].NumVerts = NumVerts;
	VBHandles[handle-1].bUseDynamicBuffer = FALSE;
	
	VBHandles[handle-1].bUseStream.P = TRUE;
	VBHandles[handle-1].bUseStream.T1 = TRUE;
	VBHandles[handle-1].bUseStream.N = (NormalArray)?TRUE:FALSE;
	VBHandles[handle-1].bUseStream.D = (DiffuseArray)?TRUE:FALSE;
	VBHandles[handle-1].bUseStream.T2 = (TexCoordArray2)?TRUE:FALSE;

	//Lock and load
	LockStaticVB();

	VertexP = StaticBuffer.VertexP + StaticBuffer.NextVBPos.P;
	VertexN = StaticBuffer.VertexN + StaticBuffer.NextVBPos.N;
	VertexD = StaticBuffer.VertexD + StaticBuffer.NextVBPos.D;
	VertexT1 = StaticBuffer.VertexT1 + StaticBuffer.NextVBPos.T1;
	VertexT2 = StaticBuffer.VertexT2 + StaticBuffer.NextVBPos.T2;

	//Update NextVBPos
	StaticBuffer.NextVBPos.P += NumVerts;
	StaticBuffer.NextVBPos.T1 += NumVerts;
#ifdef _USE_STREAM_OFFSETS
	if(useStreamOffsets) {
		if(NormalArray)		StaticBuffer.NextVBPos.N += NumVerts;
		if(DiffuseArray)	StaticBuffer.NextVBPos.D += NumVerts;
		if(TexCoordArray2)	StaticBuffer.NextVBPos.T2 += NumVerts;
	} else 
#endif
	{
		StaticBuffer.NextVBPos.N += NumVerts;
		StaticBuffer.NextVBPos.D += NumVerts;
		StaticBuffer.NextVBPos.T2 += NumVerts;
	}

	for(i=0;i<NumVerts;++i) {
		VertexP->Pos = PosArray[i];
		VertexT1->Tex = TexCoordArray1[i];

		if(NormalArray) {
			VertexN->Normal = NormalArray[i];
		}
		if(DiffuseArray) {
			VertexD->Diffuse = DiffuseArray[i];
		}
		if(TexCoordArray2) {
			VertexT2->Tex = TexCoordArray2[i];
		}
		++VertexP;
		++VertexN;
		++VertexD;
		++VertexT1;
		++VertexT2;
	}

	return handle;
}

int c_RenderStack::AddStaticIndices( WORD *OffsetArray, int NumIndices )
{
	int i, handle;
	int MinIndex, MaxIndex;
	WORD *Index;

	if(!OffsetArray) return 0;
	if(StaticBuffer.NextIBPos+NumIndices>MaxStaticIndices) {
		Error( ERR_RENDER, "Too many static indices" );
		return 0;
	}

	handle = NextStaticIBHandle++;

	StaticIBHandles[handle-1].Offset = StaticBuffer.NextIBPos;
	StaticIBHandles[handle-1].NumIndices = NumIndices;

	//Lock and Load
	LockStaticIB();

	Index = StaticBuffer.Index + StaticBuffer.NextIBPos;
	MinIndex = MaxIndex = OffsetArray[0];

	for(i=0; i<NumIndices; ++i) {
		*Index = OffsetArray[i];
		MinIndex = min( *Index, MinIndex );
		MaxIndex = max( *Index, MaxIndex );
		++Index;
	}
	StaticIBHandles[handle-1].MinIndex = MinIndex;
	StaticIBHandles[handle-1].MaxIndex = MaxIndex;

	//Update NextIBPos
	StaticBuffer.NextIBPos += NumIndices;

	return handle;
}

int c_RenderStack::NewDynamicVertexBufferHandle( int NumVerts, BOOL useNormals, BOOL useDiffuse, 
												BOOL useTex2 )
{
	int handle;

	if(DynamicBuffer.NextVBPos.P+NumVerts>MaxDynamicVerts
	  || (useNormals && DynamicBuffer.NextVBPos.N+NumVerts>MaxDynamicVerts)
	  || (useDiffuse && DynamicBuffer.NextVBPos.D+NumVerts>MaxDynamicVerts)
	  || (useTex2 && DynamicBuffer.NextVBPos.T2+NumVerts>MaxDynamicVerts) ) {
		Error( ERR_RENDER, "Too many dynamic verts" );
		handle = 0;
	}
	else {
		LockDynamicVB();

		handle = NextVBHandle++;
		if(NextVBHandle>=MAX_VBHANDLES) {
			Error( ERR_RENDER, "Insufficient VB Handles" );
			return 0;
		}

		VBHandles[handle-1].FirstIndex = DynamicBuffer.NextVBPos;
		VBHandles[handle-1].NumVerts = NumVerts;
		VBHandles[handle-1].bUseDynamicBuffer = TRUE;

		VBHandles[handle-1].bUseStream.P = TRUE;
		VBHandles[handle-1].bUseStream.N = useNormals;
		VBHandles[handle-1].bUseStream.D = useDiffuse;
		VBHandles[handle-1].bUseStream.T1 = TRUE;
		VBHandles[handle-1].bUseStream.T2 = useTex2;

#ifdef _USE_STREAM_OFFSETS
		if(useStreamOffsets) {
			DynamicBuffer.NextVBPos.P += NumVerts;
			DynamicBuffer.NextVBPos.T1 += NumVerts;

			if(useNormals) DynamicBuffer.NextVBPos.N += NumVerts;
			if(useDiffuse) DynamicBuffer.NextVBPos.D += NumVerts;
			if(useTex2)	   DynamicBuffer.NextVBPos.T2 += NumVerts;
		} else 
#endif
		{
			DynamicBuffer.NextVBPos.P += NumVerts;
			DynamicBuffer.NextVBPos.N += NumVerts;
			DynamicBuffer.NextVBPos.D += NumVerts;
			DynamicBuffer.NextVBPos.T1 += NumVerts;
			DynamicBuffer.NextVBPos.T2 += NumVerts;
		}
	}

	return handle;
}

void c_RenderStack::UpdateDynamicVerts( int handle, int Offset, int NumVerts, D3DXVECTOR3 *PosArray, D3DXVECTOR2 *TexCoordArray1, 
					  D3DXVECTOR3 *NormalArray, DWORD *DiffuseArray, D3DXVECTOR2 *TexCoordArray2, int ShaderHandleHint )
{
	int i;
	s_VertexStreamP *VertexP;
	s_VertexStreamN *VertexN;
	s_VertexStreamD *VertexD;
	s_VertexStreamT *VertexT1, *VertexT2;
	s_VertexBufferItem *HandleBuf;
	c_Shader *Shader;
	
	if(handle<1 || handle>NextVBHandle) return;
	CriticalAssert( Offset>=0 );

	HandleBuf = &VBHandles[handle-1];

	//Check parameters again handle's creation parameters
	if( !PosArray || !TexCoordArray1
	  || (NormalArray && !HandleBuf->bUseStream.N) || (!NormalArray && HandleBuf->bUseStream.N)
	  || (DiffuseArray && !HandleBuf->bUseStream.D) || (!DiffuseArray && HandleBuf->bUseStream.D)
	  || (TexCoordArray2 && !HandleBuf->bUseStream.T2) || (!TexCoordArray2 && HandleBuf->bUseStream.T2) ) 
	{ 
		Error( ERR_RENDER, "UpdateDynamicVerts : invalid parameters" );
		return;
	}
	if(Offset+NumVerts>HandleBuf->NumVerts) {
		Error( ERR_RENDER, "UpdateDynamicVerts : out of bounds of given buffer" );
		return;
	}

	//Lock and load
	LockDynamicVB();

	VertexP = DynamicBuffer.VertexP + HandleBuf->FirstIndex.P + Offset;
	VertexN = DynamicBuffer.VertexN + HandleBuf->FirstIndex.N + Offset;
	VertexD = DynamicBuffer.VertexD + HandleBuf->FirstIndex.D + Offset;
	VertexT1 = DynamicBuffer.VertexT1 + HandleBuf->FirstIndex.T1 + Offset;
	VertexT2 = DynamicBuffer.VertexT2 + HandleBuf->FirstIndex.T2 + Offset;

	//AddPoly, DrawSprite and PrepareBillboards all give 0 as the hint
	if(ShaderHandleHint>0 && ShaderHandleHint<DQRender.NumShaders) {
		//check for DeformVertexes
		Shader = DQRender.ShaderArray[ShaderHandleHint-1];
		if(	Shader->GetFlags() & ShaderFlag_UseDeformVertex ) {
			Shader->DeformVertexes->Deform( NumVerts, PosArray, VertexP, NormalArray );
			
			if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite 
			|| Shader->DeformVertexes->type == Shader_deformVertexes::autosprite2) {
				if( NumVerts<4 ) Error( ERR_RENDER, "Invalid Billboard (<4 verts)" );
				NumVerts = 4;
			}
			else {
				PosArray = NULL;	//don't overwrite position
			}
		}
	}

	//(works for both useStreamOffsets and no StreamOffsets)
	for(i=0;i<NumVerts;++i) {
		if(PosArray) VertexP->Pos = PosArray[i];
		VertexT1->Tex = TexCoordArray1[i];
		if(NormalArray) VertexN->Normal = NormalArray[i];
		if(DiffuseArray) VertexD->Diffuse = DiffuseArray[i];
		if(TexCoordArray2) VertexT2->Tex = TexCoordArray2[i];
		++VertexP;
		++VertexN;
		++VertexD;
		++VertexT1;
		++VertexT2;
	}	
}

void c_RenderStack::DrawIndexedPrimitive( int VBhandle, int IBhandle, int ShaderHandle, D3DPRIMITIVETYPE PrimType, 
	   unsigned int StartIndex, int BaseVertexOffset, int PrimCount, D3DXVECTOR3 ApproxPos, s_RenderFeatureInfo *RenderFeatures  )
{
	int SortValue, Flags;
	s_ShaderGroup *Group;
	s_RenderItem *Item;
	s_SortedStack *Stack;
	int GroupIndex, ItemIndex;
	c_Shader *Shader;

	//Parameter validation
	if( VBhandle<=0 || VBhandle>=NextVBHandle
		|| (IBhandle && IBhandle>=NextStaticIBHandle) )
	{
		Error( ERR_RENDER, "DrawIndexedPrimitive : Invalid Parameters");
		return;
	}
	if( ShaderHandle<=0 || ShaderHandle>DQRender.NumShaders) {
		ShaderHandle = DQRender.NoShaderShader;
	}

	if(IBhandle && StartIndex >= StaticIBHandles[IBhandle-1].NumIndices) {
		Error( ERR_RENDER, "DrawIndexedPrimitive  : Invalid Parameters");
		return;
	}

	if( RenderFeatures == NULL ) {
		RenderFeatures = &NullRenderFeatures;
	}

	if(OverrideShaderHandle) {
		ShaderHandle = OverrideShaderHandle;
		OverrideShaderHandle = NULL;
	}

	Shader = DQRender.ShaderArray[ShaderHandle-1];

	if( Shader->NumPasses == 0) return;

	//Decide which stack to add item to
	SortValue = Shader->SortValue;
	Flags = Shader->GetFlags();

	if( Flags & ShaderFlag_SkyboxShader ) {
		Stack = &SkyboxSortStack;
	}
	else if( SortValue <=3 ) {
		Stack = &OpaqueSortStack;
	}
	else {
		Stack = &BlendedSortStack;
	}

	if( RenderFeatures->DrawAsSprite ) {
		Stack = &SpriteSortStack;
	}
	if( Flags & ShaderFlag_ConsoleShader ) {
		Stack = &ConsoleSortStack;
	}

	//Check for autosprite
	if(Shader->DeformVertexes && !bProcessingBillboards) {
		if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite 
		|| Shader->DeformVertexes->type == Shader_deformVertexes::autosprite2) {

			s_VertexStreamP *VertexP;
			s_VertexStreamT *VertexT1;

			if(!DynamicBuffer.isVBLocked || !VBHandles[VBhandle-1].bUseDynamicBuffer) {
#if _DEBUG
				Error( ERR_RENDER, "Invalid DeformVertexes call from static Vertex Buffer" );
#endif
				return;
			}

			VertexP = DynamicBuffer.VertexP + VBHandles[VBhandle-1].FirstIndex.P;
			VertexT1 = DynamicBuffer.VertexT1 + VBHandles[VBhandle-1].FirstIndex.T1;

			if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite ) {
				DQRender.AddBillboard( VertexP, VertexT1, ShaderHandle, FALSE );
			}
			else if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite2 ) {
				DQRender.AddBillboard( VertexP, VertexT1, ShaderHandle, TRUE );
			}
			
			return;
		}
	}

	//Find ShaderGroup
	Group = &ShaderGroupHeap.Groups[ShaderHandle];

	if(Group->CurrentFrame == CurrentFrame && Group->RenderFeatures == *RenderFeatures) {
		//Group is current and we can concatenate the new item

		//Add item later
	}
	else {
		//Since group is either from last frame or has different render features :
		// - Get a new group from the heap if sibling required
		// - Add to stack's index
		// - Initialise group
		
		if(Group->CurrentFrame == CurrentFrame) {
			//Create sibling group
			GroupIndex = ShaderGroupHeap.NextGroup++;
		}
		else {
			GroupIndex = ShaderHandle;				//NB. ShaderHandle is the group index
		}
		
		//Clean up group
		Group = &ShaderGroupHeap.Groups[GroupIndex];
		Group->Clear();
	
		Group->StackIndex = Stack->SortedGroups.NextElement;
		Stack->RegisterNewGroup( GroupIndex );

		//Initialise group
		Group->CurrentFrame = CurrentFrame;
		Group->ShaderHandle = ShaderHandle;
		Group->RenderFeatures = *RenderFeatures;

		if(RenderFeatures->pTM) {
			Group->RenderFeatures.pTM = &TMArray[NextTM];
			TMArray[NextTM++] = *RenderFeatures->pTM;

			if(NextTM>=MAX_TM) {
				Error( ERR_RENDER, "RenderStack : Too many Tranformation Matricies" );
				NextTM = 0;	//crude error handling
			}
		}
		if( RenderFeatures->DrawAsSprite ) {
			DQRender.ShaderArray[ShaderHandle-1]->SpriteShader();
		}

		Group->VertexDeclaration = GetVertexDeclaration( DQRender.ShaderArray[ShaderHandle-1] );

	}

	//Get new Render Item
	ItemIndex = ItemHeap.NextItem++;

	Group->RegisterNewItem( ItemIndex );

	Item = &ItemHeap.Items[ItemIndex];

	//Fill Item Info
	Item->VBhandle = VBhandle;
	Item->IBhandle = IBhandle;
	Item->PrimitiveType = PrimType;
	Item->PrimCount = PrimCount;
	Item->IndexOffset = StartIndex;
	Item->BaseVertexOffset = BaseVertexOffset;
	Item->ApproxPos = ApproxPos;
}

//Very similar to DrawIndexedPrimitive
void c_RenderStack::DrawBSPIndexedPrimitive( int VBhandle, int IBhandle, int ShaderHandle, D3DPRIMITIVETYPE PrimType, 
	   unsigned int StartIndex, int BaseVertexOffset, int PrimCount, D3DXVECTOR3 ApproxPos, c_Texture *_LightmapTexture )
{
	int SortValue;
	s_ShaderGroup *Group;
	s_RenderItem *Item;
	s_SortedStack *Stack;
	int GroupIndex, ItemIndex;
	c_Shader *Shader;
	LPDIRECT3DTEXTURE9 LightmapTexture;

	//Parameter validation
#if _DEBUG
	if( VBhandle<=0 || VBhandle>=NextVBHandle
		|| (IBhandle && IBhandle>=NextStaticIBHandle) )
	{
		CriticalError( ERR_RENDER, "DrawPrimitive : Invalid Parameters");
		return;
	}

	if(IBhandle && StartIndex >= StaticIBHandles[IBhandle-1].NumIndices) {
		CriticalError( ERR_RENDER, "DrawPrimitive : Invalid Parameters");
		return;
	}
#endif

	if(_LightmapTexture && !bUseVertexLighting) LightmapTexture = _LightmapTexture->D3DTexture;
	else LightmapTexture = NULL;

	if( ShaderHandle<=0 || ShaderHandle>DQRender.NumShaders) {
		ShaderHandle = DQRender.NoShaderShader;
	}


	if(OverrideShaderHandle) {
		ShaderHandle = OverrideShaderHandle;
		OverrideShaderHandle = NULL;
	}

	Shader = DQRender.ShaderArray[ShaderHandle-1];

	if( Shader->NumPasses == 0) return;

	//Decide which stack to add item to
	SortValue = DQRender.ShaderArray[ShaderHandle-1]->SortValue;

	if( SortValue <=3 ) {
		Stack = &OpaqueSortStack;
	}
	else {
		Stack = &BlendedSortStack;
	}

	//Check for autosprite
	if(Shader->DeformVertexes && !bProcessingBillboards) {
		if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite 
		|| Shader->DeformVertexes->type == Shader_deformVertexes::autosprite2) {

			s_VertexStreamP *VertexP;
			s_VertexStreamT *VertexT1;

			if(!DynamicBuffer.isVBLocked || !VBHandles[VBhandle-1].bUseDynamicBuffer) {
#if _DEBUG
				Error( ERR_RENDER, "Invalid DeformVertexes call from static Vertex Buffer" );
#endif
				return;
			}
				
			VertexP = DynamicBuffer.VertexP + VBHandles[VBhandle-1].FirstIndex.P;
			VertexT1 = DynamicBuffer.VertexT1 + VBHandles[VBhandle-1].FirstIndex.T1;

			if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite ) {
				DQRender.AddBillboard( VertexP, VertexT1, ShaderHandle, FALSE );
			}
			else if(Shader->DeformVertexes->type == Shader_deformVertexes::autosprite2 ) {
				DQRender.AddBillboard( VertexP, VertexT1, ShaderHandle, TRUE );
			}
			
			return;
		}
	}

	//Find ShaderGroup
	Group = &ShaderGroupHeap.Groups[ShaderHandle];

	if( (Group->CurrentFrame == CurrentFrame) && (Group->RenderFeatures.LightmapTexture == LightmapTexture) ) {
		//Group is current and we can concatenate the new item

		//Add item later
	}
	else {
		//Since group is either from last frame or has different render features :
		// - Get a new group from the heap if sibling required
		// - Add to stack's index
		// - Initialise group
		
		if(Group->CurrentFrame == CurrentFrame) {
			//Create sibling group
			GroupIndex = ShaderGroupHeap.NextGroup++;
		}
		else {
			GroupIndex = ShaderHandle;				//NB. ShaderHandle is the group index
		}
		
		//Clean up group
		Group = &ShaderGroupHeap.Groups[GroupIndex];
		Group->Clear();
	
		Group->StackIndex = Stack->SortedGroups.NextElement;
		Stack->RegisterNewGroup( GroupIndex );

		//Initialise group
		Group->CurrentFrame = CurrentFrame;
		Group->ShaderHandle = ShaderHandle;
		Group->RenderFeatures = NullRenderFeatures;
		Group->RenderFeatures.LightmapTexture = LightmapTexture;

		Group->VertexDeclaration = GetVertexDeclaration( DQRender.ShaderArray[ShaderHandle-1] );

	}

	//Get new Render Item
	ItemIndex = ItemHeap.NextItem++;

	Group->RegisterNewItem( ItemIndex );

	Item = &ItemHeap.Items[ItemIndex];

	//Fill Item Info
	Item->VBhandle = VBhandle;
	Item->IBhandle = IBhandle;
	Item->PrimitiveType = PrimType;
	Item->PrimCount = PrimCount;
	Item->IndexOffset = StartIndex;
	Item->BaseVertexOffset = BaseVertexOffset;
	Item->ApproxPos = ApproxPos;
}

void c_RenderStack::RenderToScreen()
{
	int i, SS, u, Pass;
	MATRIX *CurrentTM;
	s_ShaderGroup *Group;
	s_RenderItem *Item;
	s_VertexBufferItem *VBStackItem;
	s_IndexBufferItem *IBStackItem;
	s_Buffer *CurrentVB, *RequiredVB;
	s_Buffer *CurrentIB, *RequiredIB;
	eVertDecl CurrentDecl;
	BOOL CurrentDrawSprite;
	s_SortedStack *Stack;
	c_Shader *Shader;

DQProfileSectionStart( 12, "Render Stack" );

#if _DEBUG
	if( DQConsole.GetCVarInteger( "r_Wireframe", eAuthClient ) ) {
		d3dcheckOK( DQDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME ) );
		d3dcheckOK( DQDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0 ) );
	}
	else {
		d3dcheckOK( DQDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID ) );
	}
#endif

	//Initialise
	CurrentTM = NULL;
	CurrentDrawSprite = FALSE;

	//force buffer change
	CurrentVB = NULL;
	CurrentIB = NULL;
	CurrentDecl = eVD_Unspecified;

	UnlockBuffers();

	for(SS = 0; SS < 5; ++SS) {
		switch(SS) {
		case 0: Stack = &SkyboxSortStack; break;
		case 1: Stack = &SpriteSortStack; break;
		case 2: Stack = &OpaqueSortStack; break;
		case 3: Stack = &BlendedSortStack; break;
		case 4: Stack = &ConsoleSortStack; break;
		};

		for(i=0; i<Stack->SortedGroups.NextElement; ++i) {
#if _DEBUG
			CriticalAssert( (Stack->SortedGroups.Array[i].index >= 0) && (Stack->SortedGroups.Array[i].index < MAX_SHADERGROUPS) );
#endif
			Group = &ShaderGroupHeap.Groups[ Stack->SortedGroups.Array[i].index ];

			//ShaderGroup Initialisation
			//*******************

			//Prepare Shader
			Shader = DQRender.ShaderArray[Group->ShaderHandle-1];
//				Shader = DQRender.ShaderArray[DQRender.NoShaderShader-1];
			Shader_BaseFunc::ShaderTime = max( 0.0f, (DQCamera.RenderTime/1000.0f - Group->RenderFeatures.ShaderTime) );
			Shader_BaseFunc::ShaderRGBA = Group->RenderFeatures.ShaderRGBA;
			Shader_BaseFunc::LightmapTexture = Group->RenderFeatures.LightmapTexture;
			
			//Portals are very slow
//			if(Shader->SortValue==1) {
//				Shader->SetAlphaPortalDistance( Stack->SortedGroups.Array[i].ZDistance );
//			}

			//Update TM
			if( CurrentTM != Group->RenderFeatures.pTM ) {
				if(Group->RenderFeatures.pTM) {
					CurrentTM = Group->RenderFeatures.pTM;
					d3dcheckOK( DQDevice->SetTransform( D3DTS_WORLD, CurrentTM ) );
				}
				else {
					CurrentTM = NULL;
					d3dcheckOK( DQDevice->SetTransform( D3DTS_WORLD, &DQRender.matIdentity ) );
				}
			}

			//Render Group to screen in the same pass
			//******************************
			for(Pass = 0; Pass<Shader->NumPasses; ++Pass) 
			{
				Shader->RunShader();

				//if DrawSprites, Set View & Projection matrices to Identity, Viewport to full, etc
				if( Group->RenderFeatures.DrawAsSprite ) 
				{
					//Draw Sprite
					if( !CurrentDrawSprite ) {
						DQCamera.SetIdentityD3D();
						CurrentDrawSprite = TRUE;
					}
				}
				else {
					if( CurrentDrawSprite ) {
						DQCamera.SetD3D();
						CurrentDrawSprite = FALSE;
					}
				}

				//for each RenderItem in the group
				for( u=0; u<Group->SortedArray.NextElement; ++u ) {
#if _DEBUG
					CriticalAssert( Group->SortedArray.Array[u].index >= 0);
#endif
					Item = &ItemHeap.Items[ Group->SortedArray.Array[u].index ];

					//RenderItem
					//*************************************

					VBStackItem = &VBHandles[Item->VBhandle-1];

					//Update Vertex Buffer if necessary
					if(VBStackItem->bUseDynamicBuffer) {
						RequiredVB = &DynamicBuffer;
					}
					else {
						RequiredVB = &StaticBuffer;
					}
					
					if(CurrentVB != RequiredVB) {			
						CurrentVB = RequiredVB;
						CurrentDecl = eVD_Unspecified;
					}

					//Update Index Buffer if necessary
					if(Item->IBhandle) {
//						if(Item->IBhandle>0) 
						{
							IBStackItem = &StaticIBHandles[Item->IBhandle-1];
							RequiredIB = &StaticBuffer;
						}
/*						else {
							IBStackItem = &DynamicIBHandles[~Item->IBhandle];
							RequiredIB = &DynamicBuffer;
						}
*/						
						if(CurrentIB != RequiredIB) {			
							CurrentIB = RequiredIB;
							d3dcheckOK( DQDevice->SetIndices( CurrentIB->IndexBuffer ) );
						}
					}

#ifdef _USE_STREAM_OFFSETS
					if(useStreamOffsets) {
						CurrentDecl = eVD_Unspecified;
					}
#endif

					//Set up Vertex Declaration and Vertex Buffer
					//*******************************************
					if(CurrentDecl != Group->VertexDeclaration) {
						CurrentDecl = Group->VertexDeclaration;
						
#ifdef _USE_STREAM_OFFSETS
						//Select streams depending on Vertex Declaration
						if(useStreamOffsets) {
							d3dcheckOK( DQDevice->SetStreamSource( 0, CurrentVB->VB_P, VBStackItem->FirstIndex.P*sizeof(s_VertexStreamP), sizeof(s_VertexStreamP) ) );

							switch( CurrentDecl ) {
							case eVD_P0T1:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1T2:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, VBStackItem->FirstIndex.N*sizeof(s_VertexStreamN), sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0D1T2:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_D, VBStackItem->FirstIndex.D*sizeof(s_VertexStreamD), sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1D2T3:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, VBStackItem->FirstIndex.N*sizeof(s_VertexStreamN), sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_D, VBStackItem->FirstIndex.D*sizeof(s_VertexStreamD), sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0T1T2:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T2, VBStackItem->FirstIndex.T2*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1T2T3:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, VBStackItem->FirstIndex.N*sizeof(s_VertexStreamN), sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T2, VBStackItem->FirstIndex.T2*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0D1T2T3:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_D, VBStackItem->FirstIndex.D*sizeof(s_VertexStreamD), sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T2, VBStackItem->FirstIndex.T2*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1D2T3T4:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, VBStackItem->FirstIndex.N*sizeof(s_VertexStreamN), sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_D, VBStackItem->FirstIndex.D*sizeof(s_VertexStreamD), sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T1, VBStackItem->FirstIndex.T1*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 4, CurrentVB->VB_T2, VBStackItem->FirstIndex.T2*sizeof(s_VertexStreamT), sizeof(s_VertexStreamT) ) );
								break;
							default:
								CriticalError( ERR_RENDER, "Unspecified Vertex Declaration" );
							};
							d3dcheckOK( DQDevice->SetVertexDeclaration( GetVertexDeclaration( CurrentDecl ) ) );
						} 
						else 
#endif //_USE_STREAM_OFFSETS
						{
							//Don't use StreamOffsets
							d3dcheckOK( DQDevice->SetStreamSource( 0, CurrentVB->VB_P, 0, sizeof(s_VertexStreamP) ) );

							switch( CurrentDecl ) {
							case eVD_P0T1:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1T2:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, 0, sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0D1T2:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_D, 0, sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1D2T3:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, 0, sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_D, 0, sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0T1T2:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T2, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1T2T3:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, 0, sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T2, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0D1T2T3:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_D, 0, sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T2, 0, sizeof(s_VertexStreamT) ) );
								break;
							case eVD_P0N1D2T3T4:
								d3dcheckOK( DQDevice->SetStreamSource( 1, CurrentVB->VB_N, 0, sizeof(s_VertexStreamN) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 2, CurrentVB->VB_D, 0, sizeof(s_VertexStreamD) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 3, CurrentVB->VB_T1, 0, sizeof(s_VertexStreamT) ) );
								d3dcheckOK( DQDevice->SetStreamSource( 4, CurrentVB->VB_T2, 0, sizeof(s_VertexStreamT) ) );
								break;
							default:
								CriticalError( ERR_RENDER, "Unspecified Vertex Declaration" );
							};
							d3dcheckOK( DQDevice->SetVertexDeclaration( GetVertexDeclaration( CurrentDecl ) ) );
						}
					}
					//***** End Set Vertex Declaration ******

#ifdef _UseLightvols
					//Set LightVol
					if( Group->RenderFeatures.bApplyLightVols ) {
						DQBSP.SetLightVol( Item->ApproxPos );
					}
#endif

//					DQDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);


					///***DRAW***
					if(Item->IBhandle) {

						#ifdef _USE_STREAM_OFFSETS
						if(useStreamOffsets) {
							d3dcheckOK( DQDevice->DrawIndexedPrimitive( Item->PrimitiveType, Item->BaseVertexOffset, 
								IBStackItem->MinIndex, (IBStackItem->MaxIndex - IBStackItem->MinIndex), 
								IBStackItem->Offset + Item->IndexOffset, Item->PrimCount ) );
						} else 
						#endif //_USE_STREAM_OFFSETS

						{
							d3dcheckOK( DQDevice->DrawIndexedPrimitive( Item->PrimitiveType, VBStackItem->FirstIndex.P + Item->BaseVertexOffset, 
								IBStackItem->MinIndex, (IBStackItem->MaxIndex - IBStackItem->MinIndex), 
								IBStackItem->Offset + Item->IndexOffset, Item->PrimCount ) );
						}
					}
					else {

						#ifdef _USE_STREAM_OFFSETS
						if(useStreamOffsets) {
							d3dcheckOK( DQDevice->DrawPrimitive( Item->PrimitiveType, 
								Item->IndexOffset, Item->PrimCount ) );
						} else 
						#endif //_USE_STREAM_OFFSETS

						{
							d3dcheckOK( DQDevice->DrawPrimitive( Item->PrimitiveType, 
								Item->IndexOffset + VBStackItem->FirstIndex.P, Item->PrimCount ) );
						}
					}

#ifdef _UseLightvols
					//UnSet LightVol
					if( Group->RenderFeatures.bApplyLightVols ) {
						DQBSP.EndLightVol();
					}
#endif
				}
			}
		}
	}
DQProfileSectionEnd( 12 );

}

//  Sort By :
// Shader Sort Value
// - Distance from Camera (Render Front to Back)
// (- Vertex Buffer)

// also write the indices to the dynamic index buffer

void c_RenderStack::SortStack()
{
	int i,u, SS;
	float GroupZDistance;
	s_ShaderGroup *Group;
	s_SortedStack *Stack;

DQProfileSectionStart( 11, "Sort Stack" );

	D3DXVECTOR3 &CameraZDir = DQCamera.CameraZDir;
	float CameraZPos = DQCamera.CameraZPos;

	for(SS = 0; SS < 2; ++SS) {
		switch(SS) {
		case 0: Stack = &OpaqueSortStack; break;
		case 1: Stack = &BlendedSortStack; break;
		};

		for(i=0; i<Stack->SortedGroups.NextElement; ++i) {
			//Quicksort items in each group in stack

			Group = &ShaderGroupHeap.Groups[Stack->SortedGroups.Array[i].index];

			GroupZDistance = 0;
			for(u=0; u<Group->SortedArray.NextElement; ++u) {
				//Calculate Z Distance
				Group->SortedArray.Array[u].ZDistance = D3DXVec3Dot( &ItemHeap.Items[Group->SortedArray.Array[u].index].ApproxPos, &CameraZDir ) - CameraZPos;
				GroupZDistance += Group->SortedArray.Array[u].ZDistance;
			}
			//Calculate average ZDistance for group
			if( Group->SortedArray.NextElement ) {
				Stack->SortedGroups.Array[i].ZDistance = GroupZDistance / Group->SortedArray.NextElement;
			}
			
			//Sort Items
			if( Stack != &BlendedSortStack ) {
				//Sort Front->Back
				Group->SortedArray.Quicksort( 0, Group->SortedArray.NextElement-1 );
			}
			else {
				//Sort Back->Front
				Group->SortedArray.QuicksortInverse( 0, Group->SortedArray.NextElement-1 );
			}
		}
	}

	//Quicksort groups in stack
	OpaqueSortStack.SortedGroups.Quicksort( 0, OpaqueSortStack.SortedGroups.NextElement-1 );
	BlendedSortStack.SortedGroups.QuicksortInverse( 0, BlendedSortStack.SortedGroups.NextElement-1 );

	//** Don't sort Console or Sprite stacks **

DQProfileSectionEnd( 11 );
}
