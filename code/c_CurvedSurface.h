// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_CurvedSurface.h: interface for the c_CurvedSurface class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_CURVEDSURFACE_H__F567B6C9_D206_4306_8250_D18E50738A60__INCLUDED_)
#define AFX_C_CURVEDSURFACE_H__F567B6C9_D206_4306_8250_D18E50738A60__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class c_CurvedSurface
{
public:
	c_CurvedSurface();
	virtual ~c_CurvedSurface();
	void c_CurvedSurface::LoadCubicBezierPatch(D3DXVECTOR3 *pVectorArray, int num1, int num2, int numPoints);
	void c_CurvedSurface::LoadQuadricBezierPatch(D3DXVECTOR3 *pVectorArray, int num1, int num2, int numPoints);
	void c_CurvedSurface::LoadQ3PatchIntoD3D(int FaceIndex, int numPoints);
	void c_CurvedSurface::LoadQ3PatchForBrush(int FaceIndex);
	void RenderObject( int ShaderHandle, c_Texture *LightmapTexture );
	int DrawLevel; //LOD
	
private:
	int VertexBufferHandle;
	int IndexBufferHandle[5];

	int numVertsPerStrip;
	int numVertStrips;
	int MaxDrawLevel;
	int NumIndices[5];
	int GetNumStrips(int level) { return numVertStrips/(1<<level); }
	int m_FaceIndex;
	D3DXVECTOR3 ApproxPos;
	float Radius;
};

#endif // !defined(AFX_C_CURVEDSURFACE_H__F567B6C9_D206_4306_8250_D18E50738A60__INCLUDED_)
