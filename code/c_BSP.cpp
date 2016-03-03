// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)

#include "stdafx.h"

c_BSP::c_BSP() {
	Initialise();
}

c_BSP::~c_BSP() {
	Unload();
}

void c_BSP::Initialise()
{
	EntityString = NULL;
	Texture = NULL;
	Plane = NULL;
	Node = NULL;
	Leaf = NULL;
	LeafFace = NULL;
	LeafBrush = NULL;
	Model = NULL;
	Brush = NULL;
	BrushSide = NULL;
	MeshVerts = NULL;
	Effect = NULL;
	Face = NULL;
	Lightmap = NULL;
	Lightvol = NULL;
	VisData.vec = NULL;
	Vertices.Pos = NULL;
	Vertices.Normal = NULL;
	Vertices.TexCoords = NULL;
	Vertices.LightmapCoords = NULL;
	Vertices.Diffuse = NULL;

	BSPShaderArray = NULL;
	BSPEffectArray = NULL;
	LightmapTextureBuffer = NULL;

	NumPatches = NumIndexBuffers = 0;
	NumVerts = NumFaces = NumTextures = NumQ3Lightmaps = NumLightmaps = NumLeaves = NumEffects = 0;
	NumPlanes = NumInlineModels = 0;
	EntityTokenPos = NULL;
	LastEntityToken = 0;

	FileHandle = NULL;
	DrawSky = FALSE;
	for(int i=0;i<6;++i) {
		SkyBoxShader[i] = NULL;
		SkyBoxFilename[i][0] = '\0';
	}
	CloudShaderHandle = 0;
	LoadedMapname[0] = '\0';
	LightNum = 0;

	memset( TraceNodeCache, -1, sizeof(int)*MAX_NODECACHE );

	DQConsole.RegisterCVar( "r_useBSP", "1", 0, eAuthClient, &r_useBSP );
	DQConsole.RegisterCVar( "cm_playerCurveClip", "1", 0, eAuthClient, &cm_playerCurveClip );

	CurveBrush = NULL;
	CurveBrushPoints = NULL;
	NumCurveBrushPoints = 0;
	NumCurveBrushes = 0;

	FacesDrawn = 0;
//	TraceCount = 0;

	isLoaded = isLoadedIntoD3D = FALSE;
}

void c_BSP::Unload()
{
	DQDeleteArray( EntityString );
	DQDeleteArray( Texture );
	DQDeleteArray( Plane );
	DQDeleteArray( Node );
	DQDeleteArray( Leaf );
	DQDeleteArray( LeafFace );
	DQDeleteArray( LeafBrush );
	DQDeleteArray( Model );
	DQDeleteArray( Brush );
	DQDeleteArray( BrushSide );
	DQDeleteArray( MeshVerts );
	DQDeleteArray( Effect );
	DQDeleteArray( Lightmap );
	DQDeleteArray( Lightvol );
	DQDeleteArray( VisData.vec );
	DQDeleteArray( Vertices.Pos );
	DQDeleteArray( Vertices.Normal );
	DQDeleteArray( Vertices.TexCoords );
	DQDeleteArray( Vertices.LightmapCoords );
	DQDeleteArray( Vertices.Diffuse );

	EntityTokenPos = NULL;
	LastEntityToken = 0;

	DQDeleteArray( CurveBrush );
	DQDeleteArray( CurveBrushPoints );

	UnloadD3D();
	DQDeleteArray( Face );

	isLoaded = FALSE;

	Initialise();	//for next load
}

void c_BSP::UnloadD3D() 
{
	int i;

	if(LightmapTextureBuffer) {
		for( i=0; i<NumLightmaps; ++i) DQDelete( LightmapTextureBuffer[i] );
		DQDeleteArray( LightmapTextureBuffer );
	}

	if(Face) {
		for(i=0;i<NumFaces;++i) {
			DQDelete(Face[i].BezierPatch);
			DQDelete(Face[i].Billboard);
		}
	}

	DQDeleteArray( BSPShaderArray );
	DQDeleteArray( BSPEffectArray );

	for(i=0;i<6;++i) SkyBoxShader[i] = NULL;
	CloudShaderHandle = 0;

	isLoadedIntoD3D = FALSE;
}

#define LoadLump(number, kNumber, s_type, dest) \
	DQFS.Seek(LumpDirectory[kNumber].offset, FS_SEEK_SET, FileHandle); \
	number = LumpDirectory[kNumber].length/sizeof(s_type); \
	dest = (s_type*)DQNewVoid( s_type[number] ); \
	DQFS.ReadFile((void*)dest, sizeof(s_type), number, FileHandle)

BOOL c_BSP::LoadBSP(const char *Mapname)
{
	S32 ident;		//Should be IBSP
	S32 version;
	int number;		//Multi-use variable
	int i;
	BOOL bUseVertexLighting;
	bUseVertexLighting = (DQConsole.GetCVarInteger( "r_vertexlight", eAuthClient ) )? TRUE:FALSE;

	EntityTokenPos = NULL;
	LastEntityToken = 0;

	if(DQstrcmpi(Mapname, LoadedMapname, MAX_QPATH)==0) return TRUE;	//skip this if map already loaded, eg. when running local server

	if(isLoaded) {
		Unload();
	}

	FileHandle = DQFS.OpenFile(Mapname, "rb");
	if(!FileHandle) {
		Error(ERR_FS, va("Map %s not found",Mapname) );
		return FALSE;
	}

	DQFS.ReadFile((void*)&ident, sizeof(S32), 1, FileHandle);
	
	if(ident==0x50534249) Endian=LITTLE_ENDIAN;
	else if(ident==0x49425350) {
		Endian = BIG_ENDIAN;
		Error(ERR_FS, "Big Endian File Format not implemented");
		return FALSE;
	}
	else {
		Error(ERR_FS, "Not a Q3 BSP file");
		return FALSE;
	}

	DQFS.ReadFile((void*)&version, sizeof(S32), 1, FileHandle);
	if(version != 46) Error(ERR_FS, "Incompatible BSP Version");

	DQFS.ReadFile((void*)LumpDirectory, sizeof(s_BSPLump), kMaxLumps, FileHandle);

	//kEntities
	DQFS.Seek(LumpDirectory[kEntities].offset, FS_SEEK_SET, FileHandle);
	EntityString = (char*)DQNewVoid( char [LumpDirectory[kEntities].length+1] );
	DQFS.ReadFile((void*)EntityString, LumpDirectory[kEntities].length, 1, FileHandle);
	EntityString[LumpDirectory[kEntities].length] = 0;

	//kTextures
	LoadLump( NumTextures, kTextures, s_Textures, Texture );

	//kPlanes
	s_Planes *Q3Plane;
	LoadLump(NumPlanes, kPlanes, s_Planes, Q3Plane);
	Plane = (s_DQPlanes*)DQNewVoid( s_DQPlanes[NumPlanes] );
	for(i=0;i<NumPlanes;++i) {
		Plane[i].dist = Q3Plane[i].distance;
		Plane[i].normal = MakeD3DXVec3FromFloat3( Q3Plane[i].normal );
		Plane[i].CalcType();
	}
	DQDeleteArray( Q3Plane );

	//kNodes
	D3DXVECTOR3 Min, Max;
	s_Nodes *Q3Nodes;
	LoadLump(number, kNodes, s_Nodes, Q3Nodes);
	Node = (s_DQNode*)DQNewVoid( s_DQNode[number] );
	for(i=0; i<number; ++i) {
		Node[i].childBack = Q3Nodes[i].childBack;
		Node[i].childFront = Q3Nodes[i].childFront;
		Node[i].PlaneIndex = Q3Nodes[i].PlaneIndex;
		Min = MakeD3DXVec3FromFloat3( Q3Nodes[i].min );
		Max = MakeD3DXVec3FromFloat3( Q3Nodes[i].max );
		Node[i].Box.MidpointPos = (Min+Max)/2;
		Node[i].Box.Extent = Max-Node[i].Box.MidpointPos;
		Node[i].Box.Extent.x = max( Node[i].Box.Extent.x, -Node[i].Box.Extent.x );
		Node[i].Box.Extent.y = max( Node[i].Box.Extent.y, -Node[i].Box.Extent.y );
		Node[i].Box.Extent.z = max( Node[i].Box.Extent.z, -Node[i].Box.Extent.z );
		Node[i].BoxRadius = D3DXVec3Length( &Node[i].Box.Extent ) + EPSILON;
		Node[i].IgnoreCluster = -2;
	}
	DQDeleteArray( Q3Nodes );

	//kLeaves
	s_Leaves *Q3Leaves;
	LoadLump(NumLeaves, kLeaves, s_Leaves, Q3Leaves);
	Leaf = (s_DQLeaf*)DQNewVoid( s_DQLeaf[NumLeaves] );
	for( i=0; i<NumLeaves; ++i ) {
		Leaf[i].Cluster = Q3Leaves[i].Cluster;
		Leaf[i].firstLeafFace = Q3Leaves[i].firstLeafFace;
		Leaf[i].numLeafFace = Q3Leaves[i].numLeafFace;
		Leaf[i].firstLeafBrush = Q3Leaves[i].firstLeafBrush;
		Leaf[i].numLeafBrush = Q3Leaves[i].numLeafBrush;
//		Leaf[i].CurveBrush = -1;
	}
	DQDeleteArray( Q3Leaves );

	//kModels
	s_Models *Q3Model;
	LoadLump(NumInlineModels, kModels, s_Models, Q3Model);
	Model = (s_DQModels*)DQNewVoid( s_DQModels[NumInlineModels] );
	for( i=0; i<NumInlineModels; ++i ) {
		Model[i].Min = MakeD3DXVec3FromFloat3( Q3Model[i].min );
		Model[i].Max = MakeD3DXVec3FromFloat3( Q3Model[i].max );
		Model[i].firstFace = Q3Model[i].firstFace;
		Model[i].numFace = Q3Model[i].numFace;
		Model[i].firstBrush = Q3Model[i].firstBrush;
		Model[i].numBrush = Q3Model[i].numBrush;
		Model[i].ent = NULL;
		Model[i].ModelHandle = 0;
	}
	DQDeleteArray( Q3Model );

	//kBrushes
	s_Brushes *Q3Brush;
	LoadLump(number, kBrushes, s_Brushes, Q3Brush);
	Brush = (s_DQBrush*)DQNewVoid( s_DQBrush[number] );
	for( i=0; i<number; ++i ) {
		Brush[i].firstBrushSide = Q3Brush[i].firstBrushSide;
		Brush[i].numBrushSide = Q3Brush[i].numBrushSide;
		Brush[i].texture = Q3Brush[i].texture;
//		Brush[i].TraceCount = -1;
	}
	DQDeleteArray( Q3Brush );

	LoadLump(number, kLeafFaces, S32, LeafFace);
	LoadLump(number, kLeafBrushes, s_LeafBrushes, LeafBrush);
	LoadLump(number, kBrushSides, s_BrushSides, BrushSide);
	LoadLump(NumEffects, kEffects, s_Effects, Effect);

	//kVertices
	//(Reorder data from Q3 BSP format)
	s_Vertices *Q3Verts;
	LoadLump(NumVerts, kVertices, s_Vertices, Q3Verts);
	Vertices.Pos			= (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );
	Vertices.Normal			= (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );
	Vertices.TexCoords		= (D3DXVECTOR2*)DQNewVoid( D3DXVECTOR2[NumVerts] );
	Vertices.LightmapCoords = (D3DXVECTOR2*)DQNewVoid( D3DXVECTOR2[NumVerts] );
	Vertices.Diffuse		= (DWORD*)DQNewVoid( DWORD[NumVerts] );
	for(i=0;i<NumVerts;++i) {
		Vertices.Pos[i]				= MakeD3DXVec3FromFloat3( Q3Verts[i].position );
		Vertices.Normal[i]			= MakeD3DXVec3FromFloat3( Q3Verts[i].normal );
		Vertices.TexCoords[i]		= MakeD3DXVec2FromFloat2( Q3Verts[i].texCoord );
		Vertices.LightmapCoords[i]	= MakeD3DXVec2FromFloat2( Q3Verts[i].lightmapCoord );
		if( bUseVertexLighting ) {
			Vertices.Diffuse[i]		= D3DCOLOR_RGBA( Q3Verts[i].colour[0], Q3Verts[i].colour[1], Q3Verts[i].colour[2], Q3Verts[i].colour[3] );
		}
		else {
			Vertices.Diffuse[i]		= 0xFFFFFFFF;
		}
	}
	DQDeleteArray( Q3Verts );

	//kMeshVerts
	s_MeshVerts *Q3MeshVerts;
	LoadLump(NumMeshVerts, kMeshVerts, s_MeshVerts, Q3MeshVerts);
	MeshVerts = (WORD*)DQNewVoid( int[NumMeshVerts] );

	for(i=0;i<NumMeshVerts;++i) {
		Assert(Q3MeshVerts[i].offset<65535);
		MeshVerts[i] = Q3MeshVerts[i].offset;
	}
	DQDeleteArray( Q3MeshVerts );

	//kFaces
//	int *CurvedFaceIndex;
	NumCurveBrushes = 0;
	s_Faces *Q3Face;
	LoadLump(NumFaces, kFaces, s_Faces, Q3Face);

//	CurvedFaceIndex = (int*) DQNewVoid( int[NumFaces] );
	Face = (s_DQFaces*)DQNewVoid( s_DQFaces[NumFaces] );
	ZeroMemory( Face, NumFaces*sizeof(s_DQFaces) );

	for(i=0;i<NumFaces;++i) {
		memcpy( &Face[i].Q3Face, &Q3Face[i], sizeof(s_Faces) );
		if( (Face[i].Q3Face.type == BSPFACE_BEZIERPATCH) && (Texture[Face[i].Q3Face.texture].contents & CONTENTS_SOLID) ) {
//			CurvedFaceIndex[i] = NumCurveBrushes;
			++NumCurveBrushes;
		}
	}
	DQDeleteArray( Q3Face );

	LoadLump(NumQ3Lightmaps, kLightmaps, s_Lightmaps, Lightmap);
	LoadLump(number, kLightvols, s_Lightvols, Lightvol);
	NumLightvols_x = floor(Model[0].Max.x / 64)  - ceil(Model[0].Min.x / 64)  + 1;
	NumLightvols_y = floor(Model[0].Max.y / 64)  - ceil(Model[0].Min.y / 64)  + 1;
	NumLightvols_z = floor(Model[0].Max.z / 128) - ceil(Model[0].Min.z / 128) + 1;
	
	DQFS.Seek(LumpDirectory[kVisData].offset, FS_SEEK_SET, FileHandle);
	DQFS.ReadFile((void*)&VisData.numVec, sizeof(S32), 1, FileHandle);
	DQFS.ReadFile((void*)&VisData.sizeVec, sizeof(S32), 1, FileHandle);
	VisData.vec = (U8*)DQNewVoid( U8 [VisData.numVec*VisData.sizeVec] );
	DQFS.ReadFile((void*)VisData.vec, sizeof(U8), VisData.numVec*VisData.sizeVec, FileHandle);

	DQstrcpy(LoadedMapname, Mapname, MAX_QPATH);

	//Create CurvedSurfaceBrushes
	
	int CBIndex;
	CurveBrush = (s_CurveBrush*) DQNewVoid( s_CurveBrush[NumCurveBrushes] );
	CurveBrushPoints = (s_CurveBrushPoint*) DQNewVoid( s_CurveBrushPoint[MAX_CURVEBRUSHPOINTS] );
	CBIndex = 0;
	NumCurveBrushPoints = 0;
	for( i=0; i<NumFaces; ++i ) {
		if( (Face[i].Q3Face.type == BSPFACE_BEZIERPATCH) && (Texture[Face[i].Q3Face.texture].contents & CONTENTS_SOLID) ) {
			//Make a Brush
			CreateCurveBrush( i, CBIndex++ );
		}
	}

/*	int j;
	for( i=0; i<NumLeaves; ++i ) {
		for( j=Leaf[i].firstLeafFace; j<Leaf[i].firstLeafFace+Leaf[i].numLeafFace; ++j ) {

			if( (Face[LeafFace[j]].Q3Face.type == BSPFACE_BEZIERPATCH) && (Texture[Face[LeafFace[j]].Q3Face.texture].contents & CONTENTS_SOLID)  ) {
				//Find the Curved surface Brush
				Leaf[i].CurveBrush = CurvedFaceIndex[LeafFace[j]];
			}
		}
	}
	DQDeleteArray( CurvedFaceIndex );
*/
	Assert( NumCurveBrushPoints<MAX_CURVEBRUSHPOINTS );

	isLoaded = TRUE;

	return TRUE;
}

#undef LoadLump

BOOL c_BSP::GetEntityToken( char *buffer, int bufsize )
{
	int pos, pos2;
	char mapname[MAX_QPATH];

	if(!isLoaded) {
		DQConsole.GetCVarString("mapname", mapname, MAX_QPATH, eAuthServer);
		if( !LoadBSP( va("maps/%s.bsp",mapname) ) )
			return FALSE;
	}

	//reset cached position
	if(LastEntityToken == 0) EntityTokenPos = EntityString;

	pos = DQSkipWhitespace( EntityTokenPos, MAX_INFO_STRING );
	if(EntityTokenPos[pos]=='"') {
		pos2 = pos + DQCopyUntil( buffer, &EntityTokenPos[pos+1], '"', bufsize ) + 1;
	}
	else {
		pos2 = pos + DQWordstrcpy( buffer, &EntityTokenPos[pos], bufsize );
	}
	if(pos2<=pos) return FALSE;

	EntityTokenPos = &EntityTokenPos[pos2];
	DQStripQuotes( buffer, bufsize );
	++LastEntityToken;

	return TRUE;	
}

//********************************************************************

void c_BSP::LoadIntoD3D() {
	int i, u, MeshVertOffset;
	int *pInt, *ShaderCount;
	s_ChainNode<int> *Node;
	c_Chain<int> DeformVertexShaders;
	D3DXVECTOR3 ApproxPos;
	int *BSPShaderPlusEffect;

	if(isLoadedIntoD3D) return;

	CurveSubdivisions = DQConsole.GetCVarInteger( "r_subdivisions", eAuthClient );
	if( CurveSubdivisions<=4 ) CurveSubdivisions = 4;
	else if( CurveSubdivisions<=8 ) CurveSubdivisions = 8;
	else CurveSubdivisions = 16;
	DQConsole.SetCVarValue( "r_subdivisions", CurveSubdivisions, eAuthClient );

	//Load Textures
	CreateTexturesFromLightmaps();

	//Load Verts
	BSPVBhandle = DQRenderStack.AddStaticVertices( Vertices.Pos, Vertices.TexCoords, Vertices.Normal, Vertices.Diffuse, Vertices.LightmapCoords, NumVerts );

	//Load Shaders
	BSPShaderArray = (int*) DQNewVoid ( int [NumTextures] );
	BSPShaderPlusEffect = (int*) DQNewVoid ( int [NumTextures] );
	ZeroMemory( BSPShaderPlusEffect, sizeof(int)*NumTextures );

	ShaderCount = (int*) DQNewVoid ( int[MAX_SHADERS] );
	ZeroMemory( ShaderCount, sizeof(int)*MAX_SHADERS );

	for(i=0;i<NumTextures;++i) {
		BSPShaderArray[i] = DQRender.RegisterShader( Texture[i].name, ShaderFlag_BSPShader );
		// Reload any verts whose faces have a VertexDeform Shader into a Dynamic Vertex Buffer
		if( DQRender.ShaderGetFlags( BSPShaderArray[i] ) & ShaderFlag_UseDeformVertex ) {
			//Keep track of these shaders, for comparison when loading faces
			pInt = DeformVertexShaders.AddUnit();
			*pInt = BSPShaderArray[i];
		}
	}
	for( i=0; i<6; ++i ) {
		if( SkyBoxFilename[i][0] )
			SkyBoxShader[i] = DQRender.RegisterShader( SkyBoxFilename[i], ShaderFlag_SkyboxShader );
	}

	BSPEffectArray = (int*) DQNewVoid ( int [NumEffects] );
	for(i=0;i<NumEffects;++i) {
		BSPEffectArray[i] = DQRender.RegisterShader( Effect[i].name, 0 );
	}

	//Initialise MeshVert index buffer
	BSPMeshIndicesHandle = DQRenderStack.AddStaticIndices( MeshVerts, NumMeshVerts );

	//Initialise s_DQFaces struct
	MeshVertOffset = 0;
	for(i=0;i<NumFaces;++i) {

		//Cache rendering information
		if( Face[i].Q3Face.type == BSPFACE_BILLBOARD ) Face[i].Q3Face.effect = -1;
		if( Face[i].Q3Face.effect>=0 ) {
			if( !BSPShaderPlusEffect[Face[i].Q3Face.texture] ) {
				BSPShaderPlusEffect[Face[i].Q3Face.texture] = DQRender.AddEffectToShader( BSPShaderArray[Face[i].Q3Face.texture], BSPEffectArray[Face[i].Q3Face.effect] );
			}
			Face[i].ShaderHandle = BSPShaderPlusEffect[Face[i].Q3Face.texture];
		}
		else {
			Face[i].ShaderHandle = BSPShaderArray[Face[i].Q3Face.texture];
		}
		++ShaderCount[Face[i].ShaderHandle];

		Face[i].DynamicVB = NULL;
		//Check if this shader uses DeformVertex
		for( Node = DeformVertexShaders.GetFirstNode(); Node ; Node = Node->next) {
			if( Face[i].ShaderHandle == *Node->current ) {
				//this face uses a DeformVertex shader

				if( (Face[i].Q3Face.type == BSPFACE_BILLBOARD) || (Face[i].Q3Face.type == BSPFACE_BEZIERPATCH) )
					continue;

				if( Face[i].Q3Face.type == BSPFACE_REGULARFACE ) {
					Face[i].Q3Face.type = BSPFACE_DYNAMICFACE;
				} else if( Face[i].Q3Face.type == BSPFACE_MESH ) {
					Face[i].Q3Face.type = BSPFACE_DYNAMICMESH;
				}
				Face[i].DynamicVB = DQRenderStack.NewDynamicVertexBufferHandle( Face[i].Q3Face.numVertex, TRUE, TRUE, TRUE );
				Assert( Face[i].DynamicVB );
			}
		}

		//Load other render information
		Face[i].LightmapTexture = (Face[i].Q3Face.lightmapIndex<0)?NULL:LightmapTextureBuffer[Face[i].Q3Face.lightmapIndex];
		Face[i].BezierPatch = NULL;
		Face[i].Billboard = NULL;

		ApproxPos = D3DXVECTOR3(0,0,0);
		if(Face[i].Q3Face.type == BSPFACE_MESH ) {
			for( u=0; u<Face[i].Q3Face.numMeshVert; ++u ) {
				ApproxPos += Vertices.Pos[MeshVerts[Face[i].Q3Face.firstMeshVert]];
			}
			Face[i].ApproxPos = ApproxPos / Face[i].Q3Face.numMeshVert;
		}
		else if( Face[i].Q3Face.type == BSPFACE_BILLBOARD ) {
			s_Billboard *Billboard;
			D3DXVECTOR3 Centre;

			Face[i].Billboard = (s_Billboard*)DQNewVoid( s_Billboard );
			Billboard = Face[i].Billboard;

			//use LightmapOrigin as origin

			Centre = MakeD3DXVec3FromFloat3( Face[i].Q3Face.lightmapOrigin );
			Billboard->Axis1 = D3DXVECTOR3( 20.0f,0,0 );
			Billboard->Axis2 = D3DXVECTOR3( 0,0,20.0f );
			Billboard->Position[0] = Centre + Billboard->Axis1 + Billboard->Axis2;	Billboard->TexCoord[0].x = 0.0f; Billboard->TexCoord[0].y = 0.0f;
			Billboard->Position[1] = Centre - Billboard->Axis1 + Billboard->Axis2;	Billboard->TexCoord[1].x = 0.0f; Billboard->TexCoord[1].y = 1.0f;
			Billboard->Position[2] = Centre + Billboard->Axis1 - Billboard->Axis2;	Billboard->TexCoord[2].x = 1.0f; Billboard->TexCoord[2].y = 0.0f;
			Billboard->Position[3] = Centre - Billboard->Axis1 - Billboard->Axis2;	Billboard->TexCoord[3].x = 1.0f; Billboard->TexCoord[3].y = 1.0f;
			D3DXVec3Normalize( &Billboard->Axis1, &Billboard->Axis1 );
			D3DXVec3Normalize( &Billboard->Axis2, &Billboard->Axis2 );
			D3DXVec3Cross( &Billboard->Normal, &Billboard->Axis2, &Billboard->Axis1 );
			D3DXVec3Normalize( &Billboard->Normal, &Billboard->Normal );

			Billboard->OnlyAlignAxis1 = FALSE;
			Billboard->RenderFeatures.ShaderRGBA = 0xFFFFFFFF;
			Billboard->RenderFeatures.ShaderTime = 0;
			Billboard->RenderFeatures.LightmapTexture = NULL;
			Billboard->RenderFeatures.DrawAsSprite = FALSE;
			Billboard->RenderFeatures.bApplyLightVols = FALSE;
			Billboard->RenderFeatures.pTM = NULL;
			Billboard->ShaderHandle = Face[i].ShaderHandle;
		}
		else {
			for( u=0; u<Face[i].Q3Face.numVertex; ++u ) {
				ApproxPos += Vertices.Pos[Face[i].Q3Face.firstVertex];
			}
			Face[i].ApproxPos = ApproxPos / Face[i].Q3Face.numVertex;
		}

#ifdef _PORTALS
		//Initialise Portal shaders
		if(DQRender.ShaderGetFlags( Face[i].ShaderHandle ) & ShaderFlag_IsPortal) {
			DQRender.SetPortalShaderPosition( Face[i].ShaderHandle, Face[i].ApproxPos );
		}
#endif

		//Initialise Curved Surfaces
		if(Face[i].Q3Face.type == BSPFACE_BEZIERPATCH) {
			Face[i].BezierPatch = DQNew( c_CurvedSurface );
			Face[i].BezierPatch->LoadQ3PatchIntoD3D( i, CurveSubdivisions );
		}
	}

	for(i=0;i<MAX_SHADERS;++i) {
		if(ShaderCount[i]) {
			DQRenderStack.AllocateShaderHint( i, ShaderCount[i] );
		}
	}

	DQDeleteArray( ShaderCount );
	DQDeleteArray( BSPShaderPlusEffect );

	//For portals
#ifdef _PORTALS
	HRESULT hr;

	for(i=0; i<DQRender.NumRenderToTextures; ++i) {
		DQRender.RenderToTexture[i].Camera.Initialise();
		DQRender.RenderToTexture[i].bEnable = FALSE;
	//	hr = Direct3D9->CheckDeviceFormat( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8 );
		
		hr = D3DXCreateRenderToSurface( DQDevice, DQRender.GLconfig.vidWidth/2, DQRender.GLconfig.vidHeight/2, D3DFMT_A8R8G8B8, TRUE, D3DFMT_D16, &DQRender.RenderToTexture[i].D3DXRenderToSurface );
		if( hr!= D3D_OK ) DQRender.RenderToTexture[i].bCaps_RenderToSurface = FALSE;
		else DQRender.RenderToTexture[i].bCaps_RenderToSurface = TRUE;
		
		hr = D3DXCreateTexture( DQDevice, DQRender.GLconfig.vidWidth/2 DQRender.GLconfig.vidHeight/2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &DQRender.RenderToTexture[i].RenderTarget );
		if( hr!= D3D_OK ) DQRender.RenderToTexture[i].bCaps_RenderToSurface = FALSE;
		else DQRender.RenderToTexture[i].bCaps_RenderToSurface = TRUE;
		
		d3dcheckOK( DQRender.RenderToTexture[i].RenderTarget->GetSurfaceLevel( 0, &DQRender.RenderToTexture[i].RenderSurface ) );
	}
#endif //_PORTALS

	//Create Skybox and Skydome (for cloud layer)
	D3DXVECTOR3 *SkyVerts;
	D3DXVECTOR2 *SkyTexCoords;
	WORD *SkyIndices;
	float Size;

	D3DXVECTOR3 Dir;
	int NumVerts, NumIndices;
	DQRender.GetSquareArrayDimensions( &NumIndices, &NumVerts, SKYBOX_TESSELATION );

	Size = ZFAR/1.74f;

	SkyVerts = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );
	SkyTexCoords = (D3DXVECTOR2*)DQNewVoid( D3DXVECTOR2[NumVerts] );
	SkyIndices = (WORD*)DQNewVoid( WORD[NumIndices] );

	for( i=0; i<6; ++i ) {
		switch(i) {
		case 0: Dir = D3DXVECTOR3( 0, 0, Size ); break; //top
		case 1: Dir = D3DXVECTOR3( 0, 0, -Size ); break; //bottom
		case 2: Dir = D3DXVECTOR3( Size, 0, 0 ); break; //left
		case 3: Dir = D3DXVECTOR3( -Size, 0, 0 ); break; //right
		case 4: Dir = D3DXVECTOR3( 0, Size, 0 ); break;	//back
		case 5: Dir = D3DXVECTOR3( 0, -Size, 0 ); break; //front
		};
		DQRender.CreateSquare( Dir, SkyVerts, SkyTexCoords, SkyIndices, SKYBOX_TESSELATION );
		SkyboxVBHandle[i] = DQRenderStack.AddStaticVertices( SkyVerts, SkyTexCoords, NULL, NULL, NULL, NumVerts );
		SkyboxIBHandle[i] = DQRenderStack.AddStaticIndices( SkyIndices, NumIndices );
	}

	NumSkySquarePrims = NumIndices - 2;

	DQDeleteArray( SkyTexCoords );
	DQDeleteArray( SkyVerts );
	DQDeleteArray( SkyIndices );

	//Create SkyDome for cloud layer
	DQRender.GetDomeArrayDimensions( &NumIndices, &NumVerts, SKYDOME_TESSELATION );

	SkyVerts = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );
	SkyTexCoords = (D3DXVECTOR2*)DQNewVoid( D3DXVECTOR2[NumVerts] );
	SkyIndices = (WORD*)DQNewVoid( WORD[NumIndices] );

	DQRender.CreateDome( Size, CloudHeight, SkyVerts, SkyTexCoords, SkyIndices, SKYDOME_TESSELATION, 3.0f );

	SkyDomeVBHandle = DQRenderStack.AddStaticVertices( SkyVerts, SkyTexCoords, NULL, NULL, NULL, NumVerts );
	SkyDomeIBHandle = DQRenderStack.AddStaticIndices( SkyIndices, NumIndices );

	NumSkyDomePrims = NumIndices - 2;

	DQDeleteArray( SkyVerts );
	DQDeleteArray( SkyIndices );
	DQDeleteArray( SkyTexCoords );
	
	LastCluster = -2;	//force update
	isLoadedIntoD3D = TRUE;
}
/*
//Generates sphere-mapped tex coords onto a box
//Sphere centre given by Centre parameter
//Cloud height should adjust the SphereSize
void c_BSP::GenerateSkyboxSphereCoords( D3DXVECTOR3 *Verts, D3DXVECTOR3 Centre, float SphereSize, float SphereTexScale, D3DXVECTOR2 *SphereTexCoords, int NumVerts )
{
	int i;
	float theta, phi;
	D3DXVECTOR3 VertPos;		//Relative to centre

	for( i=0; i<NumVerts; ++i ) {
		VertPos = Verts[i] - Centre;
		D3DXVec3Normalize( &VertPos, &VertPos );
		theta = atan( VertPos.x / VertPos.z ) / D3DX_PI;
		phi = atan( VertPos.y / VertPos.z ) / D3DX_PI;

		SphereTexCoords[i].x = theta * SphereTexScale;
		SphereTexCoords[i].y = phi * SphereTexScale;
	}
}*/

DWORD c_BSP::BrightenColour(U8 r, U8 g, U8 b)
{
	int ir, ig, ib, Gamma, imax;
	float Factor;
	Gamma = 2-DQConsole.OverbrightBits;

	ir = ((int)r)<<Gamma;
	ig = ((int)g)<<Gamma;
	ib = ((int)b)<<Gamma;

	imax = max( ir, max( ig, ib ) );
	if(imax>255) {
		Factor = 255.0f/(float)imax;
		ir = (int)((float)ir*Factor);
		ig = (int)((float)ig*Factor);
		ib = (int)((float)ib*Factor);
	}

	return D3DCOLOR_RGBA( ir, ig, ib, 255 );
}

void c_BSP::CreateTexturesFromLightmaps()
{
	int i,u,v;
	LPDIRECT3DTEXTURE9 TempTexture;
	D3DLOCKED_RECT LockedRect;
	D3DSURFACE_DESC desc;
	DWORD *pixel;

/*	//Put all lightmaps into 1 or more bigger lightmaps
	const int NewLightmapSize = 512;
	const int Scale = NewLightmapSize/128;
	int Q3LightmapNum, Q3u, Q3v;

	//Fit the Q3 lightmaps (128x128) into one or more bigger textures
	NumLightmaps = square(Scale);
	if( NumQ3Lightmaps%NumLightmaps == 0 ) NumLightmaps = NumQ3Lightmaps / NumLightmaps;
	else NumLightmaps = NumQ3Lightmaps / NumLightmaps + 1;

	LightmapTextureBuffer = (LPDIRECT3DTEXTURE9*) DQNewVoid( LPDIRECT3DTEXTURE9[NumLightmaps] );

	//Since we can't lock video-memory textures without using D3DUSAGE_DYNAMIC, we have to copy them via a system memory texture
	d3dcheckOK( D3DXCreateTexture( DQDevice, NewLightmapSize, NewLightmapSize, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &TempTexture) );

	for(i=0;i<NumLightmaps;++i) {

		//No mipmaps for lightmaps
		d3dcheckOK( D3DXCreateTexture( DQDevice, NewLightmapSize, NewLightmapSize, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &LightmapTextureBuffer[i]) );

		d3dcheckOK( TempTexture->GetLevelDesc( 0, &desc ) );
		d3dcheckOK( TempTexture->LockRect( 0, &LockedRect, NULL, 0 ) );

		for(u=0, Q3u = 0;u<NewLightmapSize;++u, ++Q3u) {
			if(Q3u>=128) Q3u = 0;
			pixel = (BYTE*)LockedRect.pBits+u*LockedRect.Pitch;

			for(v=0, Q3v=0; v<NewLightmapSize; ++v, ++Q3v, pixel+=4) {
				if(Q3v>=128) Q3v = 0;
				Q3LightmapNum = (u/128)*(Scale)+(v/128);
				if( Q3LightmapNum<NumQ3Lightmaps ) {
					//BGRA (LSB first)
					BrightenColour( Lightmap[Q3LightmapNum].map[Q3u][Q3v][0], Lightmap[Q3LightmapNum].map[Q3u][Q3v][1], Lightmap[Q3LightmapNum].map[Q3u][Q3v][2]);
					pixel[0] = Lightmap[Q3LightmapNum].map[Q3u][Q3v][2];
					pixel[1] = Lightmap[Q3LightmapNum].map[Q3u][Q3v][1];
					pixel[2] = Lightmap[Q3LightmapNum].map[Q3u][Q3v][0];
				}
				else {
					pixel[0] = pixel[1] = pixel[2] = 255;
				}
			}
		}

		TempTexture->UnlockRect(0);

		d3dcheckOK( LightmapTextureBuffer[i]->AddDirtyRect( NULL ) );	//whole texture
		d3dcheckOK( DQDevice->UpdateTexture( TempTexture, LightmapTextureBuffer[i] ) );	//Blit
	}
	SafeRelease( TempTexture );

	//Adjust TexCoords

	//Find lightmap index for each vert
	for( i=0; i<NumFaces; ++i ) {
		if( Face[i].Q3Face.type != BSPFACE_MESH ) {
			for( u=0; u<Face[i].Q3Face.numVertex; ++u) {
				if( Vertices.LightmapNum[Face[i].Q3Face.firstVertex+u] != -1 ) Breakpoint;
				Vertices.LightmapNum[Face[i].Q3Face.firstVertex+u] = Face[i].Q3Face.lightmapIndex;
				//Adjust lightmap index to new index
				Face[i].Q3Face.lightmapIndex /= (Scale*Scale);
			}
		}
		else {
			for( u=0; u<Face[i].Q3Face.numMeshVert; ++u) {
				if( Vertices.LightmapNum[MeshVerts[Face[i].Q3Face.firstMeshVert+u]] != -1 ) Breakpoint;
				Vertices.LightmapNum[MeshVerts[Face[i].Q3Face.firstMeshVert+u]] = Face[i].Q3Face.lightmapIndex;
				Face[i].Q3Face.lightmapIndex /= (Scale*Scale);
			}
		}
	}
	for( i=0; i<NumVerts; ++i ) {
		if(Vertices.LightmapNum[i]>=0 && Vertices.LightmapNum[i]<NumQ3Lightmaps) {
			Vertices.LightmapCoords[i].x = Vertices.LightmapCoords[i].x / Scale + (Vertices.LightmapNum[i]%Scale)/(float)Scale;
			Vertices.LightmapCoords[i].y = Vertices.LightmapCoords[i].y / Scale + (Vertices.LightmapNum[i]/Scale)/(float)Scale;
		}
	}*/

	//Original method
	NumLightmaps = NumQ3Lightmaps;
	//Can't create array of classes with DQNew
	LightmapTextureBuffer = (c_Texture**) DQNewVoid( c_Texture*[NumLightmaps] );

	//Since we can't lock video-memory textures without using D3DUSAGE_DYNAMIC, we have to copy them via a system memory texture
	d3dcheckOK( D3DXCreateTexture( DQDevice, 128, 128, 0, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &TempTexture) );

	for(i=0;i<NumLightmaps;++i) {

		d3dcheckOK( TempTexture->GetLevelDesc( 0, &desc ) );
		d3dcheckOK( TempTexture->LockRect( 0, &LockedRect, NULL, 0 ) );

		for(u=0;u<128;++u) {
			pixel = (DWORD*)( (BYTE*)LockedRect.pBits+u*LockedRect.Pitch );
			for(v=0;v<128;++v,++pixel) {
				//BGRA (LSB first)
				*pixel = BrightenColour( Lightmap[i].map[u][v][0], Lightmap[i].map[u][v][1], Lightmap[i].map[u][v][2]);
			}
		}

		TempTexture->UnlockRect(0);

		LightmapTextureBuffer[i] = (c_Texture*) DQNewVoid( c_Texture );
		LightmapTextureBuffer[i]->LoadTextureFromSysmemTexture( TempTexture, FALSE, 0 );
	}

	SafeRelease( TempTexture );
}


int c_BSP::FindLeafFromPos(D3DXVECTOR3 *Pos)
{
	int NodeIndex = 0;
	int PlaneIndex, Distance;
	while(1) {
		if(NodeIndex<0) {
			//This is a leaf
			return ~NodeIndex;
		}
		PlaneIndex = Node[NodeIndex].PlaneIndex;
		Distance = D3DXVec3Dot( Pos, &Plane[PlaneIndex].normal) - Plane[PlaneIndex].dist;
		if(Distance>0) NodeIndex = Node[NodeIndex].childFront;
		else NodeIndex = Node[NodeIndex].childBack;
	}

}

//Trace a box (BBox min/max) from start to end
//Reference : http://www.nathanostgard.com/tutorials/quake3/collision
//Reference : http://linux.ucla.edu/~phaethon/q3mc/immutable1.html
void c_BSP::TraceBox( s_Trace &Trace )
{
	if( !isLoaded ) {
		Error( ERR_BSP, "BSP TraceBox : BSP not loaded") ;
		return;
	}

//	++TraceCount;

	DQConsole.UpdateCVar( &cm_playerCurveClip );

	if( cm_playerCurveClip.integer ) {
		for(int i=0; i<NumCurveBrushes; ++i ) {
			CheckCurveBrush( Trace, &CurveBrush[i] );
		}
	}

	//Perform the trace

	//Check node cache
/*	NumNodeCache = 0;
	D3DXVECTOR3 Separation;
	float radius;

	for(i=0; i<MAX_NODECACHE; ++i) {
		if( TraceNodeCache[i]>0 ) {
			//Test tracebox against node's BBox
			radius = Node[TraceNodeCache[i]].BoxRadius - Trace.EnclosingSphere.radius;
			if( radius < EPSILON ) continue;
			Separation = Node[TraceNodeCache[i]].Box.MidpointPos - Trace.EnclosingSphere.centre;
			if( D3DXVec3LengthSq( &Separation ) < square( radius ) ) {
				//Trace is contained within Node's BBox
				RecursiveTrace(TraceNodeCache[i], *Trace.Start, *Trace.End, 0.0f, 1.0f);
				break;
			}
		}
	}
	if( i==MAX_NODECACHE ) */
	{	
		RecursiveTrace( 0, *Trace.Start, *Trace.End, 0.0f, 1.0f, Trace);
	}

}

//Note : Quake 3 deals with multiple plane collisions in the DLL
void c_BSP::RecursiveTrace(const int &NodeIndex, D3DXVECTOR3 &Start, D3DXVECTOR3 &End, const float &StartFraction, const float &EndFraction, s_Trace &Trace)
{
	int PlaneIndex, i;
	float StartDistance, EndDistance, InvLength, MidpointFraction, TempFraction, offset;
	D3DXVECTOR3 MidpointVec, offsetVec;

	if( Trace.CollisionFraction <= StartFraction ) return;

	if(NodeIndex<0) {
		//This is a leaf
		//Check the Brushes
		for(i=0;i<Leaf[~NodeIndex].numLeafBrush;++i) {
			CheckBrush( Trace, &Brush[LeafBrush[Leaf[~NodeIndex].firstLeafBrush+i].index] );
		}
//		if( Leaf[~NodeIndex].CurveBrush >=0 ) CheckCurveBrush( &CurveBrush[Leaf[~NodeIndex].CurveBrush] );
		return;
	}

	PlaneIndex = Node[NodeIndex].PlaneIndex;
	StartDistance = Plane[PlaneIndex].Dot( Start ) - Plane[PlaneIndex].dist;
	EndDistance = Plane[PlaneIndex].Dot( End ) - Plane[PlaneIndex].dist;

	//Trace Box
	if(!Trace.TracePointOnly) {
		offsetVec.x = (Plane[PlaneIndex].normal.x<0) ? Trace.BoxMax.x : Trace.BoxMin.x;
		offsetVec.y = (Plane[PlaneIndex].normal.y<0) ? Trace.BoxMax.y : Trace.BoxMin.y;
		offsetVec.z = (Plane[PlaneIndex].normal.z<0) ? Trace.BoxMax.z : Trace.BoxMin.z;
		offset = - Plane[PlaneIndex].Dot( offsetVec );
	}
	else offset = 0.0f;

#if 0
	RecursiveTrace( Node[NodeIndex].childFront, Start, End, StartFraction, EndFraction, Trace );
	RecursiveTrace( Node[NodeIndex].childBack, Start, End, StartFraction, EndFraction, Trace );
	return;
#endif

	//if trace line is in front of plane
	if(StartDistance>offset+EPSILON && EndDistance>offset+EPSILON) {
		RecursiveTrace( Node[NodeIndex].childFront, Start, End, StartFraction, EndFraction, Trace );
//		if(NumNodeCache<MAX_NODECACHE) TraceNodeCache[NumNodeCache++] = Node[NodeIndex].childFront;
	}

	//if trace line is in behind plane
	else if(StartDistance<-offset-EPSILON && EndDistance<-offset-EPSILON) {
		RecursiveTrace( Node[NodeIndex].childBack, Start, End, StartFraction, EndFraction, Trace );
//		if(NumNodeCache<MAX_NODECACHE) TraceNodeCache[NumNodeCache++] = Node[NodeIndex].childBack;
	}

	//else trace line is intersecting with plane
	else {
		
		//Check closest child first (ie. node on same side as Start)
		if(StartDistance<EndDistance) {		//InvLength is -ve
			InvLength = 1.0f / ( StartDistance - EndDistance );		//End->Start

			//Create Midpoint

			//TempFraction is the midpoint between this func's start and end, not the original start & end
			//TempFraction is placed EPSILON+offset **AFTER** the node plane, to avoid rounding errors
			//Reference : Q2 source for TempFraction (cmodel.c line 1298)
			TempFraction =	(StartDistance-offset+EPSILON)*InvLength;
			TempFraction = max(0.0f, TempFraction);
			TempFraction = min(1.0f, TempFraction);
			MidpointVec = Start + (End-Start)*TempFraction;
			//MidpointFraction is the fractional position of this new midpoint, between the original start & end
			MidpointFraction = StartFraction + (EndFraction-StartFraction)*TempFraction;

			RecursiveTrace( Node[NodeIndex].childBack, Start, MidpointVec, StartFraction, MidpointFraction, Trace);

			//check further child
			//Adjust the temporary midpoint to EPSILON+offset on the other side of the plane
			TempFraction = (StartDistance+offset+EPSILON)*InvLength;
			TempFraction = min(1.0f, TempFraction);
			TempFraction = max(0.0f, TempFraction);
			MidpointVec = Start + (End-Start)*TempFraction;
			MidpointFraction = StartFraction + (EndFraction-StartFraction)*TempFraction;

			RecursiveTrace( Node[NodeIndex].childFront, MidpointVec, End, MidpointFraction, EndFraction, Trace);
		}
		else if(StartDistance>EndDistance) {	//InvLength is +ve
			InvLength = 1.0f / ( StartDistance - EndDistance );		//End->Start

			//Create Midpoint
			TempFraction = (StartDistance+offset+EPSILON)*InvLength;
			TempFraction = max(0.0f, TempFraction);
			TempFraction = min(1.0f, TempFraction);
			MidpointVec = Start + (End-Start)*TempFraction;
			MidpointFraction = StartFraction + (EndFraction-StartFraction)*TempFraction;
			RecursiveTrace( Node[NodeIndex].childFront, Start, MidpointVec, StartFraction, MidpointFraction, Trace);

			//check further child
			//WHY IS THIS ONE - EPSILON? (random bugs if +)
			TempFraction = (StartDistance-offset-EPSILON)*InvLength;
			TempFraction = max(0.0f, TempFraction);
			TempFraction = min(1.0f, TempFraction);
			MidpointVec = Start + (End-Start)*TempFraction;
			MidpointFraction = StartFraction + (EndFraction-StartFraction)*TempFraction;
			RecursiveTrace( Node[NodeIndex].childBack, MidpointVec, End, MidpointFraction, EndFraction, Trace);
		}
		else { //StartDistance==EndDistance
			//Traverse both sides
			RecursiveTrace( Node[NodeIndex].childFront, Start, End, StartFraction, EndFraction, Trace );
			RecursiveTrace( Node[NodeIndex].childBack, Start, End, StartFraction, EndFraction, Trace );
		}			

/*		if(NumNodeCache<MAX_NODECACHE) {
			TraceNodeCache[NumNodeCache++] = Node[NodeIndex].childFront;
			
			if(NumNodeCache<MAX_NODECACHE) {
				TraceNodeCache[NumNodeCache++] = Node[NodeIndex].childBack;
			}
		}*/
	}
}

//This algorithm has to be *just* right, otherwise we get randomly stuck in walls
//Things which have to be set which I don't know why
//	if( (StartDistance>0.0f) && (EndDistance>=StartDistance) )
//		why can't EndDistance>0.0f?
//  for E, why is TempFraction = StartDistance + EPSILON / ...?
//	shouldn't it be a - so E is infront of the plane?

void c_BSP::CheckBrush( s_Trace &Trace, s_DQBrush *pBrush )
{
	int i, temp, PlaneIndex, PossibleCollisionBrushSideIndex;
	BOOL StartIsOutside, EndIsOutside;
	float StartDistance, EndDistance, TempFraction, S, E, BoxDistOffset;
	D3DXVECTOR3 offset;

	if( !(Texture[pBrush->texture].contents & Trace.ContentMask) ) return;
	
//	if( pBrush->TraceCount == TraceCount ) return;
//	pBrush->TraceCount = TraceCount;

	if( pBrush->numBrushSide <= 0 ) return;

	S = -1.0f;
	E = 1.0f;
	StartIsOutside = FALSE;
	EndIsOutside = FALSE;

	for(i=0;i<pBrush->numBrushSide;++i) {
		temp = pBrush->firstBrushSide + i;

		PlaneIndex = BrushSide[temp].plane;
		if(!Trace.TracePointOnly) {
			offset.x = (Plane[PlaneIndex].normal.x<0) ? Trace.BoxMax.x : Trace.BoxMin.x;
			offset.y = (Plane[PlaneIndex].normal.y<0) ? Trace.BoxMax.y : Trace.BoxMin.y;
			offset.z = (Plane[PlaneIndex].normal.z<0) ? Trace.BoxMax.z : Trace.BoxMin.z;
			BoxDistOffset = - Plane[PlaneIndex].Dot( offset );
		}
		else BoxDistOffset = 0.0f;
		
		StartDistance = Plane[PlaneIndex].Dot( (*Trace.Start) ) - Plane[PlaneIndex].dist - BoxDistOffset;
		EndDistance = Plane[PlaneIndex].Dot( (*Trace.End) ) - Plane[PlaneIndex].dist - BoxDistOffset;

		if(StartDistance>0.0f) StartIsOutside = TRUE;
		if(EndDistance>0.0f) EndIsOutside = TRUE;

		if( (StartDistance>0.0f) && (EndDistance>=StartDistance) )
			//both sides infront of brush, hence are outside
			return;
		else if( (StartDistance<=0.0f) && (EndDistance<=0.0f) )
			continue;

		//if trace is entering brushside
		if( StartDistance>EndDistance ) {
			//Set the collision fraction to a point slightly infront of the brushside plane
			TempFraction = (StartDistance - EPSILON)/( StartDistance - EndDistance );

			if(TempFraction>S) {
				S = TempFraction;
				PossibleCollisionBrushSideIndex = temp;
			}
		}
		//trace is leaving brushside
		else if(StartDistance<EndDistance) {
			//Set collision fraction slightly infront of plane
			TempFraction = (StartDistance + EPSILON)/( StartDistance - EndDistance );

			if(TempFraction<E) {
				E = TempFraction;
			}
		}
		else { //StartDistance==EndDistance
			//do nothing
		}

	}

	//Check if start is inside the brush
	if(StartIsOutside == FALSE) {
		Trace.StartSolid = TRUE;
		if(EndIsOutside == FALSE) {
			Trace.AllSolid = TRUE;
		}
		return;
	}

	//if collision with brush :
	if(S<E) {
		if( S>-1.0f && S<Trace.CollisionFraction ) {
			Trace.CollisionFraction = max( 0.0f, min( 1.0f, S ) );
			Trace.Plane.normal = Plane[BrushSide[PossibleCollisionBrushSideIndex].plane].normal;
			Trace.Plane.dist = Plane[BrushSide[PossibleCollisionBrushSideIndex].plane].dist;
			Trace.SurfaceFlags	= Texture[BrushSide[PossibleCollisionBrushSideIndex].texture].flags | Texture[pBrush->texture].flags;
			Trace.contents		= Texture[BrushSide[PossibleCollisionBrushSideIndex].texture].contents | Texture[pBrush->texture].contents;
			Trace.EntityNum		= ENTITYNUM_WORLD;
		}
	}
	return;
}

void c_BSP::CreateNewTrace( s_Trace &Trace, D3DXVECTOR3 *start, D3DXVECTOR3 *end, D3DXVECTOR3 *min, 
					  D3DXVECTOR3 *max, int contentmask )
{
	D3DXVECTOR3 diff;

	//Initialise Trace struct
	ZeroMemory( &Trace, sizeof(s_Trace) );
	Trace.CollisionFraction = 1.0f;		//default value 1.0f = no collision
	Trace.Start = start;
	Trace.End = end;

	if(!max || !min) {
		Trace.TracePointOnly = TRUE;
		Trace.MidpointStart = *start;
		Trace.MidpointEnd = *end;
		Trace.BoxRadius = 0.0f;
	}
	else {
		Trace.BoxMax = *max;
		Trace.BoxMin = *min;
		Trace.BoxExtent = ( -Trace.BoxMin + Trace.BoxMax ) / 2.0f;
		diff = Trace.BoxMax - Trace.BoxExtent;
		Trace.MidpointStart = *start + diff;
		Trace.MidpointEnd = *end + diff;

		Trace.BoxRadius = D3DXVec3Length( &Trace.BoxExtent );
	}
	//sphere enclosing the whole trace
/*	Trace.EnclosingSphere.centre = 0.5f*(Trace.MidpointStart + Trace.MidpointEnd);
	diff = Trace.MidpointEnd - Trace.EnclosingSphere.centre;
	Trace.EnclosingSphere.radius = D3DXVec3Length( &diff ) + EPSILON + Trace.BoxRadius;
*/
	Trace.Direction = - *start + *end;		//NB. NOT normalised
	Trace.ContentMask = contentmask;
	Trace.contents = 0;
	Trace.AllSolid = Trace.StartSolid = FALSE;
	Trace.EntityNum = ENTITYNUM_NONE;
	Trace.SurfaceFlags = 0;
	if( D3DXVec3LengthSq( &Trace.BoxExtent )==0.0f ) Trace.TracePointOnly = TRUE;
	else Trace.TracePointOnly = FALSE;
}

//fills the trace_t struct from a s_Trace struct
void c_BSP::ConvertTraceToResults( s_Trace *Trace, trace_t *results )
{
	D3DXVECTOR3 vec;

	if( Trace->AllSolid ) Trace->CollisionFraction = 0.0f;

	//Fill results struct
	vec = *Trace->Start + Trace->CollisionFraction * (*Trace->End - *Trace->Start);
	if( (vec.x<999999) && (vec.x>-999999) && (vec.y<999999) && (vec.y>-999999) && (vec.z<999999) && (vec.z>-999999) ) {}
	else {
//		#if _DEBUG
		DQPrint("Error : Trace out of bounds");
//		#endif
		Trace->CollisionFraction = 0.0f;
		vec = *Trace->Start;
	}

	MakeFloat3FromD3DXVec3( results->endpos, vec );

	results->allsolid		= (qboolean)Trace->AllSolid;
	results->startsolid		= (qboolean)Trace->StartSolid;
	results->fraction		= Trace->CollisionFraction;
	results->entityNum		= Trace->EntityNum;
	results->contents		= Trace->contents;
	results->surfaceFlags	= Trace->SurfaceFlags;
	//Set up plane
		results->plane.dist = Trace->Plane.dist;
		if( D3DXVec3LengthSq( &Trace->Plane.normal ) < EPSILON ) {
			//create a random plane to stop crash bug
			Trace->Plane.normal = D3DXVECTOR3( 0,0,1 );
		}
		MakeFloat3FromD3DXVec3( results->plane.normal, Trace->Plane.normal );
		results->plane.pad[0] = results->plane.pad[1] = 0.0f;					//NOT USED

		results->plane.signbits = 0;
		if(results->plane.normal[0] < 0) results->plane.signbits |= 1;
		if(results->plane.normal[1] < 0) results->plane.signbits |= 1<<1;
		if(results->plane.normal[2] < 0) results->plane.signbits |= 1<<2;

		if(results->plane.normal[0] == 1.0f && results->plane.normal[1] == 0.0f && results->plane.normal[2] == 0.0f)
			results->plane.type = 0;
		else if(results->plane.normal[0] == 0.0f && results->plane.normal[1] == 1.0f && results->plane.normal[2] == 0.0f)
			results->plane.type = 1;
		else if(results->plane.normal[0] == 0.0f && results->plane.normal[1] == 0.0f && results->plane.normal[2] == 1.0f)
			results->plane.type = 2;
		else results->plane.type = 3;	//non-axial plane
}

int c_BSP::PointContents( D3DXVECTOR3 *Point )
{
	int NodeIndex = 0;
	int PlaneIndex;
	int i, NumLeafBrush, FirstLeafBrush;
	float Distance;
	s_DQBrush *pBrush;
	int contents = 0;

	if( !isLoaded ) {
		Error( ERR_BSP, "BSP PointContents : BSP not loaded") ;
		return 0;
	}

	while(NodeIndex>=0) {

		PlaneIndex = Node[NodeIndex].PlaneIndex;
		Distance = Plane[PlaneIndex].Dot( (*Point) ) - Plane[PlaneIndex].dist;
		if(Distance>0) NodeIndex = Node[NodeIndex].childFront;
		else NodeIndex = Node[NodeIndex].childBack;
	}
	
	//NodeIndex<0 : this is a leaf
	//Each leaf may have several Brushes
	//Test point against each brush (assume brushes don't overlap)
	NumLeafBrush = Leaf[~NodeIndex].numLeafBrush;
	FirstLeafBrush = Leaf[~NodeIndex].firstLeafBrush;
	
	for(i=0;i<NumLeafBrush;++i) {
		pBrush = &Brush[LeafBrush[FirstLeafBrush+i].index];

		contents |= PointContentsBrush( Point, pBrush );
	}

	return contents;
}

int c_BSP::PointContentsBrush( D3DXVECTOR3 *Point, s_DQBrush *pBrush )
{
	int TextureIndex, j;
	int contents = 0;
	float Distance;

	//Test point against each brush side to check it is inside
	for(j=0;j<pBrush->numBrushSide;++j) {
		Distance = Plane[BrushSide[pBrush->firstBrushSide+j].plane].Dot( (*Point) ) - Plane[BrushSide[pBrush->firstBrushSide+j].plane].dist;
		if(Distance>0) break;	//Point is outside brush
		
		//if brushside has a texture, add it to contents
		TextureIndex = BrushSide[pBrush->firstBrushSide].texture;
		if(TextureIndex>0) {
			contents |= Texture[TextureIndex].contents;
		}
	}

	//if Point is inside
	if(j == pBrush->numBrushSide) {
		TextureIndex = pBrush->texture;
		if(TextureIndex>0) {
			contents |= Texture[TextureIndex].contents;
		}
	}
	else contents = 0;	//remove brushside contents

	return contents;
}

//For GetMarkFragment
void c_BSP::RecursiveGetLeavesFromLine( int *LeavesContainingPoints, int NumLeavesCP, 
	D3DXVECTOR3 *StartPoint, D3DXVECTOR3 *EndPoint, float offset, int NodeIndex )
{
	int PlaneIndex;
	float StartDistance, EndDistance;

	if(NodeIndex>=0) {
		PlaneIndex = Node[NodeIndex].PlaneIndex;
		StartDistance	= Plane[PlaneIndex].Dot( (*StartPoint) ) - Plane[PlaneIndex].dist;
		EndDistance		= Plane[PlaneIndex].Dot( (*EndPoint) ) - Plane[PlaneIndex].dist;
		if(StartDistance>offset && EndDistance>offset) RecursiveGetLeavesFromLine( LeavesContainingPoints, NumLeavesCP, StartPoint, EndPoint, offset, Node[NodeIndex].childFront);
		else if(StartDistance<-offset && EndDistance<-offset) RecursiveGetLeavesFromLine( LeavesContainingPoints, NumLeavesCP, StartPoint, EndPoint, offset, Node[NodeIndex].childBack);
		else {
			RecursiveGetLeavesFromLine( LeavesContainingPoints, NumLeavesCP, StartPoint, EndPoint, offset, Node[NodeIndex].childFront);
			RecursiveGetLeavesFromLine( LeavesContainingPoints, NumLeavesCP, StartPoint, EndPoint, offset, Node[NodeIndex].childBack);
		}
	}
	else {
		//Node is a leaf
		for(int i=0;i<NumLeavesCP;++i) {
			//Check that this leaf doesn't already exist in the list - no need to check twice
			if(LeavesContainingPoints[i] == ~NodeIndex) break;
			if(LeavesContainingPoints[i] == -1) {
				LeavesContainingPoints[i] = ~NodeIndex;
				break;
			}
		}
	}
}

//Derived using Sutherland-Hodgman Clipping Algorithm
//Reference : Introduction to Computer Graphics - Foley, Van Dam, Feiner, et al.

// Returns the projection of a polygon onto the solid brushes in the world
// Note: OrigPoints form a polygon using the TRIANGLEFAN rule
int c_BSP::GetMarkFragments( float *pfOrigPoints, int NumOrigPoints, D3DXVECTOR3 *ProjectionDir,
	float *pfPointBuffer, int PointBufferSize, markFragment_t *MarkFragment, int MaxMarkFragments )
{
	const int MaxVerts = 10;
	if(NumOrigPoints<=0 || NumOrigPoints>MaxVerts) return 0;

	int i,j,k;
	s_DQBrush *pBrush;
	float PdotN;
	float fraction;
	float Distance, DistanceToPreviousPoint;
	int c,e;
	D3DXVECTOR3 r2;
	BOOL LastVertWasIn;
	s_DQPlanes *pPlane, *pClipPlane;

	D3DXVECTOR3 *pVert, *pVertPrev, tempVert;
	D3DXVECTOR3 VertList1[MaxVerts], VertList2[MaxVerts];
	struct s_VertList {
		D3DXVECTOR3 *List;
		int NumVerts;
	} CurrentVertList, PreviousVertList;
	int FragmentNum, PointBufferPos;
	float ProjectionLength;

	const int NumLeavesCP = 20;
	int LeavesContainingPoints[NumLeavesCP];
	for(i=0;i<NumLeavesCP;++i) LeavesContainingPoints[i] = -1;

	D3DXVECTOR3 *OrigPoints;
	D3DXVECTOR3 AveragePoint(0,0,0);
	OrigPoints = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumOrigPoints] );
	for(i=0;i<NumOrigPoints;++i) {
		OrigPoints[i].x = *pfOrigPoints;
		OrigPoints[i].y = *(pfOrigPoints+1);
		OrigPoints[i].z = *(pfOrigPoints+2);
		AveragePoint += OrigPoints[i];
		pfOrigPoints += 3;
	}
	AveragePoint /= NumOrigPoints;
	//Basic funtionality - Q3 not accurate, so we're not either

	//Use Magnitude of ProjectionDir to give us a direction and line length
	//Take average of OrigPoints to give a line origin point

	//Find Leaves
	ProjectionLength = D3DXVec3Length( ProjectionDir );
	if(ProjectionLength==0) return 0;

	D3DXVECTOR3 EndPoint = AveragePoint+(*ProjectionDir); 
	RecursiveGetLeavesFromLine( LeavesContainingPoints, NumLeavesCP, &AveragePoint, &EndPoint, ProjectionLength, 0);

	*ProjectionDir /= ProjectionLength;

	//Project Polygon onto brushes in the leaves containing OrigPoints
	//Algorithm (designed it myself! :) )
	// * For Each BrushSide Plane :
	// - check Projection Dot Normal
	//   - If P.N >= 0, skip this brush plane
	//   - Project original verts onto plane
	//     - projected vertex position r2 = r - (r.N - D)/(P.N)
	//		(N is plane normal, D is plane distance, P is Projection direction)
	// - Using the Sutherland-Hodgman Algorithm, output clipped verts to a new list
	//   - Clip verts against every plane
	// * End For

	FragmentNum = 0;
	PointBufferPos = 0;

	const s_BrushSides *pBrushSide = BrushSide;

	//Iterate through leaves
	for(i=0;i<NumLeavesCP;++i) {
		if(LeavesContainingPoints[i]==-1) break;

		//Iterate through brushes
		for(j=0;j<Leaf[LeavesContainingPoints[i]].numLeafBrush;j++) {

			//On the last iteration, check if there's an additional curved surface brush
			pBrush = &Brush[LeafBrush[Leaf[LeavesContainingPoints[i]].firstLeafBrush+j].index];

			if(pBrush->texture>0) {
				if( !(Texture[pBrush->texture].contents & CONTENTS_SOLID) )
				continue;
			}
			
			//Iterate through brush sides
			for(k=0;k<pBrush->numBrushSide;++k) {
				pPlane = &Plane[pBrushSide[pBrush->firstBrushSide+k].plane];

				if(!(Texture[pBrushSide[pBrush->firstBrushSide+k].texture].contents & CONTENTS_SOLID) 
					|| (Texture[pBrushSide[pBrush->firstBrushSide+k].texture].flags & SURF_NOMARKS) ) 
					continue;

				PdotN = pPlane->Dot( (*ProjectionDir) );

				//   - If P.N == 0, skip this brush plane
				if(PdotN == 0) continue;

				//   - Project original verts onto plane
				CurrentVertList.NumVerts = NumOrigPoints;
				CurrentVertList.List = VertList1;
				PreviousVertList.List = VertList2;
				PreviousVertList.NumVerts = 0;
				for(c=0;c<NumOrigPoints;++c) {
					// r2 = r - P (r.N - D)/(P.N)
					Distance = ( pPlane->Dot( OrigPoints[c] ) - pPlane->dist ) / PdotN;
					if(Distance>0) {
						CurrentVertList.NumVerts=0;
						break;
					}
					VertList1[c] = OrigPoints[c] - *ProjectionDir *( Distance + EPSILON );
				}

				//For each brush side
				for(c=0;c<pBrush->numBrushSide;++c) {
					//Don't test against self
					if(c==k) continue;

					//Sutherland-Hodgman Clipping Algorithm

					//Swap vert lists
					pVert = PreviousVertList.List;
					PreviousVertList.List		= CurrentVertList.List;
					PreviousVertList.NumVerts	= CurrentVertList.NumVerts;
					CurrentVertList.List		= pVert;
					CurrentVertList.NumVerts	= 0;
					if(PreviousVertList.NumVerts==0) break;

					pClipPlane = &Plane[pBrushSide[pBrush->firstBrushSide+c].plane];

					//Test if last vert is inside
					pVertPrev = &PreviousVertList.List[PreviousVertList.NumVerts-1];

					DistanceToPreviousPoint = pClipPlane->Dot( (*pVertPrev) ) - pClipPlane->dist;
					//Test first vert to see if it's in or out
					if( DistanceToPreviousPoint < 0 )
						LastVertWasIn = TRUE;
					else
						LastVertWasIn = FALSE;

					//For each edge
					for(e=0;e<PreviousVertList.NumVerts;++e) {

						pVert = &PreviousVertList.List[e];

						Distance = pClipPlane->Dot( (*pVert) ) - pClipPlane->dist;
						//if point is inside
						if( Distance < 0 ) {
							if( LastVertWasIn ) {
								//Bounds check
								if(CurrentVertList.NumVerts+1>=MaxVerts) {
									CurrentVertList.NumVerts = 0;
									#if _DEBUG
										Error(ERR_BSP, "GetMarkFragments : NumVerts>MaxVerts");
									#endif
									break;
								}

								//Case 1 (in->in) : Add vertex to output list
								CurrentVertList.List[CurrentVertList.NumVerts++] = *pVert;
							}
							else {
								//Bounds check
								if(CurrentVertList.NumVerts+2>=MaxVerts) {
									CurrentVertList.NumVerts = 0;
									#if _DEBUG
										Error(ERR_BSP, "GetMarkFragments : NumVerts>MaxVerts");
									#endif
									break;
								}

								//Case 4 (out->in) : Add intersection vertex and this vertex to output list

								//Calculate intersection (same as below)
								DistanceToPreviousPoint = modulus(DistanceToPreviousPoint);
								Distance = modulus(Distance);
								fraction = DistanceToPreviousPoint / (Distance+DistanceToPreviousPoint);
								tempVert = *pVertPrev + (*pVert - *pVertPrev) * fraction;
								CurrentVertList.List[CurrentVertList.NumVerts++] = tempVert;

								CurrentVertList.List[CurrentVertList.NumVerts++] = *pVert;
							}
							LastVertWasIn = TRUE;
						}
						else {
							if( LastVertWasIn ) {
								//Bounds check
								if(CurrentVertList.NumVerts+1>=MaxVerts) {
									CurrentVertList.NumVerts = 0;
									#if _DEBUG
										Error(ERR_BSP, "GetMarkFragments : NumVerts>MaxVerts");
									#endif
									break;
								}

								//Case 2 (in->out) : Add intersection to output list

								//Calculate intersection (same as above)
								DistanceToPreviousPoint = modulus(DistanceToPreviousPoint);
								Distance = modulus(Distance);
								fraction = DistanceToPreviousPoint / (Distance+DistanceToPreviousPoint);
								tempVert = *pVertPrev + (*pVert - *pVertPrev) * fraction;
								CurrentVertList.List[CurrentVertList.NumVerts++] = tempVert;
							}
//							else {
								//Case 3 (out->out) : No Output
//							}
							LastVertWasIn = FALSE;
						}

						DistanceToPreviousPoint = Distance;
						pVertPrev = pVert;
					} // end for each edge
				} // for each clip brush plane

				if(CurrentVertList.NumVerts>2) {

					//Copy to output
					MarkFragment[FragmentNum].firstPoint = PointBufferPos;

					for(c=0; 
						c<CurrentVertList.NumVerts && PointBufferPos<PointBufferSize;
						++c) 
					{
						*pfPointBuffer		= CurrentVertList.List[c].x;
						*(pfPointBuffer+1)	= CurrentVertList.List[c].y;
						*(pfPointBuffer+2)	= CurrentVertList.List[c].z;
						pfPointBuffer += 3;
						++PointBufferPos;
					}

					MarkFragment[FragmentNum].numPoints = CurrentVertList.NumVerts;
					++FragmentNum;
				}

				if(FragmentNum>=MaxMarkFragments) {
					DQDeleteArray( OrigPoints );
					Error(ERR_BSP, "Too many Fragments");
					return MaxMarkFragments;
				}

			} // for each brush side (in the brush)
		} // for each brush (in the leaf)
	} // for each leaf

	DQDeleteArray( OrigPoints );

	return FragmentNum;
}


/*int c_BSP::GetMarkFragments( float *pfOrigPoints, int NumOrigPoints, D3DXVECTOR3 *ProjectionDir,
	float *pfPointBuffer, int PointBufferSize, markFragment_t *MarkFragment, int MaxMarkFragments )
{
#if _DEBUG
	if(NumOrigPoints<=0 || NumOrigPoints>10) return 0;
#endif

	int i,j,k;
	s_Brushes *pBrush;
	float PdotN;
	float fraction;
	float Distance, DistanceToPreviousPoint;
	int c;
	D3DXVECTOR3 r2;
	BOOL LastVertWasIn;
	s_DQPlanes *pPlane, *pClipPlane;

	D3DXVECTOR3 *pVert;
	c_Chain<D3DXVECTOR3> *CurrentVertList, *PreviousVertList;
	s_ChainNode<D3DXVECTOR3> *pVertNode;
	int FragmentNum, PointBufferPos;
	float ProjectionLength;

	const int NumLeavesCP = 20;
	int LeavesContainingPoints[NumLeavesCP];
	for(i=0;i<NumLeavesCP;++i) LeavesContainingPoints[i] = -1;

	D3DXVECTOR3 *OrigPoints;
	D3DXVECTOR3 AveragePoint(0,0,0);
	OrigPoints = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumOrigPoints] );
	for(i=0;i<NumOrigPoints;++i) {
		OrigPoints[i].x = *pfOrigPoints;
		OrigPoints[i].y = *(pfOrigPoints+1);
		OrigPoints[i].z = *(pfOrigPoints+2);
		AveragePoint += OrigPoints[i];
		pfOrigPoints += 3;
	}
	AveragePoint /= NumOrigPoints;
	//Basic funtionality - Q3 not accurate, so we're not either

	//Use Magnitude of ProjectionDir to give us a direction and line length
	//Take average of OrigPoints to give a line origin point

	//Find Leaves
	ProjectionLength = D3DXVec3Length( ProjectionDir );

	RecursiveGetLeavesFromLine( LeavesContainingPoints, NumLeavesCP, &AveragePoint, &(AveragePoint+*ProjectionDir), ProjectionLength, 0);

	*ProjectionDir /= ProjectionLength;

	//Project Polygon onto brushes in the leaves containing OrigPoints
	//Algorithm (designed it myself! :) )
	// * For Each BrushSide Plane :
	// - check Projection Dot Normal
	//   - If P.N >= 0, skip this brush plane
	//   - Project original verts onto plane
	//     - projected vertex position r2 = r - (r.N - D)/(P.N)
	//		(N is plane normal, D is plane distance, P is Projection direction)
	// - Using the Sutherland-Hodgman Algorithm, output clipped verts to a new list
	//   - Clip verts against every plane
	// * End For

	FragmentNum = 0;
	PointBufferPos = 0;
	CurrentVertList = PreviousVertList = NULL;

	//Iterate through leaves
	for(i=0;i<NumLeavesCP;++i) {
		if(LeavesContainingPoints[i]==-1) break;

		//Iterate through brushes
		for(j=0;j<Leaf[LeavesContainingPoints[i]].numLeafBrush;j++) {
			pBrush = &Brush[LeafBrush[Leaf[LeavesContainingPoints[i]].firstLeafBrush+j].index];

			if(pBrush->texture>0 && !(Texture[pBrush->texture].contents & CONTENTS_SOLID) ) continue;
			
			//Iterate through brush sides
			for(k=0;k<pBrush->numBrushSide;++k) {
				pPlane = &Plane[BrushSide[pBrush->firstBrushSide+k].plane];

				if(!(Texture[BrushSide[pBrush->firstBrushSide+k].texture].contents & CONTENTS_SOLID) 
					|| (Texture[BrushSide[pBrush->firstBrushSide+k].texture].flags & SURF_NOMARKS) ) 
					continue;

				PdotN = D3DXVec3Dot( ProjectionDir, &pPlane->normal );

				//   - If P.N == 0, skip this brush plane
				if(PdotN == 0) continue;

				//   - Project original verts onto plane
				DQDelete( CurrentVertList );
				CurrentVertList = DQNew( c_Chain<D3DXVECTOR3> );
				for(c=0;c<NumOrigPoints;++c) {
					pVert = CurrentVertList->AddUnit();
					*pVert = OrigPoints[c];
					// r2 = r - P (r.N - D)/(P.N)
					Distance = ( D3DXVec3Dot(pVert, &pPlane->normal) - pPlane->dist ) / PdotN;
					if(Distance>0) continue;
					*pVert = *pVert - *ProjectionDir *( Distance + EPSILON );
				}

				//For each brush side
				for(c=0;c<pBrush->numBrushSide;++c) {
					//Don't test against self
					if(c==k) continue;

//					if(!(Texture[BrushSide[pBrush->firstBrushSide+c].texture].contents & CONTENTS_SOLID) ) continue;

					//Sutherland-Hodgman Clipping Algorithm

					DQDelete( PreviousVertList );
					PreviousVertList = CurrentVertList;
					pVertNode = PreviousVertList->GetFirstNode();
					CurrentVertList = DQNew( c_Chain<D3DXVECTOR3> );
					if(!pVertNode) break;

					pClipPlane = &Plane[BrushSide[pBrush->firstBrushSide+c].plane];

					DistanceToPreviousPoint = D3DXVec3Dot( pVertNode->current, &pClipPlane->normal ) - pClipPlane->dist;
					//Test first vert to see if it's in or out
					if( DistanceToPreviousPoint < 0 )
						LastVertWasIn = TRUE;
					else
						LastVertWasIn = FALSE;

					//For each edge (e goes from 2nd vert to end to first then ends)
					pVert = PreviousVertList->AddUnit();
					*pVert = *(PreviousVertList->GetFirstNode()->current);

					pVertNode = PreviousVertList->GetFirstNode();
					pVertNode = pVertNode->next;
					while(pVertNode) {

						Distance = D3DXVec3Dot( pVertNode->current, &pClipPlane->normal ) - pClipPlane->dist;
						//if point is inside
						if( Distance < 0 ) {
							if( LastVertWasIn ) {
								//Case 1 (in->in) : Add vertex to output list
								pVert = CurrentVertList->AddUnit();
								*pVert = *(pVertNode->current);
							}
							else {
								//Case 4 (out->in) : Add intersection vertex and this vertex to output list
								pVert = CurrentVertList->AddUnit();

								//Calculate intersection (same as below)
								DistanceToPreviousPoint = modulus(DistanceToPreviousPoint);
								Distance = modulus(Distance);
								fraction = DistanceToPreviousPoint / (Distance+DistanceToPreviousPoint);
								*pVert = *(pVertNode->prev->current) + (*(pVertNode->current) - *(pVertNode->prev->current)) * fraction;

								pVert = CurrentVertList->AddUnit();
								*pVert = *(pVertNode->current);
							}
							LastVertWasIn = TRUE;
						}
						else {
							if( LastVertWasIn ) {
								//Case 2 (in->out) : Add intersection to output list
								pVert = CurrentVertList->AddUnit();

								//Calculate intersection (same as above)
								DistanceToPreviousPoint = modulus(DistanceToPreviousPoint);
								Distance = modulus(Distance);
								fraction = DistanceToPreviousPoint / (Distance+DistanceToPreviousPoint);
								*pVert = *(pVertNode->prev->current) + (*(pVertNode->current) - *(pVertNode->prev->current)) * fraction;
							}
//							else {
								//Case 3 (out->out) : No Output
//							}
							LastVertWasIn = FALSE;
						}

						DistanceToPreviousPoint = Distance;
						pVertNode = pVertNode->next;
					} // end for each edge
				} // for each clip brush plane

				pVertNode = CurrentVertList->GetFirstNode();
				if(pVertNode) {

					//Copy to output
					MarkFragment[FragmentNum].firstPoint = PointBufferPos;

					for(c=0; 
						pVertNode && PointBufferPos<PointBufferSize;
						++c) 
					{
						*pfPointBuffer = pVertNode->current->x;
						*(pfPointBuffer+1) = pVertNode->current->y;
						*(pfPointBuffer+2) = pVertNode->current->z;
						pfPointBuffer += 3;
						++PointBufferPos;
						pVertNode = pVertNode->next;
					}

					MarkFragment[FragmentNum].numPoints = c;
					++FragmentNum;
				}
				
				DQDelete( CurrentVertList );
				DQDelete( PreviousVertList );

				if(FragmentNum>=MaxMarkFragments) {
					DQDeleteArray( OrigPoints );
					return MaxMarkFragments;
				}

			} // for each brush side (in the brush)
		} // for each brush (in the leaf)
	} // for each leaf

	DQDeleteArray( OrigPoints );
	return FragmentNum;
}*/

//**************************************************************

void c_BSP::CreateSkyBox(int cloudheight)
{
	return;
/*	s_VertexStreamPosTex *pVertexPT;
	int ss = sqrt(0.5f*(10000*10000)-cloudheight*cloudheight);	//SkyboxSize
	d3dcheckOK( DQDevice->CreateVertexBuffer( 20*sizeof(s_VertexStreamPosTex), D3DUSAGE_WRITEONLY, FVF_PosTex, D3DPOOL_DEFAULT, &VBSkyBox, NULL ) );
	d3dcheckOK( VBSkyBox->Lock(0,0,(void**)&pVertexPT, 0) );
	//Top
	pVertexPT->Pos = D3DXVECTOR3(-ss, -ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, -ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,0 );
	++pVertexPT;
	//Back
	pVertexPT->Pos = D3DXVECTOR3(-ss, ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,0 );
	++pVertexPT;
	//Left
	pVertexPT->Pos = D3DXVECTOR3(ss, ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, -ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, -ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,0 );
	++pVertexPT;
	//Front
	pVertexPT->Pos = D3DXVECTOR3(ss, -ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, -ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, -ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, -ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,0 );
	++pVertexPT;
	//Right
	pVertexPT->Pos = D3DXVECTOR3(-ss, -ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, -ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, ss, cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,0 );
	++pVertexPT;
	//Bottom
	pVertexPT->Pos = D3DXVECTOR3(-ss, -ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(-ss, ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 0,1 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, -ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,0 );
	++pVertexPT;
	pVertexPT->Pos = D3DXVECTOR3(ss, ss, -cloudheight);
	pVertexPT->Tex0 = D3DXVECTOR2( 1,1 );
	++pVertexPT;

	d3dcheckOK( VBSkyBox->Unlock() );*/
}

//Reference : Diesel engine, Q3 Tool Source

void c_BSP::SetLightVol( D3DXVECTOR3 Pos )
{
	unsigned int lx, ly, lz;
	float px, py, pz;
	s_DQLightVol LerpedLight;

	if(!isLoadedIntoD3D) return;

	Pos.x = (Pos.x - Model[0].Min.x)/64.0f;
	Pos.y = (Pos.y - Model[0].Min.y)/64.0f;
	Pos.z = (Pos.z - Model[0].Min.z)/128.0f;

	px = (float)floor( Pos.x ); lx = (unsigned int)px;
	py = (float)floor( Pos.y ); ly = (unsigned int)py;
	pz = (float)floor( Pos.z ); lz = (unsigned int)pz;

	//Use Pos as fractional position in lightgrid
	Pos.x -= px;
	Pos.y -= py;
	Pos.z -= pz;

	if( lx>=NumLightvols_x-1 || ly>=NumLightvols_y-1 || lz>=NumLightvols_z-1) return;

	//Format : LightVol[z][y][x]
	const unsigned int BasePos = pz*(NumLightvols_y*NumLightvols_x) + py*(NumLightvols_x) +px;
	const unsigned int BasePosPlusY = BasePos + NumLightvols_x;
	const unsigned int BasePosPlusYZ = BasePosPlusY + NumLightvols_y*NumLightvols_x;
	const unsigned int BasePosPlusZ = BasePos + NumLightvols_y*NumLightvols_x;

	LerpedLight.Clear();

	LerpedLight.AddLerpedLightvol( (1.0f-Pos.x)*(1.0f-Pos.y)*(1.0f-Pos.z), Lightvol[ BasePos ] );
	LerpedLight.AddLerpedLightvol( (Pos.x)*(1.0f-Pos.y)*(1.0f-Pos.z), Lightvol[ BasePos+1 ] );
	LerpedLight.AddLerpedLightvol( (1.0f-Pos.x)*(Pos.y)*(1.0f-Pos.z), Lightvol[ BasePosPlusY ] );
	LerpedLight.AddLerpedLightvol( (Pos.x)*(Pos.y)*(1.0f-Pos.z), Lightvol[ BasePosPlusY+1 ] );
	LerpedLight.AddLerpedLightvol( (1.0f-Pos.x)*(1.0f-Pos.y)*(Pos.z), Lightvol[ BasePosPlusZ ] );
	LerpedLight.AddLerpedLightvol( (Pos.x)*(1.0f-Pos.y)*(Pos.z), Lightvol[ BasePosPlusZ+1 ] );
	LerpedLight.AddLerpedLightvol( (1.0f-Pos.x)*(Pos.y)*(Pos.z), Lightvol[ BasePosPlusYZ ] );
	LerpedLight.AddLerpedLightvol( (Pos.x)*(Pos.y)*(Pos.z), Lightvol[ BasePosPlusYZ+1 ] );

	D3DLIGHT9 D3DLight;
	ZeroMemory( &D3DLight, sizeof(D3DLIGHT9) );

	LerpedLight.GetD3DLight( D3DLight );
	
	d3dcheckOK( DQDevice->SetLight( LightNum, &D3DLight ) );
	d3dcheckOK( DQDevice->LightEnable( LightNum, TRUE ) );
}

void c_BSP::EndLightVol() 
{
	d3dcheckOK( DQDevice->LightEnable( LightNum, FALSE ) );
}

//CLIENT GAME ONLY
//****************************
void c_BSP::RegisterInlineModelHandle( int InlineModelIndex, int ModelHandle )
{
	if(InlineModelIndex<0 || InlineModelIndex>=NumInlineModels) return;
	Model[InlineModelIndex].ModelHandle = ModelHandle;
}

int c_BSP::GetModelHandleOfInlineModel( int InlineModelIndex )
{
	if(InlineModelIndex<0 || InlineModelIndex>=NumInlineModels) return 0;
	return Model[InlineModelIndex].ModelHandle;
}
//****************************

void c_BSP::TraceInlineModel( s_Trace &Trace, int InlineModelIndex )
{
	int i;
	if(InlineModelIndex<0 || InlineModelIndex>=NumInlineModels) return;

	for( i=0; i<Model[InlineModelIndex].numBrush; ++i ) {
		CheckBrush( Trace, &Brush[Model[InlineModelIndex].firstBrush+i] );
	}
}

void c_BSP::GetInlineModelBounds( int InlineModelIndex, D3DXVECTOR3 &Min, D3DXVECTOR3 &Max )
{
	if(InlineModelIndex<0 || InlineModelIndex>=NumInlineModels) {
		Min = Max = D3DXVECTOR3(0,0,0);
		return;
	}

	Min = Model[InlineModelIndex].Min;
	Max = Model[InlineModelIndex].Max;
}

//Called by c_Game trap_InPVS
BOOL c_BSP::InPVS( D3DXVECTOR3 *Vec1, D3DXVECTOR3 *Vec2 )
{
	int c1, c2;

	if( !isLoaded ) {
		Error(ERR_BSP, "InPVS : BSP Not Loaded");
		return TRUE;
	}

	if( !VisData.vec ) return TRUE;

	c1 = Leaf[FindLeafFromPos( Vec1 )].Cluster;
	c2 = Leaf[FindLeafFromPos( Vec2 )].Cluster;

	if( c1<0 || c2<0 || (VisData.vec[c1*VisData.sizeVec + c2/8] & (1<<(c2%8)) ) ) {
		return TRUE;
	}
	return FALSE;
}

void c_BSP::TraceAgainstAABB( s_Trace *Trace, D3DXVECTOR3 &EntityPosition, D3DXVECTOR3 &EntityExtent, int EntityContents, int &EntityNumber )
{
	int j,PossibleCollisionPlane;
	D3DXVECTOR3 PlanePos1, PlanePos2, Temp;
	float fraction, S, E, StartDistance, EndDistance, EntityRadius;
	BOOL StartIsOutside, EndIsOutside;

	//Quick test against bounding sphere
	Temp = EntityPosition - Trace->MidpointStart;
	EntityRadius = 2.0f * D3DXVec3Length( &EntityExtent );
	if( EntityRadius < EPSILON ) return;

	fraction = D3DXVec3Dot( &Temp, &Trace->Direction ) / D3DXVec3LengthSq( &Trace->Direction );
	fraction = max( 0.0f, min( 1.0f, fraction ) );
	Temp = EntityPosition - (Trace->MidpointStart + fraction*Trace->Direction);

	if( D3DXVec3LengthSq( &Temp ) > Square( Trace->BoxRadius + EntityRadius ) + 10.0f ) return;

	//Test tracebox against entity's planes
	S = -1.0f;
	E = 1.0f;
	StartIsOutside = FALSE;
	EndIsOutside = FALSE;
	
	//+ve Planes
	PlanePos1 = EntityPosition + EntityExtent + Trace->BoxExtent;
	//-ve Planes
	PlanePos2 = EntityPosition - EntityExtent - Trace->BoxExtent;

	for( j=0; j<6; ++j ) {
		switch(j) {
		case 0: //+x
			StartDistance = Trace->MidpointStart.x - PlanePos1.x;
			EndDistance   = Trace->MidpointEnd.x - PlanePos1.x;
			break;
		case 1: //+y
			StartDistance = Trace->MidpointStart.y - PlanePos1.y;
			EndDistance   = Trace->MidpointEnd.y - PlanePos1.y;
			break;
		case 2: //+z
			StartDistance = Trace->MidpointStart.z - PlanePos1.z;
			EndDistance   = Trace->MidpointEnd.z - PlanePos1.z;
			break;
		case 3: //-x
			StartDistance = - Trace->MidpointStart.x + PlanePos2.x;
			EndDistance   = - Trace->MidpointEnd.x + PlanePos2.x;
			break;
		case 4: //-y
			StartDistance = - Trace->MidpointStart.y + PlanePos2.y;
			EndDistance   = - Trace->MidpointEnd.y + PlanePos2.y;
			break;
		case 5: //-z
			StartDistance = - Trace->MidpointStart.z + PlanePos2.z;
			EndDistance   = - Trace->MidpointEnd.z + PlanePos2.z;
			break;
		};

		if(StartDistance>0) StartIsOutside = TRUE;
		if(EndDistance>0) EndIsOutside = TRUE;	

		if(StartDistance>0 && EndDistance>=StartDistance)
			//both sides infront of brush, hence are outside
			break;
		else if(StartDistance<=0 && EndDistance<=0)
			continue;

		//if trace is entering brushside
		if( StartDistance>EndDistance ) {
			//Set the collision fraction to a point slightly infront of the brushside plane
			fraction = (StartDistance - EPSILON)/( StartDistance - EndDistance );
			if(fraction>S) {
				S = fraction;
				PossibleCollisionPlane = j;
			}
		}
		//trace is leaving brushside
		else {
			//Set collision fraction slightly infront of plane
			fraction = (StartDistance + EPSILON)/( StartDistance - EndDistance );
			if(fraction<E) {
				E = fraction;
			}
		}
	}
	if(j!=6) return; //skip to next entity

	//Check if start is inside the brush
	if(StartIsOutside == FALSE) {
		Trace->StartSolid = TRUE;
		if(EndIsOutside == FALSE) {
			Trace->AllSolid = TRUE;
		}
		return;
	}

	if( S<E ) {
		if( S>-1.0f && S<Trace->CollisionFraction ) {
			//Collision occurred
			Trace->CollisionFraction = max( 0.0f, S );
			Trace->contents = EntityContents;
			Trace->SurfaceFlags = 0;
			Trace->EntityNum = EntityNumber;
			//Plane
			switch(PossibleCollisionPlane) {
			case 0: 
				Trace->Plane.normal = D3DXVECTOR3( 1.0f,0,0 );
				Trace->Plane.dist = EntityPosition.x + EntityExtent.x;
				break;
			case 1: 
				Trace->Plane.normal = D3DXVECTOR3( 0,1.0f,0 );
				Trace->Plane.dist = EntityPosition.y + EntityExtent.y;
				break;
			case 2: 
				Trace->Plane.normal = D3DXVECTOR3( 0,0,1.0f );
				Trace->Plane.dist = EntityPosition.z + EntityExtent.z;
				break;
			case 3: 
				Trace->Plane.normal = D3DXVECTOR3( -1.0f,0,0 );
				Trace->Plane.dist = - EntityPosition.x + EntityExtent.x;
				break;
			case 4: 
				Trace->Plane.normal = D3DXVECTOR3( 0,-1.0f,0 );
				Trace->Plane.dist = - EntityPosition.y + EntityExtent.y;
				break;
			case 5: 
				Trace->Plane.normal = D3DXVECTOR3( 0,0,-1.0f );
				Trace->Plane.dist = - EntityPosition.z + EntityExtent.z;
				break;
			default: break;
			};
		}
	}
}

int c_BSP::CheckFogStatus( D3DXVECTOR3 &Position, D3DXVECTOR3 &Min, D3DXVECTOR3 &Max )
{
	s_Trace Trace;
	int i;

	CreateNewTrace( Trace, &Position, &Position, &Min, &Max, CONTENTS_FOG );

	for( i=0; i<NumEffects; ++i ) {
		CheckBrush( Trace, &Brush[Effect[i].brush] );
		if( Trace.CollisionFraction<1.0f ) break;
	}
	if( i<NumEffects ) {
		return BSPEffectArray[i];
	}
	return 0;
}

int c_BSP::CheckFogStatus( D3DXVECTOR3 &Position )
{
	int i;

	for( i=0; i<NumEffects; ++i ) {
		if( PointContentsBrush( &Position, &Brush[Effect[i].brush] ) & CONTENTS_FOG ) break;
	}
	if( i<NumEffects ) {
		return BSPEffectArray[i];
	}
	return 0;
}