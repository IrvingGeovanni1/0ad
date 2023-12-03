/* Copyright (C) 2021 Wildfire Games.
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

#include "TurnManager.h"

#include "gui/GUIManager.h"
#include "maths/MathUtil.h"
#include "ps/Pyrogenesis.h"
#include "ps/Profile.h"
#include "ps/CLogger.h"
#include "ps/Replay.h"
#include "ps/Util.h"
#include "scriptinterface/Object.h"
#include "simulation2/Simulation2.h"

#if 0
#define NETTURN_LOG(...) debug_printf(__VA_ARGS__)
#else
#define NETTURN_LOG(...)
#endif

const CStr CTurnManager::EventNameSavegameLoaded = "SavegameLoaded";

CTurnManager::CTurnManager(CSimulation2& simulation, u32 defaultTurnLength, u32 commandDelay, int clientId, IReplayLogger& replay)
	: m_Simulation2(simulation), m_CurrentTurn(0), m_CommandDelay(commandDelay), m_ReadyTurn(commandDelay - 1), m_TurnLength(defaultTurnLength),
	m_PlayerId(-1), m_ClientId(clientId), m_DeltaSimTime(0), m_Replay(replay),
	m_FinalTurn(std::numeric_limits<u32>::max()), m_TimeWarpNumTurns(0)
{
	ScriptRequest rq(m_Simulation2.GetScriptInterface());
	m_QuickSaveMetadata.init(rq.cx);
	m_QueuedCommands.resize(1);
}

void CTurnManager::ResetState(u32 newCurrentTurn, u32 newReadyTurn)
{
	m_CurrentTurn = newCurrentTurn;
	m_ReadyTurn = newReadyTurn;
	m_DeltaSimTime = 0;
	size_t queuedCommandsSize = m_QueuedCommands.size();
	m_QueuedCommands.clear();
	m_QueuedCommands.resize(queuedCommandsSize);
}

void CTurnManager::SetPlayerID(int playerId)
{
	m_PlayerId = playerId;
}

bool CTurnManager::Update(float simFrameLength, size_t maxTurns)
{
	if (m_CurrentTurn > m_FinalTurn)
		return false;

	m_DeltaSimTime += simFrameLength;

	// If the game becomes laggy, m_DeltaSimTime increases progressively.
	// The engine will fast forward accordingly to catch up.
	// To keep the game playable, stop fast forwarding after 2 turn lengths.
	m_DeltaSimTime = std::min(m_DeltaSimTime, 2.0f * m_TurnLength / 1000.0f);

	// If we haven't reached the next turn yet, do nothing
	if (m_DeltaSimTime < 0)
		return false;

	NETTURN_LOG("Update current=%d ready=%d\n", m_CurrentTurn, m_ReadyTurn);

	// Check that the next turn is ready for execution
	if (m_ReadyTurn <= m_CurrentTurn && m_CommandDelay > 1)
	{
		// Oops, we wanted to start the next turn but it's not ready yet -
		// there must be too much network lag.
		// TODO: complain to the user.
		// TODO: send feedback to the server to increase the turn length.

		// Reset the next-turn timer to 0 so we try again next update but
		// so we don't rush to catch up in subsequent turns.
		// TODO: we should do clever rate adjustment instead of just pausing like this.
		m_DeltaSimTime = 0;

		return false;
	}

	maxTurns = std::max((size_t)1, maxTurns); // always do at least one turn

	for (size_t i = 0; i < maxTurns; ++i)
	{
		// Check that we've reached the i'th next turn
		if (m_DeltaSimTime < 0)
			break;

		// Check that the i'th next turn is still ready
		if (m_ReadyTurn <= m_CurrentTurn && m_CommandDelay > 1)
			break;

		// To avoid confusing the profiler, we need to trigger the new turn
		// while we're not nested inside any PROFILE blocks
		g_Profiler.Turn();

		NotifyFinishedOwnCommands(m_CurrentTurn + m_CommandDelay);

		// Increase now, so Update can send new commands for a subsequent turn
		++m_CurrentTurn;

		// Clean up any destroyed entities since the last turn (e.g. placement previews
		// or rally point flags generated by the GUI). (Must do this before the time warp
		// serialization.)
		m_Simulation2.FlushDestroyedEntities();

		// Save the current state for rewinding, if enabled
		if (m_TimeWarpNumTurns && (m_CurrentTurn % m_TimeWarpNumTurns) == 0)
		{
			PROFILE3("time warp serialization");
			std::stringstream stream;
			m_Simulation2.SerializeState(stream);
			m_TimeWarpStates.push_back(stream.str());
		}

		// Put all the client commands into a single list, in a globally consistent order
		std::vector<SimulationCommand> commands;
		for (std::pair<const u32, std::vector<SimulationCommand>>& p : m_QueuedCommands[0])
			commands.insert(commands.end(), std::make_move_iterator(p.second.begin()), std::make_move_iterator(p.second.end()));

		m_QueuedCommands.pop_front();
		m_QueuedCommands.resize(m_QueuedCommands.size() + 1);

		m_Replay.Turn(m_CurrentTurn-1, m_TurnLength, commands);

		NETTURN_LOG("Running %d cmds\n", commands.size());

		m_Simulation2.Update(m_TurnLength, commands);

		NotifyFinishedUpdate(m_CurrentTurn);

		// Set the time for the next turn update
		m_DeltaSimTime -= m_TurnLength / 1000.f;
	}

	return true;
}

bool CTurnManager::UpdateFastForward()
{
	m_DeltaSimTime = 0;

	NETTURN_LOG("UpdateFastForward current=%d ready=%d\n", m_CurrentTurn, m_ReadyTurn);

	// Check that the next turn is ready for execution
	if (m_ReadyTurn <= m_CurrentTurn)
		return false;

	while (m_ReadyTurn > m_CurrentTurn)
	{
		// TODO: It would be nice to remove some of the duplication with Update()
		// (This is similar but doesn't call any Notify functions or update DeltaTime,
		// it just updates the simulation state)

		++m_CurrentTurn;

		m_Simulation2.FlushDestroyedEntities();

		// Put all the client commands into a single list, in a globally consistent order
		std::vector<SimulationCommand> commands;
		for (std::pair<const u32, std::vector<SimulationCommand>>& p : m_QueuedCommands[0])
			commands.insert(commands.end(), std::make_move_iterator(p.second.begin()), std::make_move_iterator(p.second.end()));

		m_QueuedCommands.pop_front();
		m_QueuedCommands.resize(m_QueuedCommands.size() + 1);

		m_Replay.Turn(m_CurrentTurn-1, m_TurnLength, commands);

		NETTURN_LOG("Running %d cmds\n", commands.size());

		m_Simulation2.Update(m_TurnLength, commands);
	}

	return true;
}

void CTurnManager::Interpolate(float simFrameLength, float realFrameLength)
{
	// TODO: using m_TurnLength might be a bit dodgy when length changes - maybe
	// we need to save the previous turn length?

	float offset = Clamp(m_DeltaSimTime / (m_TurnLength / 1000.f) + 1.0, 0.0, 1.0);

	// Stop animations while still updating the selection highlight
	if (m_CurrentTurn > m_FinalTurn)
		simFrameLength = 0;

	m_Simulation2.Interpolate(simFrameLength, offset, realFrameLength);
}

void CTurnManager::AddCommand(int client, int player, JS::HandleValue data, u32 turn)
{
	NETTURN_LOG("AddCommand(client=%d player=%d turn=%d current=%d, ready=%d)\n", client, player, turn, m_CurrentTurn, m_ReadyTurn);

	// Reject commands for turns that we should not be able to compute (in the past).
	if (m_CurrentTurn >= turn)
	{
		// The most likely explanation is that an observer that's lagging behind is sending commands,
		// which is possible when cheats are enabled. Report & ignore.
		// It seems a bad idea to error out too badly here:
		// nefarious clients could try and send broken commands to DOS.
		LOGWARNING("Received command for invalid turn %i (current turn is %i)", turn, m_CurrentTurn);
		return;
	}

	ScriptRequest rq(m_Simulation2.GetScriptInterface());

	Script::FreezeObject(rq, data, true);

	size_t command_in_turns = turn - (m_CurrentTurn+1);
	if (m_QueuedCommands.size() <= command_in_turns)
		m_QueuedCommands.resize(command_in_turns+1);
	m_QueuedCommands[turn - (m_CurrentTurn+1)][client].emplace_back(player, rq.cx, data);
}

void CTurnManager::FinishedAllCommands(u32 turn, u32 turnLength)
{
	NETTURN_LOG("FinishedAllCommands(%d, %d)\n", turn, turnLength);

	ENSURE(turn == m_ReadyTurn + 1);
	m_ReadyTurn = turn;
	m_TurnLength = turnLength;
}

bool CTurnManager::TurnNeedsFullHash(u32 turn) const
{
	// Check immediately for errors caused by e.g. inconsistent game versions
	// (The hash is computed after the first sim update, so we start at turn == 1)
	if (turn == 1)
		return true;

	// Otherwise check the full state every ~10 seconds in multiplayer games
	// (TODO: should probably remove this when we're reasonably sure the game
	// isn't too buggy, since the full hash is still pretty slow)
	if (turn % 20 == 0)
		return true;

	return false;
}

void CTurnManager::EnableTimeWarpRecording(size_t numTurns)
{
	m_TimeWarpStates.clear();
	m_TimeWarpNumTurns = numTurns;
}

void CTurnManager::RewindTimeWarp()
{
	if (m_TimeWarpStates.empty())
		return;

	std::stringstream stream(m_TimeWarpStates.back());
	m_Simulation2.DeserializeState(stream);
	m_TimeWarpStates.pop_back();

	// Reset the turn manager state, so we won't execute stray commands and
	// won't do the next snapshot until the appropriate time.
	// (Ideally we ought to serialise the turn manager state and restore it
	// here, but this is simpler for now.)
	ResetState(1, m_CommandDelay);
}

void CTurnManager::QuickSave(JS::HandleValue GUIMetadata)
{
	TIMER(L"QuickSave");

	std::stringstream stream;
	if (!m_Simulation2.SerializeState(stream))
	{
		LOGERROR("Failed to quicksave game");
		return;
	}

	m_QuickSaveState = stream.str();

	ScriptRequest rq(m_Simulation2.GetScriptInterface());

	m_QuickSaveMetadata.set(Script::DeepCopy(rq, GUIMetadata));
	// Freeze state to ensure that consectuvie loads don't modify the state
	Script::FreezeObject(rq, m_QuickSaveMetadata, true);

	LOGMESSAGERENDER("Quicksaved game");
}

void CTurnManager::QuickLoad()
{
	TIMER(L"QuickLoad");

	if (m_QuickSaveState.empty())
	{
		LOGERROR("Cannot quickload game - no game was quicksaved");
		return;
	}

	std::stringstream stream(m_QuickSaveState);
	if (!m_Simulation2.DeserializeState(stream))
	{
		LOGERROR("Failed to quickload game");
		return;
	}

	// See RewindTimeWarp
	ResetState(1, m_CommandDelay);

	if (!g_GUI)
		return;

	ScriptRequest rq(m_Simulation2.GetScriptInterface());

	// Provide a copy, so that GUI components don't have to clone to get mutable objects
	JS::RootedValue quickSaveMetadataClone(rq.cx, Script::DeepCopy(rq, m_QuickSaveMetadata));

	JS::RootedValueArray<1> paramData(rq.cx);
	paramData[0].set(quickSaveMetadataClone);
	g_GUI->SendEventToAll(EventNameSavegameLoaded, paramData);

	LOGMESSAGERENDER("Quickloaded game");
}
