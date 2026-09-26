# Minecraft server inside File Explorer

A working Minecraft 1.8.9 server that runs inside Windows File Explorer. The process listening on port 25565 is explorer.exe. There's no helper process, no relay and nothing else running.

The world is a folder. Every block is a file, every chunk is a folder and every player is a file you can delete. Place a block in Minecraft and a file appears. Delete the file and the block's gone.

## Why

Excel, Outlook, OBS, Obsidian, Blender, PowerPoint, VLC, Word, The Sims 4, Terraria, FL Studio, VS Code, DaVinci Resolve, Unity, Firefox, Garry's Mod, the whole planet, an iPhone, Valheim, Minecraft itself, and now File Explorer.

Explorer is already running on every Windows PC, so this is the one host you don't have to launch.

## What's different about this one

The server is a shell extension, a DLL that Explorer loads when you open a folder under This PC. The first time you open that folder, Explorer starts listening on 25565 and keeps going until you delete the file that says it's running.

The interface is Explorer's own. Delete, rename, Details view, sort by date and Show hidden items all do something to the game.

## What it does

- Server list ping with a folder icon drawn in code, and an MOTD of `File Explorer | pid 1234 | 50,176 files`. That's the pid of the Explorer holding the port and the number of blocks in the world as 'files'.
- Offline mode login, no encryption, no compression
- A 7x7 chunk creative flat world of bedrock, dirt and grass, which is where the 50,176 comes from
- Players see each other walk, look and hold items
- Chat, with the last 100 lines kept
- Keep alives every 15 seconds
- The world autosaves about a second after the last change, as a diff against the flat world

In Explorer, under This PC > Minecraft Server:

| Item | What you can do |
| --- | --- |
| `World` | one folder per chunk that has blocks in it, named `chunk X, Z` |
| `chunk X, Z` | one file per block, named `grass @ 3 3 7`. Opens in Details view, newest first |
| a block file | delete it and the block goes. Rename it to `diamond_block` and it changes. Rename it to nonsense and Windows refuses |
| a chunk folder | delete it and the whole chunk is emptied |
| `Players` | one file per player, with their position in the Comment column |
| a player file | delete it and they're kicked with "You were deleted by File Explorer". Rename it and they're renamed for everyone |
| `Chat` | the last 100 messages as files |
| `Type here to chat` | rename it to say something. It goes out as `[File Explorer] hello` and the file resets |
| `Time - noon` | rename it to `sunrise`, `day`, `noon`, `sunset`, `night`, `midnight` or a tick from 0 to 23999 |
| `Weather - clear` | rename it to `clear`, `rain` or `thunder`. Thunder brings lightning |
| `Server running on 25565 (pid 1234)` | delete it and everyone's kicked with "The server was deleted" and the port is freed. It becomes `Start server`, and opening that starts it again |

The Type column says what a block is, Date modified says when it was placed and Comment says who placed it. Size is two bytes per block, because that's what a block costs in a 1.8 chunk. Every block type gets its own coloured square as an icon, drawn from a colour table, wool colours included.

When someone joins, a toast appears with the join info. It doesn't take focus and it goes away by itself.

### Herobrine

Turn on Show hidden items and a faded `Herobrine` appears in the Players folder. At the same moment he appears in the game, standing at the edge of the world facing spawn. He's not in the tab list. His Comment says `Removed`.

Turn hidden items off and he's gone from both. Try to delete him and you need his permission. Nobody can join asW or rename themselves as Herobrine.

## What it doesn't do

No survival, no mobs, no items on the ground and no other dimensions. The world doesn't grow past its 7x7 chunks, and building is limited to the height of a 1.8 world.

**No authentication.** Anyone who can reach the port can join under any name.

**Deleted means deleted.** Nothing goes to the Recycle Bin and there's no undo. Files go without asking for confirmation.

**Renamed players are renamed for the session.** Reconnecting brings their real name back, because their client logs in with it.

**Herobrine looks like Steve.** Offline mode can't send a custom skin.

**Only Explorer runs the server.** The folder also shows up in other programs' Open and Save dialogs. There it tells you to open it in File Explorer.

It's a server in the sense that a client connects to it and receives a world. Set your expectations accordingly.

## Requirements

Windows 11, on ARM64 or x64. Built and tested on ARM64, build 26200. The x64 build compiles but hasn't been tested on an x64 machine.

Minecraft Java 1.8.9, protocol 47.

To build, Visual Studio 2026 or its Build Tools with the C++ workload and a Windows SDK, plus CMake 3.25 or newer. Nothing needs admin except the firewall rule below.

## Install

**1.** Build for your machine. On an ARM PC:

```
cmake --preset windows-arm64
cmake --build --preset windows-arm64
```

On an Intel or AMD PC:

```
cmake --preset windows-x64
cmake --build --preset windows-x64
```

**2.** Deploy. This copies the DLL to `%LOCALAPPDATA%\McExplorer\bin`, registers it for your user and restarts Explorer. Your taskbar disappears for a second or two.

```
powershell -ExecutionPolicy Bypass -File scripts\deploy.ps1
```

**3.** To let other machines join, allow Explorer through the firewall. This one needs an admin PowerShell:

```
New-NetFirewallRule -DisplayName "Minecraft Server in File Explorer" -Direction Inbound -Program "$env:WINDIR\explorer.exe" -Protocol TCP -LocalPort 25565 -Action Allow
```

**4.** Open This PC > Minecraft Server. The status file changes to `Server running on 25565`.

**5.** Connect Minecraft 1.8.9 to `localhost:25565`, or the PC's IP address from another machine.

### Scripts

| Script | What it does |
| --- | --- |
| `deploy.ps1` | copies the latest build under a new name, registers it, restarts Explorer and deletes old copies that aren't in use |
| `register.ps1 <dll>` | registers a DLL for your user. No admin needed |
| `unregister.ps1` | removes Minecraft Server from This PC. Restart Explorer to unload the DLL |
| `restart-explorer.ps1` | restarts Explorer, taskbar and all |
| `reset-world.ps1` | restarts Explorer with the saved world deleted, so the next start gets the flat world back |

A running Explorer never lets go of a DLL it has loaded, so every deploy uses a new file name and a restart.

## Use

Open the folder. The server starts in whichever Explorer process owns that window, and it keeps that process alive after the window closes, so it keeps running. Delete the status file to stop it.

The world is saved to `%LOCALAPPDATA%\McExplorer\world.bin` and the log is `%LOCALAPPDATA%\McExplorer\log.txt`. The log also goes to `OutputDebugString`.

`tools/standalone` builds `mcx_standalone.exe`, the same server without Explorer, for testing:

```
mcx_standalone --port 25565 --world world.bin
```

Type `say <text>`, `list` or `stop`.

## Proving it's Explorer

```
netstat -ano | findstr :25565
tasklist /fi "pid eq 1234"
```

The pid netstat prints is explorer.exe. It's the same pid as in the server list MOTD and the status file's name.

## Troubleshooting

**Minecraft Server isn't under This PC.** Explorer only reads the registration when it starts. Run `restart-explorer.ps1`.

**Minecraft Server never appears, even after a restart.** If Smart App Control is on, Windows blocks the unsigned DLL. Check Windows Security > App & browser control > Smart App Control. On many builds turning it off can't be undone without reinstalling Windows, so decide before you do.

**Nothing loads and nothing's logged.** The DLL doesn't match the machine. Explorer only loads a DLL built for its own architecture. Build with `windows-arm64` on an ARM PC and `windows-x64` on an Intel or AMD PC, then run `deploy.ps1`.

**Explorer crashes and the taskbar's gone.** Windows usually restarts it by itself. If it doesn't, press Ctrl+Shift+Esc, choose Run new task, and run `powershell -ExecutionPolicy Bypass -File scripts\restart-explorer.ps1`.

**The status file says `Port 25565 is in use - open to retry`.** Something else has the port, often another Explorer window's server. `netstat -ano | findstr :25565` says which pid. Stop that, then open the status file.

**The status file says `Open this folder in File Explorer`.** You're looking at it in another program's file dialog. Open it in Explorer.

**Explorer is still using an old build.** Run `deploy.ps1`. Registering alone doesn't reach an Explorer that already has the old DLL loaded.

**Other machines can't connect.** Check the firewall rule and check the PC's IP hasn't changed. If Windows runs in a VM with bridged networking over Wi-Fi, the host can lose sight of the VM after changing networks. Restarting the server or the VM's network adapter brings it back.

**The world has something in it you don't want.** Run `reset-world.ps1`.

## How it works

**A namespace extension.** The DLL registers a COM class under HKCU and a junction under This PC, so no admin is needed. Explorer asks it for the folder's contents through `IShellFolder2`, and each item is a PIDL: a few bytes that start with `MCX`, a version and the item's kind, then its coordinates, block state, id or name. Anything that doesn't decode is rejected, so a PIDL saved by an older build can't turn in to the wrong thing.

**Only Explorer starts it.** The server starts when the root folder is first listed, and only if the host process is explorer.exe. Windows 11 opens many Explorer windows in a separate `explorer.exe /factory` process that exits about 30 seconds after its last window closes. The server holds `SHGetInstanceExplorer` while it runs, which keeps that process alive.

**Changes reach open windows.** Every change in the game becomes an `SHChangeNotify` with absolute PIDLs: create, delete and rename for blocks, mkdir and rmdir for chunks. Player positions refresh at most once a second.

**Delete and rename.** Delete goes through the default context menu's callback as `DFM_CMD_DELETE`, which covers the Delete key, Shift+Delete and the menu, with no confirmation. Renames go through `SetNameOf`. The rename box only holds the part you can change, so renaming a block shows `grass`.

**Threads.** An accept thread, then a reader and a writer thread per connection. The reader polls with `select` every 250ms so it can be told to stop, because on Windows closing a socket doesn't wake a blocked `recv`. The writer drains a queue, and a client that falls 8MB behind is dropped. A tick thread sends keep alives. Every COM entry point and every thread catches everything and logs it, so a bug can't take Explorer down with it.

**The DLL never unloads.** Once the server has started, `DllCanUnloadNow` says no, and the server is deliberately never destroyed. Explorer can exit with it still running, but it can't be torn down under the loader lock.

**The 1.8 chunk format.** All sections' block arrays, then all block light, then all sky light, then 256 biome bytes. Blocks are little endian unsigned shorts of `(id << 4) | meta`, the one place in the protocol that isn't big endian.

**Join order.** Login Success, Join Game, Spawn Position, Player Abilities, Time Update, the weather if it's raining, all 49 chunks, then Player Position And Look. The packets are queued and the player's marked as playing under the same lock, so a block placed mid-join can't arrive before the chunk it's in.

**Renaming a player.** 1.8 has no packet that changes a name tag. The server removes the player's tab list entry, adds it back under the new name, then despawns and respawns them for everyone else in the same spot with the same item in hand. They never leave.

**Herobrine.** A 1.8 client won't spawn a player it hasn't seen in the tab list, so he's added, spawned and removed from the list in one go. Explorer writes Show hidden items to the registry, and the DLL waits on that key, so he appears the moment you tick the box. The folder only lists him when Explorer asks for hidden items.

**Time and weather.** Time Update is sent with a negative time of day, which tells the client to stop the sun. The item's name stays true that way. The 1.8 client fades rain out by itself unless the server keeps topping it up, so the strength is resent four times a second while it's raining.

**Saving.** `world.bin` holds only the blocks that differ from the flat world, with who changed them and when. It's written to a temporary file and renamed over the old one a second after the last change. A file that won't load is moved aside as `world.bin.bad` and a new world starts.

**Favicon.** A 64x64 folder drawn pixel by pixel and encoded as a PNG in code, with stored deflate blocks and hand rolled CRC and Adler checksums.

**Toasts.** A notification area icon on its own thread with its own hidden window. A join puts a line on a queue and posts a message, so nothing ever waits on a toast.

## Layout

```
core/                portable C++20: protocol, chunks, world, blocks, saving, PNG, server
shell/               the DLL: folder, PIDLs, icons, toasts, registration
tools/standalone/    the server without Explorer
scripts/             deploy, register, unregister, restart Explorer, reset the world
```

`core` has no Windows headers except in the socket layer's source file, so everything but the sockets builds anywhere.

## Licence

MIT. Do what you like with it.

Minecraft belongs to Mojang. File Explorer belongs to Microsoft. Nothing from either ships here.
