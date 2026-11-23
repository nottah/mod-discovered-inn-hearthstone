/*
 * Discovered Inn Hearthstone for AzerothCore WotLK.
 * The normal Hearthstone cast, cast time and cooldown are retained; only its
 * homebind destination is replaced for that cast.
 */

#include "AllCreatureScript.h"
#include "AllItemScript.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameEventMgr.h"
#include "GameTime.h"
#include "Item.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "PlayerScript.h"
#include "Spell.h"
#include "ScriptedGossip.h"
#include "SpellScriptLoader.h"
#include "SpellScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr uint32 HEARTHSTONE_ITEM = 6948;
constexpr uint32 HEARTHSTONE_MENU_TEXT = 1;
constexpr uint32 HALLOWEEN_EVENT = 12;
constexpr uint32 SPELL_TRICK = 24714;
constexpr uint32 SPELL_TREAT = 24715;
constexpr uint32 SPELL_TRICKED_OR_TREATED = 24755;
constexpr uint32 INN_GOSSIP_MENU = 9733;
constexpr uint32 INN_EVENT_GOSSIP_MENU = 342;
constexpr uint32 ACTION_DISCOVER_INN = GOSSIP_ACTION_INFO_DEF + 1000;
constexpr uint32 ACTION_DESTINATION_BASE = GOSSIP_ACTION_INFO_DEF + 2000;
constexpr uint32 ACTION_PREVIOUS_PAGE = GOSSIP_ACTION_INFO_DEF + 3000;
constexpr uint32 ACTION_NEXT_PAGE = GOSSIP_ACTION_INFO_DEF + 3001;
constexpr uint32 ACTION_EMPTY_LIST = GOSSIP_ACTION_INFO_DEF + 3002;
constexpr uint32 PAGE_SIZE = 24;

struct InnDestination
{
    uint32 id = 0;
    uint32 creatureEntry = 0;
    uint32 areaId = 0;
    uint32 mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

struct PendingDestination
{
    InnDestination destination;
    uint64 selectedAt = 0;
};

std::unordered_map<uint32, std::vector<InnDestination>> activeDestinationPages;
std::unordered_map<uint32, uint32> activePageNumbers;
std::unordered_map<uint32, PendingDestination> pendingDestinations;

uint32 PlayerKey(Player const* player)
{
    return player->GetGUID().GetCounter();
}

bool IsDiscovered(Player const* player, Creature const* innkeeper)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM `character_hearthstone_inns` WHERE `guid` = {} AND `area_id` = {} AND `map_id` = {} LIMIT 1",
        PlayerKey(player), player->GetAreaId(), innkeeper->GetMapId());
    return bool(result);
}

std::string DestinationName(Player const* player, InnDestination const& destination)
{
    std::string areaName;
    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(destination.areaId))
    {
        LocaleConstant locale = player->GetSession()->GetSessionDbLocaleIndex();
        if (locale < TOTAL_LOCALES)
            areaName = area->area_name[locale];
        if (areaName.empty())
            areaName = area->area_name[LOCALE_enUS];
    }

    if (areaName.empty())
        areaName = "Map " + std::to_string(destination.mapId);
    return areaName;
}

std::vector<InnDestination> LoadDestinations(Player const* player)
{
    std::vector<InnDestination> destinations;
    std::set<std::pair<uint32, uint32>> discoveredAreas;
    QueryResult result = CharacterDatabase.Query(
        "SELECT `id`, `creature_entry`, `area_id`, `map_id`, `pos_x`, `pos_y`, `pos_z`, `pos_o` "
        "FROM `character_hearthstone_inns` WHERE `guid` = {} ORDER BY `area_id`, `id`",
        PlayerKey(player));
    if (!result)
        return destinations;

    do
    {
        Field* fields = result->Fetch();
        InnDestination destination;
        destination.id = fields[0].Get<uint32>();
        destination.creatureEntry = fields[1].Get<uint32>();
        destination.areaId = fields[2].Get<uint32>();
        destination.mapId = fields[3].Get<uint32>();
        destination.x = fields[4].Get<float>();
        destination.y = fields[5].Get<float>();
        destination.z = fields[6].Get<float>();
        destination.o = fields[7].Get<float>();

        if (!discoveredAreas.emplace(destination.mapId, destination.areaId).second)
            continue;

        destinations.push_back(destination);
    } while (result->NextRow());

    std::stable_sort(destinations.begin(), destinations.end(), [player](InnDestination const& left, InnDestination const& right)
    {
        return DestinationName(player, left) < DestinationName(player, right);
    });

    return destinations;
}

void AddTextOption(Player* player, std::string const& text, uint32 action)
{
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, text, GOSSIP_SENDER_MAIN, action);
}

void ShowDestinationPage(Player* player, Item* item, uint32 page)
{
    player->PlayerTalkClass->ClearMenus();
    std::vector<InnDestination> allDestinations = LoadDestinations(player);
    if (allDestinations.empty())
    {
        AddTextOption(player, "No inns discovered yet — close", ACTION_EMPTY_LIST);
        SendGossipMenuFor(player, HEARTHSTONE_MENU_TEXT, item->GetGUID());
        activeDestinationPages.erase(PlayerKey(player));
        activePageNumbers.erase(PlayerKey(player));
        return;
    }

    uint32 pageCount = (uint32(allDestinations.size()) + PAGE_SIZE - 1) / PAGE_SIZE;
    page = std::min(page, pageCount - 1);
    size_t begin = size_t(page) * PAGE_SIZE;
    size_t end = std::min(begin + PAGE_SIZE, allDestinations.size());

    std::vector<InnDestination> currentPage;
    currentPage.reserve(end - begin);
    for (size_t index = begin; index < end; ++index)
    {
        currentPage.push_back(allDestinations[index]);
        AddTextOption(player, DestinationName(player, allDestinations[index]),
                      ACTION_DESTINATION_BASE + uint32(index - begin));
    }

    if (page > 0)
        AddTextOption(player, "Previous page", ACTION_PREVIOUS_PAGE);
    if (page + 1 < pageCount)
        AddTextOption(player, "Next page", ACTION_NEXT_PAGE);

    activeDestinationPages[PlayerKey(player)] = std::move(currentPage);
    activePageNumbers[PlayerKey(player)] = page;
    SendGossipMenuFor(player, HEARTHSTONE_MENU_TEXT, item->GetGUID());
}

bool LoadDiscoveredDestination(Player* player, uint32 rowId, InnDestination& destination)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT `id`, `creature_entry`, `area_id`, `map_id`, `pos_x`, `pos_y`, `pos_z`, `pos_o` "
        "FROM `character_hearthstone_inns` WHERE `guid` = {} AND `id` = {} LIMIT 1",
        PlayerKey(player), rowId);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    destination.id = fields[0].Get<uint32>();
    destination.creatureEntry = fields[1].Get<uint32>();
    destination.areaId = fields[2].Get<uint32>();
    destination.mapId = fields[3].Get<uint32>();
    destination.x = fields[4].Get<float>();
    destination.y = fields[5].Get<float>();
    destination.z = fields[6].Get<float>();
    destination.o = fields[7].Get<float>();

    return MapMgr::IsValidMapCoord(destination.mapId, destination.x, destination.y,
                                   destination.z, destination.o);
}

class DiscoveredInnGossip final : public AllCreatureScript
{
public:
    DiscoveredInnGossip() : AllCreatureScript("DiscoveredInnGossip") { }

    bool CanCreatureGossipHello(Player* player, Creature* creature) override
    {
        if (!creature->IsInnkeeper() || creature->GetMap()->Instanceable())
            return false;

        ClearGossipMenuFor(player);

        if (IsEventActive(HALLOWEEN_EVENT) && !player->HasAura(SPELL_TRICKED_OR_TREATED))
            AddGossipItemFor(player, INN_EVENT_GOSSIP_MENU, 0, GOSSIP_SENDER_MAIN,
                             GOSSIP_ACTION_INFO_DEF + HALLOWEEN_EVENT);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());
        if (creature->IsVendor())
            AddGossipItemFor(player, INN_GOSSIP_MENU, 2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);
        AddGossipItemFor(player, INN_GOSSIP_MENU, 1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INN);

        if (!IsDiscovered(player, creature))
            AddTextOption(player, "Discover this inn for your Hearthstone", ACTION_DISCOVER_INN);

        player->TalkedToCreature(creature->GetEntry(), creature->GetGUID());
        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!creature->IsInnkeeper() || creature->GetMap()->Instanceable())
            return false;

        switch (action)
        {
            case ACTION_DISCOVER_INN:
            {
                CharacterDatabase.DirectExecute(
                    "INSERT IGNORE INTO `character_hearthstone_inns` "
                    "(`guid`, `inn_guid`, `creature_entry`, `area_id`, `map_id`, `pos_x`, `pos_y`, `pos_z`, `pos_o`) "
                    "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})",
                    PlayerKey(player), creature->GetGUID().GetRawValue(), creature->GetEntry(), player->GetAreaId(),
                    creature->GetMapId(), creature->GetPositionX(), creature->GetPositionY(),
                    creature->GetPositionZ(), creature->GetOrientation());
                player->GetSession()->SendAreaTriggerMessage("Inn added to your Hearthstone destinations.");
                CloseGossipMenuFor(player);
                return true;
            }
            case GOSSIP_ACTION_TRADE:
                CloseGossipMenuFor(player);
                player->GetSession()->SendListInventory(creature->GetGUID());
                return true;
            case GOSSIP_ACTION_INN:
                CloseGossipMenuFor(player);
                player->SetBindPoint(creature->GetGUID());
                return true;
            case GOSSIP_ACTION_INFO_DEF + HALLOWEEN_EVENT:
                if (!IsEventActive(HALLOWEEN_EVENT) || player->HasAura(SPELL_TRICKED_OR_TREATED))
                    return false;

                player->CastSpell(player, SPELL_TRICKED_OR_TREATED, true);
                creature->CastSpell(player, roll_chance_i(50) ? SPELL_TRICK : SPELL_TREAT, true);
                CloseGossipMenuFor(player);
                return true;
            default:
                return false;
        }
    }
};

class DiscoveredInnHearthstone final : public AllItemScript
{
public:
    DiscoveredInnHearthstone() : AllItemScript("DiscoveredInnHearthstone") { }

    bool CanItemUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (item->GetEntry() != HEARTHSTONE_ITEM)
            return false;

        if (!player->IsAlive() || player->IsInCombat() || player->IsInFlight())
        {
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            player->GetSession()->SendAreaTriggerMessage("You cannot use your Hearthstone right now.");
            return true;
        }

        // Until the character discovers an inn, preserve the stock Hearthstone
        // behavior so it can still use the character's normal home bind.
        if (LoadDestinations(player).empty())
            return false;

        player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
        activePageNumbers[PlayerKey(player)] = 0;
        player->GetSession()->SendAreaTriggerMessage("Opening discovered Hearthstone destinations.");
        ShowDestinationPage(player, item, 0);
        return true;
    }

    void OnItemGossipSelect(Player* player, Item* item, uint32 /*sender*/, uint32 action) override
    {
        if (item->GetEntry() != HEARTHSTONE_ITEM)
            return;

        uint32 playerGuid = PlayerKey(player);
        if (action == ACTION_EMPTY_LIST)
        {
            CloseGossipMenuFor(player);
            return;
        }
        if (action == ACTION_NEXT_PAGE || action == ACTION_PREVIOUS_PAGE)
        {
            uint32 page = activePageNumbers[playerGuid];
            if (action == ACTION_NEXT_PAGE)
                ++page;
            else if (page > 0)
                --page;
            ShowDestinationPage(player, item, page);
            return;
        }

        if (action < ACTION_DESTINATION_BASE || action >= ACTION_DESTINATION_BASE + PAGE_SIZE)
            return;

        auto page = activeDestinationPages.find(playerGuid);
        uint32 index = action - ACTION_DESTINATION_BASE;
        if (page == activeDestinationPages.end() || index >= page->second.size())
        {
            CloseGossipMenuFor(player);
            return;
        }

        InnDestination destination;
        if (!LoadDiscoveredDestination(player, page->second[index].id, destination) ||
            player->IsInCombat() || !player->IsAlive() || player->IsInFlight())
        {
            player->GetSession()->SendAreaTriggerMessage("That Hearthstone destination is no longer available.");
            CloseGossipMenuFor(player);
            return;
        }

        pendingDestinations[playerGuid] = {
            destination, static_cast<uint64>(GameTime::GetGameTimeMS().count()) };
        activeDestinationPages.erase(page);
        activePageNumbers.erase(playerGuid);
        CloseGossipMenuFor(player);

        // Start the item's real spell. That preserves the normal cast bar,
        // interrupt rules, item cooldown and client cooldown display.
        player->CastItemUseSpell(item, SpellCastTargets(), 0, 0);
    }
};

// Some client action-bar paths submit the item's spell directly. Catch those
// casts too; a selection in our menu sets a pending destination and is allowed
// through to the normal item spell/cooldown handling.
class DiscoveredInnCastFallback final : public PlayerScript
{
public:
    DiscoveredInnCastFallback()
        : PlayerScript("DiscoveredInnCastFallback", { PLAYERHOOK_CAN_CAST_ITEM_USE_SPELL }) { }

    bool OnPlayerCanCastItemUseSpell(Player* player, Item* item, SpellCastTargets const& /*targets*/,
                                     uint8 /*castCount*/, uint32 /*glyphIndex*/) override
    {
        if (!item || item->GetEntry() != HEARTHSTONE_ITEM)
            return true;

        uint32 playerGuid = PlayerKey(player);
        auto pending = pendingDestinations.find(playerGuid);
        if (pending != pendingDestinations.end())
        {
            uint64 now = static_cast<uint64>(GameTime::GetGameTimeMS().count());
            if (now - pending->second.selectedAt <= 120000)
                return true;
            pendingDestinations.erase(pending);
        }

        if (!player->IsAlive() || player->IsInCombat() || player->IsInFlight())
        {
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            player->GetSession()->SendAreaTriggerMessage("You cannot use your Hearthstone right now.");
            return false;
        }

        // Let the normal item spell run when there is no discovered destination.
        if (LoadDestinations(player).empty())
            return true;

        player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
        activePageNumbers[playerGuid] = 0;
        player->GetSession()->SendAreaTriggerMessage("Opening discovered Hearthstone destinations.");
        ShowDestinationPage(player, item, 0);
        return false;
    }
};

class spell_discovered_inn_hearthstone final : public SpellScriptLoader
{
public:
    spell_discovered_inn_hearthstone() : SpellScriptLoader("spell_discovered_inn_hearthstone") { }

    class SpellScriptImpl final : public SpellScript
    {
        PrepareSpellScript(SpellScriptImpl);

        void ReplaceHomebindDestination(SpellEffIndex /*effectIndex*/)
        {
            Player* player = GetCaster()->ToPlayer();
            Item* item = GetCastItem();
            if (!player || !item || item->GetEntry() != HEARTHSTONE_ITEM)
                return;

            uint32 playerGuid = PlayerKey(player);
            auto pending = pendingDestinations.find(playerGuid);
            if (pending == pendingDestinations.end())
                return;

            PendingDestination const& selected = pending->second;
            if (static_cast<uint64>(GameTime::GetGameTimeMS().count()) - selected.selectedAt > 120000)
            {
                pendingDestinations.erase(pending);
                return;
            }

            if (WorldLocation* destination = GetHitDest())
                destination->WorldRelocate(selected.destination.mapId,
                                           selected.destination.x,
                                           selected.destination.y,
                                           selected.destination.z,
                                           selected.destination.o);
            pendingDestinations.erase(pending);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(SpellScriptImpl::ReplaceHomebindDestination,
                                               EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new SpellScriptImpl();
    }
};
}

void AddSC_DiscoveredInnHearthstone()
{
    new DiscoveredInnGossip();
    new DiscoveredInnHearthstone();
    new DiscoveredInnCastFallback();
    new spell_discovered_inn_hearthstone();
}
