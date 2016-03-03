// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_CurvedSurface.cpp: implementation of the c_CurvedSurface class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_CurvedSurface.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_CurvedSurface::c_CurvedSurface()
{
	VertexBufferHandle = NULL;
	for(int i=0;i<5;++i) IndexBufferHandle[i] = NULL;
	DrawLevel = 0;
}

c_CurvedSurface::~c_CurvedSurface()
{
}

//Derivation :
//Reference : Introduction to Computer Graphics - Foley, Van Dam, Feiner, Hughes, Phillips
//notation v = vector, m = matrix, t = tensor (3D matrix)

//Hermite Curve : 
// mGeometryHermite = ( vP1 vP4 vR1 vR4 )
// vT = ( t^3 t^2 t 1 )
// vCurve = mGeometryHermite . mHermiteTransform . vT
// mHermiteTransform derived from constraints of mGeometry on vCurve
//Bezier Curve derived from Hermite curve
// mGeometryBezier = ( vP1 vP2 vP3 vP4 )
// vCurve = mGeometryBezier . mBezierTransform . vT
// mGeometryBezier derived assuming constant velocity w.r.t. t on a straight line

//Surface : mGeometryBezierPatch = ( vG1(s) vG2(s) vG3(s) vG4(s) )
// where vGi(s) is a vector which is a function of s. s is a parametric variable 0<s<1
// vGi describes our control points for the curve. As s varies, these vary along a perpendicular bezier curve (hence describing a surface)
// vGi = mGi . mBezierTransform . vS
// where vS = ( s^3 s^2 s 1 )
// and mGi = ( vgi1 vgi2 vgi3 vgi4 )
// thus mGeometryBezierPatch is in fact a tensor.
// Taking the transpose of vGi = mGi. mBezierTransform . vS, we get
// vGiT = vST . mBezierTransformT . mGiT
// substituting into vSurfacePoint = mGeometryBezierPatch . mBezierTransform . vT, we get
// vSurfacePoint = vST . mBezierTransformT . tGeometryBezierPatch . mBezierTransform . vT

//numPoints is the number of interpolated points desired between 2 control points
//num1 is the number of patches in direction 1
//num2 is the number of patches in direction 2

//Cubic patches are not used in Q3, quadric patches are.

/*
void c_CurvedSurface::LoadCubicBezierPatch(D3DXVECTOR3 *pVectorArray, int num1, int num2, int numPoints)
{
	int NumLevels;
	D3DFORMAT IndexFormat;
	s_BezierPatchVertex *pVertex;
	WORD ***pWordIndex;
	DWORD ***pDWordIndex;
	BOOL useDWord;
	int LevelSkipCount[2],level;
	int i,j,i2,j2,i3,j3,u,vertexnumber;
	D3DXVECTOR3 *ControlPoint[4];
	float **B; //Bezier coefficients
	float t,dt,OneMinus_t;
	int numjPoints, numiPoints;	//Varies if we are at the end of the series of patches

	MaxDrawLevel = 0;
	if(!(numPoints%2) && numPoints>2) {
		MaxDrawLevel = 1; 
		if(!(numPoints%4) && numPoints>4) {
			MaxDrawLevel = 2;
			if(!(numPoints%8) && numPoints>8) {
				MaxDrawLevel = 3;
				if(!(numPoints%16) && numPoints>16) {
					MaxDrawLevel = 4;
					if(!(numPoints%32) && numPoints>32) {
						MaxDrawLevel = 5;
	}}}}}
	NumLevels = MaxDrawLevel;
	MaxDrawLevel--;

	if(2*(numPoints*num1+1)*(numPoints*num2+1)>65535) {
		IndexFormat = D3DFMT_INDEX32;
		useDWord = TRUE;
	}
	else {
		IndexFormat = D3DFMT_INDEX16;
		useDWord = FALSE;
	}

	if(!useDWord) {
		pWordIndex = (WORD***)DQNewVoid( WORD**[NumLevels] );
		for(level=0;level<NumLevels;++level) {
			pWordIndex[level] = (WORD**)DQNewVoid( WORD*[numPoints+1] );
		}
	}
	else {
		pDWordIndex = (DWORD***)DQNewVoid( DWORD**[NumLevels] );
		for(level=0;level<NumLevels;++level) {
			pDWordIndex[level] = (DWORD**)DQNewVoid( DWORD*[numPoints+1] );
		}
	}

	B = (float**)DQNewVoid( float *[numPoints+1] );
	for(i=0;i<=numPoints;++i) {
		B[i] = (float*)DQNewVoid( float [4]);
	}

	dt= 1.0f / (float)numPoints;
	t=0;
	for(i=0;i<numPoints+1;++i) {
		OneMinus_t = 1.0f - t;
		if(i==numPoints) {
			t=1;
			OneMinus_t = 0.0f;
		}

		B[i][0] = OneMinus_t*OneMinus_t*OneMinus_t;
		B[i][1] = OneMinus_t*OneMinus_t*3.0f*t;
		B[i][2] = OneMinus_t*3.0f*t*t;
		B[i][3] = t*t*t;
		t+=dt;
	}

	if(numPoints<1) numPoints = 1;

	d3dcheckOK( DQDevice->CreateVertexBuffer( (numPoints*num1+1)*(numPoints*num2+1)*sizeof(s_BezierPatchVertex), D3DUSAGE_WRITEONLY,
				FVF_BezierPatchVertex, D3DPOOL_DEFAULT, &VertexBuffer, NULL ) );
	for(level=0;level<NumLevels;++level) {
		d3dcheckOK( DQDevice->CreateIndexBuffer( 2*(numPoints/(1<<level)*num1+1)*(numPoints/(1<<level)*num2+1)*sizeof(s_BezierPatchVertex), D3DUSAGE_WRITEONLY,
					IndexFormat, D3DPOOL_DEFAULT, &IndexBuffer[level], NULL) );
	}
	d3dcheckOK( VertexBuffer->Lock( 0,0, (void**)&pVertex, 0 ) );
	if(!useDWord) {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pWordIndex[level][0]), 0 ) );
	}
	else {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pDWordIndex[level][0]), 0 ) );
	}

	//Bicubic Bezier Patch
	//For each strip
	vertexnumber = 0;
	LevelSkipCount[0] = LevelSkipCount[1] = 0;
	numiPoints = numPoints;
	for(i=0;i<num1;++i) {
		if(i==num1-1) numiPoints = numPoints+1;

		//Set up index buffer array. Each index buffer element points to the row above
		//TODO : switch for order
		for(i3=1;i3<numPoints+1;++i3) {
			if(!useDWord)
				for(u=0;u<NumLevels;++u) pWordIndex[u][i3]=pWordIndex[u][i3-1]+2*(numPoints/(1<<u)*num2+1);
			else
				for(u=0;u<NumLevels;++u) pDWordIndex[u][i3]=pDWordIndex[u][i3-1]+2*(numPoints/(1<<u)*num2+1);
		}

		for(i2=0;i2<numiPoints;++i2) {
			numjPoints = numPoints;

			for(j=0;j<num2;++j) {
				if(j==num2-1) numjPoints = numPoints+1;

				//calculate pointers to control points for this patch
				//ControlPoint[row] = pointer to D3DXVECTOR3 [4]
				ControlPoint[0] = pVectorArray+3*j+(3*num2+1)*3*i;
				ControlPoint[1] = pVectorArray+3*j+(3*num2+1)*(3*i+1);
				ControlPoint[2] = pVectorArray+3*j+(3*num2+1)*(3*i+2);
				ControlPoint[3] = pVectorArray+3*j+(3*num2+1)*(3*i+3);
				for(j2=0;j2<numjPoints;++j2,++pVertex,++vertexnumber) {

					//Calculate the interpolated point
					pVertex->pos = D3DXVECTOR3(0,0,0);
					for(i3=0;i3<4;++i3) {
						for(j3=0;j3<4;++j3) {
							pVertex->pos += ControlPoint[i3][j3]*(B[i2][i3]*B[j2][j3]);
						}
					}
					pVertex->diffuse = 0x000000FF<<4*((i+j)%5);

					if(!useDWord) {
						for(level=0;level<NumLevels;++level) {
							if( (!(LevelSkipCount[0]%(1<<level)) || j2==numPoints ) 
							 && (!(LevelSkipCount[1]%(1<<level)) || i2==numPoints ) ) {
								*pWordIndex[level][i2/(1<<level)] = vertexnumber;
								++pWordIndex[level][i2/(1<<level)];
								*pWordIndex[level][i2/(1<<level)] = vertexnumber+(1<<level)*(1+num2*numPoints);
								++pWordIndex[level][i2/(1<<level)];
							}
						}
					}
					else {
						for(level=0;level<NumLevels;level++) {
							if( (!(LevelSkipCount[0]%(1<<level)) || j2==numPoints ) 
							 && (!(LevelSkipCount[1]%(1<<level)) || i2==numPoints ) ) {
								*pDWordIndex[level][i2/(1<<level)] = vertexnumber;
								++pDWordIndex[level][i2/(1<<level)];
								*pDWordIndex[level][i2/(1<<level)] = vertexnumber+(1<<level)*(1+num2*numPoints);
								++pDWordIndex[level][i2/(1<<level)];
							}
						}
					}
					++LevelSkipCount[1];
				}
			}
			++LevelSkipCount[0];
		}
		if(!useDWord)
			for(level=0;level<NumLevels;++level) pWordIndex[level][0] += 2*numPoints/(1<<level)*(numPoints/(1<<level)*num2+1)-2*(numPoints/(1<<level)*num2+1);
		else 
			for(level=0;level<NumLevels;++level) pDWordIndex[level][0] += 2*numPoints/(1<<level)*(numPoints/(1<<level)*num2+1)-2*(numPoints/(1<<level)*num2+1);
	}

	for(i=0;i<numPoints;++i) DQDeleteArray( B[i] );
	DQDeleteArray(B);
	if(!useDWord) {
		for(u=0;u<NumLevels;++u) DQDeleteArray(pWordIndex[u]);
		DQDeleteArray(pWordIndex);
	}
	else {
		for(u=0;u<NumLevels;++u) DQDeleteArray(pDWordIndex[u]);
		DQDeleteArray(pDWordIndex);
	}

	d3dcheckOK( VertexBuffer->Unlock() );
	for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Unlock() );
	numVertsPerStrip = num2*numPoints;
	numVertStrips = num1*numPoints;
}
*/
//*****************************************************************

//numPoints is the number of interpolated points desired between 2 control points
//num1 is the number of patches in direction 1
//num2 is the number of patches in direction 2
/*
//As per cubic Bezier patch, but with minor differences
void c_CurvedSurface::LoadQuadricBezierPatch(D3DXVECTOR3 *pVectorArray, int num1, int num2, int numPoints)
{
	int NumLevels;
	D3DFORMAT IndexFormat;
	s_BezierPatchVertex *pVertex;
	WORD ***pWordIndex;
	DWORD ***pDWordIndex;
	BOOL useDWord;
	int LevelSkipCount[2],level;
	int i,j,i2,j2,i3,j3,u,vertexnumber;
	D3DXVECTOR3 *ControlPoint[3];
	float **B; //Bezier coefficients
	float t,dt,OneMinus_t;
	int numjPoints, numiPoints;	//Varies if we are at the end of the series of patches

	MaxDrawLevel = 0;
	if(!(numPoints%2) && numPoints>2) {
		MaxDrawLevel = 1; 
		if(!(numPoints%4) && numPoints>4) {
			MaxDrawLevel = 2;
			if(!(numPoints%8) && numPoints>8) {
				MaxDrawLevel = 3;
				if(!(numPoints%16) && numPoints>16) {
					MaxDrawLevel = 4;
					if(!(numPoints%32) && numPoints>32) {
						MaxDrawLevel = 5;
	}}}}}
	NumLevels = MaxDrawLevel;
	MaxDrawLevel--;

	if(2*(numPoints*num1+1)*(numPoints*num2+1)>65535) {
		IndexFormat = D3DFMT_INDEX32;
		useDWord = TRUE;
	}
	else {
		IndexFormat = D3DFMT_INDEX16;
		useDWord = FALSE;
	}

	if(!useDWord) {
		pWordIndex = (WORD***)DQNewVoid( WORD**[NumLevels] );
		for(level=0;level<NumLevels;++level) {
			pWordIndex[level] = (WORD**)DQNewVoid( WORD*[numPoints+1] );
		}
	}
	else {
		pDWordIndex = (DWORD***)DQNewVoid( DWORD**[NumLevels] );
		for(level=0;level<NumLevels;++level) {
			pDWordIndex[level] = (DWORD**)DQNewVoid( DWORD*[numPoints+1] );
		}
	}

	B = (float**)DQNewVoid( float *[numPoints+1] );
	for(i=0;i<=numPoints;++i) {
		B[i] = (float*)DQNewVoid( float [3] );
	}

	dt= 1.0f / (float)numPoints;
	t=0;
	for(i=0;i<numPoints+1;++i) {
		OneMinus_t = 1.0f - t;
		if(i==numPoints) {
			t=1;
			OneMinus_t = 0.0f;
		}

		B[i][0] = OneMinus_t*OneMinus_t;
		B[i][1] = 2*t*OneMinus_t;
		B[i][2] = t*t;
		t+=dt;
	}

	if(numPoints<1) numPoints = 1;

	d3dcheckOK( DQDevice->CreateVertexBuffer( (numPoints*num1+1)*(numPoints*num2+1)*sizeof(s_BezierPatchVertex), D3DUSAGE_WRITEONLY,
				FVF_BezierPatchVertex, D3DPOOL_DEFAULT, &VertexBuffer, NULL ) );
	for(level=0;level<NumLevels;++level) {
		d3dcheckOK( DQDevice->CreateIndexBuffer( 2*(numPoints/(1<<level)*num1+1)*(numPoints/(1<<level)*num2+1)*sizeof(s_BezierPatchVertex), D3DUSAGE_WRITEONLY,
					IndexFormat, D3DPOOL_DEFAULT, &IndexBuffer[level], NULL) );
	}
	d3dcheckOK( VertexBuffer->Lock( 0,0, (void**)&pVertex, 0 ) );
	if(!useDWord) {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pWordIndex[level][0]), 0 ) );
	}
	else {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pDWordIndex[level][0]), 0 ) );
	}

	//Bicubic Bezier Patch
	//For each strip
	vertexnumber = 0;
	LevelSkipCount[0] = LevelSkipCount[1] = 0;
	numiPoints = numPoints;
	for(i=0;i<num1;++i) {
		if(i==num1-1) numiPoints = numPoints+1;

		//Set up index buffer array. Each index buffer element points to the row above
		//TODO : switch for order
		for(i3=1;i3<numPoints+1;++i3) {
			if(!useDWord)
				for(u=0;u<NumLevels;++u) pWordIndex[u][i3]=pWordIndex[u][i3-1]+2*(numPoints/(1<<u)*num2+1);
			else
				for(u=0;u<NumLevels;++u) pDWordIndex[u][i3]=pDWordIndex[u][i3-1]+2*(numPoints/(1<<u)*num2+1);
		}

		for(i2=0;i2<numiPoints;++i2) {
			numjPoints = numPoints;

			for(j=0;j<num2;++j) {
				if(j==num2-1) numjPoints = numPoints+1;

				//calculate pointers to control points for this patch
				//ControlPoint[row] = pointer to D3DXVECTOR3 [4]
				ControlPoint[0] = pVectorArray+2*j+(2*num2+1)*2*i;
				ControlPoint[1] = pVectorArray+2*j+(2*num2+1)*(2*i+1);
				ControlPoint[2] = pVectorArray+2*j+(2*num2+1)*(2*i+2);
				for(j2=0;j2<numjPoints;++j2,++pVertex,++vertexnumber) {

					//Calculate the interpolated point
					pVertex->pos = D3DXVECTOR3(0,0,0);
					for(i3=0;i3<3;++i3) {
						for(j3=0;j3<3;++j3) {
							pVertex->pos += ControlPoint[i3][j3]*(B[i2][i3]*B[j2][j3]);
						}
					}
					pVertex->diffuse = 0x000000FF<<4*((i+j)%5);

					if(!useDWord) {
						for(level=0;level<NumLevels;++level) {
							if( (!(LevelSkipCount[0]%(1<<level)) || j2==numPoints ) 
							 && (!(LevelSkipCount[1]%(1<<level)) || i2==numPoints ) ) {
								*pWordIndex[level][i2/(1<<level)] = vertexnumber;
								++pWordIndex[level][i2/(1<<level)];
								*pWordIndex[level][i2/(1<<level)] = vertexnumber+(1<<level)*(1+num2*numPoints);
								++pWordIndex[level][i2/(1<<level)];
							}
						}
					}
					else {
						for(level=0;level<NumLevels;level++) {
							if( (!(LevelSkipCount[0]%(1<<level)) || j2==numPoints ) 
							 && (!(LevelSkipCount[1]%(1<<level)) || i2==numPoints ) ) {
								*pDWordIndex[level][i2/(1<<level)] = vertexnumber;
								++pDWordIndex[level][i2/(1<<level)];
								*pDWordIndex[level][i2/(1<<level)] = vertexnumber+(1<<level)*(1+num2*numPoints);
								++pDWordIndex[level][i2/(1<<level)];
							}
						}
					}
					++LevelSkipCount[1];
				}
			}
			++LevelSkipCount[0];
		}
		if(!useDWord)
			for(level=0;level<NumLevels;++level) pWordIndex[level][0] += 2*numPoints/(1<<level)*(numPoints/(1<<level)*num2+1)-2*(numPoints/(1<<level)*num2+1);
		else 
			for(level=0;level<NumLevels;++level) pDWordIndex[level][0] += 2*numPoints/(1<<level)*(numPoints/(1<<level)*num2+1)-2*(numPoints/(1<<level)*num2+1);
	}

	for(i=0;i<numPoints;++i) DQDeleteArray(B[i]);
	DQDeleteArray(B);
	if(!useDWord) {
		for(u=0;u<NumLevels;++u) DQDeleteArray(pWordIndex[u]);
		DQDeleteArray(pWordIndex);
	}
	else {
		for(u=0;u<NumLevels;++u) DQDeleteArray(pDWordIndex[u]);
		DQDeleteArray(pDWordIndex);
	}

	d3dcheckOK( VertexBuffer->Unlock() );
	for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Unlock() );
	numVertsPerStrip = num2*numPoints;
	numVertStrips = num1*numPoints;
}*/

/*

  // OLD LoadQ3Patch Implementation without using DQRenderStack

void c_CurvedSurface::LoadQ3Patch(int FaceIndex, int numPoints) {
	int NumLevels;
	D3DFORMAT IndexFormat;
	s_VertexStreamPosNormalDiffuseTexTex *pVertex;
	WORD ***pWordIndex;
	DWORD ***pDWordIndex;
	BOOL useDWord;
	int LevelSkipCount[2],level;
	int i,j,i2,j2,i3,j3,u,vertexnumber;
	c_BSP::s_Vertices *ControlPoint[3];
	D3DXVECTOR3 normal_s, normal_t, Point;
	float **B,**dB; //Bezier coefficients
	float t,dt,OneMinus_t;
	float blendFactor;
	int numjPoints, numiPoints;	//Varies if we are at the end of the series of patches
	c_BSP::s_Faces &Face = DQBSP.Face[FaceIndex].Q3Face;

	int num1 = (Face.patchSize[1]-1)/2;
	int num2 = (Face.patchSize[0]-1)/2;
	c_BSP::s_Vertices *pVertArray = &DQBSP.Vertex[Face.firstVertex];

	MaxDrawLevel = 0;
	if(!(numPoints%2) && numPoints>2) {
		MaxDrawLevel = 1; 
		if(!(numPoints%4) && numPoints>4) {
			MaxDrawLevel = 2;
			if(!(numPoints%8) && numPoints>8) {
				MaxDrawLevel = 3;
				if(!(numPoints%16) && numPoints>16) {
					MaxDrawLevel = 4;
					if(!(numPoints%32) && numPoints>32) {
						MaxDrawLevel = 5;
	}}}}}
	NumLevels = MaxDrawLevel;
	MaxDrawLevel--;

	if(2*(numPoints*num1+1)*(numPoints*num2+1)>65535) {
		IndexFormat = D3DFMT_INDEX32;
		useDWord = TRUE;
	}
	else {
		IndexFormat = D3DFMT_INDEX16;
		useDWord = FALSE;
	}

	if(!useDWord) {
		pWordIndex = (WORD***)DQNewVoid( WORD**[NumLevels] );
		for(level=0;level<NumLevels;++level) {
			pWordIndex[level] = (WORD**)DQNewVoid( WORD*[numPoints+1] );
		}
	}
	else {
		pDWordIndex = (DWORD***)DQNewVoid( DWORD**[NumLevels] );
		for(level=0;level<NumLevels;++level) {
			pDWordIndex[level] = (DWORD**)DQNewVoid( DWORD*[numPoints+1] );
		}
	}

	B = (float**)DQNewVoid( float *[numPoints+1] );
	dB = (float**)DQNewVoid( float *[numPoints+1] );
	for(i=0;i<=numPoints;++i) {
		B[i] = (float*)DQNewVoid( float [3] );
		dB[i] = (float*)DQNewVoid( float [3] );
	}

	dt= 1.0f / (float)numPoints;
	t=0;
	for(i=0;i<numPoints+1;++i) {
		OneMinus_t = 1.0f - t;
		if(i==numPoints) {
			t=1;
			OneMinus_t = 0.0f;
		}

		B[i][0] = OneMinus_t*OneMinus_t;
		B[i][1] = 2*t*OneMinus_t;
		B[i][2] = t*t;
		dB[i][0] = 2*OneMinus_t;
		dB[i][1] = 2-4*t;
		dB[i][2] = 2*t;
		t+=dt;
	}

	if(numPoints<1) numPoints = 1;

	d3dcheckOK( DQDevice->CreateVertexBuffer( (numPoints*num1+1)*(numPoints*num2+1)*sizeof(s_VertexStreamPosNormalDiffuseTexTex), D3DUSAGE_WRITEONLY,
				FVF_PosNormalDiffuseTexTex, D3DPOOL_DEFAULT, &VertexBuffer, NULL ) );
	for(level=0;level<NumLevels;++level) {
		d3dcheckOK( DQDevice->CreateIndexBuffer( 2*(numPoints/(1<<level)*num1+1)*(numPoints/(1<<level)*num2+1)*sizeof(s_VertexStreamPosNormalDiffuseTexTex), D3DUSAGE_WRITEONLY,
					IndexFormat, D3DPOOL_DEFAULT, &IndexBuffer[level], NULL) );
	}
	d3dcheckOK( VertexBuffer->Lock( 0,0, (void**)&pVertex, 0 ) );
	if(!useDWord) {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pWordIndex[level][0]), 0 ) );
	}
	else {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pDWordIndex[level][0]), 0 ) );
	}

	//Bicubic Bezier Patch
	//For each strip
	vertexnumber = 0;
	LevelSkipCount[0] = LevelSkipCount[1] = 0;
	numiPoints = numPoints;
	for(i=0;i<num1;++i) {
		if(i==num1-1) numiPoints = numPoints+1;

		//Set up index buffer array. Each index buffer element points to the row above
		//TODO : switch for order
		for(i3=1;i3<numPoints+1;++i3) {
			if(!useDWord)
				for(u=0;u<NumLevels;++u) pWordIndex[u][i3]=pWordIndex[u][i3-1]+2*(numPoints/(1<<u)*num2+1);
			else
				for(u=0;u<NumLevels;++u) pDWordIndex[u][i3]=pDWordIndex[u][i3-1]+2*(numPoints/(1<<u)*num2+1);
		}

		for(i2=0;i2<numiPoints;++i2) {
			numjPoints = numPoints;

			for(j=0;j<num2;++j) {
				if(j==num2-1) numjPoints = numPoints+1;

				//calculate pointers to control points for this patch
				//ControlPoint[row] = pointer to D3DXVECTOR3 [4]
				ControlPoint[0] = &pVertArray[2*j+(2*num2+1)*2*i];
				ControlPoint[1] = &pVertArray[2*j+(2*num2+1)*(2*i+1)];
				ControlPoint[2] = &pVertArray[2*j+(2*num2+1)*(2*i+2)];
				for(j2=0;j2<numjPoints;++j2,++pVertex,++vertexnumber) {

					//Calculate the interpolated point
					pVertex->Pos = normal_s = normal_t = D3DXVECTOR3(0,0,0);
					pVertex->Tex0 = pVertex->Tex1 = D3DXVECTOR2(0,0);
					for(i3=0;i3<3;++i3) {
						for(j3=0;j3<3;++j3) {
							blendFactor = B[i2][i3]*B[j2][j3];
							Point = MakeD3DXVec3FromFloat3(ControlPoint[i3][j3].position);
							pVertex->Pos += Point*blendFactor;
							pVertex->Tex0 += MakeD3DXVec2FromFloat2(ControlPoint[i3][j3].texCoord)*blendFactor;
							pVertex->Tex1 += MakeD3DXVec2FromFloat2(ControlPoint[i3][j3].lightmapCoord)*blendFactor;
							normal_s += Point*(dB[i2][i3]*B[j2][j3]);
							normal_t += Point*(B[i2][i3]*dB[j2][j3]);
						}
					}
					D3DXVec3Cross( &pVertex->Normal, &normal_s, &normal_t );
					D3DXVec3Normalize( &pVertex->Normal, &pVertex->Normal );
					pVertex->Diffuse = *((DWORD*)&ControlPoint[0][0].colour);

					if(!useDWord) {
						for(level=0;level<NumLevels;++level) {
							if( (!(LevelSkipCount[0]%(1<<level)) || j2==numPoints ) 
							 && (!(LevelSkipCount[1]%(1<<level)) || i2==numPoints ) ) {
								*pWordIndex[level][i2/(1<<level)] = vertexnumber;
								++pWordIndex[level][i2/(1<<level)];
								*pWordIndex[level][i2/(1<<level)] = vertexnumber+(1<<level)*(1+num2*numPoints);
								++pWordIndex[level][i2/(1<<level)];
							}
						}
					}
					else {
						for(level=0;level<NumLevels;level++) {
							if( (!(LevelSkipCount[0]%(1<<level)) || j2==numPoints ) 
							 && (!(LevelSkipCount[1]%(1<<level)) || i2==numPoints ) ) {
								*pDWordIndex[level][i2/(1<<level)] = vertexnumber;
								++pDWordIndex[level][i2/(1<<level)];
								*pDWordIndex[level][i2/(1<<level)] = vertexnumber+(1<<level)*(1+num2*numPoints);
								++pDWordIndex[level][i2/(1<<level)];
							}
						}
					}
					++LevelSkipCount[1];
				}
			}
			++LevelSkipCount[0];
		}
		if(!useDWord)
			for(level=0;level<NumLevels;++level) pWordIndex[level][0] += 2*numPoints/(1<<level)*(numPoints/(1<<level)*num2+1)-2*(numPoints/(1<<level)*num2+1);
		else 
			for(level=0;level<NumLevels;++level) pDWordIndex[level][0] += 2*numPoints/(1<<level)*(numPoints/(1<<level)*num2+1)-2*(numPoints/(1<<level)*num2+1);
	}

	for(i=0;i<numPoints+1;++i) {
		DQDeleteArray(B[i]);
		DQDeleteArray(dB[i]);
	}
	DQDeleteArray(B);
	DQDeleteArray(dB);
	if(!useDWord) {
		for(u=0;u<NumLevels;++u) DQDeleteArray(pWordIndex[u]);
		DQDeleteArray(pWordIndex);
	}
	else {
		for(u=0;u<NumLevels;++u) DQDeleteArray(pDWordIndex[u]);
		DQDeleteArray(pDWordIndex);
	}

	d3dcheckOK( VertexBuffer->Unlock() );
	for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Unlock() );
	numVertsPerStrip = num2*numPoints;
	numVertStrips = num1*numPoints;
	//Save information for drawing
	m_FaceIndex = FaceIndex;
	ShaderHandle = DQBSP.BSPShaderArray[Face.texture];
	LightmapIndex = Face.lightmapIndex;
}*/

//***************************************************

void c_CurvedSurface::RenderObject( int ShaderHandle, c_Texture *LightmapTexture ) 
{
	D3DXVECTOR3 &CameraZDir = DQCamera.CameraZDir;
	float CameraZPos = DQCamera.CameraZPos;
	float Dist;

	Dist = D3DXVec3Dot( &ApproxPos, &CameraZDir ) - CameraZPos - Radius;

	if( Dist < 100.0f ) DrawLevel = 0;
	else if( Dist < 600.0f ) DrawLevel = 1;
	else if( Dist < 1200.0f ) DrawLevel = 2;
	else DrawLevel = 3;

	if(DrawLevel>MaxDrawLevel) DrawLevel = MaxDrawLevel;

	DQRenderStack.DrawBSPIndexedPrimitive( VertexBufferHandle, IndexBufferHandle[DrawLevel], ShaderHandle, D3DPT_TRIANGLESTRIP, 0, 0, NumIndices[DrawLevel]-2, ApproxPos, LightmapTexture );
}
//****************************************************

//Again, as per LoadQuadric patch, but takes values from a BSP Face structure
//Also, interpolates lightmap and normal coords

void c_CurvedSurface::LoadQ3PatchIntoD3D(int FaceIndex, int _numPoints) {
	int NumLevels;
	WORD *pWordIndex[5];		//pWordIndex[Level][IndexPointNum]

	D3DXVECTOR3 *VertexP, *BaseVertexP;
	D3DXVECTOR2 *VertexT1, *BaseVertexT1;
	D3DXVECTOR3 *VertexN, *BaseVertexN;
	DWORD      *VertexD, *BaseVertexD;
	D3DXVECTOR2 *VertexT2, *BaseVertexT2;
	int NumVerts, numPoints[5];

	int level;
	int i,j,i2,j2,i3,j3,u,vertexnum;
	int ControlPoint[3];
	D3DXVECTOR3 normal_s, normal_t, Point;
	float **B,**dB; //Bezier coefficients
	float t,dt,OneMinus_t;
	float blendFactor;
	int numjPoints, numiPoints;	//Varies if we are at the end of the series of patches
	c_BSP::s_Faces &Face = DQBSP.Face[FaceIndex].Q3Face;

	//Calculate the number of 3x3 Quadric patches in each direction
	int num1 = (Face.patchSize[1]-1)/2;
	int num2 = (Face.patchSize[0]-1)/2;

	const int PatchFirstVertex = Face.firstVertex;
	c_BSP::s_DQVertices &Vertices = DQBSP.Vertices;

	if(_numPoints<1) _numPoints = 1;

	MaxDrawLevel = 0;
	numPoints[0] = _numPoints;
	if(!(_numPoints%2) && _numPoints>2) {
		MaxDrawLevel = 1; 
		numPoints[0] = _numPoints;
		if(!(_numPoints%4) && _numPoints>4) {
			MaxDrawLevel = 2;
			numPoints[1] = _numPoints/(1<<1);
			if(!(_numPoints%8) && _numPoints>8) {
				MaxDrawLevel = 3;
				numPoints[2] = _numPoints/(1<<2);
				if(!(_numPoints%16) && _numPoints>16) {
					MaxDrawLevel = 4;
					numPoints[3] = _numPoints/(1<<3);
					if(!(_numPoints%32) && _numPoints>32) {
						MaxDrawLevel = 5;
						numPoints[4] = _numPoints/(1<<4);
	}}}}}
	NumLevels = MaxDrawLevel;
	MaxDrawLevel--;

	B = (float**)DQNewVoid( float *[numPoints[0]+1] );
	dB = (float**)DQNewVoid( float *[numPoints[0]+1] );
	for(i=0;i<=numPoints[0];++i) {
		B[i] = (float*)DQNewVoid( float [3] );
		dB[i] = (float*)DQNewVoid( float [3] );
	}

	dt= 1.0f / (float)numPoints[0];
	t=0;
	for(i=0;i<numPoints[0]+1;++i) {
		OneMinus_t = 1.0f - t;
		if(i==numPoints[0]) {
			t=1;
			OneMinus_t = 0.0f;
		}

		B[i][0] = OneMinus_t*OneMinus_t;
		B[i][1] = 2*t*OneMinus_t;
		B[i][2] = t*t;
		dB[i][0] = 2*OneMinus_t;
		dB[i][1] = 2-4*t;
		dB[i][2] = 2*t;
		t+=dt;
	}

	NumVerts = (numPoints[0]*num1+1)*(numPoints[0]*num2+1);
	VertexP = BaseVertexP = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );
	VertexN = BaseVertexN = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );
	VertexD = BaseVertexD = (DWORD*)DQNewVoid( DWORD[NumVerts] );
	VertexT1 = BaseVertexT1 = (D3DXVECTOR2*)DQNewVoid( D3DXVECTOR2[NumVerts] );
	VertexT2 = BaseVertexT2 = (D3DXVECTOR2*)DQNewVoid( D3DXVECTOR2[NumVerts] );

	for(level=0;level<NumLevels;++level) {
		// NumIndices[level] = 2 * (PointsPerStrip+1) * (Num Strips)
		//NB. Add 2 indices to the end of each strip to move to the next strip without drawing
		NumIndices[level] = 2*(numPoints[level]*num2+2)*(numPoints[level]*num1);
		CriticalAssert( NumIndices[level]<65535);

		pWordIndex[level] = (WORD*)DQNewVoid( WORD[NumIndices[level]] );
	}
/*
	d3dcheckOK( DQDevice->CreateVertexBuffer( (numPoints*num1+1)*(numPoints*num2+1)*sizeof(s_VertexStreamPosNormalDiffuseTexTex), D3DUSAGE_WRITEONLY,
				FVF_PosNormalDiffuseTexTex, D3DPOOL_DEFAULT, &VertexBuffer, NULL ) );
	for(level=0;level<NumLevels;++level) {
		d3dcheckOK( DQDevice->CreateIndexBuffer( 2*(numPoints/(1<<level)*num1+1)*(numPoints/(1<<level)*num2+1)*sizeof(s_VertexStreamPosNormalDiffuseTexTex), D3DUSAGE_WRITEONLY,
					IndexFormat, D3DPOOL_DEFAULT, &IndexBuffer[level], NULL) );
	}
	d3dcheckOK( VertexBuffer->Lock( 0,0, (void**)&pVertex, 0 ) );
	if(!useDWord) {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pWordIndex[level][0]), 0 ) );
	}
	else {
		for(level=0;level<NumLevels;++level) d3dcheckOK( IndexBuffer[level]->Lock( 0,0, (void**)&(pDWordIndex[level][0]), 0 ) );
	}*/

	//Create Verts and Indices for level 0 first. Create different LOD indices afterwards

	//Bicubic Bezier Patch
	//For each strip
	vertexnum = 0;
	int indexpos = 0;
	//PointsPer... is only valid for OUTPUT points
	const int PointsPerStrip = numPoints[0]*num2+1;
	const int PointsPerPatchDir1 = PointsPerStrip*numPoints[0];

	//For each patch
	for(i=0, numiPoints = numPoints[0]; i<num1; ++i) {
		if(i==num1-1) numiPoints = numPoints[0]+1;

		//Set up index buffer array for these strip of patches
		for(i2=0;i2<numPoints[0];++i2) {
		
			for(i3=0;i3<PointsPerStrip;++i3) {
				pWordIndex[0][indexpos++] = i*PointsPerPatchDir1+i2*PointsPerStrip+i3;
				pWordIndex[0][indexpos++] = i*PointsPerPatchDir1+(i2+1)*PointsPerStrip+i3;
			}

			//Repeat last & first index - moves the strip without drawing
			pWordIndex[0][indexpos++] = i*PointsPerPatchDir1+(i2+2)*PointsPerStrip-1;
			pWordIndex[0][indexpos++] = i*PointsPerPatchDir1+(i2+1)*PointsPerStrip;
		}

		for(j=0, numjPoints = numPoints[0]; j<num2; ++j) {
			if(j==num2-1) numjPoints = numPoints[0]+1;

			//calculate pointers to control points for this patch
			//ControlPoint[row] = index of Vertices struct to first in the row of current patch
			ControlPoint[0] = PatchFirstVertex + (i*2)*(2*num2+1) + (j*2);
			ControlPoint[1] = PatchFirstVertex + (i*2+1)*(2*num2+1) + (j*2);
			ControlPoint[2] = PatchFirstVertex + (i*2+2)*(2*num2+1) + (j*2);

			//For each point in the patch
			for(i2=0;i2<numiPoints;++i2) {

				vertexnum = i*PointsPerPatchDir1+i2*PointsPerStrip+j*numPoints[0];

				for(j2=0;j2<numjPoints;++j2,++vertexnum) {

					VertexP = &BaseVertexP[vertexnum];
					VertexN = &BaseVertexN[vertexnum];
					VertexD = &BaseVertexD[vertexnum];
					VertexT1 = &BaseVertexT1[vertexnum];
					VertexT2 = &BaseVertexT2[vertexnum];

					//Calculate the interpolated point
					*VertexP = normal_s = normal_t = D3DXVECTOR3(0,0,0);
					*VertexT1 = *VertexT2 = D3DXVECTOR2(0,0);

					for(i3=0;i3<3;++i3) {
						for(j3=0;j3<3;++j3) {
							blendFactor = B[i2][i3]*B[j2][j3];
							Point = Vertices.Pos[ControlPoint[i3]+j3];
							*VertexP  += Point*blendFactor;
							*VertexT1 += Vertices.TexCoords[ControlPoint[i3]+j3]*blendFactor;
							*VertexT2 += Vertices.LightmapCoords[ControlPoint[i3]+j3]*blendFactor;
							normal_s += Point*(dB[i2][i3]*B[j2][j3]);
							normal_t += Point*(B[i2][i3]*dB[j2][j3]);
						}
					}
					D3DXVec3Cross( VertexN, &normal_s, &normal_t );
					D3DXVec3Normalize( VertexN, VertexN );
					//Don't bother interpolating vertex colour
					*VertexD = Vertices.Diffuse[ControlPoint[0]];

				}
			}
		}
	}
	//Debug check
	Assert( indexpos == NumIndices[0] );

	//Create LOD levels
	for(level=1;level<NumLevels;++level) {
		indexpos = 0;	

		for(i=0; i<num1; ++i) {

			//For each strip
			for(i2=0;i2<numPoints[level];++i2) {
				for(i3=0;i3<num2*numPoints[level];++i3) {
					pWordIndex[level][indexpos++] = i*PointsPerPatchDir1+i2*(1<<level)*PointsPerStrip+i3*(1<<level);
					pWordIndex[level][indexpos++] = i*PointsPerPatchDir1+(i2+1)*(1<<level)*PointsPerStrip+i3*(1<<level);
				}
				//Last vert in strip
				pWordIndex[level][indexpos++] = i*PointsPerPatchDir1+i2*(1<<level)*PointsPerStrip+PointsPerStrip-1;
				pWordIndex[level][indexpos++] = i*PointsPerPatchDir1+(i2+1)*(1<<level)*PointsPerStrip+PointsPerStrip-1;

				//Repeat last & first index - moves the strip without drawing
				pWordIndex[level][indexpos++] = i*PointsPerPatchDir1+(i2+1)*(1<<level)*PointsPerStrip+PointsPerStrip-1;
				pWordIndex[level][indexpos++] = i*PointsPerPatchDir1+(i2+1)*(1<<level)*PointsPerStrip;
			}
		}
		//Debug check
		Assert( indexpos == NumIndices[level] );
	}

	//Put Verts and Indices in Static Render Stack
	VertexBufferHandle = DQRenderStack.AddStaticVertices( BaseVertexP, BaseVertexT1, BaseVertexN, BaseVertexD, BaseVertexT2, NumVerts );
	for(level=0;level<NumLevels;++level) {
		IndexBufferHandle[level] = DQRenderStack.AddStaticIndices( pWordIndex[level], NumIndices[level] );
	}

	for(i=0;i<numPoints[0]+1;++i) {
		DQDeleteArray(B[i]);
		DQDeleteArray(dB[i]);
	}
	DQDeleteArray(B);
	DQDeleteArray(dB);

	for(u=0;u<NumLevels;++u) DQDeleteArray(pWordIndex[u]);

	DQDeleteArray(BaseVertexP);
	DQDeleteArray(BaseVertexN);
	DQDeleteArray(BaseVertexD);
	DQDeleteArray(BaseVertexT1);
	DQDeleteArray(BaseVertexT2);

	numVertsPerStrip = num2*numPoints[0];
	numVertStrips = num1*numPoints[0];

	//Save information for drawing
	m_FaceIndex = FaceIndex;

	D3DXVECTOR3 Min, Max, *pVert;

	Max = D3DXVECTOR3(0,0,0);
	Min = D3DXVECTOR3( 999999, 999999, 999999 );

	for(i=0; i<DQBSP.Face[FaceIndex].Q3Face.numVertex; ++i) {

		pVert = &DQBSP.Vertices.Pos[DQBSP.Face[FaceIndex].Q3Face.firstVertex+i];
		Max.x = max( Max.x, pVert->x );
		Max.y = max( Max.y, pVert->y );
		Max.z = max( Max.z, pVert->z );
		Min.x = min( Min.x, pVert->x );
		Min.y = min( Min.y, pVert->y );
		Min.z = min( Min.z, pVert->z );
	}
	ApproxPos = 0.5f*(Max+Min);

	Max -= ApproxPos;
	Radius = D3DXVec3Length( &Max );
}

//********* c_BSP Stuff *************

//Creates a curved surface brush
//Tesselate the curved surface into triangles, store the plane of the triangle and its neighbours
//Collide against each tri plane and test against neighbours

// Normal to triangle = N
// Edge1 = e1, Edge2 = e2
// N = (e1 x e2)/|N|
//
// EdgeNormal1 = n1, EdgeNormal2 = n2
// x' = CollisionPoint - Vertex0
// x' = a.e1 + b.e2			(Barycentric coords)
// x'.n1 = b e2.n1
// x'.n2 = a e1.n2
// Let n1 = (N x e1)/(n1.e2)
// Therefore x'.n1 = b

//First part is based on LoadQ3PatchIntoD3D
void c_BSP::CreateCurveBrush( int FaceIndex, int CBIndex )
{
	WORD *pIndex;
	int NumIndices;		//NumIndices is used for drawing
	s_CurveBrushPoint *tri;

	D3DXVECTOR3 *VertexP, *BaseVertexP;
	int NumVerts, numPoints;

	int i,j,i2,j2,i3,j3,vertexnum;
	int ControlPoint[3];
	float B[5][3],dB[5][3]; //Bezier coefficients
	float t,dt,OneMinus_t;
	float blendFactor;
	int numjPoints, numiPoints;	//Varies if we are at the end of the series of patches

	numPoints = 4;		//Tesselate to 4 points per strip

	c_BSP::s_Faces &Face = DQBSP.Face[FaceIndex].Q3Face;

	CurveBrush[CBIndex].FirstBrushPoint = NumCurveBrushPoints;

	//Calculate the number of 3x3 Quadric patches in each direction
	int num1 = (Face.patchSize[1]-1)/2;
	int num2 = (Face.patchSize[0]-1)/2;

	const int PatchFirstVertex = Face.firstVertex;
	c_BSP::s_DQVertices &Vertices = DQBSP.Vertices;

	dt= 1.0f / (float)numPoints;
	t=0;
	for(i=0;i<numPoints+1;++i) {
		OneMinus_t = 1.0f - t;
		if(i==numPoints) {
			t=1;
			OneMinus_t = 0.0f;
		}

		B[i][0] = OneMinus_t*OneMinus_t;
		B[i][1] = 2*t*OneMinus_t;
		B[i][2] = t*t;
		dB[i][0] = 2*OneMinus_t;
		dB[i][1] = 2-4*t;
		dB[i][2] = 2*t;
		t+=dt;
	}

	NumVerts = (numPoints*num1+1)*(numPoints*num2+1);
	VertexP = BaseVertexP = (D3DXVECTOR3*)DQNewVoid( D3DXVECTOR3[NumVerts] );

	NumIndices = 2*(numPoints*num2+2)*(numPoints*num1);
	CriticalAssert( NumIndices<65535);

	pIndex = (WORD*)DQNewVoid( WORD[NumIndices] );

	//Bicubic Bezier Patch
	//For each strip
	vertexnum = 0;
	int indexpos = 0;
	//PointsPer... is only valid for OUTPUT points
	const int PointsPerStrip = numPoints*num2+1;
	const int PointsPerPatchDir1 = PointsPerStrip*numPoints;

	//For each patch
	for(i=0, numiPoints = numPoints; i<num1; ++i) {
		if(i==num1-1) numiPoints = numPoints+1;

		//Set up index buffer
		for(i2=0;i2<numPoints;++i2) {
		
			for(i3=0;i3<PointsPerStrip;++i3) {
				pIndex[indexpos++] = i*PointsPerPatchDir1+i2*PointsPerStrip+i3;
				pIndex[indexpos++] = i*PointsPerPatchDir1+(i2+1)*PointsPerStrip+i3;
			}

			//Repeat last & first index - moves the strip without drawing
			pIndex[indexpos++] = i*PointsPerPatchDir1+(i2+2)*PointsPerStrip-1;
			pIndex[indexpos++] = i*PointsPerPatchDir1+(i2+1)*PointsPerStrip;
		}

		for(j=0, numjPoints = numPoints; j<num2; ++j) {
			if(j==num2-1) numjPoints = numPoints+1;

			//calculate pointers to control points for this patch
			//ControlPoint[row] = index of Vertices struct to first in the row of current patch
			ControlPoint[0] = PatchFirstVertex + (i*2)*(2*num2+1) + (j*2);
			ControlPoint[1] = PatchFirstVertex + (i*2+1)*(2*num2+1) + (j*2);
			ControlPoint[2] = PatchFirstVertex + (i*2+2)*(2*num2+1) + (j*2);

			//For each point in the patch
			for(i2=0;i2<numiPoints;++i2) {

				vertexnum = i*PointsPerPatchDir1+i2*PointsPerStrip+j*numPoints;

				for(j2=0;j2<numjPoints;++j2,++vertexnum) {

					VertexP = &BaseVertexP[vertexnum];

					//Calculate the interpolated point
					*VertexP = D3DXVECTOR3(0,0,0);

					for(i3=0;i3<3;++i3) {
						for(j3=0;j3<3;++j3) {
							blendFactor = B[i2][i3]*B[j2][j3];
							*VertexP  += Vertices.Pos[ControlPoint[i3]+j3]*blendFactor;
						}
					}
				}
			}
		}
	}

	//Debug check
	Assert( indexpos == NumIndices );
	Assert( vertexnum == NumVerts );

	D3DXVECTOR3 Edge1, Edge2, x;
	D3DXVECTOR3 Min, Max, *pVert;
#if _DEBUG
	float temp;
#endif

	//Create tri list from Tri Strip
	Max = D3DXVECTOR3( -999999, -999999, -999999 );
	Min = D3DXVECTOR3( 999999, 999999, 999999 );
	
	VertexP = BaseVertexP;

	for(i=0; i<NumIndices-2; ++i) {
		
		pVert = &VertexP[pIndex[i]];

		Max.x = max( Max.x, pVert->x );
		Max.y = max( Max.y, pVert->y );
		Max.z = max( Max.z, pVert->z );
		Min.x = min( Min.x, pVert->x );
		Min.y = min( Min.y, pVert->y );
		Min.z = min( Min.z, pVert->z );

		if( (pIndex[i]==pIndex[i+1]) || (pIndex[i]==pIndex[i+2]) || (pIndex[i+1]==pIndex[i+2]) ) continue;

		//Normal is CW for even starting points, CCW for odd starting points
		Edge1 = VertexP[pIndex[i+1]] - VertexP[pIndex[i]];
		Edge2 = VertexP[pIndex[i+2]] - VertexP[pIndex[i]];
		tri = &CurveBrushPoints[NumCurveBrushPoints++];
		tri->FirstVert = VertexP[pIndex[i]];
		
		if( i%2 ) {
			//odd == CCW
			D3DXVec3Cross( &tri->Normal, &Edge1, &Edge2 );
		}
		else {
			//even == CW
			D3DXVec3Cross( &tri->Normal, &Edge2, &Edge1 );
		}
		if( D3DXVec3LengthSq( &tri->Normal ) < 0.001f ) {
			--NumCurveBrushPoints;
			continue;
		}
		D3DXVec3Normalize( &tri->Normal, &tri->Normal );
		tri->PlaneDist = D3DXVec3Dot( &tri->FirstVert, &tri->Normal );	

		D3DXVec3Cross( &tri->EdgeNormal1, &tri->Normal, &Edge1 );
		tri->EdgeNormal1 = tri->EdgeNormal1 / D3DXVec3Dot( &tri->EdgeNormal1, &Edge2 );
		D3DXVec3Cross( &tri->EdgeNormal2, &tri->Normal, &Edge2 );
		tri->EdgeNormal2 = tri->EdgeNormal2 / D3DXVec3Dot( &tri->EdgeNormal2, &Edge1 );

#if _DEBUG
		temp =  D3DXVec3Dot( &tri->EdgeNormal1, &Edge2 );
		Assert( square( temp-1 ) <EPSILON );
		temp =  D3DXVec3Dot( &tri->EdgeNormal2, &Edge1 );
		Assert( square( temp-1 ) <EPSILON );
#endif
	}
	for(; i<NumIndices; ++i) {
		
		pVert = &VertexP[pIndex[i]];

		Max.x = max( Max.x, pVert->x );
		Max.y = max( Max.y, pVert->y );
		Max.z = max( Max.z, pVert->z );
		Min.x = min( Min.x, pVert->x );
		Min.y = min( Min.y, pVert->y );
		Min.z = min( Min.z, pVert->z );
	}
	
	CurveBrush[CBIndex].ApproxPos = 0.5f*(Max+Min);

	Max -= CurveBrush[CBIndex].ApproxPos;
	CurveBrush[CBIndex].Radius = D3DXVec3Length( &Max );

	CurveBrush[CBIndex].NumBrushPoints = NumCurveBrushPoints - CurveBrush[CBIndex].FirstBrushPoint;

	//Cleanup
	DQDeleteArray(pIndex);
	DQDeleteArray(BaseVertexP);
}

//Test the TraceBox against the triangles
void c_BSP::CheckCurveBrush( s_Trace &Trace, s_CurveBrush *CurveBrush )
{
	int i;
	s_CurveBrushPoint *tri;
	float BoxDistOffset, TempFraction, StartDistance, EndDistance, MinSepDist;
	float alpha, beta, LengthSq;
	D3DXVECTOR3 offset, CollisionPoint, Separation;

	//Shortest distance from a point to a line
	// t = D.(P-S) / D^2
	Separation = CurveBrush->ApproxPos - Trace.MidpointStart;
	LengthSq = D3DXVec3LengthSq( &Trace.Direction );
	if( LengthSq != 0.0f ) {
		TempFraction = D3DXVec3Dot( &Trace.Direction, &Separation ) / LengthSq;
		TempFraction = min( 1.0f, max( 0.0f, TempFraction ) );
		Separation = CurveBrush->ApproxPos - ( Trace.MidpointStart + TempFraction * Trace.Direction );
	}
	//Quick test
	MinSepDist = CurveBrush->Radius + Trace.BoxRadius + EPSILON;
	if( D3DXVec3LengthSq( &Separation ) > square(MinSepDist) ) return;

	//Quick test against sphere enclosing trace
//	Separation = CurveBrush->ApproxPos - Trace.EnclosingSphere.centre;
//	if( D3DXVec3LengthSq( &Separation ) > square(CurveBrush->Radius + Trace.EnclosingSphere.radius ) ) return;

	for( i=0; i<CurveBrush->NumBrushPoints; ++i ) {
		tri = &CurveBrushPoints[CurveBrush->FirstBrushPoint+i];

		if( !Trace.TracePointOnly ) {
			offset.x = (tri->Normal.x<0) ? -Trace.BoxExtent.x : Trace.BoxExtent.x;
			offset.y = (tri->Normal.y<0) ? -Trace.BoxExtent.y : Trace.BoxExtent.y;
			offset.z = (tri->Normal.z<0) ? -Trace.BoxExtent.z : Trace.BoxExtent.z;

			BoxDistOffset = D3DXVec3Dot( &offset, &tri->Normal );
		}
		else {
			offset = D3DXVECTOR3( 0,0,0 );
			BoxDistOffset = 0;
		}

		StartDistance = D3DXVec3Dot( &Trace.MidpointStart, &tri->Normal ) - tri->PlaneDist - BoxDistOffset;
		EndDistance = D3DXVec3Dot( &Trace.MidpointEnd, &tri->Normal ) - tri->PlaneDist - BoxDistOffset;

		if( (StartDistance>0.0f) && (EndDistance>0.0f) ) continue;	//no collision
		if( (StartDistance<=0.0f) && (EndDistance<=0.0f) ) continue;	//no collision

		//Get the collision fraction for the ray, and test against the triangle
		TempFraction = StartDistance / ( StartDistance - EndDistance );
		if(TempFraction<0.0f || TempFraction>1.0f) {
			continue;
		}
		
		//Collision point is relative to tri->FirstVert
		CollisionPoint = Trace.MidpointStart - tri->FirstVert + TempFraction*(Trace.Direction) - offset;
		//Assert that CollisionPoint is in plane of triangle
//		alpha = D3DXVec3Dot( &CollisionPoint, &tri->Normal ) - tri->PlaneDist;
//		Assert( square(alpha)<EPSILON );
//		CollisionPoint -= tri->FirstVert;
		
		//Get Barycentric coords
		alpha = D3DXVec3Dot( &CollisionPoint, &tri->EdgeNormal2 );
		beta = D3DXVec3Dot( &CollisionPoint, &tri->EdgeNormal1 );

		if( (alpha<-EPSILON) || (beta<-EPSILON) || (alpha+beta)>(1+EPSILON) ) continue;   //no collision with triangle

/*		if( (StartDistance<0.0f) && (StartDistance>-2*BoxDistOffset) ) {
			Trace.StartSolid = TRUE;
			if( (EndDistance<0.0f) && (EndDistance>-2*BoxDistOffset) ) {
				Trace.AllSolid = TRUE;
			}
		}*/

		//if trace is entering brushside
		if( StartDistance>EndDistance ) {
			//Get collision fraction for box
			//Set collision fraction slightly infront of plane
			TempFraction = (StartDistance - EPSILON)/( StartDistance - EndDistance );
			TempFraction = max( 0.0f, min( 1.0f, TempFraction ) );
			if(TempFraction>Trace.CollisionFraction) continue;
		}
		else {
			continue;
		}

		//trace leaving brushside
/*		else if ( StartDistance<EndDistance ) {
			//Get collision fraction for box
			//Set collision fraction slightly infront of plane
			TempFraction = (StartDistance + BoxDistOffset + EPSILON)/( StartDistance - EndDistance );
			TempFraction = max( 0.0f, min( 1.0f, TempFraction ) );
			if(TempFraction>Trace.CollisionFraction) continue;
		}
		
		//StartDistance == EndDistance
		else {
			continue;
			TempFraction = 0.0f;
			if(TempFraction>Trace.CollisionFraction) continue;
		}*/

		//Collision occurred		
		Trace.CollisionFraction = TempFraction;
		MakeFloat3FromD3DXVec3( Trace.Plane.normal, tri->Normal );
		Trace.Plane.dist = tri->PlaneDist;
		Trace.SurfaceFlags	= CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
		Trace.contents		= 0;
		Trace.EntityNum		= ENTITYNUM_WORLD;
	}
}