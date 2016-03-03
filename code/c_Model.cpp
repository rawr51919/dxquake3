// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Model.cpp: implementation of the c_Model class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Model.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Model::c_Model():isDuplicate(FALSE)
{
	ModelFilename[0] = '\0';
	NumLOD = 0;
	Skin = 0;
	for(int i=0;i<MAX_MODEL_LOD;++i) MD3[i] = NULL;
	isInlineModel = FALSE;
	InlineModelIndex = 0;
	NextRenderInstance = NULL;
	FrameLastUpdated = -1;
}

c_Model::c_Model(c_Model *Original):isDuplicate(TRUE)
{
	DQstrcpy( ModelFilename, Original->ModelFilename, MAX_QPATH );
	DQstrcpy( Shader, Original->Shader, MAX_QPATH );
	NumLOD = Original->NumLOD;
	//Skin = Original->Skin;
	for(int i=0;i<MAX_MODEL_LOD;++i) {
		if(Original->MD3[i]) 
			MD3[i] = (c_MD3*)DQNewVoid( c_MD3(Original->MD3[i]) );
		else MD3[i] = NULL;
	}
	isInlineModel = Original->isInlineModel;
	InlineModelIndex = Original->InlineModelIndex;
	NextRenderInstance = NULL;
	FrameLastUpdated = -1;
	CurrentLOD = MD3[0];
}

c_Model::~c_Model()
{
	if(!isInlineModel) {
		for(int i=0;i<=NumLOD;++i) DQDelete(MD3[i]);
	}

	DQDelete( NextRenderInstance );
}

BOOL c_Model::LoadModel(const char *File)
{
	char Filename[MAX_QPATH];
	FILEHANDLE Handle;

	//Copy to member var
	DQstrcpy(ModelFilename, File, MAX_QPATH);

	//Copy to modifiable var
	DQstrcpy(Filename, File, MAX_QPATH);

	//Open the first LOD model
	Handle = DQFS.OpenFile(Filename, "rb");
	if(!Handle) {
//		Error(ERR_RENDER, va("Could not open model %s\n",Filename) );
		return FALSE;
	}
	MD3[0] = DQNew( c_MD3 );
	MD3[0]->LoadMD3(Handle);
	DQFS.CloseFile(Handle);

	NumLOD = 0;
	CurrentLOD = MD3[0];

	//Find and open other LOD models if they exist
	for(int i=1;i<MAX_MODEL_LOD;++i) {
		DQStripExtention(Filename, MAX_QPATH);
		DQstrcpy( Filename, va("%s_1.md3",Filename), MAX_QPATH);

		Handle = DQFS.OpenFile(Filename, "rb");
		if(Handle) {
			NumLOD = i;
			MD3[i] = DQNew( c_MD3 );
			MD3[i]->LoadMD3(Handle);
			DQFS.CloseFile(Handle);
		}
		else break;
	}
	
	return TRUE;
}

void c_Model::MakeInlineModel( int InlineModel )
{
	NumLOD = 0;
	InlineModelIndex = InlineModel;
	isInlineModel = TRUE;
}

void c_Model::LoadMD3IntoD3D(int ThisFrame, int NextFrame, float Fraction)
{
	if(isInlineModel) return;

	FrameLastUpdated = DQRender.FrameNum;

	CurrentLOD->LoadMD3IntoD3D(ThisFrame, NextFrame, Fraction);
}

BOOL c_Model::LerpTag( orientation_t *tag, int ThisFrame, int NextFrame, float Fraction, const char *TagName)
{
	if(isInlineModel) return FALSE;

	int i;

	i = CurrentLOD->GetTagNum(TagName);
	if(i==-1) {
		return FALSE;
	}
	CurrentLOD->InterpolateTag(tag, i, ThisFrame, NextFrame, Fraction);
	return TRUE;
}

void c_Model::SetSkin( int SkinHandle )
{
	c_MD3 *md3;
	int i;

	if(SkinHandle == Skin || SkinHandle==0 || isInlineModel) return;

	for(i=0;i<=NumLOD;++i) {
		md3 = MD3[i];
		if(!md3) break;
		md3->SetSkin( DQRender.GetSkin(SkinHandle) );
	}

	Skin = SkinHandle;
}

void c_Model::GetQ3ModelBounds( float mins[3], float maxs[3] )
{
	D3DXVECTOR3 Min, Max;

	GetModelBounds( Min, Max );

	MakeFloat3FromD3DXVec3( mins, Min );
	MakeFloat3FromD3DXVec3( maxs, Max );
}

void c_Model::GetModelBounds( D3DXVECTOR3 &Min, D3DXVECTOR3 &Max )
{
	if(isInlineModel) {
		DQBSP.GetInlineModelBounds( InlineModelIndex, Min, Max );
	}
	else {
		CurrentLOD->GetModelBounds( Min, Max );
	}
}

void c_Model::SetCurrentLODModel( D3DXMATRIX *pTM )
{
	//It's quicker to not use LODs
	return;

	//NB. This uses the previous frame's camera orientation
	if( NumLOD < 1 ) {
		CurrentLOD = MD3[0];
		return;
	}

	D3DXVECTOR3 ApproxPos = D3DXVECTOR3( pTM->_41, pTM->_42, pTM->_43 );
	D3DXVECTOR3 &CameraZDir = DQCamera.CameraZDir;
	float CameraZPos = DQCamera.CameraZPos;
	float Dist;

	Dist = D3DXVec3Dot( &ApproxPos, &CameraZDir ) - CameraZPos;

	if( Dist < 400.0f ) CurrentLOD = MD3[0];
	if( NumLOD>=2 ) {
		if( Dist < 1000.0f ) CurrentLOD = MD3[1];
		else CurrentLOD = MD3[2];
	}
	else CurrentLOD = MD3[1];
}

c_Model * c_Model::GetNextRenderInstance()
{
	c_Model *Model, *NextModel;

	if(FrameLastUpdated<DQRender.FrameNum) return this;

	Model = this;

	for(int i=0;i<30000;++i) {					//prevent infinite loop
		NextModel = Model->NextRenderInstance;
		if(!NextModel) {
			Model->NextRenderInstance = (c_Model*)DQNewVoid( c_Model(this) );
			return Model->NextRenderInstance;
		}
		if(NextModel->FrameLastUpdated<DQRender.FrameNum) return NextModel;
		Model = NextModel;
	}

	CriticalError(ERR_MISC, "Too many render model instances");
	return this;
}

void c_Model::DrawModel( s_RenderFeatureInfo *RenderFeature )
{
	c_MD3 *OldMD3;

	if(isInlineModel) {
		DQBSP.RenderInlineModel( InlineModelIndex );
	}
	else {
		OldMD3 = CurrentLOD;
//		SetCurrentLODModel( RenderFeature->pTM );
		OldMD3->DrawMD3( RenderFeature );
	}
	
}
