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

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_HLTVSERVER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_HLTVSERVER_H_

#include "extension.h"
#include "amtl/am-utility.h"
#include <vector>
#include <memory>

class HLTVServerWrapper {
public:
	HLTVServerWrapper(IHLTVServer *hltvserver);
	void Shutdown(bool bInformPlugins);

	IHLTVServer *GetHLTVServer();
	IServer *GetBaseServer();
	IDemoRecorder *GetDemoRecorder();
	char *GetDemoFileName();
	HLTVClientWrapper *GetClient(int index);
	int GetInstanceNumber();

	IClient *GetLastChatClient();
	void SetLastChatClient(IClient *client);
	const char *GetLastChatMessage();
	void SetLastChatMessage(const char *msg);

	bool OnHLTVBotExecuteStringCommand(const char *s);
	bool OnHLTVBotExecuteStringCommand_Post(const char *s);

private:
	void Hook();
	void Unhook();

	// Hooks
	void OnHLTVServerShutdown();

#if SOURCE_ENGINE != SE_CSGO
	void OnIClient_ClientPrintf_Post(const char *buf);
	void OnCGameClient_ClientPrintf_Post(const char *buf);
	void HandleClientPrintf(IClient *pClient, const char* buf);
#endif

private:
	bool m_Connected = false;
	IHLTVServer *m_HLTVServer = nullptr;
	IDemoRecorder *m_DemoRecorder = nullptr;
	std::vector<std::unique_ptr<HLTVClientWrapper>> m_Clients;

	IClient *m_LastChatClient = nullptr;
	const char *m_LastChatMessage = nullptr;
};

class HLTVServerWrapperManager
{
public:
	void InitHooks();
	void ShutdownHooks();
	void AddServer(IHLTVServer *hltvserver);
	void RemoveServer(IHLTVServer *hltvserver, bool bInformPlugins);
	HLTVServerWrapper *GetWrapper(IHLTVServer *hltvserver);
	HLTVServerWrapper *GetWrapper(IServer *server);
	HLTVServerWrapper *GetWrapper(IDemoRecorder *demorecorder);
	int GetInstanceNumber(IHLTVServer *hltvserver);

	IDemoRecorder *GetDemoRecorderPtr(IHLTVServer *hltv);
	int GetIServerOffset();
	bool HasClientPrintfOffset();
	bool HasShutdownOffset();

#if SOURCE_ENGINE != SE_CSGO
	bool OnHLTVBotNetChanSendNetMsg(INetMessage &msg, bool bForceReliable, bool bVoice);
#endif

private:
#if SOURCE_ENGINE != SE_CSGO
	bool m_bSendNetMsgHooked = false;
#endif
	bool m_bHasClientPrintfOffset = false;
	bool m_bHasShutdownOffset = false;
	std::vector<std::unique_ptr<HLTVServerWrapper>> m_HLTVServers;
};

extern HLTVServerWrapperManager g_HLTVServers;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_HLTVSERVER_H_