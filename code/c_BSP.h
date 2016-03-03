// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//	Definition of a Q3 BSP File, and implementation
//

#ifndef __C_BSP_H
#define __C_BSP_H

// References :
// http://graphics.stanford.edu/~kekoa/q3
// http://www.cs.brown.edu/research/graphics/games/quake/quake3.html
// http://www.gametutorials.com/Tutorials/OpenGL/Quake3Format.htm
// http://www.nathanostgard.com/tutorials/quake3/collision
// http://www.gametutorials.com/CodeDump/CodeDump_Pg1.htm - Frank Puig Placeres's BSP Collision Detection tutorial

//EPSILON = 1/32
#define EPSILON 0.03125f

#define BSPFACE_REGULARFACE 1
#define BSPFACE_BEZIERPATCH 2
#define BSPFACE_MESH		3
#define BSPFACE_BILLBOARD	4
#define BSPFACE_DYNAMICFACE	5
#define BSPFACE_DYNAMICMESH	6

#define SKYBOX_TESSELATION 2
#define SKYDOME_TESSELATION 8

#define MAX_CURVEBRUSHPOINTS 20000

#define MAX_NODECACHE 4

//Doesn't work yet
//#define _UseLightvols

class c_ClientGame;

class c_BSP : public c_Singleton<c_BSP>
{
	friend c_CurvedSurface;		//LoadQ3Patch - needs access to BSP planes, faces, etc.
public:
	c_BSP();
	~c_BSP();
	void Initialise();
	void Unload();
	void UnloadD3D();
	BOOL LoadBSP(const char *Filename);
	void LoadIntoD3D();
	void DrawBSP();
	void RenderSkybox();
	void CreateSkyBox(int cloudheight);
	BOOL GetEntityToken( char *buffer, int bufsize );
	BOOL InPVS( D3DXVECTOR3 *Vec1, D3DXVECTOR3 *Vec2 );
	int  CheckFogStatus( D3DXVECTOR3 &Position, D3DXVECTOR3 &Min, D3DXVECTOR3 &Max );
	int  CheckFogStatus( D3DXVECTOR3 &Position );

	int  GetNumInlineModels() { return NumInlineModels; }
	int  GetBrushModelIndex( sharedEntity_t *pEntity );
	void RenderInlineModel( int ModelIndex );
	void GetInlineModelBounds( int InlineModelIndex, D3DXVECTOR3 &Min, D3DXVECTOR3 &Max );
	void RegisterInlineModelHandle( int InlineModelIndex, int ModelHandle );
	int  GetModelHandleOfInlineModel( int InlineModelIndex );

	char	SkyBoxFilename[6][MAX_QPATH];
	BOOL	DrawSky;
	int		SkyBoxShader[6];
	int		CloudShaderHandle;
	int		CloudHeight;
	char	LoadedMapname[MAX_QPATH];

	BOOL isLoaded, isLoadedIntoD3D;

private:
	//Functions
	void CreateTexturesFromLightmaps();
	DWORD c_BSP::BrightenColour(U8 r, U8 g, U8 b);
	int FindLeafFromPos(D3DXVECTOR3 *pos);
	BOOL IsClusterVisible(int currentCluster, int testCluster);
	void SetD3DState(int index);

	//Variables
	eEndian Endian;
	FILEHANDLE	FileHandle;

	//D3D Buffers
	int						BSPVBhandle, *BSPDynamicVBHandles;
	int						BSPMeshIndicesHandle;
	int						*BSPShaderArray, *BSPEffectArray;
	c_Texture				**LightmapTextureBuffer;
	int NumPatches, NumIndexBuffers;

	//Useful information cache
	int NumPlanes, NumVerts, NumMeshVerts, NumFaces, NumTextures, NumQ3Lightmaps, NumLightmaps, NumLeaves, NumEffects, NumInlineModels;
	char *EntityTokenPos;
	int LastEntityToken;

//**Start of Q3 BSP File Structures*****

enum eLumps {				//ordered as they appear in BSP file
	kEntities, kTextures, 
	//BSP structures
	kPlanes, kNodes, kLeaves, kLeafFaces, kLeafBrushes,
	//other stuff
	kModels, kBrushes, kBrushSides, kVertices, kMeshVerts, kEffects, 
	kFaces, kLightmaps, kLightvols, kVisData,
	kMaxLumps
};

struct s_BSPLump {
	S32 offset;
	S32 length;
};
struct s_Textures {
	char name[MAX_QPATH];
	S32 flags;
	S32 contents;
};
struct s_Planes {
	vec3 normal;
	F32 distance;
};
struct s_Nodes {
	S32 PlaneIndex;
	S32 childFront;		//Negative values are leaf indices : -(leaf+1)
	S32 childBack;
	S32 min[3],max[3]; //Bounding Box
};
struct s_Leaves {
	S32 Cluster;
	S32 unused;
	S32 min[3],max[3];
	S32 firstLeafFace;
	S32 numLeafFace;
	S32 firstLeafBrush;
	S32 numLeafBrush;
};
/*struct s_LeafFaces {
	S32 index;			//Face index
};*/
struct s_LeafBrushes {
	S32 index;			//Brush index
};
struct s_Models {
	F32 min[3],max[3];
	S32 firstFace;
	S32 numFace;
	S32 firstBrush;
	S32 numBrush;
};
struct s_Brushes {
	S32 firstBrushSide;
	S32 numBrushSide;
	S32 texture;
};
struct s_BrushSides {
	S32 plane;
	S32 texture;
};
struct s_Vertices {
	vec3 position;
	F32 texCoord[2];
	F32 lightmapCoord[2];
	vec3 normal;
	U8 colour[4];
};
struct s_MeshVerts {
	S32 offset;
};
struct s_Effects {
	char name[64];
	S32 brush;
	S32 unused;
};
struct s_Faces {
	S32 texture;
	S32 effect;
	S32 type;
	S32 firstVertex;
	S32 numVertex;
	S32 firstMeshVert;
	S32 numMeshVert;
	S32 lightmapIndex;
	S32 lightmapStart[2];
	S32 lightmapSize[2];
	vec3 lightmapOrigin;
	F32 lightmapAxes[2][3];
	vec3 normal;
	S32 patchSize[2];
};
struct s_Lightmaps {
	U8 map[128][128][3];	//RGB
};
struct s_Lightvols {
	U8 ambientColour[3];
	U8 directionalColour[3];
	U8 direction[2];				//[0] = phi, [1] = theta
};
struct s_VisData {
	S32 numVec;
	S32 sizeVec;
	U8 *vec;
};
//**End of Q3 BSP File Structures*****

//Implementation
//Structures that will hold BSP and additional data in optimised format

ALIGN16 struct s_DQPlanes {
	D3DXVECTOR3 normal;
	float dist;
	enum ePlanetype { ePlaneNonAxial=0, ePlaneX=1, ePlaneY=2, ePlaneZ=3, ePlaneMinusX, ePlaneMinusY, ePlaneMinusZ };
	ePlanetype type;

	inline void CalcType() {
		if(normal.x==1 && normal.y==0 && normal.z==0) type = ePlaneX;
		else if(normal.x==-1 && normal.y==0 && normal.z==0) type = ePlaneMinusX;
		else if(normal.x==0 && normal.y==1 && normal.z==0) type = ePlaneY;
		else if(normal.x==0 && normal.y==-1 && normal.z==0) type = ePlaneMinusY;
		else if(normal.x==0 && normal.y==0 && normal.z==1) type = ePlaneZ;
		else if(normal.x==0 && normal.y==0 && normal.z==-1) type = ePlaneMinusZ;
		else type = ePlaneNonAxial;
	}
	inline float Dot( D3DXVECTOR3 &vec ) {
		switch(type) {
		case ePlaneX: return vec.x;
		case ePlaneY: return vec.y;
		case ePlaneZ: return vec.z;
		case ePlaneMinusX: return -vec.x;
		case ePlaneMinusY: return -vec.y;
		case ePlaneMinusZ: return -vec.z;
		default:
			return D3DXVec3Dot( &normal, &vec );
		};
	}
};
ALIGN16 struct s_DQFaces {
	s_Faces Q3Face;

	c_CurvedSurface *BezierPatch;
	int DynamicVB;
	s_Billboard *Billboard;

	//Cached information to speed up rendering
	int ShaderHandle;
	int LastFrameDrawn;
	c_Texture *LightmapTexture;
	D3DXVECTOR3 ApproxPos;
};
struct s_DQVertices {
	D3DXVECTOR3 *Pos;
	D3DXVECTOR3 *Normal;
	D3DXVECTOR2 *TexCoords, *LightmapCoords;
	DWORD *Diffuse;
};
ALIGN16 struct s_DQNode {
	S32 PlaneIndex;
	S32 childFront;		//Negative values are leaf indices : -(leaf+1)
	S32 childBack;
	int IgnoreCluster;	//This node and its children are not visible from this cluster
	s_Box Box;
	float BoxRadius;
};
ALIGN16 struct s_DQLeaf {
	S32 Cluster;
	S32 LastVisibleCluster;			//used for rendering
	S32 firstLeafFace;
	S32 numLeafFace;
	S32 firstLeafBrush;
	S32 numLeafBrush;
//	int CurveBrush;
};
struct s_DQBrush {
	S32 firstBrushSide;
	S32 numBrushSide;
	S32 texture;
//	int TraceCount;
};
struct s_DQModels {
	D3DXVECTOR3 Min, Max;
	S32 firstFace;
	S32 numFace;
	S32 firstBrush;
	S32 numBrush;
	void *ent;			//Set by SetBrushModel - GAME ONLY
	int ModelHandle;	//CLIENT GAME ONLY
};
struct s_DQLightVol {
	D3DXVECTOR3 Ambient;
	D3DXVECTOR3 Diffuse;
	float theta, phi;

	inline void Clear()
	{
		Ambient = Diffuse = D3DXVECTOR3( 0,0,0 );
		theta = phi = 0.0f;
	}

	inline void AddLerpedLightvol( float Factor, s_Lightvols &x )
	{
		Ambient.x += (float)x.ambientColour[0] * Factor;
		Ambient.y += (float)x.ambientColour[1] * Factor;
		Ambient.z += (float)x.ambientColour[2] * Factor;
		Diffuse.x += (float)x.directionalColour[0] * Factor;
		Diffuse.y += (float)x.directionalColour[1] * Factor;
		Diffuse.z += (float)x.directionalColour[2] * Factor;
		phi += (float)x.direction[0] * Factor;
		theta += (float)x.direction[1] * Factor;
	}

	inline void GetD3DLight(D3DLIGHT9 &D3DLight) {
		Ambient /= 255.0f;
		Diffuse /= 255.0f;
		theta = theta/128.0f*D3DX_PI;
		phi = phi/128.0f*D3DX_PI;

		D3DLight.Type = D3DLIGHT_DIRECTIONAL;
		D3DLight.Ambient = D3DXCOLOR( Ambient.x, Ambient.y, Ambient.z, 1.0f);
		D3DLight.Diffuse = D3DXCOLOR( Diffuse.x, Diffuse.y, Diffuse.z, 1.0f);
		D3DLight.Direction = D3DXVECTOR3( (float)sin(theta)*sin(phi), (float)cos(phi), (float)cos(theta)*sin(phi) );
		D3DLight.Range = 150.0f;
		D3DLight.Attenuation0 = 1.0f;
	}
};

struct s_CurveBrushPoint {
	D3DXVECTOR3 Normal, FirstVert, EdgeNormal1, EdgeNormal2;
	float PlaneDist;
};
struct s_CurveBrush {
	D3DXVECTOR3 ApproxPos;
	float Radius;
	int FirstBrushPoint, NumBrushPoints;
};

	s_BSPLump LumpDirectory[kMaxLumps];
	char *EntityString;
	s_Textures *Texture;
	s_DQPlanes *Plane;
	s_DQNode *Node;
	s_DQLeaf *Leaf;
	S32 *LeafFace;
	s_LeafBrushes *LeafBrush;
	s_DQModels *Model;
	s_DQBrush *Brush;
	s_BrushSides *BrushSide;
	s_DQVertices Vertices;
	WORD *MeshVerts;
	s_Effects *Effect;
	s_DQFaces *Face;
	s_Lightmaps *Lightmap;
	s_Lightvols *Lightvol;
	s_VisData VisData;

	unsigned int NumLightvols_x, NumLightvols_y, NumLightvols_z;

	//For Rendering
	BOOL c_BSP::RenderNode( int NodeIndex );
	BOOL c_BSP::RenderNodeWithoutFrustumCull( int NodeIndex );
	BOOL c_BSP::RenderLeaf( int LeafIndex );
	void c_BSP::MarkVisibleLeaves();

	s_Plane CameraFrustum;

	vmCvar_t r_useBSP, cm_playerCurveClip;

	int ThisCluster, LastCluster;

public:
	void c_BSP::SetLightVol( D3DXVECTOR3 Pos );
	void c_BSP::EndLightVol();

	int LightNum;
	int FacesDrawn;

	struct s_Trace {
		BOOL TracePointOnly;
		BOOL StartSolid, AllSolid;
		D3DXVECTOR3 BoxExtent, BoxMax, BoxMin, Direction;
		D3DXVECTOR3 *Start, *End, MidpointStart, MidpointEnd;
		float CollisionFraction, BoxRadius;
		s_DQPlanes Plane;
		int SurfaceFlags, EntityNum;
		int ContentMask;
		int contents;
		s_Sphere EnclosingSphere;
	};

	void c_BSP::TraceBox(s_Trace &Trace);
	void c_BSP::CreateNewTrace( s_Trace &Trace, D3DXVECTOR3 *start, D3DXVECTOR3 *end, D3DXVECTOR3 *min, D3DXVECTOR3 *max, int contentmask );
	void c_BSP::ConvertTraceToResults( s_Trace *Trace, trace_t *results );
	void c_BSP::TraceAgainstAABB( s_Trace *Trace, D3DXVECTOR3 &EntityPosition, D3DXVECTOR3 &EntityExtent, int EntityContents, int &EntityNumber );
	int  c_BSP::PointContents( D3DXVECTOR3 *Point );
	int  c_BSP::PointContentsBrush( D3DXVECTOR3 *Point, s_DQBrush *pBrush );
	int  c_BSP::GetMarkFragments( float *pfOrigPoints, int NumOrigPoints, D3DXVECTOR3 *ProjectionDir, float *pfPointBuffer, int PointBufferSize, markFragment_t *MarkFragment, int MaxMarkFragments );
	void c_BSP::RecursiveGetLeavesFromLine( int *LeavesContainingPoints, int NumLeavesCP, D3DXVECTOR3 *StartPoint, D3DXVECTOR3 *EndPoint, float offset, int NodeIndex );
	void c_BSP::SetBrushModel( sharedEntity_t *pEntity, int InlineModelNum );
	void c_BSP::TraceInlineModel( s_Trace &Trace, int InlineModelIndex );

private:
	void c_BSP::RecursiveTrace(const int &NodeIndex, D3DXVECTOR3 &Start, D3DXVECTOR3 &End, const float &StartFraction, const float &EndFraction, s_Trace &Trace);
	void c_BSP::CheckBrush( s_Trace &Trace, s_DQBrush *Brush );
	int TraceNodeCache[MAX_NODECACHE];
	int NumNodeCache;
	int CurrentFrameNum;
//	int TraceCount;

	void c_BSP::GenerateSkyboxSphereCoords( D3DXVECTOR3 *Verts, D3DXVECTOR3 Centre, float SphereSize, float SphereTexScale, D3DXVECTOR2 *SphereTexCoords, int NumVerts );
	int SkyboxIBHandle[6];
	int SkyboxVBHandle[6];
	int SkyDomeVBHandle;
	int SkyDomeIBHandle;
	int NumSkySquarePrims, NumSkyDomePrims;

	int CurveSubdivisions;

	//Create Curved surface brushes
	s_CurveBrush *CurveBrush;
	s_CurveBrushPoint *CurveBrushPoints;
	int NumCurveBrushPoints, NumCurveBrushes;
	void c_BSP::CreateCurveBrush( int FaceIndex, int CBIndex );
	void c_BSP::CheckCurveBrush( s_Trace &Trace, s_CurveBrush *CurveBrush );
};

#define PlaneVec3DotProduct( Plane, Vector ) \
	((Plane).type==ePlaneX) ?(Vector).x - (Plane).dist: \
	( ((Plane).type==ePlaneY) ?(Vector).y - (Plane).dist: \
	( ((Plane).type==ePlaneZ) ?(Vector).z - (Plane).dist: \
	( ((Plane).normal.x * (Vector).x + (Plane).normal.y * (Vector).y + (Plane).normal.z * (Vector).z - (Plane).dist) )))

#define DQBSP c_BSP::GetSingleton()

#endif	//__C_BSP_H