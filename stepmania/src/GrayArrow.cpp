#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 Class: GrayArrow

 Desc: A gray arrow that "receives" ColorArrows.

 Copyright (c) 2001-2002 by the persons listed below.  All rights reserved.
	Ben Nordstrom
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "GrayArrow.h"
#include "ThemeManager.h"

const float GRAY_ARROW_POP_UP_TIME			= 0.3f;


GrayArrow::GrayArrow()
{
	StopAnimating();
}

void GrayArrow::SetBeat( const float fSongBeat )
{
	int iNewState = (int)( fmod(fSongBeat,1) * Sprite::GetNumStates() );
	if( iNewState < 0 )
		iNewState += Sprite::GetNumStates();
	SetState( iNewState );
}

void GrayArrow::Step()
{
	SetZoom( 0.50 );
	BeginTweening( GRAY_ARROW_POP_UP_TIME );
	SetTweenZoom( 1.0f );
}
