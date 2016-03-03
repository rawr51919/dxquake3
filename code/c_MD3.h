// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_MD3.h: interface for the c_MD3 class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_MD3_H__E0EFF5D7_69CA_421E_A761_1A6CA4CA50F8__INCLUDED_)
#define AFX_C_MD3_H__E0EFF5D7_69CA_421E_A761_1A6CA4CA50F8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MD3_MAX_LODS 4
#define MD3_MAX_TRIANGLES 8192
#define MD3_MAX_VERTS 4096
#define MD3_MAX_SHADERS 256
#define MD3_MAX_FRAMES 1024
#define MD3_MAX_SURFACES 32
#define MD3_MAX_TAGS 16
//MD3_XYZ_SCALE = 1/64
#define MD3_XYZ_SCALE 0.015625f

struct s_Sphere
{
	D3DXVECTOR3 centre;
	float radius;
};

struct s_BoundingBox
{
	D3DXVECTOR3 max, min;
};

struct s_Box
{
	D3DXVECTOR3 MidpointPos;
	D3DXVECTOR3 Extent;				//Vector from Midpoint to corner (+ve all axes)
};

struct s_Plane
{
	D3DXVECTOR3 Normal;
	float Dist;
};

class c_Model;

class c_MD3  
{
	friend c_Model;
public:
	c_MD3();
	c_MD3(c_MD3 *Original);
	virtual ~c_MD3();
	BOOL LoadMD3(FILEHANDLE Handle);
	void DrawMD3( s_RenderFeatureInfo *RenderFeature );
private:
	BOOL		isLoaded;
	eEndian		Endian;
	BOOL		isCopy;

//Following is the specification of the MD3 File
//**********************************************
struct Header_t
{
	//- MD3 Start
	S32 Magic_Ident;
	S32 Version;
	U8	MD3Name[MAX_QPATH];	//null terminated
	S32 Flags;
	S32 NumFrames;
	S32 NumTags;
	S32 NumSurfaces;
	S32 NumSkins;				//Not used
	S32 OffsetFrames;
	S32 OffsetTags;
	S32 OffsetSurfaces;
	S32 OffsetEOF;
	//array of Frame objects
	//array of Tag objects
	//array of Surface objects
};

struct Frame_t
{
	vec3 MinBounds;
	vec3 MaxBounds;
	vec3 LocalOrigin;
	F32  Radius;
	U8	 Name[16];
};

struct Tag_t
{
	U8	 Name[MAX_QPATH];
	vec3 Origin;
	vec3 Axis[3];
};

struct Shader_t
{
	U8  Name[MAX_QPATH];
	S32 ShaderIndex;
};

struct Triangle_t
{
	S32 Index[3];
};

struct TexCoord_t
{
	F32 tex[2];				//referred to as S&T in OpenGL
};

struct Vertex_t
{
	S16 x;
	S16 y;
	S16 z;
	S16 NormalCode;		//encoded normal. lat = (NormalCode & 0xFF) *2Pi/255
						//				 long = ((NormalCode >> 8) & 0xFF) *2Pi/255
						//	x = cos lat * sin long; y = sin lat * sin long; z = cos long;
};

struct Surface_t
{
//	S32 SurfaceStartOffset;		//NOT READ FROM FILE
	S32 MagicIdent;
	U8  Name[MAX_QPATH];
	S32 Flags;
	S32 NumFrames;			//should match header.NumFrames
	S32 NumShaders;
	S32 NumVerts;			//This is also the number of TexCoord objects
	S32 NumTriangles;
	S32 OffsetTriangles;	//relative offset to triangles from Surface Start
	S32 OffsetShaders;
	S32 OffsetTexCoord;
	S32 OffsetVertices;
	S32 OffsetEnd;			//rel offset to end of Surface Object
	//array of Shader objects
	//array of Triangle objects
	//array of TexCoord objects
	//array of Vertex objects, per frame
};
//*******************************************
//End of MD3 Specification 

//Start of DXQuake3 implementation of MD3 format
//*******************************************
public:
struct s_DQSurface
{
	s_DQSurface():LastUpdatedFrame(-1),VertexBufferHandle(NULL),IndexBufferHandle(NULL),
			InterpolatedPos(NULL),InterpolatedNormal(NULL) {}

	char Name[MAX_QPATH];
	S32 Flags;
	S32 NumVerts;			//IMPORTANT: This is also the number of TexCoord objects
	S32 NumTriangles;
	Triangle_t	*Triangle;	//array of Triangle objects
	D3DXVECTOR2	*TexCoord;	//array of TexCoord objects
	D3DXVECTOR3 **VertexPos; //VertexPos[FrameNum][VertNum]
	D3DXVECTOR3 **VertexNormal; //VertexNormal[FrameNum][VertNum]
	D3DXVECTOR3 *InterpolatedPos;	//temporary storage for interpolated Vert Positions
	D3DXVECTOR3 *InterpolatedNormal;	//temporary storage

	int ShaderHandle;
	int VertexBufferHandle;
	int IndexBufferHandle;
	int LastUpdatedFrame;
};
private:
struct s_DQTag {
	D3DXQUATERNION QRotation;
	D3DXVECTOR3 Position;
};
struct s_DQTagIndex {
	char TagName[MAX_QPATH];
	S16 TagIndex;
};
struct s_DQFrame {
	s_BoundingBox BBox;
	s_Sphere BSphere;
};

	s_DQSurface *Surface;		//Surface[SurfaceNum]
	s_DQTag **Tag;				//Tag[FrameNum][TagNum]
	s_DQTagIndex *TagIndex;
	s_DQFrame *Frame;
	S32 NumFrames;
	S16 NumSurfaces;
	S16 NumTags;

	int LoadedThisFrame, LoadedNextFrame;
	float LoadedFraction;

	BOOL bUseDynamicBuffer;

	D3DXVECTOR3 c_MD3::NormalCodeToD3DX(S16 code);
	void LoadSurfaceIntoD3D(s_DQSurface *pSurface, int ThisFrame, int NextFrame, float Fraction);
	void GetModelBounds( D3DXVECTOR3 &Min, D3DXVECTOR3 &Max );
public:
	void c_MD3::LoadMD3IntoD3D(int ThisFrame, int NextFrame, float Fraction);
	int GetTagNum(const char *TagName);
	void c_MD3::InterpolateTag(orientation_t *tag, int TagNum, int ThisFrame, int NextFrame, float Fraction);
	void c_MD3::SetSkin( c_Skin *Skin );
};

#endif // !defined(AFX_C_MD3_H__E0EFF5D7_69CA_421E_A761_1A6CA4CA50F8__INCLUDED_)
