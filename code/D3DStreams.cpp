// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//

#include "stdafx.h"

//Vertex Declarations
//Notation :
// P	Declaration has a Position in it
// N	Declaration has a Vertex Normal in it
// D	Declaration has a Diffuse Colour value in it
// T	The number of T's denote the number of texture stages
// Num	Denotes the stream value of the letters before it
D3DVERTEXELEMENT9 ElementDecl_P0[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	D3DDECL_END()
};


D3DVERTEXELEMENT9 ElementDecl_P0T1[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0N1T2[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_NORMAL, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0D1T2[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_COLOR,  0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0N1D2T3[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_NORMAL, 0 },
	{ 2, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_COLOR,  0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0T1T2[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 1 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0N1T2T3[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_NORMAL, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 1 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0D1T2T3[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_COLOR,  0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 1 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 ElementDecl_P0N1D2T3T4[] = 
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_POSITION, 0 },
	{ 1, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_NORMAL, 0 },
	{ 2, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_COLOR,  0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 0 },
	{ 4, 0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, 
	                                  D3DDECLUSAGE_TEXCOORD, 1 },
	D3DDECL_END()
};
