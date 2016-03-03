// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_MD3.cpp: implementation of the c_MD3 class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_MD3.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_MD3::c_MD3():isCopy(FALSE)
{
	Surface = NULL;
	Tag = NULL;
	TagIndex = NULL;
	Frame = NULL;
	NumSurfaces = 0;
	NumFrames = 0;
	LoadedThisFrame = LoadedNextFrame = 0;
	LoadedFraction = 0.0f;
}

c_MD3::c_MD3(c_MD3 *Original):isCopy(TRUE)
{
	//Point to original's data
	Tag				= Original->Tag;
	TagIndex		= Original->TagIndex;
	Frame			= Original->Frame;
	NumSurfaces = Original->NumSurfaces;
	NumFrames	= Original->NumFrames;
	NumTags		= Original->NumTags;
	bUseDynamicBuffer = Original->bUseDynamicBuffer;

	//Create new D3D instance
	Surface			= (s_DQSurface*)DQNewVoid( s_DQSurface[NumSurfaces] );
	for(int i=0; i<NumSurfaces; ++i ) {
		Surface[i].Flags = Original->Surface[i].Flags;
		Surface[i].IndexBufferHandle = 0;
		Surface[i].VertexBufferHandle = 0;
		Surface[i].InterpolatedNormal = NULL;
		Surface[i].InterpolatedPos = NULL;
		Surface[i].LastUpdatedFrame = -1;
		DQstrcpy( Surface[i].Name, Original->Surface[i].Name, MAX_QPATH );
		Surface[i].NumTriangles = Original->Surface[i].NumTriangles;
		Surface[i].NumVerts = Original->Surface[i].NumVerts;
		Surface[i].ShaderHandle = Original->Surface[i].ShaderHandle;
		//Point to original data
		Surface[i].TexCoord = Original->Surface[i].TexCoord;
		Surface[i].Triangle = Original->Surface[i].Triangle;
		Surface[i].VertexNormal = Original->Surface[i].VertexNormal;
		Surface[i].VertexPos = Original->Surface[i].VertexPos;
	}
	LoadedThisFrame = LoadedNextFrame = 0;
	LoadedFraction = 0.0f;
	isLoaded = FALSE;
}

c_MD3::~c_MD3()
{
	int i,u;
	//cleanup
	if(Surface) {
		for(i=0;i<NumSurfaces;++i) {
			if(!isCopy) {
				DQDeleteArray(Surface[i].TexCoord);
				DQDeleteArray(Surface[i].Triangle);
				for(u=0;u<NumFrames;++u) {
					DQDeleteArray(Surface[i].VertexPos[u]);
					DQDeleteArray(Surface[i].VertexNormal[u]);
				}
				DQDeleteArray(Surface[i].VertexPos);
				DQDeleteArray(Surface[i].VertexNormal);
			}

			DQDeleteArray(Surface[i].InterpolatedPos);
			DQDeleteArray(Surface[i].InterpolatedNormal);
		}
		DQDeleteArray(Surface);
	}

	if(!isCopy) {
		if(Tag) {
			for(i=0;i<NumFrames;++i) DQDeleteArray(Tag[i]);
			DQDeleteArray(Tag);
		}
		DQDeleteArray(TagIndex);
		DQDeleteArray(Frame);
	}
}

//Read an MD3 file and load into D3D structures
BOOL c_MD3::LoadMD3(FILEHANDLE Handle)
{
	int i,u,w;
	Header_t	Header;
	Frame_t		*Q3Frame;
	Tag_t		*Q3Tag;
	Surface_t	*Q3Surface;	//Holds Q3 surface information
	s_DQSurface *pSurface;  //Pointer to a position in DQSurface data
	TexCoord_t	*Q3TexCoord;
	Shader_t	*Q3Shader;
	Vertex_t	*Q3Vertex;
	int			SurfaceStartOffset;
	MATRIX		Rot;

	//Read in Header information
	DQFS.ReadFile((void*)&Header, sizeof(Header_t), 1, Handle);

	//check Magic number
	if(Header.Magic_Ident==0x33504449)
		Endian = LITTLE_ENDIAN;
	else {
		if(Header.Magic_Ident==0x49445033) {
			Endian = BIG_ENDIAN;
			Error(ERR_MISC, "Big Endian Format not implemented");
		}
		else Error(ERR_MISC, "c_MD3::LoadMD3 - Not an MD3 File");
	}

	//allocate memory for Q3 structures
	Q3Frame		= (Frame_t*)DQNewVoid( Frame_t [Header.NumFrames] );
	Q3Tag		= (Tag_t*)DQNewVoid( Tag_t [Header.NumTags] );
	Q3Surface	= (Surface_t*) DQNewVoid( Surface_t );

	//allocate memory for DQ structures
	Tag = (s_DQTag**) DQNewVoid( s_DQTag*[Header.NumFrames] );			//Alloc DQTag structure
	for(i=0;i<Header.NumFrames;++i) Tag[i] = (s_DQTag*)DQNewVoid(s_DQTag[Header.NumTags]);
	TagIndex = (s_DQTagIndex*)DQNewVoid( s_DQTagIndex[Header.NumTags] );
	Surface	= (s_DQSurface*) DQNewVoid( s_DQSurface [Header.NumSurfaces] );

	//check data is valid
	if( Header.NumFrames > MD3_MAX_FRAMES || Header.NumSurfaces > MD3_MAX_SURFACES || Header.NumTags > MD3_MAX_TAGS )
		Error(ERR_FS, "Invalid MD3 data - exceeds max values");

	//check offsets are valid
	if( Header.OffsetFrames>Header.OffsetEOF || Header.OffsetSurfaces>Header.OffsetEOF || Header.OffsetSurfaces>Header.OffsetEOF || Header.OffsetTags>Header.OffsetEOF)
		Error(ERR_FS, "Invalid MD3 Offsets");

	//read Frames and load into DQFrame structure
	//*******************

	DQFS.Seek(Header.OffsetFrames, FS_SEEK_SET, Handle);
	DQFS.ReadFile((void*)Q3Frame, sizeof(Frame_t), Header.NumFrames, Handle);

	Frame = (s_DQFrame*) DQNewVoid( s_DQFrame[Header.NumFrames] );

	for(i=0;i<Header.NumFrames;++i) {
		Frame[i].BBox.max	= MakeD3DXVec3FromFloat3(Q3Frame[i].MaxBounds);
		Frame[i].BBox.min	= MakeD3DXVec3FromFloat3(Q3Frame[i].MinBounds);
		Frame[i].BSphere.centre = MakeD3DXVec3FromFloat3(Q3Frame[i].LocalOrigin);
		Frame[i].BSphere.radius = Q3Frame[i].Radius;
	}

	//read Tags and load into DQTag structure
	//*******************
	
	DQFS.Seek(Header.OffsetTags, FS_SEEK_SET, Handle);

	for(i=0;i<Header.NumFrames;++i) {
		DQFS.ReadFile((void*)Q3Tag, sizeof(Tag_t), Header.NumTags, Handle);

		for(u=0;u<Header.NumTags;++u) {
			//Create rotation matrix from Q3Tag
			Rot._11 = Q3Tag[u].Axis[0][0];
			Rot._12 = Q3Tag[u].Axis[0][1];
			Rot._13 = Q3Tag[u].Axis[0][2];
			Rot._14 = 0;
			Rot._21 = Q3Tag[u].Axis[1][0];
			Rot._22 = Q3Tag[u].Axis[1][1]; 
			Rot._23 = Q3Tag[u].Axis[1][2];
			Rot._24 = 0;
			Rot._31 = Q3Tag[u].Axis[2][0]; 
			Rot._32 = Q3Tag[u].Axis[2][1]; 
			Rot._33 = Q3Tag[u].Axis[2][2];			
			Rot._34 = 0;	Rot._41 = 0;	Rot._42 = 0;	Rot._43 = 0;	Rot._44 = 1;
			D3DXQuaternionRotationMatrix( &Tag[i][u].QRotation, &Rot );
			Tag[i][u].Position = MakeD3DXVec3FromFloat3(Q3Tag[u].Origin);

			//Keep a reference of which Tag is which
			if(Q3Tag[u].Name[0]) {
				DQstrcpy(TagIndex[u].TagName, (char*)Q3Tag[u].Name, MAX_QPATH);
				TagIndex[u].TagIndex = u;
			}
		}
	}

	//Fill member variables
	NumSurfaces = (S16)Header.NumSurfaces;
	NumFrames = Header.NumFrames;
	NumTags = Header.NumTags;

	if(NumFrames>1) bUseDynamicBuffer = TRUE;
	else bUseDynamicBuffer = FALSE;

	//read Surfaces and load into DQ structure
	//*******************
	DQFS.Seek(Header.OffsetSurfaces, FS_SEEK_SET, Handle);

	for(i=0,pSurface=Surface;i<Header.NumSurfaces;++i,++pSurface)
	{
		SurfaceStartOffset	= DQFS.GetPosition(Handle);

		//Read in data
		DQFS.ReadFile((void*)Q3Surface, sizeof(Surface_t), 1, Handle);

		//check data is valid
		if(		Q3Surface->MagicIdent != Header.Magic_Ident
			||	Q3Surface->NumFrames != Header.NumFrames 
			||	Q3Surface->NumShaders > MD3_MAX_SHADERS
			||	Q3Surface->NumTriangles > MD3_MAX_TRIANGLES
			||	Q3Surface->NumVerts > MD3_MAX_VERTS
			||	Q3Surface->OffsetTriangles>Q3Surface->OffsetEnd
			||	Q3Surface->OffsetShaders>Q3Surface->OffsetEnd
			||	Q3Surface->OffsetTexCoord>Q3Surface->OffsetEnd
			||	Q3Surface->OffsetVertices>Q3Surface->OffsetEnd )
			Error(ERR_FS, "c_MD3::LoadMD3 - Invalid Surface data");

		//Copy required data into DQSurface struct
		DQstrcpy(pSurface->Name, (char*)Q3Surface->Name, MAX_QPATH);
		pSurface->Flags			= Q3Surface->Flags;
		pSurface->NumTriangles	= Q3Surface->NumTriangles;
		pSurface->NumVerts		= Q3Surface->NumVerts;
		//allocate memory for DQSurface struct
		pSurface->Triangle	= (Triangle_t*) DQNewVoid( Triangle_t[pSurface->NumTriangles] );
		pSurface->TexCoord	= (D3DXVECTOR2*) DQNewVoid( D3DXVECTOR2 [pSurface->NumVerts] );
		pSurface->VertexPos	= (D3DXVECTOR3**) DQNewVoid( D3DXVECTOR3 * [NumFrames] );
		pSurface->VertexNormal = (D3DXVECTOR3**) DQNewVoid( D3DXVECTOR3 * [NumFrames] );
		for(u=0;u<NumFrames;++u) {
			pSurface->VertexPos[u]		= (D3DXVECTOR3*) DQNewVoid( D3DXVECTOR3 [pSurface->NumVerts] );
			pSurface->VertexNormal[u]	= (D3DXVECTOR3*) DQNewVoid( D3DXVECTOR3 [pSurface->NumVerts] );
		}

		//allocate memory for Q3 Surface strutures
		Q3Shader	= (Shader_t*) DQNewVoid( Shader_t[Q3Surface->NumShaders] );
		Q3TexCoord	= (TexCoord_t*) DQNewVoid( TexCoord_t [pSurface->NumVerts] );
		Q3Vertex	= (Vertex_t*) DQNewVoid( Vertex_t [pSurface->NumVerts] );

		//Read shader references (but don't open files yet)
		DQFS.Seek(Q3Surface->OffsetShaders+SurfaceStartOffset, FS_SEEK_SET, Handle);
		DQFS.ReadFile((void*)Q3Shader, sizeof(Shader_t), Q3Surface->NumShaders, Handle);
#if _DEBUG
		if( Q3Surface->NumShaders>1 ) 
			DQPrint("Multiple MD3 Shaders not implemented\n");
#endif
		if( Q3Shader[0].Name[0] ) {//if there is a shader specified
			//Load the shader into memory
			pSurface->ShaderHandle = DQRender.RegisterShader((char*)Q3Shader[0].Name);
			if(DQRender.ShaderGetFlags(pSurface->ShaderHandle) & ShaderFlag_UseDeformVertex ) bUseDynamicBuffer = TRUE;
		}
		else pSurface->ShaderHandle = DQRender.NoShaderShader;
		
		//Read Triangles
		DQFS.Seek(Q3Surface->OffsetTriangles+SurfaceStartOffset, FS_SEEK_SET, Handle);
		DQFS.ReadFile((void*)pSurface->Triangle, sizeof(Triangle_t), pSurface->NumTriangles, Handle);

		//Read TexCoords and put in DQ structure
		DQFS.Seek(Q3Surface->OffsetTexCoord+SurfaceStartOffset, FS_SEEK_SET, Handle);
		DQFS.ReadFile((void*)Q3TexCoord, sizeof(TexCoord_t), pSurface->NumVerts, Handle);
		for(u=0;u<pSurface->NumVerts;++u) pSurface->TexCoord[u] = MakeD3DXVec2FromFloat2(Q3TexCoord[u].tex);

		//Read Vertices and put in DQ structure
		DQFS.Seek(Q3Surface->OffsetVertices+SurfaceStartOffset, FS_SEEK_SET, Handle);
		for(u=0;u<NumFrames;++u)
		{
			DQFS.ReadFile((void*)Q3Vertex, sizeof(Vertex_t), pSurface->NumVerts, Handle);
			for(w=0;w<pSurface->NumVerts;++w) {
				pSurface->VertexPos[u][w].x = Q3Vertex[w].x*MD3_XYZ_SCALE;
				pSurface->VertexPos[u][w].y = Q3Vertex[w].y*MD3_XYZ_SCALE;
				pSurface->VertexPos[u][w].z = Q3Vertex[w].z*MD3_XYZ_SCALE;
				pSurface->VertexNormal[u][w] = NormalCodeToD3DX(Q3Vertex[w].NormalCode);
			}
		}

		//cleanup Q3 Surface structs
		DQDeleteArray(Q3Shader);
		DQDeleteArray(Q3TexCoord);
		DQDeleteArray(Q3Vertex);

		//Goto End of Surface
		DQFS.Seek(Q3Surface->OffsetEnd+SurfaceStartOffset, FS_SEEK_SET, Handle);
	}	//end for(surfaces)

	DQDeleteArray(Q3Tag);
	DQDeleteArray(Q3Frame);
	DQDeleteArray(Q3Surface);

	return TRUE;
}

//TODO : Vertex Blending?
void c_MD3::LoadSurfaceIntoD3D(s_DQSurface *pSurface, int ThisFrame, int NextFrame, float Fraction)
{
	Assert(pSurface);
	DebugAssert(pSurface->NumTriangles*3<65535);

	WORD *pIndex, *pBaseIndex;
	int i;

	//Only load when necessary
	if(!bUseDynamicBuffer && pSurface->VertexBufferHandle) return;
	if(pSurface->LastUpdatedFrame==DQRender.FrameNum) {
		Error( ERR_RENDER, "Multiple MD3 LoadSurfaceIntoD3D per frame" );
		return;
	}

	//Create new handle if necessary
	if(!pSurface->VertexBufferHandle) {
		if(bUseDynamicBuffer) {
			pSurface->VertexBufferHandle = DQRenderStack.NewDynamicVertexBufferHandle( pSurface->NumVerts, TRUE, FALSE, FALSE );
		}
	}

	//Create temporary storage if necessary
	if(!pSurface->InterpolatedPos) {
		pSurface->InterpolatedPos = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[pSurface->NumVerts] );
	}
	if(!pSurface->InterpolatedNormal) {
		pSurface->InterpolatedNormal = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[pSurface->NumVerts] );
	}

	//Interpolate to create new surface
	for(i=0;i<pSurface->NumVerts;++i) {
		pSurface->InterpolatedPos[i] = pSurface->VertexPos[ThisFrame][i]*(1.0f-Fraction)+pSurface->VertexPos[NextFrame][i]*Fraction;
		pSurface->InterpolatedNormal[i] = pSurface->VertexNormal[ThisFrame][i]*(1.0f-Fraction)+pSurface->VertexNormal[NextFrame][i]*Fraction;
		D3DXVec3Normalize( &pSurface->InterpolatedNormal[i], &pSurface->InterpolatedNormal[i] );
	}

	//Update RenderStack
	if(bUseDynamicBuffer) {
		DQRenderStack.UpdateDynamicVerts( pSurface->VertexBufferHandle, 0, pSurface->NumVerts, pSurface->InterpolatedPos, pSurface->TexCoord, pSurface->InterpolatedNormal, NULL, NULL, pSurface->ShaderHandle );
	}
	else {
		pSurface->VertexBufferHandle = DQRenderStack.AddStaticVertices( pSurface->InterpolatedPos, pSurface->TexCoord, pSurface->InterpolatedNormal, NULL, NULL, pSurface->NumVerts );
	}

	//Create index buffer if necessary
	if(!pSurface->IndexBufferHandle) {

		pBaseIndex = pIndex = (WORD*)DQNewVoid( WORD[pSurface->NumTriangles*3] );

		for(i=0;i<pSurface->NumTriangles;++i) {
			*pIndex = pSurface->Triangle[i].Index[0];
			++pIndex;
			*pIndex = pSurface->Triangle[i].Index[1];
			++pIndex;
			*pIndex = pSurface->Triangle[i].Index[2];
			++pIndex;
		}

		pSurface->IndexBufferHandle = DQRenderStack.AddStaticIndices( pBaseIndex, pSurface->NumTriangles*3 );

		DQDeleteArray( pBaseIndex );
	}

	pSurface->LastUpdatedFrame = DQRender.FrameNum;
	isLoaded = TRUE;
}

/*void c_MD3::LoadIntoD3D()
{
	int i,u;
	MD3_D3DVertex_t *pD3DVertex;
	Vertex_t *pMD3Vertex;
	TexCoord_t *pTexCoord;
	Triangle_t *pTriangle;
	WORD *pIndex;
	Surface_t *pSurface;
	s_chain<s_D3DModel> *pD3DModelNode;
	s_D3DModel *pD3DModel;
	BOOL useVertexShader = FALSE, isSprite = FALSE;

	if(isLoaded==FALSE) error("No data loaded");

	for(i=0,pSurface=Surface;i<Header.NumSurfaces;++i,++pSurface)
	{
		//Match up Skin model names to the MD3 model names
		pD3DModelNode = D3DModelChain.GetFirstNode();
		while(pD3DModel) {
			if(!strcmpi((char*)Surface[i].Name, pD3DModelNode->current->ModelName)) break;
			pD3DModelNode = pD3DModelNode->next;
		}
		if(pD3DModelNode==NULL) { error("Skin file and MD3 file do not match"); return; }
		pD3DModel = pD3DModelNode->current;
		pD3DModel->SurfaceNum = i;

		//Check to see if MD3 surface uses a vertex shader
		if(pD3DModel->ShaderContainer.Shader) {
			useVertexShader = pD3DModel->ShaderContainer.Shader->useVertexShader;
			isSprite = pD3DModel->ShaderContainer.Shader->isSprite;
		}

		d3dcheckOK( device->CreateVertexBuffer( sizeof(MD3_D3DVertex_t)*pSurface->NumVerts, D3DUSAGE_WRITEONLY, MD3_FVF_VERTEX, D3DPOOL_DEFAULT, &pD3DModel->VertexBuffer, NULL) );
		d3dcheckOK( device->CreateIndexBuffer( sizeof(WORD)*3*pSurface->NumTriangles, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pD3DModel->IndexBuffer, NULL) );
		//Lock for writing
		pD3DModel->VertexBuffer->Lock(0,0,(void**)&pD3DVertex,0);
		pD3DModel->IndexBuffer->Lock(0,0,(void**)&pIndex,0);

		//Fill Vertices
		pTexCoord=pSurface->TexCoord;
		pMD3Vertex=pSurface->Vertex[0];			//Load Frame 0
		for(u=0;u<pSurface->NumVerts;++u,++pD3DVertex,++pMD3Vertex,++pTexCoord)
		{
			pD3DVertex->x = pMD3Vertex->x*MD3_XYZ_SCALE;
			pD3DVertex->y = pMD3Vertex->y*MD3_XYZ_SCALE;
			pD3DVertex->z = pMD3Vertex->z*MD3_XYZ_SCALE;
			pD3DVertex->normal = NormalCodeToD3DX(pMD3Vertex->NormalCode);
			pD3DVertex->u = pTexCoord->u;
			pD3DVertex->v = pTexCoord->v;
		}
		for(u=0,pTriangle = pSurface->Triangle;u<pSurface->NumTriangles;++u,++pTriangle)
		{
			*pIndex = pTriangle->Index[0];
			++pIndex;
			*pIndex = pTriangle->Index[1];
			++pIndex;
			*pIndex = pTriangle->Index[2];
			++pIndex;
		}

		if(isSprite) {
			for(u=0;u<3;++u) {
				g_SpriteVertexData[u].x = pSurface->Vertex[u]->x*MD3_XYZ_SCALE;
				g_SpriteVertexData[u].y = pSurface->Vertex[u]->y*MD3_XYZ_SCALE;
				g_SpriteVertexData[u].z = pSurface->Vertex[u]->z*MD3_XYZ_SCALE;
			}
		}

		pD3DModel->VertexBuffer->Unlock();
		pD3DModel->IndexBuffer->Unlock();

		if(pD3DModel->ShaderContainer.Shader)
			pD3DModel->ShaderContainer.Shader->PreLoad();
	}

	//Allocate Tag TM's
	TMTag = DQNew c_Frame *[Header.NumTags];
}

//***************************************************************************
*/

void c_MD3::InterpolateTag(orientation_t *tag, int TagNum, int ThisFrame, int NextFrame, float Fraction)
{
	MATRIX TM;
	D3DXQUATERNION q;

	Fraction = max(0.0f, Fraction);
	Fraction = min(1.0f, Fraction);

	ThisFrame = min( NumFrames-1, max( 0, ThisFrame ) );
	NextFrame = min( NumFrames-1, max( 0, NextFrame ) );
	TagNum = min( NumTags-1, max( TagNum, 0 ) );

	D3DXQuaternionSlerp( &q, &Tag[ThisFrame][TagNum].QRotation, &Tag[NextFrame][TagNum].QRotation, Fraction );
	D3DXMatrixRotationQuaternion( &TM, &q );
	TM._41 = Tag[ThisFrame][TagNum].Position.x*(1.0f-Fraction)+Tag[NextFrame][TagNum].Position.x*Fraction;
	TM._42 = Tag[ThisFrame][TagNum].Position.y*(1.0f-Fraction)+Tag[NextFrame][TagNum].Position.y*Fraction;
	TM._43 = Tag[ThisFrame][TagNum].Position.z*(1.0f-Fraction)+Tag[NextFrame][TagNum].Position.z*Fraction;

	tag->axis[0][0] = TM._11; tag->axis[0][1] = TM._12; tag->axis[0][2] = TM._13;
	tag->axis[1][0] = TM._21; tag->axis[1][1] = TM._22; tag->axis[1][2] = TM._23;
	tag->axis[2][0] = TM._31; tag->axis[2][1] = TM._32; tag->axis[2][2] = TM._33;
/*
	tag->axis[0][0] = TM._11; tag->axis[1][0] = TM._12; tag->axis[2][0] = TM._13;
	tag->axis[0][1] = TM._21; tag->axis[1][1] = TM._22; tag->axis[2][1] = TM._23;
	tag->axis[0][2] = TM._31; tag->axis[1][2] = TM._32; tag->axis[2][2] = TM._33;
*/
	tag->origin[0] = TM._41; tag->origin[1] = TM._42; tag->origin[2] = TM._43;

//	BBox.max.x += TM1._41; BBox.min.x += TM1._41; BSphere.centre.x += TM1._41;
//	BBox.max.y += TM1._42; BBox.min.y += TM1._42; BSphere.centre.y += TM1._42;
//	BBox.max.z += TM1._43; BBox.min.z += TM1._43; BSphere.centre.z += TM1._43;
}

/*
//Takes input in_Time (in secs) and in_FPS, returns Frame number and the fractional frame
void c_MD3Manager::GetLinearInterpolatedFrame(float in_Time, S32 *out_ThisFrame, S32 *out_NextFrame, float *out_Fraction)
{
	float fFloorFrame;
	float fFrame;
	S32 sFloorFrame;

	fFrame = in_Time * AnmFrameInfo[AnimationType].FPS;
	fFloorFrame = (float)floor(fFrame);
	sFloorFrame = (S32)fFloorFrame;

	//check validity
	if(sFloorFrame>=AnmFrameInfo[AnimationType].NumFrames) {
		//fix it (hack)
		*out_ThisFrame = AnmFrameInfo[AnimationType].FirstFrame + AnmFrameInfo[AnimationType].NumFrames-1;
		*out_NextFrame = *out_ThisFrame;
		*out_Fraction = 0;
		return;
	}

	*out_ThisFrame = sFloorFrame + AnmFrameInfo[AnimationType].FirstFrame;
	*out_Fraction = fFrame - fFloorFrame;
	if(sFloorFrame==AnmFrameInfo[AnimationType].NumFrames-1) {  //if this is the last frame
		if(AnmFrameInfo[AnimationType].LoopingFrames!=0)
			*out_NextFrame = AnmFrameInfo[AnimationType].FirstFrame; //loop back to 1st frame
		else { *out_NextFrame = *out_ThisFrame; }
	}
	else {
		*out_NextFrame = *out_ThisFrame + 1;
	}
}

void c_MD3::LoadFrameIntoD3D(S32 ThisFrame, S32 NextFrame, float Fraction)
{
	int u,SurfaceNum;
	MD3_D3DVertex_t *pD3DVertex;
	Vertex_t *pThisVertex,*pNextVertex;
	s_chain<s_D3DModel> *pD3DModelNode;
	s_D3DModel *pD3DModel;

	if(ThisFrame>=NumFrames) return;	
	if(NextFrame>=NumFrames) return;	

	pD3DModelNode = D3DModelChain.GetFirstNode();
	while(pD3DModelNode) {
		pD3DModel = pD3DModelNode->current;
		SurfaceNum = pD3DModel->SurfaceNum;

		pD3DModel->VertexBuffer->Lock(0,0,(void**)&pD3DVertex,0);

		//Fill Vertices
		pThisVertex=Surface[SurfaceNum].Vertex[ThisFrame];
		pNextVertex=Surface[SurfaceNum].Vertex[NextFrame];
		for(u=0;u<Surface[SurfaceNum].NumVerts;++u,++pD3DVertex,++pThisVertex, ++pNextVertex)
		{
			pD3DVertex->x = (pThisVertex->x*(1.0f-Fraction)+pNextVertex->x*Fraction)*MD3_XYZ_SCALE;
			pD3DVertex->y = (pThisVertex->y*(1.0f-Fraction)+pNextVertex->y*Fraction)*MD3_XYZ_SCALE;
			pD3DVertex->z = (pThisVertex->z*(1.0f-Fraction)+pNextVertex->z*Fraction)*MD3_XYZ_SCALE;
			pD3DVertex->normal = NormalCodeToD3DX(pThisVertex->NormalCode); //TODO Transform Normals;
		}

		pD3DModelNode->current->VertexBuffer->Unlock();
		pD3DModelNode = pD3DModelNode->next;
	}

	//Update Bounding Box & Sphere
	// - No point. max & min change with orientation, so it's useless
	BBox.max.x = Frame[ThisFrame].MaxBounds[0];
	BBox.max.y = Frame[ThisFrame].MaxBounds[1];
	BBox.max.z = Frame[ThisFrame].MaxBounds[2];
	BBox.min.x = Frame[ThisFrame].MinBounds[0];
	BBox.min.y = Frame[ThisFrame].MinBounds[1];
	BBox.min.z = Frame[ThisFrame].MinBounds[2];
	BSphere.centre.x = Frame[ThisFrame].LocalOrigin[0];
	BSphere.centre.y = Frame[ThisFrame].LocalOrigin[1];
	BSphere.centre.z = Frame[ThisFrame].LocalOrigin[2];
	BSphere.radius = Frame[ThisFrame].Radius;
//	CreateBox(); //create a VB for the BBox : Significant slowdown due to lock/unlock
	eCullMethod = eCullMethodBSphere
}
*/
//*************************************************************************

D3DXVECTOR3 c_MD3::NormalCodeToD3DX(S16 code)
{
	F32 latitude,longitude;
	D3DXVECTOR3 vec3;

	latitude = (code & 0xFF) *D3DX_PI/127.5f;
	longitude = ((code >> 8) & 0xFF) *D3DX_PI/127.5f;

	vec3.x = (float)cos((float)latitude)*(float)sin((float)longitude);
	vec3.y = (float)sin((float)latitude)*(float)sin((float)longitude);
	vec3.z = (float)cos((float)longitude);

	return vec3;
}

//*************************************************************************

void c_MD3::DrawMD3( s_RenderFeatureInfo *RenderFeature ) 
{
	int i;
	s_DQSurface *pSurface;
	int FogShader, ShaderHandle;
	D3DXVECTOR3 Position, Min, Max;

	if(isLoaded==FALSE) return;

	if(RenderFeature->pTM) {
		Position = D3DXVECTOR3( RenderFeature->pTM->_41, RenderFeature->pTM->_42, RenderFeature->pTM->_43 );
	}
	else {
		Position = D3DXVECTOR3( 0,0,0 );
	}
	GetModelBounds( Min, Max );
	Position += 0.5f*( Min+Max );
	
	//Add fog if necessary
	FogShader = DQBSP.CheckFogStatus( Position, Min, Max );

	for(i=0,pSurface=&Surface[0];i<NumSurfaces;++i,++pSurface) 
	{
		if( FogShader ) {
			ShaderHandle = DQRender.AddEffectToShader( pSurface->ShaderHandle, FogShader );
		}
		else {
			ShaderHandle = pSurface->ShaderHandle;
		}

		DQRenderStack.DrawIndexedPrimitive( pSurface->VertexBufferHandle, pSurface->IndexBufferHandle, ShaderHandle, D3DPT_TRIANGLELIST, 0, 0, pSurface->NumTriangles, Position, RenderFeature );
	}
}

void c_MD3::LoadMD3IntoD3D(int ThisFrame, int NextFrame, float Fraction)
{
	int i;

//	InterpolateTags(ThisFrame, NextFrame, Fraction);
	if(NextFrame<0 || NextFrame>=NumFrames || ThisFrame<0 || ThisFrame>=NumFrames) {
		Error( ERR_RENDER, "LoadMD3 : Invalid frame" );
		return;
	}

	//if this frame is currently loaded, skip
	if(!bUseDynamicBuffer && LoadedThisFrame == ThisFrame && LoadedNextFrame==NextFrame && modulus(LoadedFraction-Fraction)<0.0001f) {
		return;
	}
	LoadedThisFrame = ThisFrame;
	LoadedNextFrame = NextFrame;
	LoadedFraction = Fraction;
	
	for(i=0;i<NumSurfaces;++i) {
		LoadSurfaceIntoD3D(&Surface[i], ThisFrame, NextFrame, Fraction);
	}
}

int c_MD3::GetTagNum(const char *TagName)
{
	int i;
	for(i=0;i<NumTags;++i) {
		if(DQstrcmpi(TagIndex[i].TagName, TagName, MAX_QPATH)==0) return i;
	}
	return -1; //not found
}

void c_MD3::SetSkin( c_Skin *Skin )
{
	int i;
	for(i=0;i<NumSurfaces;++i) {
		Surface[i].ShaderHandle = Skin->GetShaderFromSurfaceName(Surface[i].Name);
	}
}

void c_MD3::GetModelBounds( D3DXVECTOR3 &Min, D3DXVECTOR3 &Max )
{
	Min = Frame[LoadedThisFrame].BBox.min;
	Max = Frame[LoadedThisFrame].BBox.max;
}
