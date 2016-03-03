// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	c_Render2.cpp : Registering Only
//

#include "stdafx.h"

int c_Render::RegisterModel(const char *Filename)
{
	c_Model *Model;
	int i;

	if(NumModels>=MAX_MODELS) {
		Error( ERR_RENDER, "Max models reached" );
		return 0;
	}

	//Check if model has been registered before
	for(i=0;i<NumModels;++i) {
		if(!DQstrcmpi(ModelArray[i]->ModelFilename, Filename, MAX_QPATH))
			return i+1;
	}

	Model = DQNew( c_Model );
	ModelArray[NumModels] = Model;

	//if it is an inline model
	if(Filename[0]=='*') {
		DQstrcpy(Model->ModelFilename, Filename, MAX_QPATH);
		i = atoi(&Filename[1]);
		Model->MakeInlineModel( i );
		DQBSP.RegisterInlineModelHandle( i, NumModels+1 );

		++NumModels;
		return NumModels;				//Handle is ArrayIndex+1
	}

	if( Model->LoadModel(Filename) ) {
		++NumModels;
		return NumModels;				//Handle is ArrayIndex+1
	}
	else {
		DQDelete( Model );
		return 0;
	}
}

void c_Render::DrawSprite(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int hShader )
{
	static DWORD Diffuse[6];
	static D3DXVECTOR3 Pos[6];
	static D3DXVECTOR2 Tex[6];
	static count = 0;

	if( !r_RenderScene.integer ) return;

	if(CurrentSpriteInfo.NextOffset>=MAX_SPRITES*6) {

		Error(ERR_RENDER, "Too many sprites");
		return;
	}

	//Rescale Screen coords and clamp to screen
	x = 2*x/GLconfig.vidWidth - 1;
	w = 2*w/GLconfig.vidWidth;
	y = - 2*y/GLconfig.vidHeight + 1;
	h = 2*h/GLconfig.vidHeight;
	x = min( 1.0f, max( -1.0f, x ) );
	y = min( 1.0f, max( -1.0f, y ) );
	if( x+w > 1.0f ) w = 1.0f - x;
	if( y-h < -1.0f ) h = 1.0f + y;

	//Create a rectangle at the front of the screen
	Pos[0].x = x; Pos[0].y = y; Pos[0].z = 0.01f;
	Tex[0].x = s1; Tex[0].y = t1;
	
	Pos[1].x = x+w; Pos[1].y = y; Pos[1].z = 0.01f;
	Tex[1].x = s2; Tex[1].y = t1;

	Pos[2].x = x; Pos[2].y = y-h; Pos[2].z = 0.01f;
	Tex[2].x = s1; Tex[2].y = t2;

	//2nd Triangle
	Pos[3].x = x+w; Pos[3].y = y; Pos[3].z = 0.01f;
	Tex[3].x = s2; Tex[3].y = t1;

	Pos[4].x = x+w; Pos[4].y = y-h; Pos[4].z = 0.01f;
	Tex[4].x = s2; Tex[4].y = t2;

	Pos[5].x = x; Pos[5].y = y-h; Pos[5].z = 0.01f;
	Tex[5].x = s1; Tex[5].y = t2;

	Diffuse[0] = Diffuse[1] = Diffuse[2] = Diffuse[3] = Diffuse[4] = Diffuse[5] = CurrentSpriteInfo.Colour;
	
	//Hack for the non in-game menu to stop the RenderStack sorting sprites
	//This fixes the Multiplayer Map selector menu
	if( !DQClientGame.isLoaded ) {
		SpriteRenderInfo.ShaderRGBA = count++;			//random number
	}

	DQRenderStack.UpdateDynamicVerts( SpriteVBhandle, CurrentSpriteInfo.NextOffset, 6, Pos, Tex, NULL, Diffuse, NULL, 0 );

	//Draw previous sprites if shader changed
	if( (hShader != CurrentSpriteInfo.CurrentShader) || (CurrentSpriteInfo.Colour != CurrentSpriteInfo.LastColour) ) {
		RenderSpriteCache();

		CurrentSpriteInfo.CurrentShader = hShader;
		CurrentSpriteInfo.LastColour = CurrentSpriteInfo.Colour;
	}

	CurrentSpriteInfo.NextOffset += 6;
}

void c_Render::RenderSpriteCache()
{
	int PrimCount;

	PrimCount = (CurrentSpriteInfo.NextOffset-CurrentSpriteInfo.StartOffset)/3;
	if(PrimCount>0) {
		DQRenderStack.DrawPrimitive( SpriteVBhandle, CurrentSpriteInfo.CurrentShader, D3DPT_TRIANGLELIST, CurrentSpriteInfo.StartOffset, PrimCount, D3DXVECTOR3(0,0,0), &SpriteRenderInfo );
	}
	CurrentSpriteInfo.StartOffset = CurrentSpriteInfo.NextOffset;
}

void c_Render::LoadShaderFile(FILEHANDLE Handle)
{
	int pos;
	int FileLength = DQFS.GetFileLength(Handle);
	char *buffer;

	buffer = (char*) DQNewVoid( char [FileLength+1] );
	DQFS.ReadFile(buffer, sizeof(char), FileLength, Handle);
	buffer[FileLength] = '\0';

	pos = 0;
	pos += DQSkipWhitespace(&buffer[pos], FileLength);
	while(pos<FileLength) {
		ShaderArray[NumShaders] = DQNew( c_Shader );
		pos += ShaderArray[NumShaders]->LoadShader(&buffer[pos]);
		pos += DQSkipWhitespace(&buffer[pos], FileLength);
		++NumShaders;
	}

	DQDeleteArray(buffer);
}

int c_Render::RegisterShader(const char *ShaderName, int AdditionalFlags)
{
	char Filename[MAX_QPATH];
	int i,LastFoundShader=-1;

	DQstrcpy(Filename, ShaderName, MAX_QPATH);
	DQStripGfxExtention(Filename, MAX_QPATH);

	//LightningBoltNew is Q3-Engine dependant (doesn't have a shader)
	if(DQstrcmpi( Filename, "lightningBoltNew", MAX_QPATH )==0) {
		DQstrcpy( Filename, "lightningBolt", MAX_QPATH );
	}

	//Find Shader from loaded shaders
	for(i=0;i<NumShaders;++i) {
		if( !DQstrcmpi(Filename, ShaderArray[i]->Filename, MAX_QPATH)) {
			//Shader found

			//Special case : sky shader
			if( ShaderArray[i]->GetFlags() & ShaderFlag_SP( Shader_surfaceParam::Sky ) ) {
				ShaderArray[NumShaders] = (c_Shader*)DQNewVoid( c_Shader(ShaderArray[i], AdditionalFlags | ShaderFlag_SkyboxShader) );
				i = NumShaders;
				NumShaders++;
				if(!ShaderArray[i]->isInitialised) ShaderArray[i]->Initialise();
				if(!DQBSP.CloudShaderHandle) DQBSP.CloudShaderHandle = i+1;
				//Set BSP Sky shader to a zero pass Shader (so ignore original sky geometry)
				return ZeroPassShader;
			}

			//Check to see if this shader's flags match the flags we want
			if(!AdditionalFlags || (ShaderArray[i]->GetFlags()&AdditionalFlags)==AdditionalFlags)
			{
				if(!ShaderArray[i]->isInitialised) ShaderArray[i]->Initialise();
				return i+1;						//Handles are ArrayIndex+1
			}
			//if not, make a reference and keep searching
			LastFoundShader = i;
		}
	}
	//if we found a shader, but with different flags, make a copy of it with new flags
	if(LastFoundShader!=-1) {
		if(NumShaders>=MAX_SHADERS) Error(ERR_MISC, "Max Shaders Reached");
		ShaderArray[NumShaders] = (c_Shader*)DQNewVoid( c_Shader(ShaderArray[LastFoundShader], AdditionalFlags) );
		i = NumShaders;
		NumShaders++;
		if(!ShaderArray[i]->isInitialised) ShaderArray[i]->Initialise();
		return i+1;						//Handles are ArrayIndex+1
	}

	//Create a new Shader containing Shader_map (texture)
	if(NumShaders>=MAX_SHADERS) Error(ERR_MISC, "Max Shaders Reached");
	ShaderArray[NumShaders] = DQNew( c_Shader );

	if( AdditionalFlags & ShaderFlag_BSPShader ) {
		//Create new shader from file, using rgbgen lightingDiffuse and a lightmap stage
		ShaderArray[NumShaders]->LoadShader(va("%s\n {\n {\n map %s.tga\n rgbgen lightingDiffuse\n}\n {\n map $lightmap\n blendfunc filter\n}\n}\n",Filename,Filename));
		AdditionalFlags |= ShaderFlag_UseLightmap;
	}
	else {
		//Create new shader from file, using rgbgen identityLighting
		ShaderArray[NumShaders]->LoadShader(va("%s\n {\n {\n map %s.tga\n rgbgen identityLighting\n } \n}\n",Filename,Filename));
	}
	ShaderArray[NumShaders]->SetFlags(AdditionalFlags);
	ShaderArray[NumShaders]->Initialise();
	NumShaders++;
	return NumShaders;
}

//as per RegisterModel
int c_Render::RegisterSkin(const char *Filename)
{
	c_Skin *Skin;
	int i;

	if(NumSkins>=MAX_SKINS) {
		Error( ERR_RENDER, "Max skins reached" );
		return 0;
	}

	//Check if skin has been registered before
	for(i=0;i<NumSkins;++i) {
		if(!DQstrcmpi(SkinArray[i]->SkinFilename, Filename, MAX_QPATH))
			return i+1;
	}

	Skin = DQNew( c_Skin );
	SkinArray[NumSkins] = Skin;

	Skin->LoadSkin(Filename);
	++NumSkins;
	return NumSkins;
}

void c_Render::AddLight( D3DXVECTOR3 origin, float radius, D3DCOLORVALUE colour )
{
	if(NumLights>=MAX_LIGHTS) {
#if _DEBUG
		Error(ERR_RENDER, "Too many lights");
#endif
		return;
	}
	D3DLIGHT9 *Light;

	Light = &LightArray[NumLights];

	ZeroMemory( Light, sizeof(D3DLIGHT9) );
	Light->Type = D3DLIGHT_POINT;
	Light->Ambient = D3DXCOLOR( colour.r/2.0f, colour.g/2.0f, colour.b/2.0f, 1.0f );
	Light->Diffuse = colour;
	Light->Position = origin;
	Light->Range = radius;
	Light->Attenuation0 = 4.0f;
	Light->Attenuation1 = 1.0f/radius;
	Light->Attenuation2 = 1.0f/square(radius);
	
	d3dcheckOK( device->SetLight( NumLights, &LightArray[NumLights] ) );
	NumLights++;
}

void c_Render::DrawRefEntity(refEntity_t *refEntity)
{
	c_Model *Model;
	s_RenderFeatureInfo RenderFeature;

	if( !r_RenderScene.integer ) return;

	//Only draw RF_THIRD_PERSON entities if we're in Draw Third Person Mode
	if(refEntity->renderfx & RF_THIRD_PERSON) {
		if(!(DQCamera.renderfx & RF_THIRD_PERSON)) 
			return;
	}

	if(refEntity->reType == RT_SPRITE) {
		s_Billboard Billboard;
		D3DXVECTOR3 Centre;
		float angle;
		//Draw a billboard, not a model
		//Uses :rotation, radius, customShader, renderfx, shaderTime, shaderRGBA, origin

		Centre = MakeD3DXVec3FromFloat3( refEntity->origin );
		angle = refEntity->rotation /360.0f*2*D3DX_PI;
		Billboard.Axis1 = refEntity->radius * D3DXVECTOR3( (float)sin(angle), 0, (float)cos(angle) );
		Billboard.Axis2 = refEntity->radius * D3DXVECTOR3( (float)cos(angle), 0, (float)-sin(angle) );
		Billboard.Position[0] = Centre + Billboard.Axis1 + Billboard.Axis2;	Billboard.TexCoord[0].x = 0.0f; Billboard.TexCoord[0].y = 0.0f;
		Billboard.Position[1] = Centre - Billboard.Axis1 + Billboard.Axis2;	Billboard.TexCoord[1].x = 0.0f; Billboard.TexCoord[1].y = 1.0f;
		Billboard.Position[2] = Centre + Billboard.Axis1 - Billboard.Axis2;	Billboard.TexCoord[2].x = 1.0f; Billboard.TexCoord[2].y = 0.0f;
		Billboard.Position[3] = Centre - Billboard.Axis1 - Billboard.Axis2;	Billboard.TexCoord[3].x = 1.0f; Billboard.TexCoord[3].y = 1.0f;
		D3DXVec3Normalize( &Billboard.Axis1, &Billboard.Axis1 );
		D3DXVec3Normalize( &Billboard.Axis2, &Billboard.Axis2 );
		D3DXVec3Cross( &Billboard.Normal, &Billboard.Axis2, &Billboard.Axis1 );
		D3DXVec3Normalize( &Billboard.Normal, &Billboard.Normal );

		Billboard.OnlyAlignAxis1 = FALSE;
		Billboard.RenderFeatures.ShaderRGBA = D3DCOLOR_RGBA(refEntity->shaderRGBA[0], refEntity->shaderRGBA[1], refEntity->shaderRGBA[2], refEntity->shaderRGBA[3]);
		Billboard.RenderFeatures.ShaderTime = refEntity->shaderTime;
		Billboard.RenderFeatures.LightmapTexture = NULL;
		Billboard.RenderFeatures.DrawAsSprite = FALSE;
		Billboard.RenderFeatures.bApplyLightVols = FALSE;
		Billboard.RenderFeatures.pTM = NULL;
		Billboard.ShaderHandle = refEntity->customShader;

		AddBillboard( &Billboard );
		return;
	}
	if(refEntity->reType == RT_MODEL) {

		MATRIX mat;
		
		mat = MakeD3DXMatrixFromQ3(refEntity->axis, refEntity->origin);

		if( refEntity->hModel <= 0 ) return;

		if( refEntity->hModel<1 || refEntity->hModel>NumModels ) {
			CriticalError(ERR_RENDER, "Invalid Model Handle");
		}
		else {

			Model = ModelArray[refEntity->hModel-1];
 			Model = Model->GetNextRenderInstance();		//Allow for rendering Entities with same ModelHandle, but different modelling matricies and frame time

			//Inline skins not implemented (refEntity->skinNum)
			Model->SetSkin( refEntity->customSkin );			//Possible Bug : Multiple Skins for a single model handle, per frame?
			Model->LoadMD3IntoD3D(refEntity->oldframe, refEntity->frame, 1.0f-refEntity->backlerp);

			DQRenderStack.SetOverrideShader( refEntity->customShader );
			
			RenderFeature.ShaderRGBA = D3DCOLOR_RGBA(refEntity->shaderRGBA[0], refEntity->shaderRGBA[1], refEntity->shaderRGBA[2], refEntity->shaderRGBA[3]);
			RenderFeature.ShaderTime = refEntity->shaderTime;
			RenderFeature.LightmapTexture = NULL;
			RenderFeature.DrawAsSprite = FALSE;
			RenderFeature.bApplyLightVols = TRUE;
			//Release build screws up if you don't have an intermediate variable
			RenderFeature.pTM = &mat;

			Model->DrawModel( &RenderFeature );
			
			return;
		}
	}
	if(refEntity->reType == RT_LIGHTNING || refEntity->reType == RT_RAIL_CORE) {
		//Apply autosprite2 to the sprite (ie. only align along longest axis)
		//Origin is refEntity->origin
		//End point is refEntity->oldorigin

		s_Billboard Billboard;
		D3DXVECTOR3 Centre;
		float Length;

		Centre = MakeD3DXVec3FromFloat3( refEntity->origin );
		Billboard.Axis1 = MakeD3DXVec3FromFloat3( refEntity->oldorigin ) - Centre;
		if( Billboard.Axis1.z > 0.8f ) {
			Billboard.Normal = D3DXVECTOR3( 0,1.0f,0 );
		}
		else {
			Billboard.Normal = D3DXVECTOR3( 0,0,1.0f );
		}
		D3DXVec3Cross( &Billboard.Axis2, &Billboard.Axis1, &Billboard.Normal );
		D3DXVec3Normalize( &Billboard.Axis2, &Billboard.Axis2 );	//beam width = 4.0f
		Billboard.Axis2 *= 2.0f;
		Length = D3DXVec3Length( &Billboard.Axis1 );

		Billboard.Position[0] = Centre + Billboard.Axis1 + Billboard.Axis2;	Billboard.TexCoord[0].x = 0.0f; Billboard.TexCoord[0].y = 0.0f;
		Billboard.Position[1] = Centre + Billboard.Axis2;	Billboard.TexCoord[1].x = Length / 100.0f; Billboard.TexCoord[1].y = 0.0f;
		Billboard.Position[2] = Centre + Billboard.Axis1 - Billboard.Axis2;	Billboard.TexCoord[2].x = 0.0f; Billboard.TexCoord[2].y = 1.0f;
		Billboard.Position[3] = Centre - Billboard.Axis2;	Billboard.TexCoord[3].x = Length / 100.0f; Billboard.TexCoord[3].y = 1.0f;
		Billboard.Axis1 /= Length;
		D3DXVec3Normalize( &Billboard.Axis2, &Billboard.Axis2 );
		D3DXVec3Cross( &Billboard.Normal, &Billboard.Axis2, &Billboard.Axis1 );
		D3DXVec3Normalize( &Billboard.Normal, &Billboard.Normal );

		if(refEntity->reType == RT_LIGHTNING) {
			Billboard.RenderFeatures.ShaderRGBA = 0xFFFFFFFF;
			Billboard.RenderFeatures.ShaderTime = 0.0f;
		}
		else {
			Billboard.RenderFeatures.ShaderRGBA = D3DCOLOR_RGBA(refEntity->shaderRGBA[0], refEntity->shaderRGBA[1], refEntity->shaderRGBA[2], refEntity->shaderRGBA[3]);
			Billboard.RenderFeatures.ShaderTime = refEntity->shaderTime;
		}
		Billboard.RenderFeatures.LightmapTexture = NULL;
		Billboard.RenderFeatures.DrawAsSprite = FALSE;
		Billboard.RenderFeatures.bApplyLightVols = FALSE;
		Billboard.RenderFeatures.pTM = NULL;
		Billboard.ShaderHandle = refEntity->customShader;
		Billboard.OnlyAlignAxis1 = TRUE;

		AddBillboard( &Billboard );
		return;
	}

	//NOTE : Portals don't work sometimes due to the tex coords of the base texture
	//		Portals also screw up the decals (dynamic vertex buffer?) and are extremely slow
	//		so I have disabled them
#ifdef _PORTALS
	if(refEntity->reType == RT_PORTALSURFACE) {
		

		D3DXVECTOR3 Sep;
		refdef_t refdef;

		refdef.x = refdef.y = 0;
		refdef.width = GLconfig.vidWidth/2;
		refdef.height = GLconfig.vidHeight/2;
		refdef.rdflags = RF_THIRD_PERSON;
		refdef.time = DQCamera.RenderTime;
		refdef.fov_x = 90;
		refdef.fov_y = 60;
		for(int i=0; i<3; ++i) {
			refdef.vieworg[i] = refEntity->oldorigin[i];
			refdef.viewaxis[0][i] = refEntity->axis[0][i];
			refdef.viewaxis[1][i] = refEntity->axis[1][i];
			refdef.viewaxis[2][i] = refEntity->axis[2][i];
		}
		
		//Find Render Target
		for(i=0;i<NumRenderToTextures;++i) {
			Sep = RenderToTexture[i].Position - MakeD3DXVec3FromFloat3(refEntity->origin);
			if( D3DXVec3LengthSq( &Sep ) < 10000 ) 
			{
				//Set RenderToTexture flag
				RenderToTexture[i].bEnable = TRUE;
				RenderToTexture[i].Camera.SetWithRefdef( &refdef );
				//Calc viewport size
				//TODO
			}
		}
		
		return;
	}
#endif //_PORTALS

	//RT_POLY, RT_BEAM, RT_RAIL_RINGS not used in Q3 DLLs
	if(refEntity->reType == RT_POLY) {
		Error( ERR_RENDER, "Unsupported Entity Type : RT_POLY" ); return;
	}
	if(refEntity->reType == RT_BEAM) {
		Error( ERR_RENDER, "Unsupported Entity Type : RT_BEAM" ); return;
	}
	if(refEntity->reType == RT_RAIL_RINGS) {
		Error( ERR_RENDER, "Unsupported Entity Type : RT_RAIL_RINGS" ); return;
	}
}

//Similar to DrawSprite
void c_Render::AddPoly( polyVert_t *PolyVerts, int NumVerts, int ShaderHandle )
{
	const int MaxPolyVerts = 100;
	D3DXVECTOR3 Pos[MaxPolyVerts], ApproxPos;
	D3DXVECTOR2 Tex[MaxPolyVerts];
	DWORD Diffuse[MaxPolyVerts];
	int i;

	if( !r_RenderScene.integer ) return;

	if( ShaderHandle<=0 || ShaderHandle>NumShaders ) ShaderHandle = NoShaderShader;

	if( NumVerts < 3 ) return;

	if( NumVerts > MaxPolyVerts ) {
		Error( ERR_RENDER, "Too many poly verts" );
		NumVerts = MaxPolyVerts;
	}

	if( NextPolyVBPos + NumVerts > MAX_CUSTOMPOLYVERTS ) {
		Error( ERR_RENDER, "Too many polys" );
		return;
	}

	ApproxPos = D3DXVECTOR3( 0,0,0 );
	for( i=0; i<NumVerts; ++i ) {
		Pos[i] = MakeD3DXVec3FromFloat3( PolyVerts[i].xyz );
		ApproxPos += Pos[i];
		Tex[i] = MakeD3DXVec2FromFloat2( PolyVerts[i].st );
		Diffuse[i] = D3DCOLOR_RGBA( PolyVerts[i].modulate[0], PolyVerts[i].modulate[1], PolyVerts[i].modulate[2], PolyVerts[i].modulate[3] );
	}
	ApproxPos /= (float)NumVerts;

	SetSortValue( ShaderHandle, 6 );		//decal

	//Doesn't work properly as DEST is already fogged
//	int FogShader = DQBSP.CheckFogStatus( ApproxPos );
//	if( FogShader ) {
//		ShaderHandle = DQRender.AddEffectToShader( ShaderHandle, FogShader );
//	}

	DQRenderStack.UpdateDynamicVerts( PolyVBHandle, NextPolyVBPos, NumVerts, Pos, Tex, NULL, Diffuse, NULL, 0 );
	DQRenderStack.DrawPrimitive( PolyVBHandle, ShaderHandle, D3DPT_TRIANGLEFAN, NextPolyVBPos, NumVerts-2, ApproxPos, NULL );

	NextPolyVBPos += NumVerts;
}

void c_Render::AddBillboard( s_VertexStreamP *Position, s_VertexStreamT *TexCoord, int ShaderHandle, BOOL bOnlyAlignAxis1 )
{
	s_Billboard Billboard;

	Billboard.ShaderHandle = ShaderHandle;
	Billboard.Position[0]	= Position[0].Pos;
	Billboard.Position[1]	= Position[1].Pos;
	Billboard.Position[2]	= Position[2].Pos;
	Billboard.Position[3]	= Position[3].Pos;
	Billboard.TexCoord[0]	= TexCoord[0].Tex;
	Billboard.TexCoord[1]	= TexCoord[1].Tex;
	Billboard.TexCoord[2]	= TexCoord[2].Tex;
	Billboard.TexCoord[3]	= TexCoord[3].Tex;
	ZeroMemory( &Billboard.RenderFeatures, sizeof(s_RenderFeatureInfo) );

	Billboard.Axis1 = Billboard.Position[1] - Billboard.Position[0];
	Billboard.Axis2 = Billboard.Position[2] - Billboard.Position[0];
	D3DXVec3Normalize( &Billboard.Axis1, &Billboard.Axis1 );
	D3DXVec3Normalize( &Billboard.Axis2, &Billboard.Axis2 );
	D3DXVec3Cross( &Billboard.Normal, &Billboard.Axis2, &Billboard.Axis1 );
	D3DXVec3Normalize( &Billboard.Normal, &Billboard.Normal );
	Billboard.OnlyAlignAxis1 = bOnlyAlignAxis1;

	AddBillboard( &Billboard );
}

void c_Render::AddBillboard( D3DXVECTOR3 Position[4], D3DXVECTOR2 TexCoord[4], int ShaderHandle )
{
	s_Billboard Billboard;

	Billboard.ShaderHandle = ShaderHandle;
	Billboard.Position[0]	= Position[0];
	Billboard.Position[1]	= Position[1];
	Billboard.Position[2]	= Position[2];
	Billboard.Position[3]	= Position[3];
	Billboard.TexCoord[0]	= TexCoord[0];
	Billboard.TexCoord[1]	= TexCoord[1];
	Billboard.TexCoord[2]	= TexCoord[2];
	Billboard.TexCoord[3]	= TexCoord[3];
	ZeroMemory( &Billboard.RenderFeatures, sizeof(s_RenderFeatureInfo) );

	Billboard.Axis1 = Billboard.Position[1] - Billboard.Position[0];
	Billboard.Axis2 = Billboard.Position[2] - Billboard.Position[0];
	D3DXVec3Normalize( &Billboard.Axis1, &Billboard.Axis1 );
	D3DXVec3Normalize( &Billboard.Axis2, &Billboard.Axis2 );
	D3DXVec3Cross( &Billboard.Normal, &Billboard.Axis2, &Billboard.Axis1 );
	D3DXVec3Normalize( &Billboard.Normal, &Billboard.Normal );
	Billboard.OnlyAlignAxis1 = FALSE;

	AddBillboard( &Billboard );
}

//Similar to DrawSprite and AddPoly, but TM is calculated at beginning of Render
// as we don't know the camera orientation yet
void c_Render::AddBillboard( s_Billboard *BB )
{
	s_Billboard *Billboard;

	if( !r_RenderScene.integer ) return;

	if(NumDrawBillboards>=MAX_BILLBOARDS) {
		Error(ERR_RENDER, "Max Billboards Reached");
		return;
	}
	if(BB->ShaderHandle<1 || BB->ShaderHandle>NumShaders) {
		BB->ShaderHandle = DQRender.NoShaderShader;
	}

	Billboard = &BillboardArray[NumDrawBillboards];
	
	memcpy( Billboard, BB, sizeof(s_Billboard) );

	Billboard->AveragePosition = D3DXVECTOR3(0,0,0);
	for(int j=0;j<4;++j) Billboard->AveragePosition += Billboard->Position[j];
	Billboard->AveragePosition *= 0.25f;

	++NumDrawBillboards;
}

//Run by DrawScene
//This converts billboard normal and positions into verts in a vertex buffer
void c_Render::PrepareBillboards()
{

	int i,j;
	D3DXVECTOR3 AdjustedLookAt, AdjustedBillboardNormal, AdjustedBillboardAxis2;
	D3DXVECTOR3 CameraLookAt;
	float Angle, SinAngle, CosAngle;
	MATRIX RotationMatrix1, RotationMatrix2;
	D3DXVECTOR3 CalculatedVertPos[6];
	D3DXVECTOR2 TexCoord[6];
	DWORD Diffuse[6];
	s_Billboard *BB;

	if(NumDrawBillboards==0) return;

DQProfileSectionStart( 7, "Prepare BB" );

	DQRenderStack.bProcessingBillboards = TRUE;		//stop RenderStack from adding new billboards

	CameraLookAt.x = DQCamera.ViewRot._13;
	CameraLookAt.y = DQCamera.ViewRot._23;
	CameraLookAt.z = DQCamera.ViewRot._33;

	for(i=0;i<NumDrawBillboards;++i) {
		BB = &BillboardArray[i];

		//Calculate Billboard Axes

		CameraLookAt = BB->AveragePosition - DQCamera.Position;
		D3DXVec3Normalize( &CameraLookAt, &CameraLookAt );

		//Rotate Billboard about Axes
		//***************************

		//Calculate LookAt vector in plane perpendicular to Axis1
		AdjustedLookAt = -CameraLookAt + BB->Axis1 * D3DXVec3Dot( &BB->Axis1, &CameraLookAt );
		D3DXVec3Normalize( &AdjustedLookAt, &AdjustedLookAt );
		//Calculate the angle required to rotate the billboard to face us
		CosAngle = D3DXVec3Dot( &AdjustedLookAt, &BB->Normal );
		SinAngle = D3DXVec3Dot( D3DXVec3Cross( &AdjustedLookAt, &AdjustedLookAt, &BB->Normal ), &BB->Axis1 );
		if(SinAngle>0)
			Angle = -(float)acos((float)CosAngle);
		else 
			Angle = (float)acos((float)CosAngle);
		D3DXMatrixRotationAxis( &RotationMatrix1, &BB->Axis1, Angle );

		if( !BB->OnlyAlignAxis1 ) {

			//Rotate the billboard normal and 2nd axis
			D3DXVec3TransformNormal( &AdjustedBillboardNormal, &BB->Normal, &RotationMatrix1 );
			D3DXVec3TransformNormal( &AdjustedBillboardAxis2, &BB->Axis2, &RotationMatrix1 );

			//Rotate Billboard about (adjusted) Axis2
			//Calculate the angle required to rotate the billboard to face us
			CosAngle = D3DXVec3Dot( &CameraLookAt, &AdjustedBillboardNormal );
			SinAngle = D3DXVec3Dot( D3DXVec3Cross( &CameraLookAt, &CameraLookAt, &AdjustedBillboardNormal ), &AdjustedBillboardAxis2 );
			if(SinAngle>0)
				Angle = D3DX_PI - (float)acos((float)CosAngle);
			else 
				Angle = D3DX_PI + (float)acos((float)CosAngle);
			D3DXMatrixRotationAxis( &RotationMatrix2, &AdjustedBillboardAxis2, Angle );

			RotationMatrix1 *= RotationMatrix2;
		}

		//Concatenate Rotations to calculate final billboard TM
		RotationMatrix1._41 = BB->AveragePosition.x;
		RotationMatrix1._42 = BB->AveragePosition.y;
		RotationMatrix1._43 = BB->AveragePosition.z;

		CalculatedVertPos[1] = BB->Position[0] - BB->AveragePosition;
		CalculatedVertPos[2] = BB->Position[2] - BB->AveragePosition;
		CalculatedVertPos[3] = BB->Position[1] - BB->AveragePosition;
		CalculatedVertPos[4] = BB->Position[3] - BB->AveragePosition;

		TexCoord[0] = BB->TexCoord[0];
		TexCoord[1] = BB->TexCoord[0];
		TexCoord[2] = BB->TexCoord[2];
		TexCoord[3] = BB->TexCoord[1];
		TexCoord[4] = BB->TexCoord[3];
		TexCoord[5] = BB->TexCoord[3];

		for(j=0; j<6; ++j) Diffuse[j] = BB->RenderFeatures.ShaderRGBA;

		//Rotate Billboard verts
		D3DXVec3TransformCoordArray( &CalculatedVertPos[1], sizeof(D3DXVECTOR3), &CalculatedVertPos[1], sizeof(D3DXVECTOR3), &RotationMatrix1, 4 );
		CalculatedVertPos[0] = CalculatedVertPos[1];
		CalculatedVertPos[5] = CalculatedVertPos[4];

		//Send Billboard Verts to Render Stack
		DQRenderStack.UpdateDynamicVerts( BillboardVBHandle, 6*i, 6, CalculatedVertPos, TexCoord, NULL, Diffuse, NULL, 0 );

		//Draw Primitives
		DQRenderStack.DrawPrimitive( BillboardVBHandle, BB->ShaderHandle, D3DPT_TRIANGLESTRIP, 6*i, 4, BB->AveragePosition, &BB->RenderFeatures );
	}

	DQRenderStack.bProcessingBillboards = FALSE;

DQProfileSectionEnd( 7 );

}

//Using Q3UI UI_DrawText for reference
void c_Render::DrawString( float x, float y, int CharWidth, int CharHeight, char *str, int MaxLength )
{
	if(!str) return;
	if(y<0 || y>GLconfig.vidHeight) return;

	int i, xpos;
	D3DXVECTOR2 TexturePos;
	char *c;
	static float ColourBlack[4]		= { 0.0f, 0.0f, 0.0f, 1.0f };
	static float ColourRed[4]		= { 1.0f, 0.0f, 0.0f, 1.0f };
	static float ColourGreen[4]		= { 0.0f, 1.0f, 0.0f, 1.0f };
	static float ColourYellow[4]	= { 1.0f, 1.0f, 0.0f, 1.0f };
	static float ColourBlue[4]		= { 0.0f, 0.0f, 1.0f, 1.0f };
	static float ColourCyan[4]		= { 0.0f, 1.0f, 1.0f, 1.0f };
	static float ColourMagenta[4]	= { 1.0f, 0.0f, 1.0f, 1.0f };
	static float ColourWhite[4]		= { 1.0f, 1.0f, 1.0f, 1.0f };

	c = str;
	xpos = x;
	for(i=0;*c!='\0' && i<MaxLength;++i,++c) {

		switch(*c) {
		case ' ': break;
		case '\n': continue;
		case '\t': 
			xpos += 4*CharWidth; 
			continue;
		case Q_COLOR_ESCAPE:
			++i; 
			++c;
			switch(*c) {
			case COLOR_BLACK:	SetSpriteColour( ColourBlack ); break;
			case COLOR_RED:		SetSpriteColour( ColourRed ); break;
			case COLOR_GREEN:	SetSpriteColour( ColourGreen ); break;
			case COLOR_YELLOW:	SetSpriteColour( ColourYellow ); break;
			case COLOR_BLUE:	SetSpriteColour( ColourBlue ); break;
			case COLOR_CYAN:	SetSpriteColour( ColourCyan ); break;
			case COLOR_MAGENTA:	SetSpriteColour( ColourMagenta ); break;
			case COLOR_WHITE:	SetSpriteColour( ColourWhite ); break;
			default: SetSpriteColour( NULL );
			};
			continue;
		default:
			TexturePos.x = ((*c)&15)*0.0625f;
			TexturePos.y = ((*c)>>4)*0.0625f;
			DrawSprite( xpos, y, CharWidth, CharHeight, TexturePos.x, TexturePos.y, TexturePos.x + 0.0625f, TexturePos.y + 0.0625f, charsetShader );
		};

		xpos += CharWidth;
	}

}

int c_Render::AddEffectToShader( int ShaderHandle, int EffectShaderHandle )
{
	int NextFogHandle, LastFogHandle;

	if( ShaderHandle<=0 || ShaderHandle>NumShaders || EffectShaderHandle<=0 || EffectShaderHandle>NumShaders ) {
		Error( ERR_RENDER, "Invalid Shader Handle" );
		return NoShaderShader;
	}

	if(NumShaders>=MAX_SHADERS) {
		Error(ERR_MISC, "Max Shaders Reached");
		return NoShaderShader;
	}

	NextFogHandle = ShaderHandle;
	do {
		if( ShaderArray[NextFogHandle-1]->Fog == ShaderArray[EffectShaderHandle-1]->Fog ) {
			break;
		}
		LastFogHandle = NextFogHandle;
		NextFogHandle = ShaderArray[LastFogHandle-1]->NextFoggedShaderHandle;
	} while( NextFogHandle );

	if( NextFogHandle ) {
		//This shader has already been created with this fog
		return NextFogHandle;
	}
	else {
		//Create a new shader identical to ShaderHandle, but with this fog
		ShaderArray[NumShaders] = (c_Shader*)DQNewVoid( c_Shader(ShaderArray[ShaderHandle-1], 0) );
		ShaderArray[NumShaders]->AddFog( ShaderArray[EffectShaderHandle-1] );
		ShaderArray[NumShaders]->Initialise();

		++NumShaders;

		//Add to NextFogHandle
		ShaderArray[LastFogHandle-1]->NextFoggedShaderHandle = NumShaders;
	}

	return NumShaders;
}