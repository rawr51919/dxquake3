// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_Camera.h: interface for the c_Camera class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_CAMERA_H__97FE7F96_E423_4DBF_A644_3792C89DB1DF__INCLUDED_)
#define AFX_C_CAMERA_H__97FE7F96_E423_4DBF_A644_3792C89DB1DF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define ZNEAR 4.0f
#define ZFAR 10000.0f

struct s_Frustum {
	s_Plane Forward, Up, Right;
	D3DXVECTOR3 ForwardPositive, UpPositive, RightPositive;
	float ratio_x, ratio_y;
};

#define Vec3LessThan( v1, v2 ) ( (v1.x<v2.x) && (v1.y<v2.y) && (v1.z<v2.z) )
#define Vec3MoreThan( v1, v2 ) ( (v1.x>v2.x) && (v1.y>v2.y) && (v1.z>v2.z) )

enum eCullState { eCullNotVisible=0, eCullTotallyVisible=1, eCullPartiallyVisible=2 };

class c_Camera
{
public:
	c_Camera();
	virtual ~c_Camera();
	void c_Camera::Reset();
	void SetWithRefdef(refdef_t *refdef);
	void c_Camera::SetD3D();
	void c_Camera::SetIdentityD3D();
	void c_Camera::Initialise();
//	void c_Camera::SetZNear( float _zNear );

	MATRIX ViewRot,ViewTrans,Projection,View,ViewProj;
	D3DXVECTOR3 Position, CameraZDir;
	float CameraZPos;
	D3DVIEWPORT9 DefaultViewPort, CurrentViewPort;
	int renderfx;
	byte areamask[MAX_MAP_AREA_BYTES];
	float RenderTime;

	inline eCullState IsBoxVisible( s_Box &BBox )
	{
		static D3DXVECTOR3 temp1, temp2;
		register float distance, offset, maxdist;
		float maxdist_x, maxdist_y;

		//check if Camera is contained inside BBox
		temp1 = BBox.MidpointPos + BBox.Extent;
		temp2 = BBox.MidpointPos - BBox.Extent;
		if( Vec3LessThan( Position, temp1) && Vec3MoreThan( Position, temp2 ) )  {
			//camera inside box
			return eCullPartiallyVisible;
		}

		//Front plane
/*		offset = fabs( BBox.Extent.x * Frustum.Forward.Normal.x )
					+ fabs( BBox.Extent.y * Frustum.Forward.Normal.y )
					+ fabs( BBox.Extent.z * Frustum.Forward.Normal.z );*/
		offset = D3DXVec3Dot( &BBox.Extent, &Frustum.ForwardPositive );
		distance = D3DXVec3Dot( &BBox.MidpointPos, &Frustum.Forward.Normal ) - Frustum.Forward.Dist;
		if( distance < -offset ) return eCullNotVisible;		//box totally behind plane
		if( distance < offset ) return eCullPartiallyVisible;	//box spanning plane
		//else box is entirely infront of plane

		maxdist_x = Frustum.ratio_x * (distance+offset);
		maxdist_y = Frustum.ratio_y * (distance+offset);

		//Horizontal plane
		maxdist = maxdist_x;
/*		offset = fabs( BBox.Extent.x * Frustum.Right.Normal.x )
					+ fabs( BBox.Extent.y * Frustum.Right.Normal.y )
					+ fabs( BBox.Extent.z * Frustum.Right.Normal.z );*/
		offset = D3DXVec3Dot( &BBox.Extent, &Frustum.RightPositive );
		distance = D3DXVec3Dot( &BBox.MidpointPos, &Frustum.Right.Normal ) - Frustum.Right.Dist;
		if( (distance+offset < -maxdist) || (distance-offset > maxdist) ) return eCullNotVisible;			//box totally outside planes
		if( (distance-offset < -maxdist) || (distance+offset > maxdist)  ) return eCullPartiallyVisible;	//box spanning planes
		//else box inside planes

		//Vertical plane
		maxdist = maxdist_y;
/*		offset = fabs( BBox.Extent.x * Frustum.Up.Normal.x )
					+ fabs( BBox.Extent.y * Frustum.Up.Normal.y )
					+ fabs( BBox.Extent.z * Frustum.Up.Normal.z );*/
		offset = D3DXVec3Dot( &BBox.Extent, &Frustum.UpPositive );
		distance = D3DXVec3Dot( &BBox.MidpointPos, &Frustum.Up.Normal ) - Frustum.Up.Dist;
		if( (distance+offset < -maxdist) || (distance-offset > maxdist) ) return eCullNotVisible;			//box totally outside planes
		if( (distance-offset < -maxdist) || (distance+offset > maxdist)  ) return eCullPartiallyVisible;	//box spanning planes

		//Box completely inside frustum (ignoring far plane)
		return eCullTotallyVisible;
	}

private:
	POINT MidPoint;
	s_Frustum Frustum;
//	float zNear;

	float fov_x, fov_y;		//in radians

	void c_Camera::UpdateFrustum();
	void c_Camera::CreateProjectionMatrix( float z_near, float z_far );
};

#endif // !defined(AFX_C_CAMERA_H__97FE7F96_E423_4DBF_A644_3792C89DB1DF__INCLUDED_)
