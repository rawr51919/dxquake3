// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Camera.cpp: implementation of the c_Camera class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "c_Camera.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

c_Camera::c_Camera()
{
}

c_Camera::~c_Camera()
{

}

void c_Camera::Initialise()
{
	//Create default ViewPort
	DefaultViewPort.X = 0;
	DefaultViewPort.Y = 0;
	DefaultViewPort.Height = DQRender.GLconfig.vidHeight;
	DefaultViewPort.Width = DQRender.GLconfig.vidWidth;
	DefaultViewPort.MinZ = 0.0f;
	DefaultViewPort.MaxZ = 1.0f;
//	zNear = 4.0f;
	fov_x = D3DX_PI/2.0f;
	fov_y = D3DX_PI/3.0f;

	Reset();
}

void c_Camera::CreateProjectionMatrix( float z_near, float z_far )
{
	float Q = z_far/(z_far-z_near);
	Projection._11 = (float)1.0f/tan(fov_x/2.0f);
	Projection._21 = 0.0f;
	Projection._31 = 0.0f;
	Projection._41 = 0.0f;
	Projection._12 = 0.0f;
	Projection._22 = (float)1.0f/tan(fov_y/2.0f);
	Projection._32 = 0.0f;
	Projection._42 = 0.0f;
	Projection._13 = 0.0f;
	Projection._23 = 0.0f;
	Projection._33 = Q;
	Projection._43 = -z_near*Q;
	Projection._14 = 0.0f;
	Projection._24 = 0.0f;
	Projection._34 = 1.0f;
	Projection._44 = 0.0f;
}

void c_Camera::SetWithRefdef( refdef_t *refdef )
{
	//Clamp viewport to screen
	refdef->width = min( refdef->width, DQRender.GLconfig.vidWidth );
	refdef->height = min( refdef->height, DQRender.GLconfig.vidHeight );

	refdef->x = max( 0, refdef->x );
	refdef->y = max( 0, refdef->y );
	if(refdef->x + refdef->width > DQRender.GLconfig.vidWidth) {
		refdef->width = DQRender.GLconfig.vidWidth - refdef->x;
	}

	if(refdef->y + refdef->height > DQRender.GLconfig.vidHeight) {
		refdef->height = DQRender.GLconfig.vidHeight - refdef->y;
	}

	//Set Viewport
	//(D3DViewport set during render)
	CurrentViewPort.X = refdef->x;
	CurrentViewPort.Y = refdef->y;
	CurrentViewPort.Width = refdef->width;
	CurrentViewPort.Height = refdef->height;

	renderfx = refdef->rdflags;
	memcpy( &areamask, &refdef->areamask, MAX_MAP_AREA_BYTES*sizeof(byte));
	RenderTime = (float)refdef->time;

	//convert degrees to radians
	fov_x = 2*D3DX_PI/360.0f*refdef->fov_x;
	fov_y = 2*D3DX_PI/360.0f*refdef->fov_y;

	Position = MakeD3DXVec3FromFloat3(refdef->vieworg);
	D3DXMatrixTranslation( &ViewTrans, -Position.x, -Position.y, -Position.z );

	ViewRot._11 = -refdef->viewaxis[1][0];
	ViewRot._12 = refdef->viewaxis[2][0];
	ViewRot._13 = refdef->viewaxis[0][0];
	ViewRot._21 = -refdef->viewaxis[1][1];
	ViewRot._22 = refdef->viewaxis[2][1];
	ViewRot._23 = refdef->viewaxis[0][1];
	ViewRot._31 = -refdef->viewaxis[1][2];
	ViewRot._32 = refdef->viewaxis[2][2];
	ViewRot._33 = refdef->viewaxis[0][2];
	//D3DXMatrixInverse(&ViewRot, NULL, &ViewRot);

	View = ViewTrans*ViewRot;

	UpdateFrustum();

	CreateProjectionMatrix( ZNEAR, ZFAR+100.0f );
	ViewProj = View*Projection;

	CameraZDir = D3DXVECTOR3( DQCamera.View._13, DQCamera.View._23, DQCamera.View._33 );
	CameraZPos = D3DXVec3Dot( &Position, &CameraZDir );
}
/*
void c_Camera::SetZNear( float _zNear )
{
	zNear = max( 4.0f, _zNear );
	CreateProjectionMatrix( zNear, ZFAR+100.0f );
	ViewProj = View*Projection;
}*/

void c_Camera::Reset()
{
	CurrentViewPort = DefaultViewPort;
//	zNear = 4.0f;

	ViewRot = MATRIX(-1.0f, 0.0f, 0.0f, 0.0f,
					  0.0f, 0.0f, 1.0f, 0.0f,
					  0.0f, 1.0f, 0.0f, 0.0f,
					  0.0f, 0.0f, 0.0f, 1.0f);
	Position = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
	D3DXMatrixTranslation( &ViewTrans, -Position.x, -Position.y, -Position.z );

	//Set up Projection Matrix (View Matrix->Screen)
	D3DXMatrixPerspectiveFovLH( &Projection, 1.28700221f			//FOV y-axis=2*arctan(1/A*tan(90/2))
		, 800.0f/600.0f, 4.0f, 10000.0f); 		//Aspect Ratio, Near Clip, Far Clip

	if(DQRender.IsFullscreen()) {
		MidPoint.x = DQRender.GetPresentParameters()->BackBufferWidth/2;
		MidPoint.y = DQRender.GetPresentParameters()->BackBufferHeight/2;
	}
	else {
		RECT rect;
		GetWindowRect(DQRender.GetHWnd(), &rect);
		MidPoint.x = (rect.right-rect.left)/2+rect.left;
		MidPoint.y = (rect.bottom-rect.top)/2+rect.top;
	}
	View = ViewTrans*ViewRot;
}

void c_Camera::SetD3D()
{
	d3dcheckOK( DQDevice->SetTransform( D3DTS_PROJECTION, &Projection) );
	d3dcheckOK( DQDevice->SetTransform( D3DTS_VIEW, &View) );
	d3dcheckOK( DQDevice->SetViewport( &CurrentViewPort ) );
}

void c_Camera::SetIdentityD3D()
{
	d3dcheckOK( DQDevice->SetTransform( D3DTS_PROJECTION, &DQRender.matIdentity) );
	d3dcheckOK( DQDevice->SetTransform( D3DTS_VIEW, &DQRender.matIdentity) );
	d3dcheckOK( DQDevice->SetViewport( &DefaultViewPort ) );
}

void c_Camera::UpdateFrustum()
{
	Frustum.Forward.Normal = D3DXVECTOR3( ViewRot._13, ViewRot._23, ViewRot._33 );
	Frustum.Forward.Dist = D3DXVec3Dot( &Position, &Frustum.Forward.Normal );

	Frustum.Right.Normal = D3DXVECTOR3( ViewRot._11, ViewRot._21, ViewRot._31 );
	Frustum.Right.Dist = D3DXVec3Dot( &Position, &Frustum.Right.Normal );

	Frustum.Up.Normal = D3DXVECTOR3( ViewRot._12, ViewRot._22, ViewRot._32 );
	Frustum.Up.Dist = D3DXVec3Dot( &Position, &Frustum.Up.Normal );

	Frustum.ForwardPositive.x = max( Frustum.Forward.Normal.x, -Frustum.Forward.Normal.x);
	Frustum.ForwardPositive.y = max( Frustum.Forward.Normal.y, -Frustum.Forward.Normal.y);
	Frustum.ForwardPositive.z = max( Frustum.Forward.Normal.z, -Frustum.Forward.Normal.z);

	Frustum.RightPositive.x = max( Frustum.Right.Normal.x, -Frustum.Right.Normal.x);
	Frustum.RightPositive.y = max( Frustum.Right.Normal.y, -Frustum.Right.Normal.y);
	Frustum.RightPositive.z = max( Frustum.Right.Normal.z, -Frustum.Right.Normal.z);

	Frustum.UpPositive.x = max( Frustum.Up.Normal.x, -Frustum.Up.Normal.x);
	Frustum.UpPositive.y = max( Frustum.Up.Normal.y, -Frustum.Up.Normal.y);
	Frustum.UpPositive.z = max( Frustum.Up.Normal.z, -Frustum.Up.Normal.z);

	Frustum.ratio_x = atan( fov_x );
	Frustum.ratio_y = atan( fov_y );
}