-- Session 5 example pack for xrMPNet script replication.
-- These snippets assume a bound `script_replication_api` instance named `net_api`.

-- 1. Quest sync: server tells clients that a quest stage advanced.
net_api:on_rpc("quest_stage_update", function(args)
    local quest_id = args[1]
    local stage = args[2]
    local started = args[3]
    printf("quest %s stage %d started=%s", quest_id, stage, tostring(started))
    return true
end)

-- 2. UI event: server sends a safe HUD notification.
net_api:on_rpc("ui_notification", function(args)
    local title = args[1]
    local message = args[2]
    local timeout_seconds = args[3]
    printf("ui notification [%s] %s (%0.1fs)", title, message, timeout_seconds)
    return true
end)

-- 3. Game mode event: client asks the server to vote for a map.
function send_map_vote(map_name, vote_value)
    return net_api:send_rpc("vote_map", { map_name, vote_value })
end

-- 4. NPC authority trigger: server notifies clients that an AI squad changed state.
net_api:on_rpc("npc_squad_state", function(args)
    local squad_name = args[1]
    local state_name = args[2]
    local focus_entity_id = args[3]
    printf("npc squad %s -> %s focus=%d", squad_name, state_name, focus_entity_id)
    return true
end)

-- 5. Coop marker sync: client requests a ping marker, server rebroadcasts if allowed.
function send_coop_ping(marker_name, marker_x, marker_y, marker_z)
    return net_api:send_rpc("coop_ping_marker", { marker_name, marker_x, marker_y, marker_z })
end
