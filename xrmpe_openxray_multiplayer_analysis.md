# Analyse xrMPE / OpenXRay - Multiplayer CoP

## 1. Findings Ghidra sur le mod installe

### Structure globale

- Le mod installe une distribution engine complete, pas un simple addon Lua:
  - `game/bin/xrEngine.exe`, `xrGame.dll`, `xrNetServer.dll`, `xrGameSpy.dll`, `xrCore.dll`, renderers, physics, sound.
  - `game/bin/dedicated/xrEngine.exe` et `xrD3D9-Null.dll` pour serveur dedie.
  - archives `game/patches/xrmpe_*.db`, `game/addons/*.addon`, `game/mp/df_*.db`.
  - `server_config.json` expose `port:5446`, mot de passe, visibilite, rotation de maps, min players.

### Transport reseau

Ghidra confirme que `xrNetServer.dll` est un fork de la couche reseau X-Ray:

- chemins PDB/strings: `G:\xrMPE\GIT\xray16_xrMPE\xray\xrNetServer\...`
- classes exportees:
  - `IPureClient`, `IPureServer`, `IClient`
  - `IPureSteamClient`, `IPureSteamServer`
  - `MultipacketSender`, `MultipacketReciever`
  - `NET_Compressor`, `INetQueue`, `syncQueue`
- imports directs:
  - `GameNetworkingSockets.dll::SteamNetworkingIPAddr_ParseString`
  - `GameNetworkingSockets.dll::SteamNetworkingIPAddr_ToString`
  - `steam_api.dll` charge dynamiquement via `CSteamHelper::Load`
  - fonctions Steam dynamiques: `SteamAPI_InitFlat`, `SteamAPI_RunCallbacks`, `SteamAPI_SteamNetworkingSockets_SteamAPI_v012`, `GameNetworkingSockets_Init`, `GameNetworkingSockets_Kill`

Conclusion: le protocole bas niveau n'est pas RakNet/ENet. C'est une migration de la couche X-Ray `NET_Packet` vers Steam GameNetworkingSockets, avec compat/heritage DirectPlay conserve dans les interfaces et flags.

### Forme des paquets

OpenXRay `xrNetServer/NET_Common.cpp` et le binaire xrMPE convergent:

- plusieurs `NET_Packet` logiques sont concatenees dans un paquet physique.
- chaque sous-paquet est prefixe par `u16 packet_size`.
- paquet physique:
  - `u8 tag` (`NET_TAG_MERGED` ou `NET_TAG_NONMERGED`)
  - `u16 unpacked_size`
  - payload compresse via `NET_Compressor`
- limite observee: `MaxMultipacketSize = 32768`.

Les flags X-Ray originaux restent visibles dans `NET_Messages.h`: reliable, sequential, high priority, immediate. Dans xrMPE ils sont adaptes au transport Steam/GNS.

### Gameplay et synchronisation

`xrGame.dll` importe massivement `NET_Packet` depuis `xrCore.dll`:

- `w_u16`, `w_u8`, `w_u32`, `w_vec3`, `w_float_q8/q16`, `w_angle8/16`
- `r_u16`, `r_u8`, `r_vec3`, `r_float_q8/q16`, `r_angle8/16`
- `w_begin`, `r_begin`, chunks 8/16, `w_clientID`, `r_clientID`

Ghidra detecte dans `xrGame.dll` une extension absente des sources OpenXRay locales:

- `NET_SyncStuff::CAI_StalkerSyncState`
- `ReadPosition`, `WritePosition`
- `ReadRotation`, `WriteRotation`
- `ReadAnim`, `WriteAnim`
- `SyncDifference`
- `IsAnimationDifferent`, `IsRotationDifferent`

Interpretation: xrMPE ajoute une synchronisation specifique des NPC stalkers, probablement par dirty checks position/rotation/animation.

OpenXRay conserve les primitives serveur qui servent de base:

- `xrServer::Process_spawn`
- `xrServer::Process_update`
- `xrServer::Process_event`
- `Process_event_ownership`, `Process_event_reject`, `Process_event_destroy`, `Process_event_activate`
- `server_updates_compressor`

### Inventaire, mort, respawn, scripts

Dans `xrServer_process_event.cpp`, les evenements existants couvrent:

- inventaire: `GE_INV_ACTION`, `GE_INV_BOX_STATUS`, `GE_INV_OWNER_STATUS`, `GE_OWNERSHIP_TAKE`, `GE_OWNERSHIP_REJECT`, `GE_TRANSFER_AMMO`, `GE_ADDON_ATTACH/DETACH`, `GE_INSTALL_UPGRADE`, `GE_MONEY`
- mort/combat: `GE_HIT`, `GE_DIE`, `GE_ASSIGN_KILLER`, `GE_KILL_SOMEONE`
- respawn: `GE_RESPAWN`
- scripts/gameplay: `GE_GAME_EVENT`, `GAME_EVENT_ON_HIT`, delayed events

`xrNetServer.dll` contient aussi `NET_AuthCheck` qui ignore certains scripts/configs et verifie d'autres chemins, dont:

- `$game_config$\scripts`
- `$game_config$\misc\script_sound_pripyat.ltx`
- `$game_scripts$\state_mgr_pri_a15.script`

Cela ressemble a un compromis anti-desync/anti-cheat: verifier le gros des scripts, mais exclure des fichiers variables ou bruyants.

## 2. Croisement OpenXRay

### Parties exploitees directement

- `xrNetServer`: transport, queue, compression, time sync, bandwidth stats.
- `xrCore::NET_Packet`: serialization binaire historique.
- `xrGame::xrServer`: spawn/update/event authoritative-ish.
- `CSE_Abstract` et descendants: etat serveur serialize par `UPDATE_Read`, `UPDATE_Write`, `load`, `STATE_Write`.
- `CObject` / `CObjectList`: `net_Spawn`, `net_Import`, `net_Export`, migration.
- Lua/Luabind: `xrGame.dll` charge LuaJIT/Luabind, donc les scripts restent dans la boucle gameplay.

### Ecarts xrMPE vs OpenXRay local

- OpenXRay local contient encore DirectPlay8 dans `NET_Client.cpp` / `NET_Server.cpp`.
- xrMPE remplace/complete le transport par Steam GameNetworkingSockets (`IPureSteamClient/Server`).
- xrMPE ajoute `NET_SyncStuff::CAI_StalkerSyncState`, non present dans les sources locales.
- xrMPE embarque un serveur dedie et des assets/configs specifiques defence/coop/PvP.

## 3. Faiblesses probables de l'implementation

- Le modele conserve beaucoup de semantics X-Ray historiques: paquets monolithiques, flags DirectPlay, compression globale, event forwarding.
- Validation serveur incomplete: plusieurs events client sont forwardes ou appliques apres controle minimal d'ownership.
- Synchronisation NPC ajoutee par cas special (`CAI_StalkerSyncState`) plutot qu'un systeme generique de replication components.
- Scripts Lua difficiles a rendre deterministes; verifier les fichiers ne garantit pas l'ordre d'execution ni les side effects.
- Compression/merge global peut amplifier la latence si un sous-paquet fiable bloque ou si la priorisation est trop grossiere.
- Anti-cheat faible si les inputs ne sont pas l'unique source acceptee et si les clients peuvent proposer des etats complets.

## 4. Architecture recommandee pour une refonte

```text
Game Lua / gameplay scripts
        |
Script Replication API
        |
Gameplay Net Facade
        |
Replication Layer
  - entity registry
  - authority / ownership
  - dirty component masks
  - interest management
        |
Snapshot + Event Channels
  - unreliable sequenced snapshots
  - reliable ordered commands/events
  - reliable unordered asset/session control
        |
Transport Adapter
  - Steam GameNetworkingSockets or ENet
  - metrics, loss, RTT, congestion
        |
OpenXRay Core
  - NET_Packet replacement/adaptor
  - CSE_Abstract serialization
  - Lua bridge
```

### Choix techniques proposes

- Architecture: serveur autoritaire dedie/listen server, pas peer-to-peer.
- Transport: GameNetworkingSockets si Steam/NAT traversal/relay compte; ENet si independance et simplicite.
- Serialization: schemas explicites versionnes. FlatBuffers/Cap'n Proto pour snapshots, ou bitstream maison pour hot path; eviter Protobuf pour transforms frequents.
- Tick serveur: 30 Hz MVP, 20 Hz possible pour coop PvE, 60 Hz seulement pour PvP exigeant.
- Snapshots: 10-20 Hz avec interpolation client 100-150 ms.
- Inputs: client envoie inputs horodates, serveur simule/valide, client predit seulement son acteur.
- Events Lua: RPC nommes et whitelistes, idempotents, jamais execution arbitraire de code recu du reseau.

## 5. MVP par priorites

1. Transport + handshake: version protocole, checksum assets/scripts, auth minimale.
2. Replication joueur: transform, stance, animation, weapon state, health.
3. Serveur autoritaire inputs: mouvement, tir, hit validation simple.
4. Spawn/despawn monde: mapping `CSE_Abstract` vers `NetEntityId`.
5. Inventaire minimal: ownership take/reject, equip/unequip, ammo, money.
6. NPC PvE: position/rotation/animation, puis AI authority serveur.
7. Lua RPC whitelist: quest/event notifications, UI-safe events.
8. Interest management: zones, distance, relevance per entity class.
9. Persistence/saves coop: snapshot serveur, migration map.
10. Anti-cheat: rate limits, sanity checks, command validation, server-side hit authority.

## 6. Exemples critiques

### C++: dirty replication component

```cpp
enum class RepField : uint32_t {
    Transform = 1 << 0,
    Animation = 1 << 1,
    Health    = 1 << 2,
    Inventory = 1 << 3,
};

struct RepState {
    Fvector pos;
    Fvector dir;
    uint16_t anim;
    uint16_t health;
};

struct RepComponent {
    NetEntityId id;
    RepState lastSent{};
    uint32_t dirtyMask = 0;

    void markDirty(RepField f) { dirtyMask |= static_cast<uint32_t>(f); }

    void writeDelta(BitWriter& w, const RepState& current) {
        w.write(id);
        w.writeBits(dirtyMask, 16);
        if (dirtyMask & uint32_t(RepField::Transform)) {
            w.writeQuantizedVec3(current.pos, 0.01f);
            w.writePackedDir(current.dir);
        }
        if (dirtyMask & uint32_t(RepField::Animation))
            w.write(current.anim);
        if (dirtyMask & uint32_t(RepField::Health))
            w.write(current.health);
        lastSent = current;
        dirtyMask = 0;
    }
};
```

### C++: serveur autoritaire par input

```cpp
void ServerPlayerController::onInput(ClientId client, const InputCmd& cmd) {
    Player& p = players.byClient(client);
    if (!rateLimiter.allow(client) || cmd.sequence <= p.lastInputSeq)
        return;

    if (!isInputPlausible(p, cmd))
        return flagSuspicious(client, "implausible input");

    simulatePlayer(p, cmd, fixedDt);
    p.lastInputSeq = cmd.sequence;
    replication.markDirty(p.entityId, RepField::Transform);
}
```

### Lua: RPC whitelist

```lua
net.register_rpc("quest_update", function(ctx, quest_id, state)
    if not ctx.from_server then return end
    quests.set_state(quest_id, state)
end)

function notify_quest_update(player, quest_id, state)
    net.send_rpc(player, "quest_update", quest_id, state)
end
```

## 7. Pieges a eviter

- Ne pas faire confiance a `GE_*` venant du client sans validation ownership/cooldown/etat.
- Ne pas synchroniser les scripts par fichier seulement; synchroniser les intentions gameplay.
- Ne pas envoyer des etats complets NPC a chaque frame; utiliser dirty masks + relevance.
- Ne pas melanger reliable inventory/events avec transforms frequents dans le meme canal bloquant.
- Ne pas attacher le gameplay a Steam/GNS directement; passer par un `ITransport`.
- Ne pas patcher au cas par cas chaque classe AI; creer des composants de replication reutilisables.
