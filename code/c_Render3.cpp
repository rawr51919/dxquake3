// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//	c_Render3.cpp : Rendering Only
//

#include "stdafx.h"

void c_Render::BeginRender()
{
	if(ToggleVidRestart) {
		ToggleVidRestart = FALSE;
		VidRestart();
	}

	//possible fix to gamma bug
	if( !DQClientGame.isLoaded && !CheckOverbright() ) {
		DQConsole.SetCVarString( "r_overbrightBits", "0", eAuthClient );
		Error( ERR_RENDER, "Overbright Error!!" );
		ToggleVidRestart = TRUE;
	}

	DQConsole.UpdateCVar( &r_RenderScene );

	if( r_RenderScene.integer ) {

		DQTime.NextFrame();

		d3dcheckOK( device->BeginScene() );
		
		RenderNewFrame = TRUE;
	}

	//Reset Renderer
	c_Shader::LastRunShader = NULL;
	bClearScreen = FALSE;
	NumD3DCalls = 0;
	DQMemory.NumDQNewCalls = 0;
	CurrentSpriteInfo.Clear();

//	NumDrawSprites = 0;
//	NextSpriteVBPos = 0;
	NextPolyVBPos = 0;
	NumDrawBillboards = 0;
//	ZeroMemory( NumRenderObjects, sizeof(NumRenderObjects) );
	NumLights = 0;
//	NumDrawEntities = 0;
}

void c_Render::DrawScene()
{
DQProfileSectionStart(1, "DrawScene");
	int u;

	if( r_RenderScene.integer ) {

		//Force Draw of cached sprites
		RenderSpriteCache();

		++FrameNum;

		//Set view
		d3dcheckOK( device->SetViewport( &Camera.DefaultViewPort ) );
		pCamera = &Camera;

#ifdef _PORTALS
		int i;
		HRESULT hr;
		for( i=0; i<NumRenderToTextures; ++i ) {
			if(RenderToTexture[i].bEnable) {
				//End render to screen
				d3dcheckOK( device->EndScene() );

				//Render to texture
				hr = RenderToTexture[i].D3DXRenderToSurface->BeginScene( RenderToTexture[i].RenderSurface, NULL );
				if(FAILED(hr)) {
					RenderToTexture[i].bEnable = FALSE;
				}
				
				//Set camera
				pCamera = &RenderToTexture[i].Camera;

				break;
			}
		}
#endif

/*
		if(RenderNewFrame) {
			if(1||bClearScreen) {
	//			d3dcheckOK( device->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0 ) );
				d3dcheckOK( device->Clear( 0, NULL, D3DCLEAR_ZBUFFER, NULL, 1.0f, 0 ) );
			}
			RenderNewFrame = FALSE;
		}
		else {
			d3dcheckOK( device->Clear( 0, NULL, D3DCLEAR_ZBUFFER, NULL, 1.0f, 0 ) );
		}
	*/

		d3dcheckOK( device->Clear( 0, NULL, D3DCLEAR_ZBUFFER, NULL, 1.0f, 0 ) );

		pCamera->SetD3D();

		//Set lights
		for(u=0;u<NumLights;++u) d3dcheckOK( device->LightEnable( u, TRUE ) );
		for(;u<MAX_LIGHTS;++u) d3dcheckOK( device->LightEnable( u, FALSE ) );

		d3dcheckOK( DQDevice->SetTransform( D3DTS_WORLD, &DQRender.matIdentity ) );

		if(!(pCamera->renderfx & RDF_NOWORLDMODEL)) DQBSP.DrawBSP();

		//Prepare Billboards-now we know the camera orientation, set their verts & send to RenderStack
		PrepareBillboards();

		RenderStack.SortStack();

		RenderStack.RenderToScreen();

		d3dcheckOK( device->SetViewport( &pCamera->DefaultViewPort ) );

#ifdef _PORTALS
		//End Render to Texture and begin Render to Screen
		if(i!=NumRenderToTextures) {
			//End render to texture
			d3dcheckOK( RenderToTexture[i].D3DXRenderToSurface->EndScene( D3DX_FILTER_LINEAR ) );

			//Render to screen
			d3dcheckOK( device->BeginScene() );

			RenderToTexture[i].bEnable = FALSE;
			DQProfileSectionEnd(1);
			DrawScene();
			return;
		} else 
#endif //_PORTALS
		{
			DQProfileSectionEnd(1);
		}

	}

	//Reset Renderer
	bClearScreen = FALSE;
	RenderStack.Clear();
	NumLights = 0;
	NumDrawBillboards = 0;
	NextPolyVBPos = 0;
	CurrentSpriteInfo.Clear();
}

void c_Render::EndRender() {
	HRESULT hr;

	if( !r_RenderScene.integer ) return;
DQProfileSectionStart(2, "End Render");
	
	d3dcheckOK( device->EndScene() );
	hr = device->Present( 0,0,0,0 );
	if( FAILED(hr) ) {
		if( hr == D3DERR_DEVICELOST ) {
			ToggleVidRestart = TRUE;
		}
		else {
			CriticalError( ERR_RENDER, "Unable to draw to Direct3D" );
		}
	}

	//Update Camera time in case of dedicated server (ie. no calls to DQCamera.Update)
	DQCamera.RenderTime = (float)DQTime.GetMillisecs();

DQProfileSectionEnd(2);
}

int c_Render::ShaderGetNumPasses(int ShaderHandle)
{
	if(ShaderHandle-1<0 || ShaderHandle-1>NumShaders) return 0;
	return ShaderArray[ShaderHandle-1]->NumPasses;
}

/*
void c_Render::RenderSprite(s_Sprite *Sprite) {
	//Used for drawing line below console
	if(Sprite->ShaderHandle == 0) {
		d3dcheckOK( device->SetTexture( 0, NULL ) );
		UpdateRS( RS_COLOR_DIFFUSE );
		UpdateTSS( 0, RS_TSS_SAMP_POINT, NULL );
		UpdateTSS( 1, RS_TSS_COLOR_DISABLE | RS_TSS_ALPHA_DISABLE, NULL );

		d3dcheckOK( device->SetStreamSource( 0, SpriteVB, 0, sizeof(s_VertexStreamPosTDiffuseTex) ) );
		d3dcheckOK( device->SetFVF( FVF_PosTDiffuseTex ) );
		d3dcheckOK( device->DrawPrimitive( D3DPT_TRIANGLESTRIP, Sprite->SpriteVBPos, 2 ) );
		return;
	}

	for(int Pass=0;Pass<ShaderGetNumPasses(Sprite->ShaderHandle);++Pass) {
		RunShader(Sprite->ShaderHandle, NULL);
		if(!Shader_BaseFunc::UseCustomBlendFunc) {
			UpdateRS( CurrentRS.RS | RS_ALPHABLENDENABLE | RS_ZWRITEDISABLE | RS_COLOR_DIFFUSE );
			SetBlendFuncs( D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA );
		}
		else {
			UpdateRS( CurrentRS.RS | RS_ZWRITEDISABLE | RS_COLOR_DIFFUSE );
		}
		d3dcheckOK( device->SetStreamSource( 0, SpriteVB, 0, sizeof(s_VertexStreamPosTDiffuseTex) ) );
		d3dcheckOK( device->SetFVF( FVF_PosTDiffuseTex ) );
		d3dcheckOK( device->DrawPrimitive( D3DPT_TRIANGLESTRIP, Sprite->SpriteVBPos, 2 ) );
	}
}

void c_Render::RenderPoly(s_Poly *Poly) {
	d3dcheckOK( DQDevice->SetTransform( D3DTS_WORLD, &DQRender.matIdentity ) );
/*
	for(int Pass=0;Pass<ShaderGetNumPasses(Poly->ShaderHandle);++Pass) {
		RunShader(Poly->ShaderHandle, NULL);
#if 0
		d3dcheckOK( device->SetTexture( 0, NULL ) );
		UpdateRS( 0 );
		UpdateTSS( 0, 0, NULL );
		UpdateTSS( 1, RS_TSS_COLOR_DISABLE | RS_TSS_ALPHA_DISABLE, NULL );
#endif
		d3dcheckOK( device->SetStreamSource( 0, PolyVB, 0, sizeof(s_VertexStreamPosDiffuseTex) ) );
		d3dcheckOK( device->SetFVF( FVF_PosDiffuseTex ) );
//		d3dcheckOK( device->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME ) );
		d3dcheckOK( device->DrawPrimitive( D3DPT_TRIANGLEFAN, Poly->PolyVBPos, Poly->NumVerts-2 ) );
//		d3dcheckOK( device->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID ) );
	}
}

void c_Render::RenderBillboard(int BillboardIndex) 
{
	s_Billboard &Billboard = BillboardArray[BillboardIndex];

	d3dcheckOK( DQDevice->SetTransform( D3DTS_WORLD, &DQRender.matIdentity ) );

	//Only draw RF_THIRD_PERSON entities if we're in Draw Third Person Mode
	if(Billboard.renderfx & RF_THIRD_PERSON) {
		if(!(Camera.renderfx & RF_THIRD_PERSON)) 
			return;
	}

//	Shader_BaseFunc::TFactor = Billboard.ShaderRGBA;
	Shader_BaseFunc::ShaderTime = max( 0.0f, (Camera.RenderTime/1000.0f - Billboard.ShaderTime) );

	for(int Pass=0;Pass<ShaderGetNumPasses(Billboard.ShaderHandle);++Pass) {
		RunShader(Billboard.ShaderHandle, NULL);
#if 0
		d3dcheckOK( device->SetTexture( 0, NULL ) );
		UpdateRS( 0 );
		UpdateTSS( 0, 0, NULL );
		UpdateTSS( 1, RS_TSS_COLOR_DISABLE | RS_TSS_ALPHA_DISABLE, NULL );
		UpdateRS( RS_CULLNONE );
#endif
		d3dcheckOK( device->SetStreamSource( 0, BillboardVB, 0, sizeof(s_VertexStreamPosNormalDiffuseTexTex) ) );
		d3dcheckOK( device->SetFVF( FVF_PosNormalDiffuseTexTex ) );
		d3dcheckOK( device->DrawPrimitive( D3DPT_TRIANGLEFAN, BillboardIndex*4, 2 ) );
	}
}

void c_Render::RenderEntity(s_DQEntity *Entity)
{

	//Only draw RF_THIRD_PERSON entities if we're in Draw Third Person Mode
	if(Entity->renderfx & RF_THIRD_PERSON) {
		if(!(Camera.renderfx & RF_THIRD_PERSON)) 
			return;
	}

	//TODO: LightingOrigin, shadowPlane, 
	if(Entity->customShader>0 && Entity->customShader<NumShaders) {
		OverrideShaderHandle = Entity->customShader;
	}
	CriticalAssert(Entity->Model);

	Shader_BaseFunc::ShaderTime = max( 0.0f, (Camera.RenderTime/1000.0f - Entity->shaderTime) );
	Entity->Model->CullAndRenderObject(FALSE);
	Shader_BaseFunc::ShaderTime = Camera.RenderTime/1000.0f;

	OverrideShaderHandle = NULL;
}*/

void c_Render::UpdateRS(int RequiredRS)
{
	int diff;		//CurrentRS xor RequiredRS
	if(RequiredRS == CurrentRS.RS) return;

	diff = (CurrentRS.RS | RequiredRS) & (~(CurrentRS.RS & RequiredRS));

	if(diff&RS_NORMALIZENORMALS) d3dcheckOK( device->SetRenderState( D3DRS_NORMALIZENORMALS, (RequiredRS & RS_NORMALIZENORMALS)?1:0 ) );
	if(diff&RS_LIGHTING) d3dcheckOK( device->SetRenderState( D3DRS_LIGHTING, (RequiredRS & RS_LIGHTING)?1:0 ) );
	if(diff&RS_COLOR_DIFFUSE) d3dcheckOK( device->SetTextureStageState( 0, D3DTSS_COLORARG2, (RequiredRS & RS_COLOR_DIFFUSE)?D3DTA_DIFFUSE:D3DTA_TFACTOR ) );
	if(diff&RS_ALPHA_DIFFUSE) d3dcheckOK( device->SetTextureStageState( 0, D3DTSS_ALPHAARG2, (RequiredRS & RS_ALPHA_DIFFUSE)?D3DTA_DIFFUSE:D3DTA_TFACTOR ) );
	if(diff&RS_ALPHABLENDENABLE) d3dcheckOK( device->SetRenderState( D3DRS_ALPHABLENDENABLE, (RequiredRS & RS_ALPHABLENDENABLE)?1:0 ) );
	if(diff&RS_CULLFRONT || diff&RS_CULLNONE) {
		if(RequiredRS & RS_CULLNONE) {
			d3dcheckOK( device->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ) );
		}
		else if(RequiredRS & RS_CULLFRONT) {
			d3dcheckOK( device->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW ) );
		}
		else {
			d3dcheckOK( device->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW ) );
		}
	}
	if(diff&RS_ZWRITEDISABLE) d3dcheckOK( device->SetRenderState( D3DRS_ZWRITEENABLE, (RequiredRS & RS_ZWRITEDISABLE)?FALSE:TRUE ) );
	if(diff&RS_ZFUNC_EQUAL)  d3dcheckOK( device->SetRenderState( D3DRS_ZFUNC, (RequiredRS & RS_ZFUNC_EQUAL)?D3DCMP_EQUAL:D3DCMP_LESSEQUAL ) );
	if(diff&RS_ALPHAREF80)  d3dcheckOK( device->SetRenderState( D3DRS_ALPHAREF, (RequiredRS & RS_ALPHAREF80)?0x00000080:0 ) );
	if(diff&RS_ALPHATEST)  d3dcheckOK( device->SetRenderState( D3DRS_ALPHATESTENABLE, (RequiredRS & RS_ALPHATEST)?1:0 ) );
	if(diff&RS_ALPHAFUNC) {
		if(RequiredRS & RS_ALPHAFUNC_GREATER) {
			d3dcheckOK( device->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER ) );
		}
		else if(RequiredRS & RS_ALPHAFUNC_GREATEREQUAL) {
			d3dcheckOK( device->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL ) );
		}
		else if(RequiredRS & RS_ALPHAFUNC_LESS) {
			d3dcheckOK( device->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_LESS ) );
		}
		else {
			d3dcheckOK( device->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_ALWAYS ) );
		}
	}
	if(diff&RS_FOGENABLE) d3dcheckOK( device->SetRenderState( D3DRS_FOGENABLE, (RequiredRS & RS_FOGENABLE)?TRUE:FALSE ) );

	CurrentRS.RS = RequiredRS;
}

void c_Render::UpdateTSS(int T, int RequiredTSS, MATRIX *pTCM)	// T is TextureStage, pTCM is a pointer to the TexCoordMatrix (=NULL if identity)
{
	DWORD diff;		//CurrentRS xor RequiredRS

	//Set TexCoord Matrix
	if((RequiredTSS & RS_TSS_TTF) && pTCM) {
		d3dcheckOK( DQDevice->SetTransform( (D3DTRANSFORMSTATETYPE)(T+D3DTS_TEXTURE0), pTCM ) );
		CurrentRS.bf_TTF_IsIdentity &= ~(1<<T);
	}
	else {
		//Set to identity if required
		if( !(CurrentRS.bf_TTF_IsIdentity & (1<<T)) ) {
			d3dcheckOK( DQDevice->SetTransform( (D3DTRANSFORMSTATETYPE)(T+D3DTS_TEXTURE0), &matIdentity ) );
			CurrentRS.bf_TTF_IsIdentity |= (1<<T);
		}
	}

#if _DEBUG
	//Check TSS is valid
	diff = RequiredTSS & RS_TSS_COLOROP;
	switch(diff) {
	case 0:
	case RS_TSS_COLOR_ADD:
	case RS_TSS_COLOR_SEL1:
	case RS_TSS_COLOR_SEL2:
	case RS_TSS_COLOR_DISABLE:
	case RS_TSS_COLOR_ADDSMOOTH:
	case RS_TSS_COLOR_MODULATE2X:
	case RS_TSS_COLOR_MODULATE4X:
	case RS_TSS_COLOR_BLENDTEXTUREALPHA:
	case RS_TSS_MODALPHA_ADDCOLOR:
	case RS_TSS_MODINVALPHA_ADDCOLOR:
	case RS_TSS_BLENDCURRENTALPHA:
		break;
	default:
		Breakpoint;	//invalid TSS
	};
	diff = RequiredTSS & RS_TSS_ALPHAOP;
	switch(diff) {
	case 0:
	case RS_TSS_ALPHA_SEL1:
	case RS_TSS_ALPHA_SEL2:
	case RS_TSS_ALPHA_ADDSMOOTH:
	case RS_TSS_ALPHA_DISABLE:
	case RS_TSS_ALPHA_ADD:
	case RS_TSS_ALPHA_MODULATE2X:
	case RS_TSS_ALPHA_MODULATE4X:
	case RS_TSS_ALPHA_BLENDTEXTUREALPHA:
		break;
	default:
		Breakpoint;	//invalid TSS
	};
#endif

	if(RequiredTSS == CurrentRS.TSS[T]) return;

	diff = (CurrentRS.TSS[T] | RequiredTSS) & (~(CurrentRS.TSS[T] & RequiredTSS));

	if( (diff&RS_TSS_TCI_1) || (diff&RS_TSS_TCI_CSRV) ) {
		d3dcheckOK( device->SetTextureStageState( T, D3DTSS_TEXCOORDINDEX, 
			( (RequiredTSS&RS_TSS_TCI_CSRV)?D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR:0 ) | ( (RequiredTSS&RS_TSS_TCI_1)?1:0) ) );
	}

	if(diff&RS_TSS_CLAMP || diff&RS_TSS_MIRROR) {
		if(RequiredTSS&RS_TSS_MIRROR) {
			d3dcheckOK( device->SetSamplerState( T, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR ) );
			d3dcheckOK( device->SetSamplerState( T, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR ) );
		}
		else if(RequiredTSS&RS_TSS_CLAMP) {
			d3dcheckOK( device->SetSamplerState( T, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP ) );
			d3dcheckOK( device->SetSamplerState( T, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP ) );
		}
		else {
			d3dcheckOK( device->SetSamplerState( T, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP ) );
			d3dcheckOK( device->SetSamplerState( T, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP ) );
		}
	}

	if(diff&RS_TSS_TTF || diff&RS_TSS_TTFC3) {
		if(!(RequiredTSS & RS_TSS_TTF)) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE ) );
		}
		else if(RequiredTSS & RS_TSS_TTFC3) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3 ) );
		}
		else {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 ) );
		}
	}

	if(diff&RS_TSS_COMPLEMENT_1) {
		d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLORARG1, D3DTA_TEXTURE | ( (RequiredTSS&RS_TSS_COMPLEMENT_1)?D3DTA_COMPLEMENT:0 ) ) );
	}
	if(diff&RS_TSS_COMPLEMENT_2) {
		if(T==0) {
			d3dcheckOK( device->SetTextureStageState( 0, D3DTSS_COLORARG2, 
				( (CurrentRS.RS&RS_COLOR_DIFFUSE)?D3DTA_DIFFUSE:D3DTA_TFACTOR ) | ( (RequiredTSS&RS_TSS_COMPLEMENT_2)?D3DTA_COMPLEMENT:0 ) ) );
		}
		else {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLORARG2, D3DTA_CURRENT | 
				( (RequiredTSS&RS_TSS_COMPLEMENT_2)?D3DTA_COMPLEMENT:0) ) );
		}
	}
	if(diff&RS_TSS_ALPHA_COMPLEMENT_1) {
		d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAARG1, D3DTA_TEXTURE | 
			( (RequiredTSS&RS_TSS_COMPLEMENT_1)?D3DTA_COMPLEMENT:0 ) ) );
	}
	if(diff&RS_TSS_ALPHA_COMPLEMENT_2) {
		if(T==0) {
			d3dcheckOK( device->SetTextureStageState( 0, D3DTSS_ALPHAARG2, 
				( (CurrentRS.RS&RS_ALPHA_DIFFUSE)?D3DTA_DIFFUSE:D3DTA_TFACTOR ) | ( (RequiredTSS&RS_TSS_ALPHA_COMPLEMENT_2)?D3DTA_COMPLEMENT:0 ) ) );
		}
		else {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAARG2, D3DTA_CURRENT | 
				( (RequiredTSS&RS_TSS_ALPHA_COMPLEMENT_2)?D3DTA_COMPLEMENT:0) ) );
		}
	}

	if(diff&RS_TSS_COLOROP) {
		if(!(RequiredTSS & RS_TSS_COLOROP)) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_MODULATE ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_DISABLE) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_DISABLE ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_ADD) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_ADD ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_SEL1) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_SELECTARG1 ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_SEL2) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_SELECTARG2 ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_ADDSMOOTH) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_ADDSMOOTH ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_MODULATE2X) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_MODULATE2X ) );
		}
		else if(RequiredTSS & RS_TSS_COLOR_BLENDTEXTUREALPHA) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA ) );
		}
		else if(RequiredTSS & RS_TSS_BLENDCURRENTALPHA) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_BLENDCURRENTALPHA ) );
		}
		else if(RequiredTSS & RS_TSS_MODALPHA_ADDCOLOR) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR ) );
		}
		else if(RequiredTSS & RS_TSS_MODINVALPHA_ADDCOLOR) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_COLOROP, D3DTOP_MODULATEINVALPHA_ADDCOLOR ) );
		}
	}

	if(diff&RS_TSS_ALPHAOP) {
		if(!(RequiredTSS & RS_TSS_ALPHAOP)) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_MODULATE  ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_DISABLE) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_DISABLE ) );
		}
		else if(RequiredTSS & RS_TSS_MODALPHA_ADDCOLOR) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_MODULATEALPHA_ADDCOLOR ) );
		}
		else if(RequiredTSS & RS_TSS_MODINVALPHA_ADDCOLOR) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_MODULATEINVALPHA_ADDCOLOR ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_SEL1) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_SEL2) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_ADDSMOOTH) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_ADDSMOOTH ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_BLENDTEXTUREALPHA) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_BLENDTEXTUREALPHA ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_ADD) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_ADD ) );
		}
		else if(RequiredTSS & RS_TSS_ALPHA_MODULATE2X) {
			d3dcheckOK( device->SetTextureStageState( T, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X ) );
		}
	}

	if(diff&RS_TSS_SAMP_POINT) {
		d3dcheckOK( device->SetSamplerState( T, D3DSAMP_MINFILTER, (RequiredTSS&RS_TSS_SAMP_POINT)?D3DTEXF_POINT:D3DTEXF_LINEAR ) );
	}

	CurrentRS.TSS[T] = RequiredTSS;
}

void c_Render::SetTFactor( DWORD rgba )
{
	if(rgba != CurrentRS.TFactorRGBA ) {
		d3dcheckOK( device->SetRenderState( D3DRS_TEXTUREFACTOR, rgba ) );
		CurrentRS.TFactorRGBA = rgba;
	}
}

void c_Render::SetBlendFuncs( D3DBLEND BlendSrc, D3DBLEND BlendDest )
{
	if( BlendSrc != CurrentRS.BlendSrc || BlendDest != CurrentRS.BlendDest ) {
		d3dcheckOK( device->SetRenderState( D3DRS_SRCBLEND, BlendSrc ) );
		d3dcheckOK( device->SetRenderState( D3DRS_DESTBLEND, BlendDest ) );
		CurrentRS.BlendSrc = BlendSrc;
		CurrentRS.BlendDest = BlendDest;
	}
}

void c_Render::DisableStage( int TextureStage ) 
{ 
	if( (CurrentRS.TSS[TextureStage] & (RS_TSS_COLOR_DISABLE | RS_TSS_ALPHA_DISABLE) ) == (RS_TSS_COLOR_DISABLE | RS_TSS_ALPHA_DISABLE) ) {
		return;
	}
	
	d3dcheckOK( device->SetTextureStageState( TextureStage, D3DTSS_COLOROP, D3DTOP_DISABLE ) );
	d3dcheckOK( device->SetTextureStageState( TextureStage, D3DTSS_ALPHAOP, D3DTOP_DISABLE ) );

	CurrentRS.TSS[TextureStage] |= (RS_TSS_COLOR_DISABLE | RS_TSS_ALPHA_DISABLE);
}
