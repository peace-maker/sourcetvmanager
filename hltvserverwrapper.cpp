#include "hltvserverwrapper.h"
#include "forwards.h"
#include "commonhooks.h"

void *old_host_client = nullptr;
bool g_HostClientOverridden = false;

SH_DECL_MANUALHOOK0_void(CHLTVServer_Shutdown, 0, 0, 0);

#if SOURCE_ENGINE != SE_CSGO

// Stuff to print to demo console
SH_DECL_HOOK0_void_vafmt(IClient, ClientPrintf, SH_NOATTRIB, 0);

// Linux has the ClientPrintf method in both CGameClient and IClient's vtables
// and uses both.. Need to hook both....... i guess?
#ifndef WIN32
SH_DECL_MANUALHOOK0_void_vafmt(CGameClient_ClientPrintf, 0, 0, 0);
#endif

// This should be large enough.
#define FAKE_VTBL_LENGTH 70
static void *FakeNetChanVtbl[FAKE_VTBL_LENGTH];
static void *FakeNetChan = &FakeNetChanVtbl;
SH_DECL_MANUALHOOK3(NetChan_SendNetMsg, 0, 0, 0, bool, INetMessage &, bool, bool);
#endif // SOURCE_ENGINE != SE_CSGO

HLTVServerWrapper::HLTVServerWrapper(IHLTVServer *hltvserver)
{
	m_HLTVServer = hltvserver;
	m_DemoRecorder = g_HLTVServers.GetDemoRecorderPtr(hltvserver);
	m_Connected = true;
	m_LastChatClient = nullptr;

	Hook();

	// Inform the plugins
	g_pSTVForwards.CallOnServerStart(hltvserver);
}

void HLTVServerWrapper::Shutdown(bool bInformPlugins)
{
	if (!m_Connected)
		return;

	if (bInformPlugins)
		g_pSTVForwards.CallOnServerShutdown(m_HLTVServer);

	Unhook();

	m_HLTVServer = nullptr;
	m_DemoRecorder = nullptr;
	m_Connected = false;
}

IServer *HLTVServerWrapper::GetBaseServer()
{
	return m_HLTVServer->GetBaseServer();
}

IHLTVServer *HLTVServerWrapper::GetHLTVServer()
{
	return m_HLTVServer;
}

IDemoRecorder *HLTVServerWrapper::GetDemoRecorder()
{
	return m_DemoRecorder;
}

char *HLTVServerWrapper::GetDemoFileName()
{
	if (!m_DemoRecorder)
		return nullptr;

#if SOURCE_ENGINE == SE_CSGO
	return (char *)m_DemoRecorder + 8;
#else
	return (char *)m_DemoRecorder->GetDemoFile();
#endif
}

int HLTVServerWrapper::GetInstanceNumber()
{
	return g_HLTVServers.GetInstanceNumber(m_HLTVServer);
}

IClient *HLTVServerWrapper::GetLastChatClient()
{
	return m_LastChatClient;
}

void HLTVServerWrapper::SetLastChatClient(IClient *client)
{
	m_LastChatClient = client;
}

const char *HLTVServerWrapper::GetLastChatMessage()
{
	return m_LastChatMessage;
}

void HLTVServerWrapper::SetLastChatMessage(const char *msg)
{
	m_LastChatMessage = msg;
}

HLTVClientWrapper *HLTVServerWrapper::GetClient(int index)
{
	// Grow the vector with null pointers
	// There might have been clients with lower indexes before we were loaded.
	if (m_Clients.size() < (size_t)index)
	{
		int start = m_Clients.size();
		m_Clients.resize(index);
		for (int i = start; i < index; i++)
		{
			m_Clients[i] = nullptr;
		}
	}

	if (!m_Clients[index - 1])
	{
		m_Clients[index - 1] = std::make_unique<HLTVClientWrapper>();
	}

	return m_Clients[index - 1].get();
}

void HLTVServerWrapper::Hook()
{
	if (!m_Connected)
		return;

	g_pSTVForwards.HookServer(this);
	if (m_DemoRecorder)
		g_pSTVForwards.HookRecorder(m_DemoRecorder);

	if (g_HLTVServers.HasShutdownOffset())
		SH_ADD_MANUALHOOK(CHLTVServer_Shutdown, m_HLTVServer->GetBaseServer(), SH_MEMBER(this, &HLTVServerWrapper::OnHLTVServerShutdown), false);

	if (iserver)
	{
		IClient *pClient = iserver->GetClient(m_HLTVServer->GetHLTVSlot());
		if (pClient)
		{
			// Hook ExecuteStringCommand
			g_pSTVCommonHooks.AddHLTVClientHook(this, pClient);
#if SOURCE_ENGINE != SE_CSGO
			SH_ADD_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnIClient_ClientPrintf_Post), false);
#ifndef WIN32
			// The IClient vtable is one pointer after the CBaseClient vtable due to multiple inheritance.
			void *pGameClient = (void *)((intptr_t)pClient - sizeof(void *));
			if (g_HLTVServers.HasClientPrintfOffset())
				SH_ADD_MANUALHOOK(CGameClient_ClientPrintf, pGameClient, SH_MEMBER(this, &HLTVServerWrapper::OnCGameClient_ClientPrintf_Post), false);
#endif // !WIN32
#endif // SOURCE_ENGINE != SE_CSGO
		}
	}
}

void HLTVServerWrapper::Unhook()
{
	if (!m_Connected)
		return;

	g_pSTVForwards.UnhookServer(this);
	if (m_DemoRecorder)
		g_pSTVForwards.UnhookRecorder(m_DemoRecorder);

	if (g_HLTVServers.HasShutdownOffset())
		SH_REMOVE_MANUALHOOK(CHLTVServer_Shutdown, m_HLTVServer->GetBaseServer(), SH_MEMBER(this, &HLTVServerWrapper::OnHLTVServerShutdown), false);

	if (iserver)
	{
		IClient *pClient = iserver->GetClient(m_HLTVServer->GetHLTVSlot());
		if (pClient)
		{
			// Remove ExecuteStringCommand hook
			g_pSTVCommonHooks.RemoveHLTVClientHook(this, pClient);
#if SOURCE_ENGINE != SE_CSGO
			SH_REMOVE_HOOK(IClient, ClientPrintf, pClient, SH_MEMBER(this, &HLTVServerWrapper::OnIClient_ClientPrintf_Post), false);
#ifndef WIN32
			// The IClient vtable is one pointer after the CBaseClient vtable due to multiple inheritance.
			void *pGameClient = (void *)((intptr_t)pClient - sizeof(void *));
			if (g_HLTVServers.HasClientPrintfOffset())
				SH_REMOVE_MANUALHOOK(CGameClient_ClientPrintf, pGameClient, SH_MEMBER(this, &HLTVServerWrapper::OnCGameClient_ClientPrintf_Post), false);
#endif // !WIN32
#endif // SOURCE_ENGINE != SE_CSGO
		}
	}
}

// CHLTVServer::Shutdown deregisters the hltvserver from the hltvdirector, 
// so RemoveHLTVServer/SetHLTVServer(NULL) is called too on the master proxy.
void HLTVServerWrapper::OnHLTVServerShutdown()
{
	if (!m_Connected)
		RETURN_META(MRES_IGNORED);

	Shutdown(true);

	RETURN_META(MRES_IGNORED);
}

// When bots issue a command that would print stuff to their console, 
// the server might crash, because ExecuteStringCommand doesn't set the 
// global host_client pointer to the client on whom the command is run.
// Host_Client_Printf blatantly tries to call host_client->ClientPrintf
// while the pointer might point to some other player or garbage.
// This leads to e.g. the output of the "status" command not being 
// recorded in the SourceTV demo.
// The approach here is to set host_client correctly for the SourceTV
// bot and reset it to the old value after command execution.
bool HLTVServerWrapper::OnHLTVBotExecuteStringCommand(const char *s)
{
	if (!host_client)
	{
		// Block crash in status command.
		if (!Q_stricmp(s, "status"))
			RETURN_META_VALUE(MRES_SUPERCEDE, 0);
		else
			RETURN_META_VALUE(MRES_IGNORED, 0);
	}

	IClient *pClient = META_IFACEPTR(IClient);
	if (!pClient)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	// The IClient vtable is one pointer after the CBaseClient vtable due to multiple inheritance.
	void *pGameClient = (void *)((intptr_t)pClient - sizeof(void *));

	old_host_client = *(void **)host_client;
	*(void **)host_client = pGameClient;
	g_HostClientOverridden = true;

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

bool HLTVServerWrapper::OnHLTVBotExecuteStringCommand_Post(const char *s)
{
	if (!host_client || !g_HostClientOverridden)
		RETURN_META_VALUE(MRES_IGNORED, 0);

	*(void **)host_client = old_host_client;
	g_HostClientOverridden = false;
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

#if SOURCE_ENGINE != SE_CSGO
void HLTVServerWrapper::OnCGameClient_ClientPrintf_Post(const char* buf)
{
	void *pGameClient = META_IFACEPTR(void);
	IClient *pClient = (IClient *)((intptr_t)pGameClient + sizeof(void *));
	HandleClientPrintf(pClient, buf);

	// We already called the function in HandleClientPrintf.
	// Would crash or not do anything anyways.
	RETURN_META(MRES_SUPERCEDE);
}

void HLTVServerWrapper::OnIClient_ClientPrintf_Post(const char* buf)
{
	IClient *pClient = META_IFACEPTR(IClient);
	HandleClientPrintf(pClient, buf);

	// We already called the function in HandleClientPrintf.
	// Would crash or not do anything anyways.
	RETURN_META(MRES_SUPERCEDE);
}

void HLTVServerWrapper::HandleClientPrintf(IClient *pClient, const char* buf)
{
	// Craft our own "NetChan" pointer
	static int offset = -1;
	if (!g_pGameConf->GetOffset("CBaseClient::m_NetChannel", &offset) || offset == -1)
	{
		smutils->LogError(myself, "Failed to find CBaseClient::m_NetChannel offset. Can't print to demo console.");
		return;
	}

#ifdef WIN32
	void *pNetChannel = (void *)((char *)pClient + offset);
#else
	void *pNetChannel = (void *)((char *)pClient + offset - sizeof(void *));
#endif
	// Set our fake netchannel
	*(void **)pNetChannel = &FakeNetChan;
	// Call ClientPrintf again, this time with a "Netchannel" set on the bot.
	// This will call our own OnHLTVBotNetChanSendNetMsg function
	SH_CALL(pClient, &IClient::ClientPrintf)("%s", buf);
	// Set the fake netchannel back to 0.
	*(void **)pNetChannel = nullptr;
}
#endif

/**
 * Manage the wrappers!
 */
void HLTVServerWrapperManager::InitHooks()
{
	int offset;
	if (g_pGameConf->GetOffset("CHLTVServer::Shutdown", &offset))
	{
		SH_MANUALHOOK_RECONFIGURE(CHLTVServer_Shutdown, offset, 0, 0);
		m_bHasShutdownOffset = true;
	}
	else
	{
		smutils->LogError(myself, "Failed to find CHLTVServer::Shutdown offset.");
	}

#if SOURCE_ENGINE != SE_CSGO
#ifndef WIN32
	if (g_pGameConf->GetOffset("CGameClient::ClientPrintf", &offset))
	{
		SH_MANUALHOOK_RECONFIGURE(CGameClient_ClientPrintf, offset, 0, 0);
		m_bHasClientPrintfOffset = true;
	}
	else
	{
		smutils->LogError(myself, "Failed to find CGameClient::ClientPrintf offset. Won't catch \"status\" console output.");
	}
#endif // !WIN32

	if (g_pGameConf->GetOffset("CNetChan::SendNetMsg", &offset))
	{
		if (offset >= FAKE_VTBL_LENGTH)
		{
			smutils->LogError(myself, "CNetChan::SendNetMsg offset too big. Need to raise define and recompile. Contact the author.");
		}
		else
		{
			// This is a hack. Bots don't have a net channel, but ClientPrintf tries to call m_NetChannel->SendNetMsg directly.
			// CGameClient::SendNetMsg would have redirected it to the hltvserver correctly, but isn't used there..
			// We craft a fake object with a large enough "vtable" and hook it using sourcehook.
			// Before a call to ClientPrintf, this fake object is set as CBaseClient::m_NetChannel, so ClientPrintf creates 
			// the SVC_Print INetMessage and calls our "hooked" m_NetChannel->SendNetMsg function.
			// In that function we just call CGameClient::SendNetMsg with the given INetMessage to flow it through the same
			// path as other net messages.
			SH_MANUALHOOK_RECONFIGURE(NetChan_SendNetMsg, offset, 0, 0);
			SH_ADD_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg), false);
			m_bSendNetMsgHooked = true;
		}
	}
	else
	{
		smutils->LogError(myself, "Failed to find CNetChan::SendNetMsg offset. Can't print to demo console.");
	}
#endif
}

void HLTVServerWrapperManager::ShutdownHooks()
{
	g_pSTVForwards.RemoveBroadcastLocalChatDetour();
#ifndef WIN32
	g_pSTVForwards.RemoveStartRecordingDetour();
	g_pSTVForwards.RemoveStopRecordingDetour();
#endif

#if SOURCE_ENGINE != SE_CSGO
	if (m_bSendNetMsgHooked)
	{
		SH_REMOVE_MANUALHOOK(NetChan_SendNetMsg, &FakeNetChan, SH_MEMBER(this, &HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg), false);
		m_bSendNetMsgHooked = false;
	}
#endif
}

void HLTVServerWrapperManager::AddServer(IHLTVServer *hltvserver)
{
	// Create the detours once the first sourcetv server is created.
	g_pSTVForwards.CreateBroadcastLocalChatDetour();
#ifndef WIN32
	g_pSTVForwards.CreateStartRecordingDetour();
	g_pSTVForwards.CreateStopRecordingDetour();
#endif

	m_HLTVServers.emplace_back(new HLTVServerWrapper(hltvserver));
}

void HLTVServerWrapperManager::RemoveServer(IHLTVServer *hltvserver, bool bInformPlugins)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->GetHLTVServer() != hltvserver)
			continue;

		wrapper->Shutdown(bInformPlugins);
		m_HLTVServers.erase(m_HLTVServers.begin() + i);
		break;
	}
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IHLTVServer *hltvserver)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->GetHLTVServer() == hltvserver)
			return wrapper;
	}
	return nullptr;
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IServer *server)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->GetBaseServer() == server)
			return wrapper;
	}
	return nullptr;
}

HLTVServerWrapper *HLTVServerWrapperManager::GetWrapper(IDemoRecorder *demorecorder)
{
	for (unsigned int i = 0; i < m_HLTVServers.size(); i++)
	{
		HLTVServerWrapper *wrapper = m_HLTVServers[i].get();
		if (wrapper->GetDemoRecorder() != nullptr && wrapper->GetDemoRecorder() == demorecorder)
			return wrapper;
	}
	return nullptr;
}

int HLTVServerWrapperManager::GetInstanceNumber(IHLTVServer *hltvserver)
{
#if SOURCE_ENGINE == SE_CSGO
	for (int i = 0; i < hltvdirector->GetHLTVServerCount(); i++)
	{
		if (hltvserver == hltvdirector->GetHLTVServer(i))
			return i;
	}

	// We should have found it in the above loop :S
	smutils->LogError(myself, "Failed to find IHLTVServer instance in director.");
	return -1;
#else
	return 0;
#endif
}

IDemoRecorder *HLTVServerWrapperManager::GetDemoRecorderPtr(IHLTVServer *hltv)
{
	static int offset = -1;
	if (offset == -1)
	{
		void *addr;
		if (!g_pGameConf->GetAddress("CHLTVServer::m_DemoRecorder", &addr))
		{
			smutils->LogError(myself, "Failed to get CHLTVServer::m_DemoRecorder offset.");
			return nullptr;
		}

		// The "read" gives a pointer-sized value. The offset is its low 32 bits.
		offset = (int)(intptr_t)addr;

		// See if we have to subtract something from the offset.
		int baseOffset = 0;
		if (g_pGameConf->GetOffset("CHLTVDemoRecorder_BaseOffset", &baseOffset))
		{
			offset -= baseOffset;
		}
	}

	if (hltv)
	{
		IServer *baseServer = hltv->GetBaseServer();
#ifndef WIN32
	return (IDemoRecorder *)((intptr_t)baseServer + offset);
#else
#if SOURCE_ENGINE == SE_CSGO
		return (IDemoRecorder *)((intptr_t)hltv + offset);
#else
		return (IDemoRecorder *)((intptr_t)baseServer + offset);
#endif // SOURCE_ENGINE == SE_CSGO
#endif // !WIN32
	}
	else
	{
		return nullptr;
	}
}

int HLTVServerWrapperManager::GetIServerOffset()
{
	static int offset = -1;
	if (offset == -1)
	{
		if (!g_pGameConf->GetOffset("IServer_from_CHLTVServer", &offset) || offset == -1)
		{
			// IServer is +8 from CHLTVServer due to multiple inheritance - except for TF2?
			offset = 8;
		}
	}
	return offset;
}

bool HLTVServerWrapperManager::HasShutdownOffset()
{
	return m_bHasShutdownOffset;
}

bool HLTVServerWrapperManager::HasClientPrintfOffset()
{
	return m_bHasClientPrintfOffset;
}

#if SOURCE_ENGINE != SE_CSGO
bool HLTVServerWrapperManager::OnHLTVBotNetChanSendNetMsg(INetMessage &msg, bool bForceReliable, bool bVoice)
{
	// No need to worry about the right selected hltvserver, because there can only be one.
	IClient *pClient = iserver->GetClient(hltvserver->GetHLTVServer()->GetHLTVSlot());
	if (!pClient)
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	// Let the message flow through the intended path like CGameClient::SendNetMsg wants to.
	bool bRetSent = pClient->SendNetMsg(msg, bForceReliable);

	// It's important to supercede, because there is no original function to call.
	// (the "vtable" was empty before hooking it)
	// See FakeNetChan variable at the top.
	RETURN_META_VALUE(MRES_SUPERCEDE, bRetSent);
}
#endif

HLTVServerWrapperManager g_HLTVServers;
