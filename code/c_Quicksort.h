// DXQuake3 Source
// Copyright (c) 2003 Richard Geary (richard.geary@dial.pipex.com)
//
//
//		Quicksort implementation for c_RenderStack
//

#ifndef __C_QUICKSORT_H
#define __C_QUICKSORT_H

// References : Quicksort Algorithm by James Gosling
//  - http://www.cs.ubc.ca/spider/harrison/Java/FastQSortAlgorithm.java.html
// GameDev.net : Understanding the QuickSort Algorithm by Joe Farrell
//  - http://www.gamedev.net/reference/articles/article1073.asp.

struct s_QuicksortUnit {
	int index;
	float ZDistance;					//calculated by SortStack from ApproxPos and Camera Orientation
};

#define DEFAULT_QUICKSORTSIZE 3

//Has a DefaultArray as many instances may be made without using them
class c_QuicksortArray
{
public:
	c_QuicksortArray():ArraySize(DEFAULT_QUICKSORTSIZE),NextElement(0),Array(DefaultArray) {}
	c_QuicksortArray(const unsigned int _ArraySize):ArraySize(_ArraySize),NextElement(0) 
	{
		Array = (s_QuicksortUnit*)DQNewVoid( s_QuicksortUnit[ArraySize] ); 
	}

	~c_QuicksortArray() { 
		if( Array != DefaultArray ) {
			DQDeleteArray(Array); 
		}
	}

	s_QuicksortUnit *Array;
	s_QuicksortUnit DefaultArray[DEFAULT_QUICKSORTSIZE];

	int NextElement;
	unsigned int ArraySize;

	inline void IncreaseSize(const unsigned int NewArraySize) 
	{
		ArraySize = NewArraySize;

		s_QuicksortUnit *temp;
		temp = (s_QuicksortUnit*)DQNewVoid( s_QuicksortUnit[ArraySize] );		//replace with bigger array

		memcpy( temp, Array, sizeof(s_QuicksortUnit)*NextElement );

		if( Array != DefaultArray ) {
			DQDeleteArray( Array ); 
		}

		Array = temp;		
	}

private:

	inline void Swap( int Elem1, int Elem2 )
	{
		register s_QuicksortUnit temp;
		temp = Array[Elem1];
		Array[Elem1] = Array[Elem2];
		Array[Elem2] = temp;		
	}

public:
	//Sort Lowest ZDistance to Highest (Front->Back)
	void Quicksort( int Low, int High )
	{
		int PartitionElement;
		register int Bottom, Top;
		register float PartitionValue;

		if( Low < High ) {
			if( High == Low+1 ) {
				if( Array[Low].ZDistance > Array[High].ZDistance ) {
					Swap( Low, High );
				}
			}
			else
			{
				PartitionElement = (Low+High)/2;

				//Swap PartitionElement and High Element
				PartitionValue = Array[PartitionElement].ZDistance;

				Swap( PartitionElement, High );

				//Initialise
				Bottom = Low;
				Top = High;

				while( Top>Bottom ) {
					while( (Array[Bottom].ZDistance <= PartitionValue) && (Top>Bottom) ) ++Bottom;
					while( (Array[Top].ZDistance >= PartitionValue) && (Top>Bottom) ) --Top;
					if( Top>Bottom ) {
						Swap( Top, Bottom );
					}
				}

				Swap( Bottom, High );

				Quicksort( Low, Bottom-1 );
				Quicksort( Bottom+1, High );
			}
		}
	}

	//Sort Highest ZDistance to Lowest (Back->Front)
	void QuicksortInverse( int Low, int High )
	{
		int PartitionElement;
		register int Bottom, Top;
		register float PartitionValue;

		if( Low < High ) {
			if( High == Low+1 ) {
				if( Array[Low].ZDistance < Array[High].ZDistance ) {
					Swap( Low, High );
				}
			}
			else
			{
				PartitionElement = (Low+High)/2;

				//Swap PartitionElement and High Element
				PartitionValue = Array[PartitionElement].ZDistance;

				Swap( PartitionElement, High );

				//Initialise
				Bottom = Low;
				Top = High;

				while( Top>Bottom ) {
					while( (Array[Bottom].ZDistance >= PartitionValue) && (Top>Bottom) ) ++Bottom;
					while( (Array[Top].ZDistance <= PartitionValue) && (Top>Bottom) ) --Top;
					if( Top>Bottom ) {
						Swap( Top, Bottom );
					}
				};

				Swap( Bottom, High );

				QuicksortInverse( Low, Bottom-1 );
				QuicksortInverse( Bottom+1, High );
			}
		}
	}
};

#endif