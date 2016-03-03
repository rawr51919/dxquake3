// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	Direct3D Stream Declarations
//

#ifndef __D3DSTREAMS_H
#define __D3DSTREAMS_H
/*
struct s_VertexStreamPos
{
    D3DXVECTOR3 Pos;
};
#define FVF_Pos ( D3DFVF_XYZ )

struct s_VertexStreamPosTex
{
	D3DXVECTOR3 Pos;
	D3DXVECTOR2 Tex0;
};
#define FVF_PosTex ( D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )

struct s_VertexStreamPosNormal
{
    D3DXVECTOR3 Pos;
	D3DXVECTOR3 Normal;
};
#define FVF_PosNormal ( D3DFVF_XYZ | D3DFVF_NORMAL )

struct s_VertexStreamPosNormalDiffuse
{
    D3DXVECTOR3 Pos;
	D3DXVECTOR3 Normal;
    DWORD Diffuse;
};
#define FVF_PosNormalDiffuse ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE )

struct s_VertexStreamPosDiffuseTex
{
    D3DXVECTOR3 Pos;
	DWORD Diffuse;
	D3DXVECTOR2 Tex;
};
#define FVF_PosDiffuseTex ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )

struct s_VertexStreamPosNormalDiffuseTex
{
    D3DXVECTOR3 Pos;
	D3DXVECTOR3 Normal;
	DWORD Diffuse;
	D3DXVECTOR2 Tex;
};
#define FVF_PosNormalDiffuseTex ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )

struct s_VertexStreamPosNormalDiffuseTexTex
{
    D3DXVECTOR3 Pos;
	D3DXVECTOR3 Normal;
	DWORD Diffuse;
	D3DXVECTOR2 Tex0;
	D3DXVECTOR2 Tex1;
};
#define FVF_PosNormalDiffuseTexTex ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1) )

struct s_VertexStreamPosTTex
{
	D3DXVECTOR4 PosT;
	D3DXVECTOR2 Tex;
};
#define FVF_PosTTex ( D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )

struct s_VertexStreamPosTDiffuseTex
{
	D3DXVECTOR4 PosT;
	DWORD Diffuse;
	D3DXVECTOR2 Tex;
};
#define FVF_PosTDiffuseTex ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )
*/
//Vertex stream structs for multi-stream vertex declarations
struct s_VertexStreamP
{
	D3DXVECTOR3 Pos;
};

struct s_VertexStreamN
{
	D3DXVECTOR3 Normal;
};

struct s_VertexStreamD
{
	DWORD Diffuse;
};

struct s_VertexStreamT
{
	D3DXVECTOR2 Tex;
};

//Vertex Declarations
//Notation :
// P	Declaration has a Position in it
// N	Declaration has a Vertex Normal in it
// D	Declaration has a Diffuse Colour value in it
// T	The number of T's denote the number of texture stages
// Num	Denotes the stream value of the letter before it
extern D3DVERTEXELEMENT9 ElementDecl_P0[];
extern D3DVERTEXELEMENT9 ElementDecl_P0T1[];
extern D3DVERTEXELEMENT9 ElementDecl_P0N1T2[];
extern D3DVERTEXELEMENT9 ElementDecl_P0D1T2[];
extern D3DVERTEXELEMENT9 ElementDecl_P0N1D2T3[];
extern D3DVERTEXELEMENT9 ElementDecl_P0T1T2[];
extern D3DVERTEXELEMENT9 ElementDecl_P0N1T2T3[];
extern D3DVERTEXELEMENT9 ElementDecl_P0D1T2T3[];
extern D3DVERTEXELEMENT9 ElementDecl_P0N1D2T3T4[];
#endif //__D3DSTREAMS_H