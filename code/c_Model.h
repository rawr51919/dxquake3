// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Model.h: interface for the c_Model class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_MODEL_H__E049BF43_CE00_410F_AD36_4A4B45D958D1__INCLUDED_)
#define AFX_C_MODEL_H__E049BF43_CE00_410F_AD36_4A4B45D958D1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_MODEL_LOD 3

class c_MD3;

class c_Model
{
public:
	c_Model();
	c_Model(c_Model *Original);
	virtual ~c_Model();

	BOOL LoadModel(const char *Filename);
	void MakeInlineModel( int InlineModel );
	void LoadMD3IntoD3D(int ThisFrame, int NextFrame, float Fraction);
	void SetSkin( int SkinHandle );
	void DrawModel( s_RenderFeatureInfo *RenderFeature );

	BOOL LerpTag( orientation_t *tag, int ThisFrame, int NextFrame, float Fraction, const char *TagName);
	c_Model * GetNextRenderInstance();
	void c_Model::GetQ3ModelBounds( float mins[3], float maxs[3] );
	void c_Model::GetModelBounds( D3DXVECTOR3 &Min, D3DXVECTOR3 &Max );
	inline int GetInlineModelIndex() { return (isInlineModel)?InlineModelIndex:0; }

	char ModelFilename[MAX_QPATH];
	int FrameLastUpdated;
private:
	void SetCurrentLODModel( D3DXMATRIX *pTM );
	c_MD3 *CurrentLOD;

	int NumLOD;
	char Shader[MAX_QPATH];
	c_MD3 *MD3[MAX_MODEL_LOD];		//Different LODs
	int InlineModelIndex;
	int Skin;
	BOOL isInlineModel;
	c_Model *NextRenderInstance;
	const BOOL isDuplicate;
};

#endif // !defined(AFX_C_MODEL_H__E049BF43_CE00_410F_AD36_4A4B45D958D1__INCLUDED_)
