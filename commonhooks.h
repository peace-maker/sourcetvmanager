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

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_COMMON_HOOKS_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_COMMON_HOOKS_H_

class IClient;
class CForwardManager;
class HLTVServerWrapper;

// defined() in a macro body is UB
#if !defined(WIN32) && SOURCE_ENGINE == SE_TF2
#define NEED_EXECUTESTRINGCMD_CBASECLIENT 1
#else
#define NEED_EXECUTESTRINGCMD_CBASECLIENT 0
#endif

class CCommonHooks {
public:
	void Init();

	// For forwards
	void AddSpectatorHook(CForwardManager *fwdmgr, IClient *client);
	void RemoveSpectatorHook(CForwardManager *fwdmgr, IClient *client);

	// For host_client and status fix
	void AddHLTVClientHook(HLTVServerWrapper *wrapper, IClient *client);
	void RemoveHLTVClientHook(HLTVServerWrapper *wrapper, IClient *client);

private:
#if NEED_EXECUTESTRINGCMD_CBASECLIENT
	bool m_bHasExecuteStringCommandOffset = false;
#endif
};

extern CCommonHooks g_pSTVCommonHooks;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_COMMON_HOOKS_H_
