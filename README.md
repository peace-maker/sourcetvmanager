# SourceTV Manager
Interface to interact with the SourceTV server from SourcePawn.

This is a [SourceMod](http://www.sourcemod.net/) extension providing an API for plugins to access the [SourceTV](https://developer.valvesoftware.com/wiki/SourceTV) server instance(s).
[Forum thread](https://forums.alliedmods.net/showthread.php?t=280402)

#### API
There are natives and forwards to access
  * Basic SourceTV information (stats, delay, active)
  * Interacting with spectators (chat/console messages, kick, ip, name, tv title)
  * Forcing camera shots on the director
  * Demo recording (filename, recording tick, print message to demo console)

Have a look at the [include file](sourcetvmanager.inc).

#### Steam authentication for spectators
The extension adds a convar `tv_force_steamauth` (defaults to 0) which lets you enable steam validation on SourceTV clients.
Currently when this is enabled SourceTV relays are rejected.

#### `status` command fixes
There are several quirks around recording the `status` command output in a SourceTV demo.
While this extension is running, you're going to be able to do
```sourcepawn
FakeClientCommand(SourceTV_GetBotIndex(), "status");
```
and have the output recorded in the demo. This can help quickly identifying the players in the demo while playing it back.

Note: Doesn't run on relay proxies yet.

