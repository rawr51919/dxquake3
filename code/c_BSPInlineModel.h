// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
// c_BSPInlineModel.h: interface for the c_BSPInlineModel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_C_BSPINLINEMODEL_H__8686CD85_A00A_4E04_9532_3B1EC6B66FA9__INCLUDED_)
#define AFX_C_BSPINLINEMODEL_H__8686CD85_A00A_4E04_9532_3B1EC6B66FA9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct s_BSPInlineModel  
{
	s_BoundingBox BBox;
	s_Sphere BSphere;
	int FirstBrush, FirstFace, NumBrushes, NumFaces;

};

#endif // !defined(AFX_C_BSPINLINEMODEL_H__8686CD85_A00A_4E04_9532_3B1EC6B66FA9__INCLUDED_)
