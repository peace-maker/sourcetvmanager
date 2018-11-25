/**
* vim: set ts=4 :
* =============================================================================
* SourceMod SourceTV Manager Extension
* Copyright (C) 2004-2016 AlliedModders LLC.  All rights reserved.
* =============================================================================
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License, version 3.0, as published by the
* Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*
* As a special exception, AlliedModders LLC gives you permission to link the
* code of this program (as well as its derivative works) to "Half-Life 2," the
* "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
* by the Valve Corporation.  You must obey the GNU General Public License in
* all respects for all other code used.  Additionally, AlliedModders LLC grants
* this exception to all derivative works.  AlliedModders LLC defines further
* exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
* or <http://www.sourcemod.net/license.php>.
*
* Version: $Id$
*/

#include "extension.h"
#include "forwards.h"
#include "hltvserverwrapper.h"
#include "commonhooks.h"

CForwardManager g_pSTVForwards;

// Only windows always uses the vtable for these. Linux does direct calls, so we use detours there.
#ifdef WIN32
SH_DECL_HOOK2_void(IDemoRecorder, StartRecording, SH_NOATTRIB, 0, const char *, bool)
#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0, CGameInfo const *)
#else
SH_DECL_HOOK0_void(IDemoRecorder, StopRecording, SH_NOATTRIB, 0)
#endif // SOURCE_ENGINE == SE_CSGO
#endif // !WIN32

#if SOURCE_ENGINE == SE_CSGO
SH_DECL_MANUALHOOK13(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, const netadr_t &, int, int, int, const char *, const char *, const char *, int, CUtlVector<NetMsg_SplitPlayerConnect *> &, bool, CrossPlayPlatform_t, const unsigned char *, int);
SH_DECL_MANUALHOOK1_void_vafmt(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &);
SH_DECL_HOOK1_void(IClient, Disconnect, SH_NOATTRIB, 0, const char *);
#ifndef WIN32
SH_DECL_MANUALHOOK1_void(CBaseClient_Disconnect, 0, 0, 0, const char *);
#endif // !WIN32

#elif SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
SH_DECL_MANUALHOOK10(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, const netadr_t &, int, int, int, const char *, const char *, const char *, int, CUtlVector<CLC_SplitPlayerConnect *> &, bool);
SH_DECL_MANUALHOOK1_void_vafmt(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &);
SH_DECL_HOOK0_void_vafmt(IClient, Disconnect, SH_NOATTRIB, 0);
#ifndef WIN32
SH_DECL_MANUALHOOK0_void_vafmt(CBaseClient_Disconnect, 0, 0, 0);
#endif // !WIN32

#else
SH_DECL_MANUALHOOK9(CHLTVServer_ConnectClient, 0, 0, 0, IClient *, netadr_t &, int, int, int, int, const char *, const char *, const char *, int);
SH_DECL_MANUALHOOK3_void(CHLTVServer_RejectConnection, 0, 0, 0, const netadr_t &, int, const char *);
SH_DECL_HOOK0_void_vafmt(IClient, Disconnect, SH_NOATTRIB, 0);
#ifndef WIN32
SH_DECL_MANUALHOOK0_void_vafmt(CBaseClient_Disconnect, 0, 0, 0);
#endif // !WIN32

#endif
SH_DECL_MANUALHOOK0_void(CBaseClient_ActivatePlayer, 0, 0, 0);

SH_DECL_MANUALHOOK1(CHLTVServer_GetChallengeType, 0, 0, 0, int, const netadr_t &);

SH_DECL_MANUALHOOK1(CBaseClient_ExecuteStringCommand, 0, 0, 0, bool, const char *);

void CForwardManager::Init()
{
	int offset = -1;
	if (!g_pGameConf->GetOffset("CHLTVServer::ConnectClient", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::ConnectClient offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_ConnectClient, offset, 0, 0);
		m_bHasClientConnectOffset = true;
	}

	if (!g_pGameConf->GetOffset("CHLTVServer::RejectConnection", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::RejectConnection offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_RejectConnection, offset, 0, 0);
		m_bHasRejectConnectionOffset = true;
	}

	if (!g_pGameConf->GetOffset("CHLTVServer::GetChallengeType", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CHLTVServer::GetChallengeType offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_GetChallengeType, offset, 0, 0);
		m_bHasGetChallengeTypeOffset = true;
	}

	if (!g_pGameConf->GetOffset("CBaseClient::ActivatePlayer", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::ActivatePlayer offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_ActivatePlayer, offset, 0, 0);
		m_bHasActivatePlayerOffset = true;
	}

#ifndef WIN32
	if (!g_pGameConf->GetOffset("CBaseClient::Disconnect", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::Disconnect offset.");
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_Disconnect, offset, 0, 0);
		m_bHasDisconnectOffset = true;
	}

	if (!g_pGameConf->GetOffset("CBaseClient::ExecuteStringCommand", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to get CBaseClient::ExecuteStringCommand offset.");
	}
	else
	{
		// vtbloffs must be 0, not sure about thisoffs
		SH_MANUALHOOK_RECONFIGURE(CBaseClient_ExecuteStringCommand, offset, 0, 0);
		m_bHasExecuteStringCommandOffset = true;
	}
#endif

	m_StartRecordingFwd = forwards->CreateForward("SourceTV_OnStartRecording", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_StopRecordingFwd = forwards->CreateForward("SourceTV_OnStopRecording", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_Cell);
	m_SpectatorPreConnectFwd = forwards->CreateForward("SourceTV_OnSpectatorPreConnect", ET_LowEvent, 4, NULL, Param_String, Param_String, Param_String, Param_String);
	m_SpectatorConnectedFwd = forwards->CreateForward("SourceTV_OnSpectatorConnected", ET_Ignore, 1, NULL, Param_Cell);
	m_SpectatorDisconnectFwd = forwards->CreateForward("SourceTV_OnSpectatorDisconnect", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_SpectatorDisconnectedFwd = forwards->CreateForward("SourceTV_OnSpectatorDisconnected", ET_Ignore, 2, NULL, Param_Cell, Param_String);
	m_SpectatorPutInServerFwd = forwards->CreateForward("SourceTV_OnSpectatorPutInServer", ET_Ignore, 1, NULL, Param_Cell);
	m_SpectatorChatMessageFwd = forwards->CreateForward("SourceTV_OnSpectatorChatMessage", ET_Hook, 3, NULL, Param_Cell, Param_String, Param_String);
	m_SpectatorChatMessagePostFwd = forwards->CreateForward("SourceTV_OnSpectatorChatMessage_Post", ET_Ignore, 3, NULL, Param_Cell, Param_String, Param_String);

	m_ServerStartFwd = forwards->CreateForward("SourceTV_OnServerStart", ET_Ignore, 1, NULL, Param_Cell);
	m_ServerShutdownFwd = forwards->CreateForward("SourceTV_OnServerShutdown", ET_Ignore, 1, NULL, Param_Cell);
}

void CForwardManager::Shutdown()
{
	forwards->ReleaseForward(m_StartRecordingFwd);
	forwards->ReleaseForward(m_StopRecordingFwd);
	forwards->ReleaseForward(m_SpectatorPreConnectFwd);
	forwards->ReleaseForward(m_SpectatorConnectedFwd);
	forwards->ReleaseForward(m_SpectatorDisconnectFwd);
	forwards->ReleaseForward(m_SpectatorDisconnectedFwd);
	forwards->ReleaseForward(m_SpectatorPutInServerFwd);
	forwards->ReleaseForward(m_SpectatorChatMessageFwd);
	forwards->ReleaseForward(m_SpectatorChatMessagePostFwd);

	forwards->ReleaseForward(m_ServerStartFwd);
	forwards->ReleaseForward(m_ServerShutdownFwd);
}

void CForwardManager::HookRecorder(IDemoRecorder *recorder)
{
#ifdef WIN32
	SH_ADD_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), true);
	SH_ADD_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording), false);
#endif
}

void CForwardManager::UnhookRecorder(IDemoRecorder *recorder)
{
#ifdef WIN32
	SH_REMOVE_HOOK(IDemoRecorder, StartRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStartRecording_Post), true);
	SH_REMOVE_HOOK(IDemoRecorder, StopRecording, recorder, SH_MEMBER(this, &CForwardManager::OnStopRecording), false);
#endif
}

void CForwardManager::HookServer(HLTVServerWrapper *wrapper)
{
	IServer *server = wrapper->GetBaseServer();
	if (m_bHasClientConnectOffset)
		SH_ADD_MANUALHOOK(CHLTVServer_ConnectClient, server, SH_MEMBER(this, &CForwardManager::OnSpectatorConnect), false);
	
	if (m_bHasGetChallengeTypeOffset)
		SH_ADD_MANUALHOOK(CHLTVServer_GetChallengeType, server, SH_MEMBER(this, &CForwardManager::OnGetChallengeType), false);

	// Hook all already connected clients as well for late loading
	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient *client = server->GetClient(i);
		if (client->IsConnected())
		{
			HookClient(client);
			// Ip and password unknown :(
			// Could add more gamedata to fetch it if people really lateload the extension and expect it to work :B
			wrapper->GetClient(i + 1)->Initialize("", "", client);
		}
	}
}

void CForwardManager::UnhookServer(HLTVServerWrapper *wrapper)
{
	IServer *server = wrapper->GetBaseServer();
	if (m_bHasClientConnectOffset)
		SH_REMOVE_MANUALHOOK(CHLTVServer_ConnectClient, server, SH_MEMBER(this, &CForwardManager::OnSpectatorConnect), false);

	if (m_bHasGetChallengeTypeOffset)
		SH_REMOVE_MANUALHOOK(CHLTVServer_GetChallengeType, server, SH_MEMBER(this, &CForwardManager::OnGetChallengeType), false);

	// Unhook all connected clients as well.
	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient *client = server->GetClient(i);
		if (client->IsConnected())
			UnhookClient(client);
	}
}

void CForwardManager::HookClient(IClient *client)
{
	// Hook ExecuteStringCommand for chat messages
	g_pSTVCommonHooks.AddSpectatorHook(this, client);

	void *pGameClient = (void *)((intptr_t)client - 4);
	if (m_bHasActivatePlayerOffset)
		SH_ADD_MANUALHOOK(CBaseClient_ActivatePlayer, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorPutInServer), true);
	
	// Linux' engine uses the CGameClient vtable internally, but we're using the IClient vtable to kick players.
	// Need to hook both to catch all cases >.<
	SH_ADD_HOOK(IClient, Disconnect, client, SH_MEMBER(this, &CForwardManager::IClient_OnSpectatorDisconnect), false);
#ifndef WIN32
	if (m_bHasDisconnectOffset)
		SH_ADD_MANUALHOOK(CBaseClient_Disconnect, pGameClient, SH_MEMBER(this, &CForwardManager::BaseClient_OnSpectatorDisconnect), false);
	
	if (m_bHasExecuteStringCommandOffset)
	{
		SH_ADD_MANUALHOOK(CBaseClient_ExecuteStringCommand, pGameClient, SH_MEMBER(this, &CForwardManager::BaseClient_OnSpectatorExecuteStringCommand), false);
		SH_ADD_MANUALHOOK(CBaseClient_ExecuteStringCommand, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorExecuteStringCommand_Post), true);
	}
#endif
}

void CForwardManager::UnhookClient(IClient *client)
{
	// Remove ExecuteStringCommand hook
	g_pSTVCommonHooks.RemoveSpectatorHook(this, client);

	void *pGameClient = (void *)((intptr_t)client - 4);
	if (m_bHasActivatePlayerOffset)
		SH_REMOVE_MANUALHOOK(CBaseClient_ActivatePlayer, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorPutInServer), true);
	
	SH_REMOVE_HOOK(IClient, Disconnect, client, SH_MEMBER(this, &CForwardManager::IClient_OnSpectatorDisconnect), false);
#ifndef WIN32
	if (m_bHasDisconnectOffset)
		SH_REMOVE_MANUALHOOK(CBaseClient_Disconnect, pGameClient, SH_MEMBER(this, &CForwardManager::BaseClient_OnSpectatorDisconnect), false);
	
	if (m_bHasExecuteStringCommandOffset)
	{
		SH_REMOVE_MANUALHOOK(CBaseClient_ExecuteStringCommand, pGameClient, SH_MEMBER(this, &CForwardManager::BaseClient_OnSpectatorExecuteStringCommand), false);
		SH_REMOVE_MANUALHOOK(CBaseClient_ExecuteStringCommand, pGameClient, SH_MEMBER(this, &CForwardManager::OnSpectatorExecuteStringCommand_Post), true);
	}
#endif
}

void CForwardManager::CallOnServerStart(IHLTVServer *server)
{
	m_ServerStartFwd->PushCell(g_HLTVServers.GetInstanceNumber(server));
	m_ServerStartFwd->Execute();
}

void CForwardManager::CallOnServerShutdown(IHLTVServer *server)
{
	m_ServerShutdownFwd->PushCell(g_HLTVServers.GetInstanceNumber(server));
	m_ServerShutdownFwd->Execute();
}

#if SOURCE_ENGINE == SE_CSGO
static bool ExtractPlayerName(CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, char *name, int maxlen)
{
	for (int i = 0; i < pSplitPlayerConnectVector.Count(); i++)
	{
		NetMsg_SplitPlayerConnect *split = pSplitPlayerConnectVector[i];
		if (!split->has_convars())
			continue;

		const CMsg_CVars cvars = split->convars();
		for (int c = 0; c < cvars.cvars_size(); c++)
		{
			const CMsg_CVars_CVar cvar = cvars.cvars(c);
			if (!cvar.has_name() || !cvar.has_value())
				continue;

			if (!strcmp(cvar.name().c_str(), "name"))
			{
				strncpy(name, cvar.value().c_str(), maxlen);
				return true;
			}
		}
	}
	return false;
}
#endif

// Mimic Connect extension https://forums.alliedmods.net/showthread.php?t=162489
// Thanks asherkin!
char passwordBuffer[255];
#if SOURCE_ENGINE == SE_CSGO
// CHLTVServer::ConnectClient(ns_address const&, int, int, int, char const*, char const*, char const*, int, CUtlVector<CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true> *, CUtlMemory<CNetMessagePB<16, CCLCMsg_SplitPlayerConnect, 0, true> *, int>> &, bool, CrossPlayPlatform_t, unsigned char const*, int)
IClient *CForwardManager::OnSpectatorConnect(const netadr_t & address, int nProtocol, int iChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie, CUtlVector<NetMsg_SplitPlayerConnect *> &pSplitPlayerConnectVector, bool bUnknown, CrossPlayPlatform_t platform, const unsigned char *pUnknown, int iUnknown)
#elif SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
IClient *CForwardManager::OnSpectatorConnect(const netadr_t & address, int nProtocol, int iChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie, CUtlVector<CLC_SplitPlayerConnect *> &pSplitPlayerConnectVector, bool bUnknown)
#else
IClient *CForwardManager::OnSpectatorConnect(netadr_t & address, int nProtocol, int iChallenge, int iClientChallenge, int nAuthProtocol, const char *pchName, const char *pchPassword, const char *pCookie, int cbCookie)
#endif
{
	if (!pCookie || (size_t)cbCookie < sizeof(uint64))
		RETURN_META_VALUE(MRES_IGNORED, nullptr);

#if SOURCE_ENGINE == SE_CSGO
	// CS:GO doesn't send the player name in pchName, but only in the client info convars.
	// Try to extract the name from the protobuf msg.
	char playerName[MAX_PLAYER_NAME_LENGTH];
	if (ExtractPlayerName(pSplitPlayerConnectVector, playerName, sizeof(playerName)))
		pchName = playerName;
#endif

	char ipString[16];
	V_snprintf(ipString, sizeof(ipString), "%u.%u.%u.%u", address.ip[0], address.ip[1], address.ip[2], address.ip[3]);
	V_strncpy(passwordBuffer, pchPassword, 255);

	// SourceTV doesn't validate steamids?!

	char rejectReason[255];

	m_SpectatorPreConnectFwd->PushString(pchName);
	m_SpectatorPreConnectFwd->PushStringEx(passwordBuffer, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorPreConnectFwd->PushString(ipString);
	m_SpectatorPreConnectFwd->PushStringEx(rejectReason, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);

	cell_t retVal = 1;
	m_SpectatorPreConnectFwd->Execute(&retVal);

	IServer *server = META_IFACEPTR(IServer);
	if (retVal == 0)
	{
		if (m_bHasRejectConnectionOffset)
		{
#if SOURCE_ENGINE == SE_CSGO || SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, rejectReason);
#else
			SH_MCALL(server, CHLTVServer_RejectConnection)(address, iClientChallenge, rejectReason);
#endif
		}
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);
	}

	// Call the original function.
#if SOURCE_ENGINE == SE_CSGO
	IClient *client = SH_MCALL(server, CHLTVServer_ConnectClient)(address, nProtocol, iChallenge, nAuthProtocol, pchName, passwordBuffer, pCookie, cbCookie, pSplitPlayerConnectVector, bUnknown, platform, pUnknown, iUnknown);
#elif SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
	IClient *client = SH_MCALL(server, CHLTVServer_ConnectClient)(address, nProtocol, iChallenge, nAuthProtocol, pchName, passwordBuffer, pCookie, cbCookie, pSplitPlayerConnectVector, bUnknown);
#else
	IClient *client = SH_MCALL(server, CHLTVServer_ConnectClient)(address, nProtocol, iChallenge, iClientChallenge, nAuthProtocol, pchName, passwordBuffer, pCookie, cbCookie);
#endif

	if (!client)
		RETURN_META_VALUE(MRES_SUPERCEDE, nullptr);

	HookClient(client);

	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(server);
	if (wrapper)
	{
		HLTVClientWrapper *clientWrapper = wrapper->GetClient(client->GetPlayerSlot() + 1);
		clientWrapper->Initialize(ipString, pchPassword, client);
	}

	m_SpectatorConnectedFwd->PushCell(client->GetPlayerSlot() + 1);
	m_SpectatorConnectedFwd->Execute();

	// Don't call the hooked function again, just return its value.
	RETURN_META_VALUE(MRES_SUPERCEDE, client);
}

// Force steam authentication
// Thanks GoD-Tony :)
int CForwardManager::OnGetChallengeType(const netadr_t &address)
{
	if (!tv_force_steamauth.GetBool())
		RETURN_META_VALUE(MRES_IGNORED, k_EAuthProtocolHashedCDKey);

	RETURN_META_VALUE(MRES_SUPERCEDE, k_EAuthProtocolSteam);
}

void CForwardManager::BaseClient_OnSpectatorDisconnect(const char *reason)
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META(MRES_IGNORED);

	IClient *client = (IClient *)((intptr_t)pGameClient + 4);
	HandleSpectatorDisconnect(client, reason);

	RETURN_META(MRES_SUPERCEDE);
}

void CForwardManager::IClient_OnSpectatorDisconnect(const char *reason)
{
	IClient *client = META_IFACEPTR(IClient);
	if (!client)
		RETURN_META(MRES_IGNORED);

	HandleSpectatorDisconnect(client, reason);

	RETURN_META(MRES_SUPERCEDE);
}

void CForwardManager::HandleSpectatorDisconnect(IClient *client, const char *reason)
{
	UnhookClient(client);

	char disconnectReason[255];
	V_strncpy(disconnectReason, reason, 255);
	int clientIndex = client->GetPlayerSlot() + 1;

	m_SpectatorDisconnectFwd->PushCell(clientIndex);
	m_SpectatorDisconnectFwd->PushStringEx(disconnectReason, 255, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorDisconnectFwd->Execute();

	// We always call the IClient::Disconnect variant, even if we're coming from CGameClient::Disconnect on linux.
	// They point to the same function in the engine though. Could only confuse other hooks, so might need to revisit this.
#if SOURCE_ENGINE == SE_CSGO
	SH_CALL(client, &IClient::Disconnect)(disconnectReason);
#else
	SH_CALL(client, &IClient::Disconnect)("%s", disconnectReason);
#endif

	m_SpectatorDisconnectedFwd->PushCell(clientIndex);
	m_SpectatorDisconnectedFwd->PushString(disconnectReason);
	m_SpectatorDisconnectedFwd->Execute();
}

void CForwardManager::OnSpectatorPutInServer()
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META(MRES_IGNORED);

	// The IClient vtable is + 4 from the CBaseClient vtable due to multiple inheritance.
	IClient *client = (IClient *)((intptr_t)pGameClient + 4);

	m_SpectatorPutInServerFwd->PushCell(client->GetPlayerSlot() + 1);
	m_SpectatorPutInServerFwd->Execute();

	RETURN_META(MRES_IGNORED);
}

bool CForwardManager::OnSpectatorExecuteStringCommand(const char *s)
{
	IClient *client = META_IFACEPTR(IClient);

	RETURN_META_VALUE(MRES_IGNORED, HandleOnSpectatorExecuteStringCommand(client, s));
}

bool CForwardManager::BaseClient_OnSpectatorExecuteStringCommand(const char *s)
{
	void *pGameClient = META_IFACEPTR(void);
	if (!pGameClient)
		RETURN_META_VALUE(MRES_IGNORED, true);

	IClient *client = (IClient *)((intptr_t)pGameClient + 4);

	RETURN_META_VALUE(MRES_IGNORED, HandleOnSpectatorExecuteStringCommand(client, s));
}

bool CForwardManager::HandleOnSpectatorExecuteStringCommand(IClient *client, const char *s)
{
	if (!hltvserver)
		RETURN_META_VALUE(MRES_IGNORED, true);

	if (!s || !s[0])
		RETURN_META_VALUE(MRES_IGNORED, true);

	CCommand args;
	if (!args.Tokenize(s))
		RETURN_META_VALUE(MRES_IGNORED, true);

	// See if the client wants to chat.
	if (!Q_stricmp(args[0], "say") && args.ArgC() > 1)
	{
		// TODO find correct hltvserver this client is connected to!

		// Save the client index and message.
		hltvserver->SetLastChatClient(client);
		hltvserver->SetLastChatMessage(args[1]);
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
}

// Reset the pointers after the command was executed.
bool CForwardManager::OnSpectatorExecuteStringCommand_Post(const char *s)
{
	if (!hltvserver)
		RETURN_META_VALUE(MRES_IGNORED, true);

	// TODO find correct hltvserver this client is connected to!

	hltvserver->SetLastChatClient(nullptr);
	hltvserver->SetLastChatMessage(nullptr);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

DETOUR_DECL_MEMBER2(DetourHLTVServer_BroadcastLocalChat, void, const char *, chat, const char *, chatgroup)
{
	// IServer is +8 from CHLTVServer due to multiple inheritance
	// IServer is +4 in TF2
	IServer *server = (IServer *)((intptr_t)this + 4);
	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(server);

	// Copy content in changeable buffer.
	char chatBuffer[256], groupBuffer[256];
	ke::SafeStrcpy(chatBuffer, sizeof(chatBuffer), chat);
	ke::SafeStrcpy(groupBuffer, sizeof(groupBuffer), chatgroup);

	// See if we can provide only the chat message without the name printed before.
	// The engine formats the chat like "Name : Message" before calling BroadcastLocalChat..
	if (wrapper)
	{
		const char *msg = wrapper->GetLastChatMessage();
		// Use only the typed text if possible
		if (msg)
			ke::SafeStrcpy(chatBuffer, sizeof(chatBuffer), msg);
	}

	// Call the forward for this message.
	bool supercede = g_pSTVForwards.CallOnSpectatorChatMessage(wrapper, chatBuffer, sizeof(chatBuffer), groupBuffer, sizeof(groupBuffer));
	if (supercede)
		return;

	// Use the potentially modified strings.
	chat = chatBuffer;
	chatgroup = groupBuffer;

	char messageBuffer[1024];
	// Might have to add the name again, if we had the chat message in our hands.
	if (wrapper)
	{
		if (wrapper->GetLastChatMessage())
		{
			// Format the message the same as the engine does.
			ke::SafeSprintf(messageBuffer, sizeof(messageBuffer), "%s : %s", wrapper->GetLastChatClient()->GetClientName(), chatBuffer);
			chat = messageBuffer;
		}
	}
	
	// Print the message
	DETOUR_MEMBER_CALL(DetourHLTVServer_BroadcastLocalChat)(chat, chatgroup);
	
	// Call the post message forward with the changed output.
	g_pSTVForwards.CallOnSpectatorChatMessage_Post(wrapper, chatBuffer, groupBuffer);
}

void CForwardManager::CreateBroadcastLocalChatDetour()
{
	if (m_bBroadcastLocalChatDetoured)
		return;

	m_DBroadcastLocalChat = DETOUR_CREATE_MEMBER(DetourHLTVServer_BroadcastLocalChat, "CHLTVServer::BroadcastLocalChat");

	if (m_DBroadcastLocalChat != nullptr)
	{
		m_DBroadcastLocalChat->EnableDetour();
		m_bBroadcastLocalChatDetoured = true;
		return;
	}
	smutils->LogError(myself, "CHLTVServer::BroadcastLocalChat detour could not be initialized.");
}

void CForwardManager::RemoveBroadcastLocalChatDetour()
{
	if (m_DBroadcastLocalChat != nullptr)
	{
		m_DBroadcastLocalChat->Destroy();
		m_DBroadcastLocalChat = nullptr;
	}
	m_bBroadcastLocalChatDetoured = false;
}

bool CForwardManager::CallOnSpectatorChatMessage(HLTVServerWrapper *server, char *msg, int msglen, char *chatgroup, int grouplen)
{
	int clientIndex = 0;
	if (server)
	{
		IClient *client = server->GetLastChatClient();
		if (client)
			clientIndex = client->GetPlayerSlot() + 1;
	}

	m_SpectatorChatMessageFwd->PushCell(clientIndex);
	m_SpectatorChatMessageFwd->PushStringEx(msg, msglen, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	m_SpectatorChatMessageFwd->PushStringEx(chatgroup, grouplen, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);

	cell_t res = Pl_Continue;
	m_SpectatorChatMessageFwd->Execute(&res);
	if (res >= Pl_Handled)
		return true;
	return false;
}

void CForwardManager::CallOnSpectatorChatMessage_Post(HLTVServerWrapper *server, const char *msg, const char *chatgroup)
{
	int clientIndex = 0;
	if (server)
	{
		IClient *client = server->GetLastChatClient();
		if (client)
			clientIndex = client->GetPlayerSlot() + 1;
	}

	m_SpectatorChatMessagePostFwd->PushCell(clientIndex);
	m_SpectatorChatMessagePostFwd->PushString(msg);
	m_SpectatorChatMessagePostFwd->PushString(chatgroup);
	m_SpectatorChatMessagePostFwd->Execute();
}

// These two hooks are actually only hooked on windows.
void CForwardManager::OnStartRecording_Post(const char *filename, bool bContinuously)
{
	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	CallOnStartRecording(recorder, filename, bContinuously);
	RETURN_META(MRES_IGNORED);
}

#if SOURCE_ENGINE == SE_CSGO
void CForwardManager::OnStopRecording(CGameInfo const *info)
#else
void CForwardManager::OnStopRecording()
#endif
{
	IDemoRecorder *recorder = META_IFACEPTR(IDemoRecorder);
	CallOnStopRecording(recorder);
	RETURN_META(MRES_IGNORED);
}

void CForwardManager::CallOnStartRecording(IDemoRecorder *recorder, const char *filename, bool bContinuously)
{
	if (m_StartRecordingFwd->GetFunctionCount() == 0)
		return;

	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(recorder);
	int instance = -1;
	if (wrapper)
		instance = wrapper->GetInstanceNumber();

	m_StartRecordingFwd->PushCell(instance);
	m_StartRecordingFwd->PushString(filename);
	m_StartRecordingFwd->Execute();
}

void CForwardManager::CallOnStopRecording(IDemoRecorder *recorder)
{
	if (m_StopRecordingFwd->GetFunctionCount() == 0)
		return;

	if (!recorder->IsRecording())
		return;
	
	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(recorder);
	int instance = -1;
	const char *pDemoFile = "";
	if (wrapper)
	{
		instance = wrapper->GetInstanceNumber();
		pDemoFile = wrapper->GetDemoFileName();
	}

	m_StopRecordingFwd->PushCell(instance);
	m_StopRecordingFwd->PushString(pDemoFile);
	m_StopRecordingFwd->PushCell(recorder->GetRecordingTick());
	m_StopRecordingFwd->Execute();
}

// Only need to detour these on Linux. Windows always uses the vtable.
#ifndef WIN32
DETOUR_DECL_MEMBER2(DetourHLTVStartRecording, void, const char *, filename, bool, bContinuously)
{
	// Call the original first.
	DETOUR_MEMBER_CALL(DetourHLTVStartRecording)(filename, bContinuously);
	
	IDemoRecorder *recorder = (IDemoRecorder *)this;
	g_pSTVForwards.CallOnStartRecording(recorder, filename, bContinuously);
}

#if SOURCE_ENGINE == SE_CSGO
DETOUR_DECL_MEMBER1(DetourHLTVStopRecording, void, CGameInfo const *, info)
#else
DETOUR_DECL_MEMBER0(DetourHLTVStopRecording, void)
#endif
{
	IDemoRecorder *recorder = (IDemoRecorder *)this;
	g_pSTVForwards.CallOnStopRecording(recorder);

#if SOURCE_ENGINE == SE_CSGO
	DETOUR_MEMBER_CALL(DetourHLTVStopRecording)(info);
#else
	DETOUR_MEMBER_CALL(DetourHLTVStopRecording)();
#endif	
}

void CForwardManager::CreateStartRecordingDetour()
{
	if (m_bStartRecordingDetoured)
		return;

	m_DStartRecording = DETOUR_CREATE_MEMBER(DetourHLTVStartRecording, "CHLTVDemoRecorder::StartRecording");

	if (m_DStartRecording != nullptr)
	{
		m_DStartRecording->EnableDetour();
		m_bStartRecordingDetoured = true;
		return;
	}
	smutils->LogError(myself, "CHLTVDemoRecorder::StartRecording detour could not be initialized.");
	return;
}

void CForwardManager::RemoveStartRecordingDetour()
{
	if (m_DStartRecording != nullptr)
	{
		m_DStartRecording->Destroy();
		m_DStartRecording = nullptr;
	}
	m_bStartRecordingDetoured = false;
}

void CForwardManager::CreateStopRecordingDetour()
{
	if (m_bStopRecordingDetoured)
		return;

	m_DStopRecording = DETOUR_CREATE_MEMBER(DetourHLTVStopRecording, "CHLTVDemoRecorder::StopRecording");

	if (m_DStopRecording != nullptr)
	{
		m_DStopRecording->EnableDetour();
		m_bStopRecordingDetoured = true;
		return;
	}
	smutils->LogError(myself, "CHLTVDemoRecorder::StopRecording detour could not be initialized.");
	return;
}

void CForwardManager::RemoveStopRecordingDetour()
{
	if (m_DStopRecording != nullptr)
	{
		m_DStopRecording->Destroy();
		m_DStopRecording = nullptr;
	}
	m_bStopRecordingDetoured = false;
}
#endif