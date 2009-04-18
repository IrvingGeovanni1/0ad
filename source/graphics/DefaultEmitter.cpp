/* Copyright (C) 2009 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * =========================================================================
 * File        : DefaultEmitter.cpp
 * Project     : 0 A.D.
 * Description : Default particle emitter implementation.
 * =========================================================================
 */

#include "precompiled.h"
#include "maths/MathUtil.h"
#include "DefaultEmitter.h"

CDefaultEmitter::CDefaultEmitter(const int MAX_PARTICLES, const int lifetime) : CEmitter(MAX_PARTICLES, lifetime)
{
	Setup();
}

CDefaultEmitter::~CDefaultEmitter(void)
{
}

bool CDefaultEmitter::Setup()
{
	// XYZ Position
	m_pos.X = 0.0f;
	m_pos.Y = 20.0f;
	m_pos.Z = 0.0f;

	m_yaw = DEGTORAD(0.0f);
	m_yawVar = DEGTORAD(360.0f);
	m_pitch = DEGTORAD(90.0f);
	m_pitchVar = DEGTORAD(45.0f);
	m_speed = 0.05f;
	m_speedVar = 0.001f;

	m_blendMode		= 1;
	m_particleCount	= 0;
	m_emitsPerFrame	= 100;
	m_emitsVar	= 15;
	m_life = 90;
	m_lifeVar = 65;
	m_startColor.r = 100;
	m_startColor.g = 100;
	m_startColor.b = 100;
	m_startColorVar.r = 15;
	m_startColorVar.g = 15;
	m_startColorVar.b = 15;
	m_endColor.r = 0;
	m_endColor.g = 0;
	m_endColor.b = 0;
	m_endColorVar.r = 15;
	m_endColorVar.g = 15;
	m_endColorVar.b = 15;

	m_force.X = 0.000f;
	m_force.Y = 0.001f;
	m_force.Z = 0.0f;
	return true;
}
