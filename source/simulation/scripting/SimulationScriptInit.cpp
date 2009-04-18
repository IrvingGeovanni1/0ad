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

#include "precompiled.h"

#include "lib/timer.h"
#include "simulation/Scheduler.h"
#include "simulation/EntityTemplate.h"
#include "simulation/EntityTemplateCollection.h"
#include "simulation/Entity.h"
#include "simulation/Projectile.h"
#include "simulation/EventHandlers.h"
#include "simulation/TriggerManager.h"
#include "simulation/FormationManager.h"
#include "simulation/FormationCollection.h"
#include "simulation/ProductionQueue.h"
#include "simulation/Technology.h"
#include "simulation/TechnologyCollection.h"
#include "simulation/PathfindEngine.h"

void SimulationScriptInit()
{
	CJSProgressTimer::ScriptingInit();
	CEntityTemplate::ScriptingInit();
	CEntity::ScriptingInit();
	CProjectile::ScriptingInit();
	CTrigger::ScriptingInit();
	CProductionItem::ScriptingInit();
	CProductionQueue::ScriptingInit();
	CTechnology::ScriptingInit();

	EntityCollection::Init( "EntityCollection" );

	g_ScriptingHost.DefineConstant( "FORMATION_ENTER", CFormationEvent::FORMATION_ENTER );
	g_ScriptingHost.DefineConstant( "FORMATION_LEAVE", CFormationEvent::FORMATION_LEAVE );
	g_ScriptingHost.DefineConstant( "FORMATION_DAMAGE", CFormationEvent::FORMATION_DAMAGE );
	g_ScriptingHost.DefineConstant( "FORMATION_ATTACK", CFormationEvent::FORMATION_ATTACK );

	g_ScriptingHost.DefineConstant( "NOTIFY_NONE", CEntityListener::NOTIFY_NONE );
	g_ScriptingHost.DefineConstant( "NOTIFY_GOTO", CEntityListener::NOTIFY_GOTO );
	g_ScriptingHost.DefineConstant( "NOTIFY_RUN", CEntityListener::NOTIFY_RUN );
	g_ScriptingHost.DefineConstant( "NOTIFY_FOLLOW", CEntityListener::NOTIFY_FOLLOW );
	g_ScriptingHost.DefineConstant( "NOTIFY_ATTACK", CEntityListener::NOTIFY_ATTACK );
	g_ScriptingHost.DefineConstant( "NOTIFY_DAMAGE", CEntityListener::NOTIFY_DAMAGE );
	g_ScriptingHost.DefineConstant( "NOTIFY_COMBAT", CEntityListener::NOTIFY_COMBAT );
	g_ScriptingHost.DefineConstant( "NOTIFY_ESCORT", CEntityListener::NOTIFY_ESCORT );
	g_ScriptingHost.DefineConstant( "NOTIFY_HEAL", CEntityListener::NOTIFY_HEAL );
	g_ScriptingHost.DefineConstant( "NOTIFY_GATHER", CEntityListener::NOTIFY_GATHER );
	g_ScriptingHost.DefineConstant( "NOTIFY_IDLE", CEntityListener::NOTIFY_IDLE );
	g_ScriptingHost.DefineConstant( "NOTIFY_ORDER_CHANGE", CEntityListener::NOTIFY_ORDER_CHANGE );
	g_ScriptingHost.DefineConstant( "NOTIFY_ALL", CEntityListener::NOTIFY_ALL );

	g_ScriptingHost.DefineConstant( "ORDER_NONE", -1 );
	g_ScriptingHost.DefineConstant( "ORDER_GOTO", CEntityOrder::ORDER_GOTO );
	g_ScriptingHost.DefineConstant( "ORDER_RUN", CEntityOrder::ORDER_RUN );
	g_ScriptingHost.DefineConstant( "ORDER_PATROL", CEntityOrder::ORDER_PATROL );
	g_ScriptingHost.DefineConstant( "ORDER_CONTACT_ACTION", CEntityOrder::ORDER_CONTACT_ACTION );
	g_ScriptingHost.DefineConstant( "ORDER_PRODUCE", CEntityOrder::ORDER_PRODUCE );
	g_ScriptingHost.DefineConstant( "ORDER_SET_RALLY_POINT", CEntityOrder::ORDER_SET_RALLY_POINT );
	g_ScriptingHost.DefineConstant( "ORDER_SET_STANCE", CEntityOrder::ORDER_SET_STANCE );
	g_ScriptingHost.DefineConstant( "ORDER_START_CONSTRUCTION", CEntityOrder::ORDER_START_CONSTRUCTION );
}


void SimulationInit()
{
	TIMER("SimulationInit");

	new CEntityTemplateCollection;
	new CFormationCollection;
	new CTechnologyCollection;
	g_EntityFormationCollection.LoadTemplates();
	g_TechnologyCollection.LoadTechnologies();
	new CFormationManager;
	new CTriggerManager;
	g_TriggerManager.LoadXml(CStr("scripts/TriggerSpecs.xml"));
	g_ScriptingHost.RunScript("scripts/trigger_functions.js");

	// CEntityManager is managed by CWorld
	//new CEntityManager;

	new CPathfindEngine;
}

void SimulationShutdown()
{
	TIMER_BEGIN("shutdown Pathfinder");
	delete &g_Pathfinder;
	TIMER_END("shutdown Pathfinder");

	// Managed by CWorld
	// delete &g_EntityManager;

	delete &g_TriggerManager;
	delete &g_FormationManager;
	delete &g_TechnologyCollection;
	delete &g_EntityFormationCollection;
	delete &g_EntityTemplateCollection;
}
