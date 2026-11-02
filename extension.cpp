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
#include "hltvserverwrapper.h"
#include "forwards.h"
#include "natives.h"

IHLTVDirector *hltvdirector = nullptr;
void *host_client = nullptr;
HLTVServerWrapper *hltvserver = nullptr;

IGameEventManager2 *gameevents = nullptr;
CGlobalVars *gpGlobals = nullptr;
ICvar *icvar = nullptr;

IBinTools *bintools = nullptr;
ISDKTools *sdktools = nullptr;
IServer *iserver = nullptr;
IGameConfig *g_pGameConf = nullptr;

#if SOURCE_ENGINE != SE_CSGO
bool g_SendNetMsgHooked = false;
#endif

#if SOURCE_ENGINE == SE_CSGO
SH_DECL_HOOK1_void(IHLTVDirector, AddHLTVServer, SH_NOATTRIB, 0, IHLTVServer *);
SH_DECL_HOOK1_void(IHLTVDirector, RemoveHLTVServer, SH_NOATTRIB, 0, IHLTVServer *);
#else
SH_DECL_HOOK1_void(IHLTVDirector, SetHLTVServer, SH_NOATTRIB, 0, IHLTVServer *);
#endif

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

SourceTVManager g_STVManager;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_STVManager);

extern const sp_nativeinfo_t sourcetv_natives[];

ConVar tv_force_steamauth("tv_force_steamauth", "0", FCVAR_NONE, "Validate SourceTV clients with Steam.");

bool SourceTVManager::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddDependency(myself, "sdktools.ext", true, true);

	char conf_error[255];
	if (!gameconfs->LoadGameConfigFile("sourcetvmanager.games", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		if (error)
		{
			snprintf(error, maxlength, "Could not read sourcetvmanager.games: %s", conf_error);
		}
		return false;
	}

	// Get the host_client pointer
	// This is used to fix a null pointer crash when executing fake commands on bots.
	if (!g_pGameConf->GetAddress("host_client", &host_client) || !host_client)
	{
		smutils->LogError(myself, "Failed to find host_client pointer. Server might crash when executing commands on SourceTV bot.");
	}
#ifdef _WIN64
	// The x64 engine reads host_client RIP-relative. The address points to the rel32 at the end of that instruction.
	else
	{
		host_client = (char *)host_client + sizeof(int32_t) + *(int32_t *)host_client;
	}
#endif

	g_HLTVServers.InitHooks();

	CDetourManager::Init(smutils->GetScriptingEngine(), g_pGameConf);

	sharesys->AddNatives(myself, sourcetv_natives);
	sharesys->RegisterLibrary(myself, "sourcetvmanager");

	return true;
}

void SourceTVManager::SDK_OnAllLoaded()
{
#if SOURCE_ENGINE == SE_CSGO
	SH_ADD_HOOK(IHLTVDirector, AddHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnAddHLTVServer_Post), true);
	SH_ADD_HOOK(IHLTVDirector, RemoveHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnRemoveHLTVServer), false);
#else
	SH_ADD_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnSetHLTVServer_Post), true);
#endif

	SM_GET_LATE_IFACE(BINTOOLS, bintools);
	if (!bintools)
	{
		smutils->LogError(myself, "Could not find interface: %s. Won't be able to send events to local spectators only.", SMINTERFACE_BINTOOLS_NAME);
	}
	SM_GET_LATE_IFACE(SDKTOOLS, sdktools);
	if (!sdktools)
	{
		smutils->LogError(myself, "Could not find interface: %s. Some functions won't work.", SMINTERFACE_SDKTOOLS_NAME);
	}

	g_pSTVForwards.Init();
	SetupNativeCalls();

	if (sdktools)
		iserver = sdktools->GetIServer();
	if (!iserver)
		smutils->LogError(myself, "Failed to get IServer interface from SDKTools. Some functions won't work.");

#if SOURCE_ENGINE == SE_CSGO
	// Hook all the exisiting servers.
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		g_HLTVServers.AddServer(hltvdirector->GetHLTVServer(i));
	}

	if (hltvdirector->GetHLTVServerCount() > 0)
		SelectSourceTVServer(hltvdirector->GetHLTVServer(0));
#else
	if (hltvdirector->GetHLTVServer())
	{
		g_HLTVServers.AddServer(hltvdirector->GetHLTVServer());
		SelectSourceTVServer(hltvdirector->GetHLTVServer());
	}
#endif
}

bool SourceTVManager::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetServerFactory, hltvdirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR);
	GET_V_IFACE_CURRENT(GetEngineFactory, gameevents, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2);
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);

	gpGlobals = ismm->GetCGlobals();

	g_pCVar = icvar;
	ConVar_Register(0, this);

	return true;
}

bool SourceTVManager::RegisterConCommandBase(ConCommandBase *pCommandBase)
{
	/* Always call META_REGCVAR instead of going through the engine. */
	return META_REGCVAR(pCommandBase);
}

void SourceTVManager::SDK_OnUnload()
{
#if SOURCE_ENGINE == SE_CSGO
	SH_REMOVE_HOOK(IHLTVDirector, AddHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnAddHLTVServer_Post), true);
	SH_REMOVE_HOOK(IHLTVDirector, RemoveHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnRemoveHLTVServer), false);
#else
	SH_REMOVE_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SourceTVManager::OnSetHLTVServer_Post), true);
#endif

	g_HLTVServers.ShutdownHooks();

	gameconfs->CloseGameConfigFile(g_pGameConf);

#if SOURCE_ENGINE == SE_CSGO
	// Unhook all the existing servers.
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		// We don't know if the extension is just being unloaded or the server is shutting down.
		// So don't inform the plugins of this removal.
		g_HLTVServers.RemoveServer(hltvdirector->GetHLTVServer(i), false);
	}
#else
	// Unhook the server
	g_HLTVServers.RemoveServer(hltvdirector->GetHLTVServer(), false);
#endif
	g_pSTVForwards.Shutdown();
}

bool SourceTVManager::QueryRunning(char *error, size_t maxlength)
{
	SM_CHECK_IFACE(BINTOOLS, bintools);
	SM_CHECK_IFACE(SDKTOOLS, sdktools);

	return true;
}

void SourceTVManager::SelectSourceTVServer(IHLTVServer *hltv)
{
	// Select the new server.
	hltvserver = g_HLTVServers.GetWrapper(hltv);
}

#if SOURCE_ENGINE == SE_CSGO
void SourceTVManager::OnAddHLTVServer_Post(IHLTVServer *hltv)
{
	// Only hook this server if it's a new one.
	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(hltv);
	if (!wrapper)
		g_HLTVServers.AddServer(hltv);

	// We already selected some SourceTV server. Keep it.
	if (hltvserver != nullptr)
		RETURN_META(MRES_IGNORED);
	
	// This is the first SourceTV server to be added.
	SelectSourceTVServer(hltv);
	RETURN_META(MRES_IGNORED);
}

void SourceTVManager::OnRemoveHLTVServer(IHLTVServer *hltv)
{
	HLTVServerWrapper *wrapper = g_HLTVServers.GetWrapper(hltv);
	if (!wrapper)
		RETURN_META(MRES_IGNORED);

	// With the CHLTVServer::Shutdown hook, this isn't needed?
	// Doesn't hurt either..
	g_HLTVServers.RemoveServer(hltv, true);

	// We got this SourceTV server selected. Now it's gone :(
	if (hltvserver == wrapper)
	{
		// Is there another one available? Try to keep us operable.
		if (hltvdirector->GetHLTVServerCount() > 0)
		{
			SelectSourceTVServer(hltvdirector->GetHLTVServer(0));
		}
		// No sourcetv active.
		else
		{
			SelectSourceTVServer(nullptr);
		}
	}
	RETURN_META(MRES_IGNORED);
}
#else
void SourceTVManager::OnSetHLTVServer_Post(IHLTVServer *hltv)
{
	// Server shut down?
	if (!hltv)
	{
		// We didn't catch the server being set..
		if (!hltvserver)
			RETURN_META(MRES_IGNORED);

		// With the CHLTVServer::Shutdown hook, this isn't needed?
		// Doesn't hurt either..
		g_HLTVServers.RemoveServer(hltvserver->GetHLTVServer(), true);
	}
	// Only hook this server if it's a new one.
	else if (!hltvserver || g_HLTVServers.GetWrapper(hltv) != hltvserver)
	{
		g_HLTVServers.AddServer(hltv);
	}
	SelectSourceTVServer(hltv);
	RETURN_META(MRES_IGNORED);
}
#endif
