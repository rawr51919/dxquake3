// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//	Render the BSP
//

#include "stdafx.h"


void c_BSP::DrawBSP() {

	if(!isLoadedIntoD3D) return;
DQProfileSectionStart(0,"Draw BSP");

	//for lightvols
	LightNum = min( MAX_LIGHTS-1, DQRender.NumLights );

	FacesDrawn = 0;

	CurrentFrameNum = DQRender.FrameNum;

	//Render Skybox
//	RenderSkybox();

	ThisCluster = Leaf[FindLeafFromPos( &DQCamera.Position )].Cluster;

#if 0
	for(FaceIndex=0;FaceIndex<NumFaces;++FaceIndex) {
				if(Face[FaceIndex].Q3Face.type == BSPFACE_REGULARFACE) RenderFace(FaceIndex);
				if(Face[FaceIndex].Q3Face.type == BSPFACE_BEZIERPATCH) Face[FaceIndex].BezierPatch->RenderObject();
				else if(Face[FaceIndex].Q3Face.type == BSPFACE_MESH) RenderMesh(FaceIndex);
	}
#endif

	DQConsole.UpdateCVar( &r_useBSP );
	if(r_useBSP.integer) {
		MarkVisibleLeaves();

		//Traverse BSP tree
		RenderNode( 0 );
	}
	else {
		ThisCluster = -1;
		MarkVisibleLeaves();
		RenderNodeWithoutFrustumCull(0);
	}

	LastCluster = ThisCluster;

	//Render Skybox
	s_RenderFeatureInfo SkyboxRenderFeatures;
	MATRIX SkyboxTM;
	D3DXVECTOR3 ApproxPos;
	
	//Put Skybox at back of the stack
	ApproxPos = D3DXVECTOR3( 0,0,0 );

	ZeroMemory( &SkyboxRenderFeatures, sizeof(SkyboxRenderFeatures) );
	D3DXMatrixTranslation( &SkyboxTM, DQCamera.Position.x, DQCamera.Position.y, DQCamera.Position.z );
	SkyboxRenderFeatures.pTM = &SkyboxTM;
	
	int i;

	for( i=0; i<6; ++i ) 
	{
		//Draw Skybox
		if( SkyBoxShader[i] ) {
			DQRenderStack.DrawIndexedPrimitive( SkyboxVBHandle[i], SkyboxIBHandle[i], SkyBoxShader[i], D3DPT_TRIANGLESTRIP, 0, 0, NumSkySquarePrims, ApproxPos, &SkyboxRenderFeatures );
		}
	}
	//Draw Clouds
	if( CloudShaderHandle ) {
		DQRenderStack.DrawIndexedPrimitive( SkyDomeVBHandle, SkyDomeIBHandle, CloudShaderHandle, D3DPT_TRIANGLESTRIP, 0, 0, NumSkyDomePrims, ApproxPos, &SkyboxRenderFeatures );
	}

DQProfileSectionEnd( 0 );
}

//Reference : Diesel engine for BSP Rendering optimizations

BOOL c_BSP::RenderNode( int NodeIndex )
{
	eCullState Cull;

	if( NodeIndex < 0 ) {
		return RenderLeaf( ~NodeIndex );
	}

	if( ThisCluster == Node[NodeIndex].IgnoreCluster ) {
		return FALSE;
	}
	
	//Frustum Culling
	Cull = DQCamera.IsBoxVisible( Node[NodeIndex].Box );
	switch(Cull) {
	case eCullNotVisible: return TRUE;	//BBox completely outside frustum
	case eCullTotallyVisible: return RenderNodeWithoutFrustumCull( NodeIndex );	//BBox completely inside frustum
	};
	//BBox partially visible

	BOOL bAreChildrenVisible;

	bAreChildrenVisible = RenderNode( Node[NodeIndex].childFront );
	bAreChildrenVisible |= RenderNode( Node[NodeIndex].childBack );

	if( !bAreChildrenVisible ) Node[NodeIndex].IgnoreCluster = ThisCluster;
	
	return bAreChildrenVisible;
}

BOOL c_BSP::RenderNodeWithoutFrustumCull( int NodeIndex )
{
	if( NodeIndex < 0 ) {
		return RenderLeaf( ~NodeIndex );
	}

	if( ThisCluster == Node[NodeIndex].IgnoreCluster ) {
		return FALSE;
	}

	BOOL bAreChildrenVisible;

	bAreChildrenVisible = RenderNodeWithoutFrustumCull( Node[NodeIndex].childFront );
	bAreChildrenVisible |= RenderNodeWithoutFrustumCull( Node[NodeIndex].childBack );

	if( !bAreChildrenVisible ) Node[NodeIndex].IgnoreCluster = ThisCluster;
	
	return bAreChildrenVisible;
}

BOOL c_BSP::RenderLeaf( int LeafIndex )
{
	register int i;
	register s_DQFaces *F;

	if( ThisCluster != Leaf[LeafIndex].LastVisibleCluster || Leaf[LeafIndex].Cluster == -1 ) {
		return FALSE;
	}

	const int FirstFace = Leaf[LeafIndex].firstLeafFace;
	const int NumFaces = Leaf[LeafIndex].numLeafFace;

	for( i=0; i<NumFaces; ++i ) {
//		RenderFace( LeafFace[FirstFace+i].index );

		F = &Face[LeafFace[FirstFace+i]];
		//check if face already drawn
		if(F->LastFrameDrawn == CurrentFrameNum ) continue;
		F->LastFrameDrawn = CurrentFrameNum;

#ifdef _PROFILE
		++FacesDrawn;
#endif

//		if( Leaf[LeafIndex].bIncludesCurvedSurface ) continue;

		switch(F->Q3Face.type) {
		case BSPFACE_REGULARFACE:
			DQRenderStack.DrawBSPPrimitive( BSPVBhandle, F->ShaderHandle, D3DPT_TRIANGLEFAN, F->Q3Face.firstVertex, F->Q3Face.numVertex-2, F->ApproxPos, F->LightmapTexture );
			break;
		case BSPFACE_BEZIERPATCH:
			F->BezierPatch->RenderObject( F->ShaderHandle, F->LightmapTexture );
			break;
		case BSPFACE_MESH:
			DQRenderStack.DrawBSPIndexedPrimitive( BSPVBhandle, BSPMeshIndicesHandle, F->ShaderHandle, D3DPT_TRIANGLELIST, F->Q3Face.firstMeshVert, F->Q3Face.firstVertex, F->Q3Face.numMeshVert/3, F->ApproxPos, F->LightmapTexture );
			break;
		case BSPFACE_BILLBOARD:
			DQRender.AddBillboard( F->Billboard );
			break;
		case BSPFACE_DYNAMICFACE:
			DQRenderStack.UpdateDynamicVerts( F->DynamicVB, 0, F->Q3Face.numVertex, &Vertices.Pos[F->Q3Face.firstVertex], &Vertices.TexCoords[F->Q3Face.firstVertex], &Vertices.Normal[F->Q3Face.firstVertex], &Vertices.Diffuse[F->Q3Face.firstVertex], &Vertices.LightmapCoords[F->Q3Face.firstVertex], F->ShaderHandle );
			DQRenderStack.DrawBSPPrimitive( F->DynamicVB, F->ShaderHandle, D3DPT_TRIANGLEFAN, 0, F->Q3Face.numVertex-2, F->ApproxPos, F->LightmapTexture );
			break;
		case BSPFACE_DYNAMICMESH:
			DQRenderStack.UpdateDynamicVerts( F->DynamicVB, 0, F->Q3Face.numVertex, &Vertices.Pos[F->Q3Face.firstVertex], &Vertices.TexCoords[F->Q3Face.firstVertex], &Vertices.Normal[F->Q3Face.firstVertex], &Vertices.Diffuse[F->Q3Face.firstVertex], &Vertices.LightmapCoords[F->Q3Face.firstVertex], F->ShaderHandle );
			DQRenderStack.DrawBSPIndexedPrimitive( F->DynamicVB, BSPMeshIndicesHandle, F->ShaderHandle, D3DPT_TRIANGLELIST, F->Q3Face.firstMeshVert, 0, F->Q3Face.numMeshVert/3, F->ApproxPos, F->LightmapTexture );
			break;
		};
	}

	return TRUE;
}

void c_BSP::RenderInlineModel( int ModelIndex )
{
	register int i;
	register s_DQFaces *F;

	const int FirstFace = Model[ModelIndex].firstFace;
	const int NumFaces = Model[ModelIndex].numFace;

	for( i=0; i<NumFaces; ++i ) {

		F = &Face[FirstFace+i];
		//check if face already drawn
		if(F->LastFrameDrawn == CurrentFrameNum ) continue;
		F->LastFrameDrawn = CurrentFrameNum;
		++FacesDrawn;

		switch(F->Q3Face.type) {
		case BSPFACE_REGULARFACE:
			DQRenderStack.DrawBSPPrimitive( BSPVBhandle, F->ShaderHandle, D3DPT_TRIANGLEFAN, F->Q3Face.firstVertex, F->Q3Face.numVertex-2, F->ApproxPos, F->LightmapTexture );
			break;
		case BSPFACE_BEZIERPATCH:
			F->BezierPatch->RenderObject( F->ShaderHandle, F->LightmapTexture );
			break;
		case BSPFACE_MESH:
			DQRenderStack.DrawBSPIndexedPrimitive( BSPVBhandle, BSPMeshIndicesHandle, F->ShaderHandle, D3DPT_TRIANGLELIST, F->Q3Face.firstMeshVert, F->Q3Face.firstVertex, F->Q3Face.numMeshVert/3, F->ApproxPos, F->LightmapTexture );
			break;
		case BSPFACE_BILLBOARD:
			DQRender.AddBillboard( F->Billboard );
			break;
		case BSPFACE_DYNAMICFACE:
			DQRenderStack.UpdateDynamicVerts( F->DynamicVB, 0, F->Q3Face.numVertex, &Vertices.Pos[F->Q3Face.firstVertex], &Vertices.TexCoords[F->Q3Face.firstVertex], &Vertices.Normal[F->Q3Face.firstVertex], &Vertices.Diffuse[F->Q3Face.firstVertex], &Vertices.LightmapCoords[F->Q3Face.firstVertex], F->ShaderHandle );
			DQRenderStack.DrawBSPPrimitive( F->DynamicVB, F->ShaderHandle, D3DPT_TRIANGLEFAN, 0, F->Q3Face.numVertex-2, F->ApproxPos, F->LightmapTexture);
			break;
		case BSPFACE_DYNAMICMESH:
			DQRenderStack.UpdateDynamicVerts( F->DynamicVB, 0, F->Q3Face.numVertex, &Vertices.Pos[F->Q3Face.firstVertex], &Vertices.TexCoords[F->Q3Face.firstVertex], &Vertices.Normal[F->Q3Face.firstVertex], &Vertices.Diffuse[F->Q3Face.firstVertex], &Vertices.LightmapCoords[F->Q3Face.firstVertex], F->ShaderHandle );
			DQRenderStack.DrawBSPIndexedPrimitive( F->DynamicVB, BSPMeshIndicesHandle, F->ShaderHandle, D3DPT_TRIANGLELIST, F->Q3Face.firstMeshVert, 0, F->Q3Face.numMeshVert/3, F->ApproxPos, F->LightmapTexture );
			break;
		};
	}
}

void c_BSP::MarkVisibleLeaves()
{
	register int LeafCluster,i;

	if(ThisCluster == LastCluster) return;

	if( ThisCluster < 0 ) {
		//Mark all leaves visible
		for(i=0 ; i<NumLeaves; ++i) Leaf[i].LastVisibleCluster = -1;
	}
	else {
		for(i=0 ; i<NumLeaves; ++i) {
			LeafCluster = Leaf[i].Cluster;
			if(LeafCluster<0 || VisData.vec[LeafCluster*VisData.sizeVec + ThisCluster/8] & (1<<(ThisCluster%8)) ) {
				Leaf[i].LastVisibleCluster = ThisCluster;
			}
		}
	}
}

void c_BSP::RenderSkybox()
{
/*	int i, Pass;

	if(!VBSkyBox) CreateSkyBox(SkyCloudHeight);

	DrawSky = TRUE;
	MATRIX ViewPos;
	D3DXMatrixIdentity( &ViewPos );
	ViewPos._41 = DQCamera.Position.x;
	ViewPos._42 = DQCamera.Position.y;
	ViewPos._43 = DQCamera.Position.z;
	d3dcheckOK( DQDevice->SetTransform( D3DTS_WORLD, &ViewPos ) );

	d3dcheckOK( DQDevice->SetStreamSource( 0, VBSkyBox, 0, sizeof(s_VertexStreamPosTex) ) );
	d3dcheckOK( DQDevice->SetFVF( FVF_PosTex ) );
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE ) );
	for(i=0;i<6;++i) {
		if(!SkyBoxTexture[i]) continue;
		d3dcheckOK( DQDevice->SetTexture( 0, SkyBoxTexture[i] ) );
		d3dcheckOK( DQDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, i*4, 2 ) );
	}

	if(!SkyBoxShaderHandle)
		SkyBoxShaderHandle = DQRender.RegisterShader( SkyBoxFilename, ShaderFlag_DoNotUseMipmaps );
	for(Pass=0;Pass<DQRender.ShaderGetNumPasses(SkyBoxShaderHandle);++Pass) {
		DQRender.RunShader( SkyBoxShaderHandle, NULL );
		d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE ) );
		d3dcheckOK( DQDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 ) );
		d3dcheckOK( DQDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4, 2 ) );
		d3dcheckOK( DQDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 8, 2 ) );
		d3dcheckOK( DQDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 12, 2 ) );
		d3dcheckOK( DQDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 16, 2 ) );
		//Draw SkyDome
		//...
	}
	d3dcheckOK( DQDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE ) );
	DrawSky = FALSE;*/
}
