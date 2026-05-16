#include "BotTokenExchangerMgr.h"

#include "Chat.h"
#include "Bag.h"
#include "AiFactory.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupReference.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "StringFormat.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>
#include <unordered_map>

namespace
{
    constexpr std::array<uint32, 6> kKnownTbcVendors = { 20613, 20616, 21905, 21906, 23381, 25976 };
    constexpr std::array<uint32, 25> kKnownWotlkNarrowVendors =
    {
        28992, 28995, 28997, 29523, 34252, 35496, 35497, 35498, 35500, 37688,
        37696, 37991, 37992, 37993, 37997, 37998, 37999, 38054, 38181, 38182,
        38283, 38284, 38316, 38840, 38841
    };

    constexpr std::array<std::string_view, 5> kTokenNameFragments =
    {
        "Fallen",
        "Vanquished",
        "Champion",
        "Defender",
        "Hero"
    };

    bool ContainsCaseInsensitive(std::string const& haystack, std::string_view needle)
    {
        if (needle.empty())
            return true;

        if (haystack.size() < needle.size())
            return false;

        std::string lowerHaystack(haystack);
        std::string lowerNeedle(needle);

        std::transform(lowerHaystack.begin(), lowerHaystack.end(), lowerHaystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return lowerHaystack.find(lowerNeedle) != std::string::npos;
    }

    void CollectTokenCounts(Player* player, std::unordered_map<uint32, uint32>& tokenCounts)
    {
        if (!player)
            return;

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                ItemTemplate const* proto = item->GetTemplate();
                if (proto && proto->InventoryType == INVTYPE_NON_EQUIP)
                    tokenCounts[proto->ItemId] += item->GetCount();
            }
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            if (Bag* bag = player->GetBagByPos(bagSlot))
            {
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                {
                    if (Item* item = bag->GetItemByPos(slot))
                    {
                        ItemTemplate const* proto = item->GetTemplate();
                        if (proto && proto->InventoryType == INVTYPE_NON_EQUIP)
                            tokenCounts[proto->ItemId] += item->GetCount();
                    }
                }
            }
        }
    }

    TeamId GetTeamIdForFiltering(Player const* player)
    {
        return player ? player->GetTeamId(true) : TEAM_NEUTRAL;
    }

    char const* GetTeamLabel(TeamId teamId)
    {
        switch (teamId)
        {
            case TEAM_ALLIANCE:
                return "Alliance";
            case TEAM_HORDE:
                return "Horde";
            default:
                return "Neutral";
        }
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool ContainsAny(std::string const& text, std::initializer_list<char const*> needles)
    {
        std::string lowered = ToLowerCopy(text);
        for (char const* needle : needles)
        {
            if (needle && *needle && lowered.find(ToLowerCopy(needle)) != std::string::npos)
                return true;
        }
        return false;
    }

    char const* RollVoteToString(RollVote vote)
    {
        switch (vote)
        {
            case NEED:
                return "NEED";
            case GREED:
                return "GREED";
            case DISENCHANT:
                return "DISENCHANT";
            case PASS:
                return "PASS";
            default:
                return "UNKNOWN";
        }
    }

}

BotTokenExchangerMgr& BotTokenExchangerMgr::instance()
{
    static BotTokenExchangerMgr instance;
    return instance;
}

void BotTokenExchangerMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("BotTokenExchanger.Enable", true);
    _debug = sConfigMgr->GetOption<bool>("BotTokenExchanger.Debug", false);
    _onlyPlayerbots = sConfigMgr->GetOption<bool>("BotTokenExchanger.OnlyPlayerbots", true);
    _exchangeDelayMs = sConfigMgr->GetOption<uint32>("BotTokenExchanger.ExchangeDelayMs", 1000);
    _announceToBotOwner = sConfigMgr->GetOption<bool>("BotTokenExchanger.AnnounceToBotOwner", false);
    _discoveryWriteDb = sConfigMgr->GetOption<bool>("BotTokenExchanger.DiscoveryWriteDb", false);
    _resolveOnly = sConfigMgr->GetOption<bool>("BotTokenExchanger.ResolveOnly", true);
    _dryRun = sConfigMgr->GetOption<bool>("BotTokenExchanger.DryRun", true);
    _exchangeEnable = sConfigMgr->GetOption<bool>("BotTokenExchanger.ExchangeEnable", false);
    _allowDebugTargetCommand = sConfigMgr->GetOption<bool>("BotTokenExchanger.AllowDebugTargetCommand", false);
    _playerbotLootPassEnable = sConfigMgr->GetOption<bool>("BotTokenExchanger.PlayerbotLootPassEnable", false);
    _autoExchangeEnable = sConfigMgr->GetOption<bool>("BotTokenExchanger.AutoExchangeEnable", false);
    _wotlkExchangeEnable = sConfigMgr->GetOption<bool>("BotTokenExchanger.WotlkExchangeEnable", false);
    _wotlkDryRun = sConfigMgr->GetOption<bool>("BotTokenExchanger.WotlkDryRun", true);
    _wotlkAutoExchangeEnable = sConfigMgr->GetOption<bool>("BotTokenExchanger.WotlkAutoExchangeEnable", false);
    _autoExchangeDelayMs = sConfigMgr->GetOption<uint32>("BotTokenExchanger.AutoExchangeDelayMs", 1500);
    _autoExchangeOnLoot = sConfigMgr->GetOption<bool>("BotTokenExchanger.AutoExchangeOnLoot", true);
    _autoExchangeOnLogin = sConfigMgr->GetOption<bool>("BotTokenExchanger.AutoExchangeOnLogin", false);
    _autoExchangeMaxPerBotPerPass = sConfigMgr->GetOption<uint32>("BotTokenExchanger.AutoExchangeMaxPerBotPerPass", 1);

    LOG_INFO(
        "server",
        "BotTokenExchanger config loaded: Enable={} Debug={} OnlyPlayerbots={} DiscoveryWriteDb={} ResolveOnly={} DryRun={} ExchangeEnable={} AllowDebugTargetCommand={} PlayerbotLootPassEnable={} AutoExchangeEnable={} WotlkExchangeEnable={} WotlkDryRun={} WotlkAutoExchangeEnable={} AutoExchangeDelayMs={} AutoExchangeOnLoot={} AutoExchangeOnLogin={} AutoExchangeMaxPerBotPerPass={}",
        _enabled ? 1 : 0,
        _debug ? 1 : 0,
        _onlyPlayerbots ? 1 : 0,
        _discoveryWriteDb ? 1 : 0,
        _resolveOnly ? 1 : 0,
        _dryRun ? 1 : 0,
        _exchangeEnable ? 1 : 0,
        _allowDebugTargetCommand ? 1 : 0,
        _playerbotLootPassEnable ? 1 : 0,
        _autoExchangeEnable ? 1 : 0,
        _wotlkExchangeEnable ? 1 : 0,
        _wotlkDryRun ? 1 : 0,
        _wotlkAutoExchangeEnable ? 1 : 0,
        _autoExchangeDelayMs,
        _autoExchangeOnLoot ? 1 : 0,
        _autoExchangeOnLogin ? 1 : 0,
        _autoExchangeMaxPerBotPerPass);

    UpdatePlayerbotLootPassCallback();
}

void BotTokenExchangerMgr::PreloadRuntimeCaches()
{
    if (!_enabled)
        return;

    LoadResolverMappings();
    LoadWotlkResolverMappings();
    LoadPreferenceMappings();
}

void BotTokenExchangerMgr::UpdatePlayerbotLootPassCallback()
{
    if (!_enabled || !_playerbotLootPassEnable)
    {
        SetPlayerbotBeforeLootRollCallback({});
        return;
    }

    SetPlayerbotBeforeLootRollCallback([](Player* bot, ItemTemplate const* itemTemplate, RollVote& rollVote)
    {
        return sBotTokenExchangerMgr.HandlePlayerbotBeforeLootRoll(bot, itemTemplate, rollVote);
    });
}

void BotTokenExchangerMgr::HandlePlayerStoreNewItem(Player* player, Item* item, uint32 count)
{
    if (!_enabled || !player || !item)
        return;

    if (!player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (_exchangeActive)
        return;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        if (_debug)
            LOG_DEBUG("bot_token_exchanger", "BotTokenExchanger: {} received an item with no template (count {}).", player->GetName(), count);
        return;
    }

    if (_autoExchangeEnable && _autoExchangeOnLoot)
    {
        std::vector<ResolverEntry> const* entries = GetResolverEntries(proto->ItemId);
        if (entries && !entries->empty())
        {
            if (_debug)
                LOG_DEBUG("bot_token_exchanger", "BotTokenExchanger: bot {} received staged TBC token {} ({}) x{}.", player->GetName(), proto->ItemId, proto->Name1, count);
            QueueAutoExchange(player, proto->ItemId, false, false, "loot");
        }
    }

    if (_wotlkAutoExchangeEnable && _autoExchangeOnLoot)
    {
        std::vector<ResolverEntry> const* wotlkEntries = GetWotlkResolverEntries(proto->ItemId);
        if (wotlkEntries && !wotlkEntries->empty())
        {
            LOG_INFO("bot_token_exchanger", "WOTLK auto detected staged token {} ({}) x{} for bot {}", proto->ItemId, proto->Name1, count, player->GetName());
            QueueWotlkAutoExchange(player, proto->ItemId, false, false, "loot");
        }
    }
}

void BotTokenExchangerMgr::HandlePlayerLogin(Player* player)
{
    if (!_enabled || !player || !player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (_exchangeActive)
        return;

    if (_autoExchangeEnable && _autoExchangeOnLogin)
        QueueAutoExchange(player, 0, true, false, "login");

    if (_wotlkAutoExchangeEnable && _autoExchangeOnLogin)
        QueueWotlkAutoExchange(player, 0, true, false, "login");
}

void BotTokenExchangerMgr::HandlePlayerUpdate(Player* player, uint32 /*diff*/)
{
    if (!_enabled || !player || !player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (!_autoExchangeEnable && !_wotlkAutoExchangeEnable)
        return;

    if (_autoExchangeEnable)
        ProcessAutoExchangeQueue(player);
    if (_wotlkAutoExchangeEnable)
        ProcessWotlkAutoExchangeQueue(player);
}

void BotTokenExchangerMgr::HandlePlayerLogout(Player* player)
{
    if (!player)
        return;

    uint64 const botGuid = player->GetGUID().GetCounter();
    _autoExchangeQueueByBotGuid.erase(botGuid);
    _wotlkAutoExchangeQueueByBotGuid.erase(botGuid);
}

bool BotTokenExchangerMgr::IsKnownTbcVendor(uint32 vendorEntry)
{
    return std::find(kKnownTbcVendors.begin(), kKnownTbcVendors.end(), vendorEntry) != kKnownTbcVendors.end();
}

bool BotTokenExchangerMgr::IsKnownWotlkNarrowVendor(uint32 vendorEntry)
{
    return std::find(kKnownWotlkNarrowVendors.begin(), kKnownWotlkNarrowVendors.end(), vendorEntry) != kKnownWotlkNarrowVendors.end();
}

void BotTokenExchangerMgr::QueueAutoExchange(Player* player, uint32 tokenItemId, bool queueAllStagedTokens, bool logWhenEmpty, char const* reason)
{
    if (!_autoExchangeEnable || !player || !player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (_exchangeActive)
        return;

    LoadResolverMappings();

    uint64 const botGuid = player->GetGUID().GetCounter();

    std::unordered_set<uint32> stagedTokenIds;
    if (queueAllStagedTokens)
    {
        std::unordered_map<uint32, uint32> tokenCounts;
        CollectTokenCounts(player, tokenCounts);
        for (auto const& [tokenId, count] : tokenCounts)
        {
            (void)count;
            std::vector<ResolverEntry> const* entries = GetResolverEntries(tokenId);
            if (entries && !entries->empty())
                stagedTokenIds.insert(tokenId);
        }
    }
    else if (tokenItemId != 0)
    {
        std::vector<ResolverEntry> const* entries = GetResolverEntries(tokenItemId);
        if (entries && !entries->empty())
            stagedTokenIds.insert(tokenItemId);
    }

    if (stagedTokenIds.empty())
    {
        if (logWhenEmpty)
        {
            LOG_INFO(
                "bot_token_exchanger",
                "Queued auto exchange skipped for bot {} guid {} reason {}: no staged token items found.",
                player->GetName(),
                botGuid,
                reason ? reason : "unknown");
            LOG_INFO(
                "server",
                "BotTokenExchanger queued bot {} guid {} reason {}: no staged token items found.",
                player->GetName(),
                botGuid,
                reason ? reason : "unknown");
        }
        return;
    }

    uint64 const nowMs = GameTime::GetGameTimeMS().count();
    uint64 const dueMs = nowMs + _autoExchangeDelayMs;

    AutoExchangeQueueEntry& entry = _autoExchangeQueueByBotGuid[botGuid];
    if (entry.botGuid == 0)
        entry.botGuid = botGuid;
    entry.botName = player->GetName();
    entry.tokenItemIds.insert(stagedTokenIds.begin(), stagedTokenIds.end());
    entry.scheduledTimeMs = std::max(entry.scheduledTimeMs, dueMs);

    LOG_INFO(
        "bot_token_exchanger",
        "Queued auto exchange for bot {} guid {} reason {} due {} ms tokens {}",
        entry.botName,
        entry.botGuid,
        reason ? reason : "unknown",
        entry.scheduledTimeMs,
        entry.tokenItemIds.size());
    LOG_INFO(
        "server",
        "BotTokenExchanger queued bot {} guid {} reason {} tokens {} due {} ms",
        entry.botName,
        entry.botGuid,
        reason ? reason : "unknown",
        entry.tokenItemIds.size(),
        entry.scheduledTimeMs);
}

void BotTokenExchangerMgr::QueueWotlkAutoExchange(Player* player, uint32 tokenItemId, bool queueAllStagedTokens, bool logWhenEmpty, char const* reason)
{
    if (!_wotlkAutoExchangeEnable || !player || !player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (_exchangeActive)
        return;

    LoadWotlkResolverMappings();

    uint64 const botGuid = player->GetGUID().GetCounter();

    std::unordered_set<uint32> stagedTokenIds;
    if (queueAllStagedTokens)
    {
        std::unordered_map<uint32, uint32> tokenCounts;
        CollectTokenCounts(player, tokenCounts);
        for (auto const& [tokenId, count] : tokenCounts)
        {
            (void)count;
            std::vector<ResolverEntry> const* entries = GetWotlkResolverEntries(tokenId);
            if (entries && !entries->empty())
                stagedTokenIds.insert(tokenId);
        }
    }
    else if (tokenItemId != 0)
    {
        std::vector<ResolverEntry> const* entries = GetWotlkResolverEntries(tokenItemId);
        if (entries && !entries->empty())
            stagedTokenIds.insert(tokenItemId);
    }

    if (stagedTokenIds.empty())
    {
        if (logWhenEmpty)
            LOG_INFO("bot_token_exchanger", "WOTLK auto queue skipped for bot {} guid {} reason {}: no staged single-token-chain token items found.", player->GetName(), botGuid, reason ? reason : "unknown");
        return;
    }

    uint64 const nowMs = GameTime::GetGameTimeMS().count();
    uint64 const dueMs = nowMs + _autoExchangeDelayMs;

    AutoExchangeQueueEntry& entry = _wotlkAutoExchangeQueueByBotGuid[botGuid];
    if (entry.botGuid == 0)
        entry.botGuid = botGuid;
    entry.botName = player->GetName();
    entry.tokenItemIds.insert(stagedTokenIds.begin(), stagedTokenIds.end());
    entry.scheduledTimeMs = std::max(entry.scheduledTimeMs, dueMs);

    LOG_INFO("bot_token_exchanger", "WOTLK auto queued bot {} guid {} reason {} tokens {} due {} ms", entry.botName, entry.botGuid, reason ? reason : "unknown", entry.tokenItemIds.size(), entry.scheduledTimeMs);
}

void BotTokenExchangerMgr::ProcessAutoExchangeQueue(Player* player)
{
    if (!_autoExchangeEnable || !player || !player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (!player->IsInWorld())
        return;

    uint64 const botGuid = player->GetGUID().GetCounter();
    auto itr = _autoExchangeQueueByBotGuid.find(botGuid);
    if (itr == _autoExchangeQueueByBotGuid.end())
        return;

    uint64 const nowMs = GameTime::GetGameTimeMS().count();
    if (nowMs < itr->second.scheduledTimeMs)
        return;

    AutoExchangeQueueEntry entry = std::move(itr->second);
    _autoExchangeQueueByBotGuid.erase(itr);

    if (entry.tokenItemIds.empty())
    {
        LOG_INFO("bot_token_exchanger", "Auto exchange queue for bot {} guid {} was empty at processing time.", player->GetName(), botGuid);
        LOG_INFO("server", "BotTokenExchanger auto queue for bot {} guid {} was empty at processing time.", player->GetName(), botGuid);
        return;
    }

    LOG_INFO(
        "bot_token_exchanger",
        "Processing auto exchange queue for bot {} guid {} with {} staged token ids.",
        player->GetName(),
        botGuid,
        entry.tokenItemIds.size());
    LOG_INFO(
        "server",
        "BotTokenExchanger processing queued bot {} guid {} with {} staged token ids.",
        player->GetName(),
        botGuid,
        entry.tokenItemIds.size());

    ExchangePlayerTokens(player, "auto", _autoExchangeMaxPerBotPerPass, nullptr);
}

void BotTokenExchangerMgr::ProcessWotlkAutoExchangeQueue(Player* player)
{
    if (!_wotlkAutoExchangeEnable || !player || !player->GetSession())
        return;

    if (_onlyPlayerbots && !player->GetSession()->IsBot())
        return;

    if (!player->IsInWorld())
        return;

    uint64 const botGuid = player->GetGUID().GetCounter();
    auto itr = _wotlkAutoExchangeQueueByBotGuid.find(botGuid);
    if (itr == _wotlkAutoExchangeQueueByBotGuid.end())
        return;

    uint64 const nowMs = GameTime::GetGameTimeMS().count();
    if (nowMs < itr->second.scheduledTimeMs)
        return;

    AutoExchangeQueueEntry entry = std::move(itr->second);
    _wotlkAutoExchangeQueueByBotGuid.erase(itr);

    if (entry.tokenItemIds.empty())
    {
        LOG_INFO("bot_token_exchanger", "WOTLK auto queue for bot {} guid {} was empty at processing time.", player->GetName(), botGuid);
        return;
    }

    LOG_INFO("bot_token_exchanger", "WOTLK auto processing bot {} guid {} with {} staged single-token-chain token ids.", player->GetName(), botGuid, entry.tokenItemIds.size());
    ExchangeWotlkPlayerTokens(player, "auto-wotlk", nullptr, 1);
}

bool BotTokenExchangerMgr::IsTierTokenCandidate(ItemTemplate const* item)
{
    if (!item)
        return false;

    if (item->InventoryType != INVTYPE_NON_EQUIP)
        return false;

    if (item->Class != ITEM_CLASS_QUEST && item->Class != ITEM_CLASS_MISC)
        return false;

    for (std::string_view fragment : kTokenNameFragments)
    {
        if (ContainsCaseInsensitive(item->Name1, fragment))
            return true;
    }

    return false;
}

bool BotTokenExchangerMgr::IsLikelyWotlkTierTokenCandidate(ItemTemplate const* item)
{
    if (!IsTierTokenCandidate(item))
        return false;

    std::string const& name = item->Name1;
    return ContainsCaseInsensitive(name, "Conqueror") ||
        ContainsCaseInsensitive(name, "Protector") ||
        ContainsCaseInsensitive(name, "Vanquisher") ||
        ContainsCaseInsensitive(name, "Triumph") ||
        ContainsCaseInsensitive(name, "Sanctification");
}

namespace
{
    std::string ClassifyWotlkRequiredItemRole(ItemTemplate const* item)
    {
        if (!item)
            return "unknown";

        std::string const lowered = ToLowerCopy(item->Name1);

        if (item->InventoryType == INVTYPE_NON_EQUIP && item->Class == ITEM_CLASS_MISC &&
            (ContainsCaseInsensitive(lowered, "mark of sanctification") ||
             ContainsCaseInsensitive(lowered, "of triumph") ||
             ContainsCaseInsensitive(lowered, "conqueror") ||
             ContainsCaseInsensitive(lowered, "protector") ||
             ContainsCaseInsensitive(lowered, "vanquisher") ||
             ContainsCaseInsensitive(lowered, "lost ") ||
             ContainsCaseInsensitive(lowered, "wayward ")))
        {
            return "token";
        }

        if (item->InventoryType == INVTYPE_NON_EQUIP &&
            ContainsCaseInsensitive(lowered, "emblem of "))
        {
            return "emblem_like";
        }

        if (item->Class == ITEM_CLASS_ARMOR && item->InventoryType != INVTYPE_NON_EQUIP)
            return "prior_armor";

        return "unknown";
    }
}

bool BotTokenExchangerMgr::IsValidRewardInventoryType(uint32 inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_ROBE:
            return true;
        default:
            return false;
    }
}

bool BotTokenExchangerMgr::IsFactionPreferred(ItemTemplate const* item, uint32 teamId)
{
    if (!item)
        return false;

    if (item->HasFlag2(ITEM_FLAG2_FACTION_HORDE))
        return teamId == TEAM_HORDE;

    if (item->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE))
        return teamId == TEAM_ALLIANCE;

    return true;
}

bool BotTokenExchangerMgr::IsFactionRewardMatch(ItemTemplate const* item, uint32 teamId)
{
    if (!item)
        return false;

    if (item->HasFlag2(ITEM_FLAG2_FACTION_HORDE))
        return teamId == TEAM_HORDE;

    if (item->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE))
        return teamId == TEAM_ALLIANCE;

    return false;
}

std::string BotTokenExchangerMgr::FormatResolverCandidate(ResolverEntry const& entry)
{
    return Acore::StringFormat(
        "reward {} ({}) token {} ({}) vendor {} extcost {} invtype {} class {} status {} confidence {}",
        entry.rewardItemId,
        entry.rewardName,
        entry.tokenItemId,
        entry.tokenName,
        entry.vendorEntry,
        entry.extendedCostId,
        entry.inventoryType,
        entry.allowableClass,
        entry.status,
        entry.confidence);
}

std::string BotTokenExchangerMgr::EscapeSql(std::string value)
{
    WorldDatabase.EscapeString(value);
    return value;
}

std::string BotTokenExchangerMgr::SqlQuote(std::string value)
{
    return Acore::StringFormat("'{}'", EscapeSql(std::move(value)));
}

std::string BotTokenExchangerMgr::SqlNullable(std::string const& value)
{
    if (value.empty())
        return "NULL";

    return SqlQuote(value);
}

bool BotTokenExchangerMgr::IsHybridClass(uint32 classId)
{
    return classId == CLASS_PALADIN || classId == CLASS_DRUID || classId == CLASS_SHAMAN;
}

bool BotTokenExchangerMgr::UsesRoleFiltering(uint32 classId)
{
    return classId == CLASS_WARRIOR || classId == CLASS_PRIEST || IsHybridClass(classId);
}

std::string BotTokenExchangerMgr::NormalizeRole(std::string role)
{
    role = ToLowerCopy(std::move(role));
    for (char& ch : role)
    {
        if (ch == '-' || ch == ' ')
            ch = '_';
    }

    if (role == "dps" || role == "damage")
        return "melee_dps";

    return role;
}

bool BotTokenExchangerMgr::IsRoleAllowedForClass(uint32 classId, std::string const& role)
{
    if (role.empty())
        return false;

    switch (classId)
    {
        case CLASS_PALADIN:
            return role == "tank" || role == "healer" || role == "melee_dps";
        case CLASS_PRIEST:
            return role == "healer" || role == "caster_dps";
        case CLASS_DRUID:
            return role == "tank" || role == "healer" || role == "melee_dps" || role == "caster_dps";
        case CLASS_SHAMAN:
            return role == "healer" || role == "melee_dps" || role == "caster_dps";
        default:
            return true;
    }
}

std::string BotTokenExchangerMgr::RoleFromBotRoles(uint32 classId, uint32 roles)
{
    if (roles & BOT_ROLE_TANK)
        return "tank";

    if (roles & BOT_ROLE_HEALER)
        return "healer";

    if (!(roles & BOT_ROLE_DPS))
        return {};

    switch (classId)
    {
        case CLASS_PALADIN:
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_DEATH_KNIGHT:
            return "melee_dps";
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return "caster_dps";
        case CLASS_HUNTER:
            return "ranged_dps";
        default:
            return "melee_dps";
    }
}

std::string BotTokenExchangerMgr::ClassifyRewardRole(ItemTemplate const* item)
{
    if (!item || item->Class != ITEM_CLASS_ARMOR || !IsValidRewardInventoryType(item->InventoryType))
        return {};

    std::string name = ToLowerCopy(item->Name1);
    uint32 tankScore = 0;
    uint32 healerScore = 0;
    uint32 meleeScore = 0;
    uint32 casterScore = 0;
    uint32 rangedScore = 0;

    auto addScore = [&](uint32 statType, int32 value)
    {
        if (value <= 0)
            return;

        switch (statType)
        {
            case ITEM_MOD_DEFENSE_SKILL_RATING:
            case ITEM_MOD_DODGE_RATING:
            case ITEM_MOD_PARRY_RATING:
            case ITEM_MOD_BLOCK_RATING:
            case ITEM_MOD_BLOCK_VALUE:
                tankScore += uint32(value) * 3;
                break;
            case ITEM_MOD_STAMINA:
                tankScore += uint32(value);
                healerScore += uint32(value / 2);
                break;
            case ITEM_MOD_INTELLECT:
                healerScore += uint32(value) * 2;
                casterScore += uint32(value);
                break;
            case ITEM_MOD_SPIRIT:
                healerScore += uint32(value) * 2;
                casterScore += uint32(value);
                break;
            case ITEM_MOD_MANA_REGENERATION:
                healerScore += uint32(value) * 4;
                casterScore += uint32(value) * 2;
                break;
            case ITEM_MOD_SPELL_HEALING_DONE:
                healerScore += uint32(value) * 4;
                break;
            case ITEM_MOD_SPELL_DAMAGE_DONE:
            case ITEM_MOD_SPELL_POWER:
                healerScore += uint32(value) * 2;
                casterScore += uint32(value) * 3;
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_HASTE_SPELL_RATING:
            case ITEM_MOD_SPELL_PENETRATION:
                casterScore += uint32(value) * 2;
                break;
            case ITEM_MOD_STRENGTH:
                tankScore += uint32(value);
                meleeScore += uint32(value) * 2;
                break;
            case ITEM_MOD_AGILITY:
                meleeScore += uint32(value) * 2;
                rangedScore += uint32(value) * 2;
                break;
            case ITEM_MOD_ATTACK_POWER:
                meleeScore += uint32(value) * 3;
                rangedScore += uint32(value) * 2;
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                rangedScore += uint32(value) * 3;
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
            case ITEM_MOD_HIT_MELEE_RATING:
            case ITEM_MOD_HASTE_MELEE_RATING:
            case ITEM_MOD_EXPERTISE_RATING:
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                meleeScore += uint32(value) * 2;
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
            case ITEM_MOD_HIT_RANGED_RATING:
            case ITEM_MOD_HASTE_RANGED_RATING:
                rangedScore += uint32(value) * 2;
                break;
            default:
                break;
        }
    };

    if (ContainsAny(name, { "protection", "prot", "tank", "defender", "shield" }))
        tankScore += 6;
    if (ContainsAny(name, { "holy", "healing", "restoration", "resto", "healer" }))
        healerScore += 6;
    if (ContainsAny(name, { "retribution", "ret", "enhancement", "feral", "melee", "dps" }))
        meleeScore += 6;
    if (ContainsAny(name, { "balance", "elemental", "spell", "caster", "arcane", "shadow", "fire", "frost" }))
        casterScore += 6;
    if (ContainsAny(name, { "ranged", "hunter", "marksman", "marksmanship", "survival" }))
        rangedScore += 6;

    for (uint32 i = 0; i < item->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        addScore(item->ItemStat[i].ItemStatType, item->ItemStat[i].ItemStatValue);

    auto selectRole = [&]() -> std::string
    {
        struct Entry { uint32 score; std::string_view role; };
        std::array<Entry, 5> scores =
        {{
            { tankScore, "tank" },
            { healerScore, "healer" },
            { meleeScore, "melee_dps" },
            { casterScore, "caster_dps" },
            { rangedScore, "ranged_dps" }
        }};

        std::sort(scores.begin(), scores.end(), [](Entry const& a, Entry const& b)
        {
            if (a.score != b.score)
                return a.score > b.score;
            return a.role < b.role;
        });

        if (scores.front().score == 0)
            return {};

        if (scores.size() > 1 && scores.front().score == scores[1].score)
            return {};

        return std::string(scores.front().role);
    };

    std::string role = selectRole();
    if (!role.empty())
        return role;

    if (ContainsAny(name, { "lightbringer", "justicar", "crystalforge" }))
    {
        // Fall back only when stat scoring is inconclusive.
        if (ContainsAny(name, { "holy", "healing" }))
            return "healer";
        if (ContainsAny(name, { "protection", "prot", "shield", "defender" }))
            return "tank";
    }

    if (ContainsAny(name, { "malorne", "nordrassil", "thunderheart" }))
    {
        if (ContainsAny(name, { "restoration", "resto", "healing", "holy" }))
            return "healer";
        if (ContainsAny(name, { "balance", "spell" }))
            return "caster_dps";
        if (ContainsAny(name, { "feral", "cat", "bear", "melee" }))
            return "melee_dps";
    }

    if (ContainsAny(name, { "cataclysm", "cyclone", "skyshatter" }))
    {
        if (ContainsAny(name, { "restoration", "resto", "healing" }))
            return "healer";
        if (ContainsAny(name, { "enhancement", "melee" }))
            return "melee_dps";
        if (ContainsAny(name, { "elemental", "balance", "spell" }))
            return "caster_dps";
    }

    return {};
}

std::string BotTokenExchangerMgr::ClassifyRewardRole(Player const* player, ItemTemplate const* item, RoleResolution const& roleResolution)
{
    std::string role = ClassifyRewardRole(item);
    if (!player || !item)
        return role;

    std::string name = ToLowerCopy(item->Name1);
    std::string preferredRole = roleResolution.manualConfigured ? roleResolution.manualRole : roleResolution.detectedRole;

    auto matchesAny = [&](std::initializer_list<char const*> needles)
    {
        return ContainsAny(name, needles);
    };

    auto classAwareOverride = [&](std::initializer_list<char const*> roleNeedles, std::string const& overrideRole) -> std::string
    {
        if (matchesAny(roleNeedles))
            return overrideRole;
        return {};
    };

    switch (player->getClass())
    {
        case CLASS_WARRIOR:
        {
            if (matchesAny({ "warbringer" }))
            {
                if (auto overrideRole = classAwareOverride({ "chestguard", "handguards", "greathelm", "legguards", "shoulderguards" }, "tank"); !overrideRole.empty())
                    return overrideRole;
                if (auto overrideRole = classAwareOverride({ "breastplate", "gauntlets", "battle-helm", "greaves", "shoulderplates" }, "melee_dps"); !overrideRole.empty())
                    return overrideRole;
            }
            break;
        }
        case CLASS_SHAMAN:
        {
            if (matchesAny({ "cyclone", "cataclysm" }))
            {
                if (auto overrideRole = classAwareOverride({ "hauberk", "grips", "helm", "war-kilt" }, "melee_dps"); !overrideRole.empty())
                    return overrideRole;
                if (auto overrideRole = classAwareOverride({ "chestguard", "gloves", "headdress", "headguard", "kilt", "leggings", "shoulderguards" }, "healer"); !overrideRole.empty())
                    return overrideRole;
                if (auto overrideRole = classAwareOverride({ "chestpiece", "handguards", "faceguard", "headpiece", "legguards", "leggings" }, "caster_dps"); !overrideRole.empty())
                    return overrideRole;

                if (matchesAny({ "shoulderpads" }))
                    return "caster_dps";

                if (matchesAny({ "shoulderplates" }))
                    return "melee_dps";
            }
            break;
        }
        case CLASS_DRUID:
        {
            if (matchesAny({ "malorne", "nordrassil" }))
            {
                if (matchesAny({ "mantle of malorne", "wrath-mantle" }))
                    return "caster_dps";

                if (matchesAny({ "pauldrons of malorne", "light-mantle" }))
                    return "healer";

                if (matchesAny({ "shoulderguards of malorne", "feral-mantle" }))
                {
                    if (preferredRole == "tank")
                        return "tank";
                    return "melee_dps";
                }

                if (matchesAny({ "life-", "light-", "restoration", "resto", "healing" }))
                    return "healer";

                if (matchesAny({ "soul-", "wrath-", "balance", "spell" }))
                    return "caster_dps";

                if (matchesAny({ "feral-" }))
                {
                    if (preferredRole == "tank")
                        return "tank";
                    if (preferredRole == "melee_dps")
                        return "melee_dps";
                }

                if (preferredRole == "tank" && matchesAny({ "chestguard", "handguards", "greathelm", "legguards", "shoulderguards", "crown", "antlers", "stag-helm" }))
                    return "tank";

                if (preferredRole == "melee_dps" && matchesAny({ "breastplate", "gauntlets", "faceguard", "war-kilt", "shoulderplates", "greaves", "feral-kilt", "feral-mantle" }))
                    return "melee_dps";

                if (preferredRole == "healer" && matchesAny({ "chestpiece", "chestguard", "gloves", "headguard", "headdress", "hood", "kilt", "leggings", "shoulderpads", "light-mantle", "light-collar", "pauldrons", "shroud", "vestments" }))
                    return "healer";

                if (preferredRole == "caster_dps" && matchesAny({ "chestpiece", "handgrips", "headpiece", "leggings", "britches", "shoulderpads", "soul-mantle", "soul-collar", "wrath-kilt", "wrath-mantle", "pauldrons", "gloves", "shroud", "vestments" }))
                    return "caster_dps";
            }
            break;
        }
        case CLASS_PRIEST:
        {
            if (matchesAny({ "incarnate", "avatar", "absolution" }))
            {
                if (matchesAny({ "light-", "light-collar", "light-mantle", "raiment" }))
                    return "healer";

                if (matchesAny({ "soul-", "soul-collar", "soul-mantle", "regalia" }))
                    return "caster_dps";
            }
            break;
        }
        default:
            break;
    }

    return role;
}

std::string BotTokenExchangerMgr::ClassifyWotlkRewardRole(Player const* player, ItemTemplate const* item, RoleResolution const& roleResolution)
{
    if (!player || !item || item->Class != ITEM_CLASS_ARMOR || !IsValidRewardInventoryType(item->InventoryType))
        return {};

    std::string const role = roleResolution.manualConfigured ? roleResolution.manualRole : roleResolution.detectedRole;
    std::string const name = ToLowerCopy(item->Name1);

    bool const isWotlkTierFamily = ContainsAny(name, {
        "heroes'",
        "valorous",
        "conqueror's",
        "sanctified",
        "dreamwalker",
        "nightsong",
        "bonescythe",
        "frostfire",
        "scourgeborne",
        "dreadnaught",
        "siegebreaker",
        "redemption",
        "aegis",
        "earthshatter",
        "worldbreaker",
        "plagueheart",
        "terrorblade",
        "cryptstalker",
        "scourgestalker",
        "darkruned",
        "deathbringer",
        "raiments of faith",
        "robe of faith"
    });

    if (!isWotlkTierFamily)
        return {};

    auto has = [&](std::initializer_list<char const*> needles) { return ContainsAny(name, needles); };

    switch (player->getClass())
    {
        case CLASS_PALADIN:
        {
            if (has({ "faceguard", "chestpiece", "legguards", "handguards", "shoulderguards" }))
                return "tank";
            if (has({ "helmet", "breastplate", "legplates", "gauntlets", "shoulderplates" }))
                return "melee_dps";
            if (has({ "headpiece", "tunic", "greaves", "gloves", "spaulders", "handwraps", "cowl", "circlet", "mantle", "pants", "raiments" }))
                return "healer";
            break;
        }
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
        {
            if (has({ "faceguard", "chestguard", "legguards", "handguards", "shoulderguards" }))
                return "tank";
            if (has({ "helmet", "battleplate", "breastplate", "legplates", "gauntlets", "shoulderplates" }))
                return "melee_dps";
            break;
        }
        case CLASS_DRUID:
        {
            if (has({ "headguard", "chestguard", "legguards", "handguards", "shoulderguards" }))
                return (role == "tank") ? "tank" : "melee_dps";
            if (has({ "cover", "raiments", "vestments", "gloves", "mantle", "trousers" }))
                return "healer";
            if (has({ "headpiece", "robe", "leggings", "handgrips", "spaulders" }))
                return "caster_dps";
            break;
        }
        case CLASS_SHAMAN:
        {
            if (has({ "chestguard", "headpiece", "kilt", "gloves", "shoulderpads" }))
                return "healer";
            if (has({ "hauberk", "faceguard", "war-kilt", "grips", "spaulders", "helmet", "battleplate", "legplates", "gauntlets", "shoulderplates" }))
                return "melee_dps";
            if (has({ "tunic", "headguard", "leggings", "handguards", "mantle", "chestpiece" }))
                return "caster_dps";
            break;
        }
        case CLASS_PRIEST:
        {
            if (has({ "raiments", "cover", "cowl", "mantle", "handwraps" }))
                return "healer";
            if (has({ "robe", "circlet", "hood", "trousers", "gloves", "shoulderpads", "crown" }))
                return "caster_dps";
            break;
        }
        case CLASS_ROGUE:
            if (has({ "bonescythe", "terrorblade", "shadowblade", "helmet", "mask", "hood", "headpiece" }))
                return "melee_dps";
            break;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            if (has({ "frostfire", "plagueheart", "gul'dan", "kel'thuzad", "circlet", "robe", "leggings", "gloves", "spaulders" }))
                return "caster_dps";
            break;
        case CLASS_HUNTER:
            if (has({ "cryptstalker", "scourgestalker", "headpiece", "tunic", "leggings", "gloves", "spaulders" }))
                return "ranged_dps";
            break;
        default:
            break;
    }

    return {};
}

bool BotTokenExchangerMgr::RoleMatchesHint(std::string const& hint, std::string const& role)
{
    if (hint.empty() || role.empty())
        return false;

    if (hint == role)
        return true;

    if (role == "melee_dps" && hint == "dps")
        return true;

    if (role == "caster_dps" && hint == "ranged_dps")
        return true;

    return false;
}

void BotTokenExchangerMgr::EnsurePreferenceTable()
{
    if (_preferenceTableEnsured)
        return;

    WorldDatabase.DirectExecute(R"SQL(
        CREATE TABLE IF NOT EXISTS `bot_token_exchanger_bot_preference` (
            `bot_guid` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
            `bot_name` VARCHAR(64) NOT NULL,
            `class` TINYINT UNSIGNED NOT NULL,
            `preferred_role` VARCHAR(32) NOT NULL,
            `notes` TEXT NULL
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )SQL");

    _preferenceTableEnsured = true;
}

void BotTokenExchangerMgr::LoadPreferenceMappings()
{
    if (_preferenceLoaded)
        return;

    EnsurePreferenceTable();
    _preferenceByBotGuid.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT bot_guid, bot_name, class, preferred_role, notes "
        "FROM bot_token_exchanger_bot_preference");

    uint32 loadedCount = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            RoleResolution resolution;
            resolution.manualConfigured = true;
            resolution.manualRole = NormalizeRole(fields[3].Get<std::string>());
            uint64 botGuid = fields[0].Get<uint64>();
            _preferenceByBotGuid[botGuid] = std::move(resolution);
            ++loadedCount;
        } while (result->NextRow());
    }

    _preferenceLoaded = true;
    LOG_INFO("bot_token_exchanger", "Loaded {} bot role preference rows", loadedCount);
}

BotTokenExchangerMgr::RoleResolution BotTokenExchangerMgr::GetRoleResolution(Player* player)
{
    RoleResolution resolution;
    if (!player)
        return resolution;

    resolution.detectedSpec = AiFactory::GetPlayerSpecName(player);
    uint8 specTab = AiFactory::GetPlayerSpecTab(player);
    BotRoles roles = AiFactory::GetPlayerRoles(player);
    uint32 classId = player->getClass();

    if (classId == CLASS_PALADIN)
    {
        if (specTab == PALADIN_TAB_HOLY)
            resolution.detectedRole = "healer";
        else if (specTab == PALADIN_TAB_PROTECTION)
            resolution.detectedRole = "tank";
        else if (specTab == PALADIN_TAB_RETRIBUTION)
            resolution.detectedRole = "melee_dps";
        resolution.detectedReliable = true;
    }
    else if (classId == CLASS_DRUID)
    {
        if (specTab == DRUID_TAB_BALANCE)
        {
            resolution.detectedRole = "caster_dps";
            resolution.detectedReliable = true;
        }
        else if (specTab == DRUID_TAB_RESTORATION)
        {
            resolution.detectedRole = "healer";
            resolution.detectedReliable = true;
        }
        else if (specTab == DRUID_TAB_FERAL)
        {
            if (PlayerbotAI::IsTank(player, true))
                resolution.detectedRole = "tank";
            else
                resolution.detectedRole = "melee_dps";
            resolution.detectedReliable = true;
        }
    }
    else if (classId == CLASS_SHAMAN)
    {
        if (specTab == SHAMAN_TAB_RESTORATION)
            resolution.detectedRole = "healer";
        else if (specTab == SHAMAN_TAB_ENHANCEMENT)
            resolution.detectedRole = "melee_dps";
        else
            resolution.detectedRole = "caster_dps";
        resolution.detectedReliable = true;
    }
    else
    {
        resolution.detectedRole = RoleFromBotRoles(classId, roles);
        resolution.detectedReliable = !resolution.detectedRole.empty();
    }

    LoadPreferenceMappings();
    auto itr = _preferenceByBotGuid.find(player->GetGUID().GetCounter());
    if (itr != _preferenceByBotGuid.end())
    {
        resolution.manualConfigured = true;
        resolution.manualRole = itr->second.manualRole;
    }

    return resolution;
}

BotTokenExchangerMgr::RoleResolution BotTokenExchangerMgr::GetLootPassRoleResolution(Player* player) const
{
    RoleResolution resolution;
    if (!player)
        return resolution;

    resolution.detectedSpec = AiFactory::GetPlayerSpecName(player);
    uint8 specTab = AiFactory::GetPlayerSpecTab(player);
    BotRoles roles = AiFactory::GetPlayerRoles(player);
    uint32 classId = player->getClass();

    if (classId == CLASS_PALADIN)
    {
        if (specTab == PALADIN_TAB_HOLY)
            resolution.detectedRole = "healer";
        else if (specTab == PALADIN_TAB_PROTECTION)
            resolution.detectedRole = "tank";
        else if (specTab == PALADIN_TAB_RETRIBUTION)
            resolution.detectedRole = "melee_dps";
        resolution.detectedReliable = true;
    }
    else if (classId == CLASS_DRUID)
    {
        if (specTab == DRUID_TAB_BALANCE)
        {
            resolution.detectedRole = "caster_dps";
            resolution.detectedReliable = true;
        }
        else if (specTab == DRUID_TAB_RESTORATION)
        {
            resolution.detectedRole = "healer";
            resolution.detectedReliable = true;
        }
        else if (specTab == DRUID_TAB_FERAL)
        {
            if (PlayerbotAI::IsTank(player, true))
                resolution.detectedRole = "tank";
            else
                resolution.detectedRole = "melee_dps";
            resolution.detectedReliable = true;
        }
    }
    else if (classId == CLASS_SHAMAN)
    {
        if (specTab == SHAMAN_TAB_RESTORATION)
            resolution.detectedRole = "healer";
        else if (specTab == SHAMAN_TAB_ENHANCEMENT)
            resolution.detectedRole = "melee_dps";
        else
            resolution.detectedRole = "caster_dps";
        resolution.detectedReliable = true;
    }
    else
    {
        resolution.detectedRole = RoleFromBotRoles(classId, roles);
        resolution.detectedReliable = !resolution.detectedRole.empty();
    }

    return resolution;
}

void BotTokenExchangerMgr::DescribeRoleResolution(ChatHandler* handler, Player* player)
{
    if (!handler || !player)
        return;

    RoleResolution resolution = GetRoleResolution(player);
    char const* faction = GetTeamLabel(GetTeamIdForFiltering(player));

    handler->PSendSysMessage(
        "Bot {} faction {} class {} spec {} detected role {} reliable {} manual preference {} can-resolve {}",
        player->GetName(),
        faction,
        player->getClass(),
        resolution.detectedSpec.empty() ? "unknown" : resolution.detectedSpec,
        resolution.detectedRole.empty() ? "unknown" : resolution.detectedRole,
        resolution.detectedReliable ? "yes" : "no",
        resolution.manualConfigured ? resolution.manualRole : "none",
        (resolution.detectedReliable || resolution.manualConfigured) ? "yes" : "no");

    LOG_INFO(
        "bot_token_exchanger",
        "Role check bot {} faction {} class {} spec {} detected {} reliable {} manual {}",
        player->GetName(),
        faction,
        player->getClass(),
        resolution.detectedSpec.empty() ? "unknown" : resolution.detectedSpec,
        resolution.detectedRole.empty() ? "unknown" : resolution.detectedRole,
        resolution.detectedReliable ? "yes" : "no",
        resolution.manualConfigured ? resolution.manualRole : "none");
}

bool BotTokenExchangerMgr::BuildHybridCandidates(Player* player, std::vector<FilteredCandidate> const& input, std::vector<FilteredCandidate>& output, std::vector<std::string>& notes, std::vector<std::string>& skipReasons, RoleResolution const& roleResolution) const
{
    output.clear();
    notes.clear();
    skipReasons.clear();

    if (!player)
        return false;

    if (input.empty())
        return true;

    if (!UsesRoleFiltering(player->getClass()))
    {
        output = input;
        return true;
    }

    std::string preferredRole = roleResolution.manualConfigured ? roleResolution.manualRole : roleResolution.detectedRole;
    bool const hasPreference = roleResolution.manualConfigured || roleResolution.detectedReliable;

    if (!hasPreference || preferredRole.empty())
    {
        skipReasons.emplace_back("role-sensitive class needs manual role preference or reliable spec detection");
        return false;
    }

    if (!IsRoleAllowedForClass(player->getClass(), preferredRole))
    {
        skipReasons.emplace_back(Acore::StringFormat("preferred role {} is not valid for class {}", preferredRole, player->getClass()));
        return false;
    }

    for (FilteredCandidate const& candidate : input)
    {
        std::string candidateRole = ClassifyRewardRole(player, candidate.rewardTemplate, roleResolution);
        if (candidateRole.empty())
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) could not be classified safely for role filtering",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1));
            continue;
        }

        if (RoleMatchesHint(candidateRole, preferredRole))
        {
            output.push_back(candidate);
            notes.emplace_back(Acore::StringFormat(
                "kept reward {} ({}) as {} for preferred role {}",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1,
                candidateRole,
                preferredRole));
        }
        else
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) classified as {} does not match preferred role {}",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1,
                candidateRole,
                preferredRole));
        }
    }

    if (output.size() == 1)
        return true;

    if (output.empty())
        return false;

    notes.emplace_back(Acore::StringFormat(
        "hybrid role filter kept {} candidates for preferred role {}",
        output.size(),
        preferredRole));
    return true;
}

bool BotTokenExchangerMgr::BuildWotlkHybridCandidates(Player* player, std::vector<FilteredCandidate> const& input, std::vector<FilteredCandidate>& output, std::vector<std::string>& notes, std::vector<std::string>& skipReasons, RoleResolution const& roleResolution) const
{
    output.clear();
    notes.clear();
    skipReasons.clear();

    if (!player)
        return false;

    if (input.empty())
        return true;

    if (!UsesRoleFiltering(player->getClass()))
    {
        output = input;
        return true;
    }

    std::string preferredRole = roleResolution.manualConfigured ? roleResolution.manualRole : roleResolution.detectedRole;
    bool const hasPreference = roleResolution.manualConfigured || roleResolution.detectedReliable;
    if (!hasPreference || preferredRole.empty())
    {
        skipReasons.emplace_back("role-sensitive class needs manual role preference or reliable spec detection");
        return false;
    }

    if (!IsRoleAllowedForClass(player->getClass(), preferredRole))
    {
        skipReasons.emplace_back(Acore::StringFormat("preferred role {} is not valid for class {}", preferredRole, player->getClass()));
        return false;
    }

    for (FilteredCandidate const& candidate : input)
    {
        std::string candidateRole = ClassifyWotlkRewardRole(player, candidate.rewardTemplate, roleResolution);
        if (candidateRole.empty())
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "WOTLK reward {} ({}) could not be classified safely for role filtering",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1));
            continue;
        }

        if (RoleMatchesHint(candidateRole, preferredRole))
        {
            output.push_back(candidate);
            notes.emplace_back(Acore::StringFormat(
                "kept WOTLK reward {} ({}) as {} for preferred role {}",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1,
                candidateRole,
                preferredRole));
        }
        else
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "WOTLK reward {} ({}) classified as {} does not match preferred role {}",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1,
                candidateRole,
                preferredRole));
        }
    }

    if (output.size() == 1)
        return true;
    if (output.empty())
        return false;

    notes.emplace_back(Acore::StringFormat(
        "WOTLK hybrid role filter kept {} candidates for preferred role {}",
        output.size(),
        preferredRole));
    return true;
}

bool BotTokenExchangerMgr::BuildFilteredCandidatesFromEntriesForWotlkReadOnly(Player* player, uint32 tokenItemId, std::vector<ResolverEntry> const& entries, std::vector<FilteredCandidate>& filtered, std::vector<std::string>& skipReasons, std::vector<std::string>& notes) const
{
    bool ok = BuildFilteredCandidatesFromEntries(player, tokenItemId, entries, filtered, skipReasons, notes);
    if (!ok || !filtered.empty())
        return ok;

    // For read-only WOTLK resolver diagnostics, allow fallback when CanUseItem blocks otherwise class-safe tier pieces
    // (typically level-gated bots on PTR). This does not affect exchange paths.
    filtered.clear();
    std::vector<std::string> localSkips;
    ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
    if (!tokenTemplate || !player)
        return ok;

    uint32 const classMask = player->getClassMask();
    TeamId const teamId = GetTeamIdForFiltering(player);

    for (ResolverEntry const& entry : entries)
    {
        ItemTemplate const* rewardTemplate = sObjectMgr->GetItemTemplate(entry.rewardItemId);
        if (!rewardTemplate)
            continue;
        if (entry.allowableClass && (entry.allowableClass & static_cast<int32>(classMask)) == 0)
            continue;
        if (rewardTemplate->Class != ITEM_CLASS_ARMOR || !IsValidRewardInventoryType(rewardTemplate->InventoryType))
            continue;
        if (!rewardTemplate->AllowableClass || (rewardTemplate->AllowableClass & classMask) == 0)
            continue;
        if (!IsFactionPreferred(rewardTemplate, teamId) && (rewardTemplate->HasFlag2(ITEM_FLAG2_FACTION_HORDE) || rewardTemplate->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE)))
            continue;

        filtered.push_back({ &entry, rewardTemplate });
    }

    if (!filtered.empty())
        notes.emplace_back("WOTLK read-only fallback: kept class/faction-safe candidates despite CanUseItem() gating");

    return true;
}

void BotTokenExchangerMgr::LogHybridResolutionDecision(char const* phase, Player* player, uint32 tokenItemId, ItemTemplate const* tokenTemplate, std::vector<FilteredCandidate> const& input, std::vector<FilteredCandidate> const& output, RoleResolution const& roleResolution, std::vector<std::string> const& skipReasons, std::vector<std::string> const& notes) const
{
    if (!player || !tokenTemplate)
        return;

    std::string preferredRole = roleResolution.manualConfigured ? roleResolution.manualRole : roleResolution.detectedRole;
    LOG_INFO(
        "bot_token_exchanger",
        "{} hybrid decision bot {} class {} faction {} spec {} role {} preferred {} token {} ({}) input {} output {}",
        phase ? phase : "hybrid",
        player->GetName(),
        player->getClass(),
        GetTeamLabel(GetTeamIdForFiltering(player)),
        roleResolution.detectedSpec.empty() ? "unknown" : roleResolution.detectedSpec,
        roleResolution.detectedRole.empty() ? "unknown" : roleResolution.detectedRole,
        preferredRole.empty() ? "none" : preferredRole,
        tokenItemId,
        tokenTemplate->Name1,
        input.size(),
        output.size());

    for (std::string const& note : notes)
        LOG_INFO("bot_token_exchanger", "{} hybrid note token {} ({}): {}", phase ? phase : "hybrid", tokenItemId, tokenTemplate->Name1, note);

    for (std::string const& reason : skipReasons)
        LOG_INFO("bot_token_exchanger", "{} hybrid skip token {} ({}): {}", phase ? phase : "hybrid", tokenItemId, tokenTemplate->Name1, reason);

    for (FilteredCandidate const& candidate : input)
    {
        std::string candidateRole = ClassifyRewardRole(player, candidate.rewardTemplate, roleResolution);
        bool const keep = !candidateRole.empty() && RoleMatchesHint(candidateRole, preferredRole);
        LOG_INFO(
            "bot_token_exchanger",
            "{} hybrid candidate bot {} class {} faction {} spec {} role {} token {} ({}) reward {} ({}) candidate_role {} decision {}",
            phase ? phase : "hybrid",
            player->GetName(),
            player->getClass(),
            GetTeamLabel(GetTeamIdForFiltering(player)),
            roleResolution.detectedSpec.empty() ? "unknown" : roleResolution.detectedSpec,
            roleResolution.detectedRole.empty() ? "unknown" : roleResolution.detectedRole,
            tokenItemId,
            tokenTemplate->Name1,
            candidate.entry->rewardItemId,
            candidate.rewardTemplate ? candidate.rewardTemplate->Name1 : "unknown",
            candidateRole.empty() ? "unknown" : candidateRole,
            keep ? "keep" : "reject");
    }
}

void BotTokenExchangerMgr::ResolveTokenRewardsForPlayer(ChatHandler* handler, Player* player, char const* label)
{
    if (!handler || !player)
        return;

    std::unordered_map<uint32, uint32> tokenCounts;
    CollectTokenCounts(player, tokenCounts);

    std::unordered_map<uint32, uint32> stagedTokenCounts;
    for (auto const& [tokenItemId, count] : tokenCounts)
    {
        std::vector<ResolverEntry> const* entries = GetResolverEntries(tokenItemId);
        if (entries && !entries->empty())
            stagedTokenCounts[tokenItemId] = count;
    }

    if (stagedTokenCounts.empty())
    {
        handler->PSendSysMessage("{} bot {} has no staged token items in bags.", label, player->GetName());
        return;
    }

    handler->PSendSysMessage("{} bot {} token scan found {} staged token item IDs.", label, player->GetName(), stagedTokenCounts.size());

    for (auto const& [tokenItemId, count] : stagedTokenCounts)
    {
        handler->PSendSysMessage("Scanning token {} x{}.", tokenItemId, count);
        ResolveTokenItem(handler, player, tokenItemId);
    }
}

void BotTokenExchangerMgr::ShowSelectedRole(ChatHandler* handler)
{
    if (!handler)
        return;

    Player* player = handler->getSelectedPlayer();
    if (!player)
    {
        handler->SendSysMessage("Select a Playerbot first.");
        return;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    DescribeRoleResolution(handler, player);
}

void BotTokenExchangerMgr::SetSelectedRolePreference(ChatHandler* handler, std::string const& role)
{
    if (!handler)
        return;

    Player* player = handler->getSelectedPlayer();
    if (!player)
    {
        handler->SendSysMessage("Select a Playerbot first.");
        return;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    std::string normalized = NormalizeRole(role);
    if (!IsRoleAllowedForClass(player->getClass(), normalized))
    {
        handler->PSendSysMessage(
            "Role {} is not valid for class {}. Allowed roles are class-dependent.",
            normalized,
            player->getClass());
        return;
    }

    EnsurePreferenceTable();
    uint64 botGuid = player->GetGUID().GetCounter();
    WorldDatabase.DirectExecute(
        "INSERT INTO `bot_token_exchanger_bot_preference` "
        "(`bot_guid`, `bot_name`, `class`, `preferred_role`, `notes`) "
        "VALUES ({}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "`bot_name` = VALUES(`bot_name`), "
        "`class` = VALUES(`class`), "
        "`preferred_role` = VALUES(`preferred_role`), "
        "`notes` = VALUES(`notes`)",
        botGuid,
        SqlQuote(player->GetName()),
        uint32(player->getClass()),
        SqlQuote(normalized),
        SqlNullable("manual preference set via .tokenex prefer selected"));

    LoadPreferenceMappings();
    RoleResolution& stored = _preferenceByBotGuid[botGuid];
    stored.manualConfigured = true;
    stored.manualRole = normalized;

    handler->PSendSysMessage(
        "Saved manual role preference {} for bot {} class {}.",
        normalized,
        player->GetName(),
        player->getClass());

    LOG_INFO(
        "bot_token_exchanger",
        "Saved manual role preference {} for bot {} class {} guid {}",
        normalized,
        player->GetName(),
        player->getClass(),
        botGuid);
}

Player* BotTokenExchangerMgr::ResolveOnlineBotByName(ChatHandler* handler, std::string const& botName, char const* action)
{
    if (!handler)
        return nullptr;

    if (!_allowDebugTargetCommand)
    {
        handler->SendSysMessage("Debug bot targeting is disabled.");
        return nullptr;
    }

    if (botName.empty())
    {
        handler->SendSysMessage("Usage: .tokenex {} bot <botName>", action ? action : "exchange");
        return nullptr;
    }

    Player* player = ObjectAccessor::FindPlayerByName(botName, false);
    if (!player)
    {
        handler->PSendSysMessage("No online Playerbot named {} was found.", botName);
        return nullptr;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->PSendSysMessage("Selected target {} is not a Playerbot.", botName);
        return nullptr;
    }

    LOG_INFO(
        "bot_token_exchanger",
        "Debug bot target command {} used for bot {} guid {}",
        action ? action : "unknown",
        player->GetName(),
        player->GetGUID().GetRawValue());

    return player;
}

bool BotTokenExchangerMgr::BuildFilteredCandidatesFromEntries(Player* player, uint32 tokenItemId, std::vector<ResolverEntry> const& entries, std::vector<FilteredCandidate>& filtered, std::vector<std::string>& skipReasons, std::vector<std::string>& notes) const
{
    filtered.clear();
    skipReasons.clear();
    notes.clear();

    if (!player)
        return false;

    ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
    if (!tokenTemplate)
    {
        skipReasons.emplace_back(Acore::StringFormat("token item {} missing from item_template", tokenItemId));
        return false;
    }

    uint32 const classMask = player->getClassMask();
    TeamId const teamId = GetTeamIdForFiltering(player);

    std::vector<FilteredCandidate> safeCandidates;
    safeCandidates.reserve(entries.size());

    for (ResolverEntry const& entry : entries)
    {
        ItemTemplate const* rewardTemplate = sObjectMgr->GetItemTemplate(entry.rewardItemId);
        if (!rewardTemplate)
        {
            skipReasons.emplace_back(Acore::StringFormat("reward {} missing from item_template", entry.rewardItemId));
            continue;
        }

        if (entry.allowableClass && (entry.allowableClass & static_cast<int32>(classMask)) == 0)
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) rejected by class mask {} vs allowable_class {}",
                entry.rewardItemId,
                rewardTemplate->Name1,
                classMask,
                entry.allowableClass));
            continue;
        }

        if (rewardTemplate->Class != ITEM_CLASS_ARMOR || !IsValidRewardInventoryType(rewardTemplate->InventoryType))
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) is not a safe armor piece (class {}, inventory type {})",
                entry.rewardItemId,
                rewardTemplate->Name1,
                rewardTemplate->Class,
                rewardTemplate->InventoryType));
            continue;
        }

        if (!rewardTemplate->AllowableClass)
        {
            skipReasons.emplace_back(Acore::StringFormat("reward {} ({}) has no allowable class restriction", entry.rewardItemId, rewardTemplate->Name1));
            continue;
        }

        if ((rewardTemplate->AllowableClass & classMask) == 0)
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) is not usable by class mask {}",
                entry.rewardItemId,
                rewardTemplate->Name1,
                classMask));
            continue;
        }

        if (player->CanUseItem(rewardTemplate) != EQUIP_ERR_OK)
        {
            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) failed CanUseItem() for selected bot",
                entry.rewardItemId,
                rewardTemplate->Name1));
            continue;
        }

        safeCandidates.push_back({ &entry, rewardTemplate });
    }

    if (safeCandidates.empty())
        return true;

    std::vector<FilteredCandidate> factionMatched;
    std::vector<FilteredCandidate> neutralCandidates;
    factionMatched.reserve(safeCandidates.size());
    neutralCandidates.reserve(safeCandidates.size());

    for (FilteredCandidate const& candidate : safeCandidates)
    {
        bool const hordeFaction = candidate.rewardTemplate->HasFlag2(ITEM_FLAG2_FACTION_HORDE);
        bool const allianceFaction = candidate.rewardTemplate->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE);

        if (hordeFaction || allianceFaction)
        {
            if (IsFactionRewardMatch(candidate.rewardTemplate, teamId))
            {
                factionMatched.push_back(candidate);
                continue;
            }

            skipReasons.emplace_back(Acore::StringFormat(
                "reward {} ({}) skipped by faction preference for {} bot",
                candidate.entry->rewardItemId,
                candidate.rewardTemplate->Name1,
                GetTeamLabel(teamId)));
            continue;
        }

        neutralCandidates.push_back(candidate);
    }

    if (!factionMatched.empty())
    {
        if (factionMatched.size() < safeCandidates.size())
        {
            notes.emplace_back(Acore::StringFormat(
                "faction preference kept {} of {} candidates for {} bot",
                factionMatched.size(),
                safeCandidates.size(),
                GetTeamLabel(teamId)));
        }

        filtered = std::move(factionMatched);
        return true;
    }

    filtered = std::move(neutralCandidates);
    if (!filtered.empty() && filtered.size() < safeCandidates.size())
    {
        notes.emplace_back(Acore::StringFormat(
            "no explicit faction variant matched {} bot; kept {} neutral candidates",
            GetTeamLabel(teamId),
            filtered.size()));
    }

    return true;
}

bool BotTokenExchangerMgr::BuildFilteredCandidates(Player* player, uint32 tokenItemId, std::vector<FilteredCandidate>& filtered, std::vector<std::string>& skipReasons, std::vector<std::string>& notes)
{
    std::vector<ResolverEntry> const* entries = GetResolverEntries(tokenItemId);
    if (!entries || entries->empty())
        return false;

    return BuildFilteredCandidatesFromEntries(player, tokenItemId, *entries, filtered, skipReasons, notes);
}

bool BotTokenExchangerMgr::TryExchangeToken(Player* player, ResolverEntry const& entry, ItemTemplate const* tokenTemplate, ItemTemplate const* rewardTemplate, bool dryRun, std::string& transactionLog)
{
    transactionLog.clear();

    if (!player || !tokenTemplate || !rewardTemplate)
    {
        transactionLog = "invalid exchange context";
        return false;
    }

    if (player->GetItemCount(tokenTemplate->ItemId, false) == 0)
    {
        transactionLog = Acore::StringFormat(
            "token {} ({}) is not present in bags",
            tokenTemplate->ItemId,
            tokenTemplate->Name1);
        return false;
    }

    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, rewardTemplate->ItemId, 1);
    if (msg != EQUIP_ERR_OK)
    {
        transactionLog = Acore::StringFormat(
            "no bag space for reward {} ({}) after token {} ({})",
            rewardTemplate->ItemId,
            rewardTemplate->Name1,
            tokenTemplate->ItemId,
            tokenTemplate->Name1);
        return false;
    }

    if (dryRun)
    {
        transactionLog = Acore::StringFormat(
            "dry-run would exchange token {} ({}) -> reward {} ({}) via vendor {} extcost {}",
            tokenTemplate->ItemId,
            tokenTemplate->Name1,
            rewardTemplate->ItemId,
            rewardTemplate->Name1,
            entry.vendorEntry,
            entry.extendedCostId);
        return true;
    }

    player->DestroyItemCount(tokenTemplate->ItemId, 1, true);

    Item* rewardItem = player->StoreNewItem(dest, rewardTemplate->ItemId, true);
    if (!rewardItem)
    {
        if (!player->AddItem(tokenTemplate->ItemId, 1))
        {
            transactionLog = Acore::StringFormat(
                "hard failure: could not add reward {} ({}) and token {} could not be restored",
                rewardTemplate->ItemId,
                rewardTemplate->Name1,
                tokenTemplate->ItemId);
        }
        else
        {
            transactionLog = Acore::StringFormat(
                "reward add failed for {} ({}); token {} restored",
                rewardTemplate->ItemId,
                rewardTemplate->Name1,
                tokenTemplate->ItemId);
        }

        return false;
    }

    player->SendNewItem(rewardItem, 1, true, false);
    transactionLog = Acore::StringFormat(
        "exchanged token {} ({}) -> reward {} ({}) via vendor {} extcost {}",
        tokenTemplate->ItemId,
        tokenTemplate->Name1,
        rewardTemplate->ItemId,
        rewardTemplate->Name1,
        entry.vendorEntry,
        entry.extendedCostId);
    return true;
}

void BotTokenExchangerMgr::EnsureDiscoveryTable()
{
    if (_discoveryTableEnsured)
        return;

    WorldDatabase.DirectExecute(R"SQL(
        CREATE TABLE IF NOT EXISTS `bot_token_exchanger_token_map` (
            `token_item_id` INT UNSIGNED NOT NULL,
            `reward_item_id` INT UNSIGNED NOT NULL,
            `token_name` VARCHAR(255) NOT NULL,
            `reward_name` VARCHAR(255) NOT NULL,
            `inventory_type` INT UNSIGNED NOT NULL,
            `allowable_class` INT NOT NULL,
            `extended_cost_id` INT UNSIGNED NOT NULL,
            `vendor_entry` INT UNSIGNED NOT NULL,
            `source_expansion` VARCHAR(16) NOT NULL DEFAULT 'TBC',
            `source_tier` VARCHAR(32) NULL,
            `source_raid` VARCHAR(64) NULL,
            `confidence` VARCHAR(32) NOT NULL DEFAULT 'runtime_dbc_verified',
            `status` VARCHAR(32) NOT NULL DEFAULT 'staged',
            `notes` TEXT NULL,
            PRIMARY KEY (`token_item_id`, `reward_item_id`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )SQL");

    _discoveryTableEnsured = true;
}

void BotTokenExchangerMgr::EnsureWotlkDiscoveryTables()
{
    if (_wotlkDiscoveryTablesEnsured)
        return;

    WorldDatabase.DirectExecute(R"SQL(
        CREATE TABLE IF NOT EXISTS `bot_token_exchanger_wotlk_map` (
            `reward_item_id` INT UNSIGNED NOT NULL,
            `reward_name` VARCHAR(255) NOT NULL,
            `inventory_type` INT UNSIGNED NOT NULL,
            `allowable_class` INT NOT NULL,
            `vendor_entry` INT UNSIGNED NOT NULL,
            `extended_cost_id` INT UNSIGNED NOT NULL,
            `source_expansion` VARCHAR(16) NOT NULL DEFAULT 'WOTLK',
            `source_tier` VARCHAR(32) NULL,
            `source_raid` VARCHAR(64) NULL,
            `confidence` VARCHAR(32) NOT NULL DEFAULT 'runtime_dbc_inspected',
            `status` VARCHAR(32) NOT NULL DEFAULT 'staged',
            `notes` TEXT NULL,
            PRIMARY KEY (`reward_item_id`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )SQL");

    WorldDatabase.DirectExecute(R"SQL(
        CREATE TABLE IF NOT EXISTS `bot_token_exchanger_wotlk_cost` (
            `reward_item_id` INT UNSIGNED NOT NULL,
            `required_item_id` INT UNSIGNED NOT NULL,
            `required_item_name` VARCHAR(255) NOT NULL,
            `required_count` INT UNSIGNED NOT NULL,
            `required_item_role` VARCHAR(32) NOT NULL DEFAULT 'unknown',
            PRIMARY KEY (`reward_item_id`, `required_item_id`),
            KEY `idx_required_item` (`required_item_id`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )SQL");

    _wotlkDiscoveryTablesEnsured = true;
}

bool BotTokenExchangerMgr::TryStageMapping(uint32 tokenItemId, uint32 rewardItemId, uint32 inventoryType, int32 allowableClass, uint32 extendedCostId, uint32 vendorEntry, std::string const& sourceExpansion, std::string const& sourceTier, std::string const& sourceRaid, std::string const& notes)
{
    if (!_discoveryWriteDb)
        return true;

    EnsureDiscoveryTable();

    ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
    ItemTemplate const* rewardTemplate = sObjectMgr->GetItemTemplate(rewardItemId);
    if (!tokenTemplate || !rewardTemplate)
        return false;

    std::string tokenName = tokenTemplate->Name1;
    std::string rewardName = rewardTemplate->Name1;

    WorldDatabase.DirectExecute(
        "INSERT INTO `bot_token_exchanger_token_map` "
        "(`token_item_id`, `reward_item_id`, `token_name`, `reward_name`, `inventory_type`, `allowable_class`, `extended_cost_id`, `vendor_entry`, `source_expansion`, `source_tier`, `source_raid`, `confidence`, `status`, `notes`) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "`token_name` = VALUES(`token_name`), "
        "`reward_name` = VALUES(`reward_name`), "
        "`inventory_type` = VALUES(`inventory_type`), "
        "`allowable_class` = VALUES(`allowable_class`), "
        "`extended_cost_id` = VALUES(`extended_cost_id`), "
        "`vendor_entry` = VALUES(`vendor_entry`), "
        "`source_expansion` = VALUES(`source_expansion`), "
        "`source_tier` = VALUES(`source_tier`), "
        "`source_raid` = VALUES(`source_raid`), "
        "`confidence` = VALUES(`confidence`), "
        "`status` = VALUES(`status`), "
        "`notes` = VALUES(`notes`)",
        tokenItemId,
        rewardItemId,
        SqlQuote(std::move(tokenName)),
        SqlQuote(std::move(rewardName)),
        inventoryType,
        allowableClass,
        extendedCostId,
        vendorEntry,
        SqlQuote(sourceExpansion),
        SqlNullable(sourceTier),
        SqlNullable(sourceRaid),
        SqlQuote("runtime_dbc_verified"),
        SqlQuote("staged"),
        SqlNullable(notes));

    return true;
}

void BotTokenExchangerMgr::LoadResolverMappings()
{
    if (_resolverLoaded)
        return;

    _resolverEntriesByToken.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT token_item_id, reward_item_id, token_name, reward_name, inventory_type, allowable_class, extended_cost_id, vendor_entry, source_tier, source_raid, confidence, status, notes "
        "FROM bot_token_exchanger_token_map "
        "WHERE source_expansion = 'TBC' AND status = 'staged' AND confidence = 'runtime_dbc_verified' "
        "ORDER BY token_item_id, vendor_entry, reward_item_id");

    uint32 loadedCount = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            ResolverEntry entry;
            entry.tokenItemId = fields[0].Get<uint32>();
            entry.rewardItemId = fields[1].Get<uint32>();
            entry.tokenName = fields[2].Get<std::string>();
            entry.rewardName = fields[3].Get<std::string>();
            entry.inventoryType = fields[4].Get<uint32>();
            entry.allowableClass = fields[5].Get<int32>();
            entry.extendedCostId = fields[6].Get<uint32>();
            entry.vendorEntry = fields[7].Get<uint32>();
            entry.sourceTier = fields[8].IsNull() ? std::string() : fields[8].Get<std::string>();
            entry.sourceRaid = fields[9].IsNull() ? std::string() : fields[9].Get<std::string>();
            entry.confidence = fields[10].Get<std::string>();
            entry.status = fields[11].Get<std::string>();
            entry.notes = fields[12].IsNull() ? std::string() : fields[12].Get<std::string>();

            _resolverEntriesByToken[entry.tokenItemId].push_back(std::move(entry));
            ++loadedCount;
        } while (result->NextRow());
    }

    _resolverLoaded = true;
    LOG_INFO("bot_token_exchanger", "Loaded {} staged token resolver mappings from bot_token_exchanger_token_map", loadedCount);
}

void BotTokenExchangerMgr::LoadWotlkResolverMappings()
{
    if (_wotlkResolverLoaded)
        return;

    _wotlkResolverEntriesByToken.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT c.required_item_id, m.reward_item_id, c.required_item_name, m.reward_name, m.inventory_type, m.allowable_class, m.extended_cost_id, m.vendor_entry, m.source_tier, m.source_raid, m.confidence, m.status, m.notes "
        "FROM bot_token_exchanger_wotlk_map m "
        "JOIN bot_token_exchanger_wotlk_cost c ON c.reward_item_id = m.reward_item_id "
        "WHERE m.source_expansion = 'WOTLK' "
        "AND m.status = 'staged' "
        "AND c.required_item_role = 'token' "
        "AND c.required_count = 1 "
        "AND m.reward_item_id IN ("
            "SELECT reward_item_id "
            "FROM bot_token_exchanger_wotlk_cost "
            "GROUP BY reward_item_id "
            "HAVING COUNT(*) = 1 "
               "AND SUM(required_item_role = 'token') = 1 "
               "AND SUM(required_count = 1) = 1"
        ") "
        "ORDER BY c.required_item_id, m.vendor_entry, m.reward_item_id");

    uint32 loadedCount = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            ResolverEntry entry;
            entry.tokenItemId = fields[0].Get<uint32>();
            entry.rewardItemId = fields[1].Get<uint32>();
            entry.tokenName = fields[2].Get<std::string>();
            entry.rewardName = fields[3].Get<std::string>();
            entry.inventoryType = fields[4].Get<uint32>();
            entry.allowableClass = fields[5].Get<int32>();
            entry.extendedCostId = fields[6].Get<uint32>();
            entry.vendorEntry = fields[7].Get<uint32>();
            entry.sourceTier = fields[8].IsNull() ? std::string() : fields[8].Get<std::string>();
            entry.sourceRaid = fields[9].IsNull() ? std::string() : fields[9].Get<std::string>();
            entry.confidence = fields[10].Get<std::string>();
            entry.status = fields[11].Get<std::string>();
            entry.notes = fields[12].IsNull() ? std::string() : fields[12].Get<std::string>();

            _wotlkResolverEntriesByToken[entry.tokenItemId].push_back(std::move(entry));
            ++loadedCount;
        } while (result->NextRow());
    }

    _wotlkResolverLoaded = true;
    LOG_INFO("bot_token_exchanger", "Loaded {} staged WOTLK single-token-chain resolver mappings from bot_token_exchanger_wotlk_map/wotlk_cost", loadedCount);
}

std::vector<BotTokenExchangerMgr::ResolverEntry> const* BotTokenExchangerMgr::GetResolverEntries(uint32 tokenItemId)
{
    LoadResolverMappings();

    auto itr = _resolverEntriesByToken.find(tokenItemId);
    if (itr == _resolverEntriesByToken.end())
        return nullptr;

    return &itr->second;
}

std::vector<BotTokenExchangerMgr::ResolverEntry> const* BotTokenExchangerMgr::GetResolverEntriesCached(uint32 tokenItemId) const
{
    if (!_resolverLoaded)
        return nullptr;

    auto itr = _resolverEntriesByToken.find(tokenItemId);
    if (itr == _resolverEntriesByToken.end())
        return nullptr;

    return &itr->second;
}

std::vector<BotTokenExchangerMgr::ResolverEntry> const* BotTokenExchangerMgr::GetWotlkResolverEntries(uint32 tokenItemId)
{
    LoadWotlkResolverMappings();

    auto itr = _wotlkResolverEntriesByToken.find(tokenItemId);
    if (itr == _wotlkResolverEntriesByToken.end())
        return nullptr;

    return &itr->second;
}

std::vector<BotTokenExchangerMgr::ResolverEntry> const* BotTokenExchangerMgr::GetWotlkResolverEntriesCached(uint32 tokenItemId) const
{
    if (!_wotlkResolverLoaded)
        return nullptr;

    auto itr = _wotlkResolverEntriesByToken.find(tokenItemId);
    if (itr == _wotlkResolverEntriesByToken.end())
        return nullptr;

    return &itr->second;
}

bool BotTokenExchangerMgr::HasStagedResolverEntries(uint32 tokenItemId) const
{
    return GetResolverEntriesCached(tokenItemId) != nullptr;
}

bool BotTokenExchangerMgr::HasRewardItemEquipped(Player* player, uint32 itemId) const
{
    if (!player || itemId == 0)
        return false;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            ItemTemplate const* proto = item->GetTemplate();
            if (proto && proto->ItemId == itemId)
                return true;
        }
    }

    return false;
}

bool BotTokenExchangerMgr::HasRewardItemInBags(Player* player, uint32 itemId) const
{
    if (!player || itemId == 0)
        return false;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            ItemTemplate const* proto = item->GetTemplate();
            if (proto && proto->ItemId == itemId)
                return true;
        }
    }

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                if (Item* item = bag->GetItemByPos(slot))
                {
                    ItemTemplate const* proto = item->GetTemplate();
                    if (proto && proto->ItemId == itemId)
                        return true;
                }
            }
        }
    }

    return false;
}

bool BotTokenExchangerMgr::HasRewardItemInEquipmentOrBags(Player* player, uint32 itemId) const
{
    return HasRewardItemEquipped(player, itemId) || HasRewardItemInBags(player, itemId);
}

bool BotTokenExchangerMgr::TryResolveUniqueCandidate(Player* player, uint32 tokenItemId, FilteredCandidate& resolvedCandidate) const
{
    resolvedCandidate = {};

    if (!player)
        return false;

    std::vector<ResolverEntry> const* entries = GetResolverEntriesCached(tokenItemId);
    if (!entries || entries->empty())
        return false;

    std::vector<FilteredCandidate> filtered;
    std::vector<std::string> skipReasons;
    std::vector<std::string> notes;
    if (!BuildFilteredCandidatesFromEntries(player, tokenItemId, *entries, filtered, skipReasons, notes))
        return false;

    if (filtered.empty())
        return false;

    RoleResolution roleResolution = GetLootPassRoleResolution(player);
    std::vector<FilteredCandidate> hybridFiltered;
    std::vector<std::string> hybridNotes;
    std::vector<std::string> hybridSkips;
    if (!BuildHybridCandidates(player, filtered, hybridFiltered, hybridNotes, hybridSkips, roleResolution))
        return false;

    if (!hybridFiltered.empty())
        filtered = std::move(hybridFiltered);

    if (filtered.size() != 1)
        return false;

    resolvedCandidate = filtered.front();
    return true;
}

bool BotTokenExchangerMgr::HandlePlayerbotBeforeLootRoll(Player* bot, ItemTemplate const* itemTemplate, RollVote& rollVote)
{
    if (!_enabled || !_playerbotLootPassEnable || !bot || !itemTemplate)
        return false;

    if (_onlyPlayerbots && (!bot->GetSession() || !bot->GetSession()->IsBot()))
        return false;

    if (!HasStagedResolverEntries(itemTemplate->ItemId))
        return false;

    RollVote const originalVote = rollVote;
    RoleResolution roleResolution = GetLootPassRoleResolution(bot);
    FilteredCandidate resolvedCandidate;
    if (!TryResolveUniqueCandidate(bot, itemTemplate->ItemId, resolvedCandidate))
    {
        if (_debug)
        {
            LOG_DEBUG(
                "bot_token_exchanger",
                "loot-pass debug bot {} faction {} class {} spec {} role {} token {} ({}) vote {} resolution ambiguous or unavailable",
                bot->GetName(),
                GetTeamLabel(GetTeamIdForFiltering(bot)),
                bot->getClass(),
                roleResolution.detectedSpec.empty() ? "unknown" : roleResolution.detectedSpec,
                roleResolution.detectedRole.empty() ? "unknown" : roleResolution.detectedRole,
                itemTemplate->ItemId,
                itemTemplate->Name1,
                RollVoteToString(originalVote));
        }
        return false;
    }

    if (!resolvedCandidate.rewardTemplate || !resolvedCandidate.entry)
        return false;

    bool const equipped = HasRewardItemEquipped(bot, resolvedCandidate.rewardTemplate->ItemId);
    bool const inBags = HasRewardItemInBags(bot, resolvedCandidate.rewardTemplate->ItemId);

    if (_debug)
    {
        LOG_DEBUG(
            "bot_token_exchanger",
            "loot-pass debug bot {} faction {} class {} spec {} role {} token {} ({}) vote {} resolved reward {} ({}) equipped={} bags={} final {}",
            bot->GetName(),
            GetTeamLabel(GetTeamIdForFiltering(bot)),
            bot->getClass(),
            roleResolution.detectedSpec.empty() ? "unknown" : roleResolution.detectedSpec,
            roleResolution.detectedRole.empty() ? "unknown" : roleResolution.detectedRole,
            itemTemplate->ItemId,
            itemTemplate->Name1,
            RollVoteToString(originalVote),
            resolvedCandidate.rewardTemplate->ItemId,
            resolvedCandidate.rewardTemplate->Name1,
            equipped ? "true" : "false",
            inBags ? "true" : "false",
            (equipped || inBags) ? "PASS" : RollVoteToString(originalVote));
    }

    if (!equipped && !inBags)
        return false;

    if (rollVote == PASS)
        return false;

    rollVote = PASS;
    LOG_INFO(
        "bot_token_exchanger",
        "loot-pass forced PASS for bot {} token {} ({}) resolved reward {} ({}) already owned",
        bot->GetName(),
        itemTemplate->ItemId,
        itemTemplate->Name1,
        resolvedCandidate.rewardTemplate->ItemId,
        resolvedCandidate.rewardTemplate->Name1);
    return true;
}

void BotTokenExchangerMgr::ResolveTokenItem(ChatHandler* handler, Player* player, uint32 tokenItemId)
{
    if (!handler || !player)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    std::vector<ResolverEntry> const* entries = GetResolverEntries(tokenItemId);
    uint32 const stagedCount = entries ? static_cast<uint32>(entries->size()) : 0;

    if (_onlyPlayerbots && (!player->GetSession() || !player->GetSession()->IsBot()))
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    std::vector<FilteredCandidate> filtered;
    std::vector<std::string> skipReasons;
    std::vector<std::string> notes;
    if (!BuildFilteredCandidates(player, tokenItemId, filtered, skipReasons, notes))
    {
        if (!skipReasons.empty())
        {
            for (std::string const& reason : skipReasons)
            {
                LOG_INFO("bot_token_exchanger", "Resolver skip token {}: {}", tokenItemId, reason);
                handler->PSendSysMessage("  skip: {}", reason);
            }
        }

        handler->PSendSysMessage("No staged resolver entries found for token item {}.", tokenItemId);
        return;
    }

    ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
    if (!tokenTemplate)
    {
        handler->PSendSysMessage("Token item {} is missing from item_template.", tokenItemId);
        return;
    }

    RoleResolution roleResolution = GetRoleResolution(player);
    std::vector<FilteredCandidate> hybridFiltered;
    std::vector<std::string> hybridNotes;
    std::vector<std::string> hybridSkips;
    if (!BuildHybridCandidates(player, filtered, hybridFiltered, hybridNotes, hybridSkips, roleResolution))
    {
        LogHybridResolutionDecision("resolver", player, tokenItemId, tokenTemplate, filtered, std::vector<FilteredCandidate>{}, roleResolution, hybridSkips, hybridNotes);
        for (std::string const& reason : hybridSkips)
        {
            LOG_INFO("bot_token_exchanger", "Resolver hybrid skip token {}: {}", tokenItemId, reason);
            handler->PSendSysMessage("  skip: {}", reason);
        }

        handler->PSendSysMessage("No safe reward resolved for token {} ({}).", tokenItemId, tokenTemplate->Name1);
        LOG_INFO("bot_token_exchanger", "No safe reward resolved for token {} ({})", tokenItemId, tokenTemplate->Name1);
        return;
    }

    for (std::string const& reason : hybridSkips)
    {
        LOG_INFO("bot_token_exchanger", "Resolver hybrid skip token {} ({}): {}", tokenItemId, tokenTemplate->Name1, reason);
        handler->PSendSysMessage("  skip: {}", reason);
    }

    for (std::string const& note : hybridNotes)
    {
        LOG_INFO("bot_token_exchanger", "Resolver hybrid note token {} ({}): {}", tokenItemId, tokenTemplate->Name1, note);
        handler->PSendSysMessage("  note: {}", note);
    }

    if (!hybridFiltered.empty())
        filtered = std::move(hybridFiltered);

    if (filtered.size() > 1)
        LogHybridResolutionDecision("resolver", player, tokenItemId, tokenTemplate, filtered, filtered, roleResolution, hybridSkips, hybridNotes);

    handler->PSendSysMessage(
        "Resolver token {} ({}) for bot {} class {} mask {}: {} staged candidates, {} passed safe filters.",
        tokenItemId,
        tokenTemplate->Name1,
        player->GetName(),
        player->getClass(),
        player->getClassMask(),
        stagedCount,
        filtered.size());

    LOG_INFO(
        "bot_token_exchanger",
        "Resolver token {} ({}) for bot {} class {} mask {}: {} staged candidates, {} passed safe filters.",
        tokenItemId,
        tokenTemplate->Name1,
        player->GetName(),
        player->getClass(),
        player->getClassMask(),
        stagedCount,
        filtered.size());

    for (std::string const& reason : skipReasons)
    {
        LOG_INFO("bot_token_exchanger", "Resolver skip token {} ({}): {}", tokenItemId, tokenTemplate->Name1, reason);
        handler->PSendSysMessage("  skip: {}", reason);
    }

    for (std::string const& note : notes)
    {
        LOG_INFO("bot_token_exchanger", "Resolver note token {} ({}): {}", tokenItemId, tokenTemplate->Name1, note);
        handler->PSendSysMessage("  note: {}", note);
    }

    if (filtered.empty())
    {
        handler->PSendSysMessage("No safe reward resolved for token {} ({}).", tokenItemId, tokenTemplate->Name1);
        LOG_INFO("bot_token_exchanger", "No safe reward resolved for token {} ({})", tokenItemId, tokenTemplate->Name1);
        return;
    }

    if (filtered.size() == 1)
    {
        ResolverEntry const* entry = filtered.front().entry;
        ItemTemplate const* rewardTemplate = filtered.front().rewardTemplate;

        handler->PSendSysMessage(
            "Resolved token {} ({}) to reward {} ({}) via vendor {} extcost {}.",
            tokenItemId,
            tokenTemplate->Name1,
            entry->rewardItemId,
            rewardTemplate->Name1,
            entry->vendorEntry,
            entry->extendedCostId);

        LOG_INFO(
            "bot_token_exchanger",
            "Resolved token {} ({}) for bot {} to reward {} ({}) via vendor {} extcost {}",
            tokenItemId,
            tokenTemplate->Name1,
            player->GetName(),
            entry->rewardItemId,
            rewardTemplate->Name1,
            entry->vendorEntry,
            entry->extendedCostId);
        return;
    }

    std::ostringstream ss;
    ss << "Ambiguous token " << tokenItemId << " (" << tokenTemplate->Name1 << ") candidates:";
    for (FilteredCandidate const& candidate : filtered)
        ss << " [" << candidate.entry->rewardItemId << " " << candidate.rewardTemplate->Name1 << "]";

    handler->SendSysMessage(ss.str());
    LOG_INFO("bot_token_exchanger", "{}", ss.str());
}

void BotTokenExchangerMgr::ResolveSelectedTokenRewards(ChatHandler* handler)
{
    if (!handler)
        return;

    Player* player = handler->getSelectedPlayer();
    if (!player)
    {
        handler->SendSysMessage("Select a Playerbot first.");
        return;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    ResolveTokenRewardsForPlayer(handler, player, "selected");
}

void BotTokenExchangerMgr::ResolveWotlkTokenItem(ChatHandler* handler, Player* player, uint32 tokenItemId)
{
    if (!handler || !player)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    std::vector<ResolverEntry> const* entries = GetWotlkResolverEntries(tokenItemId);
    uint32 const stagedCount = entries ? static_cast<uint32>(entries->size()) : 0;

    if (_onlyPlayerbots && (!player->GetSession() || !player->GetSession()->IsBot()))
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    std::vector<FilteredCandidate> filtered;
    std::vector<std::string> skipReasons;
    std::vector<std::string> notes;
    if (!entries || !BuildFilteredCandidatesFromEntriesForWotlkReadOnly(player, tokenItemId, *entries, filtered, skipReasons, notes))
    {
        if (!skipReasons.empty())
        {
            for (std::string const& reason : skipReasons)
                handler->PSendSysMessage("  skip: {}", reason);
        }
        handler->PSendSysMessage("No WOTLK staged single-token-chain resolver entries found for token item {}.", tokenItemId);
        return;
    }

    ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
    if (!tokenTemplate)
    {
        handler->PSendSysMessage("Token item {} is missing from item_template.", tokenItemId);
        return;
    }

    RoleResolution roleResolution = GetRoleResolution(player);
    std::vector<FilteredCandidate> hybridFiltered;
    std::vector<std::string> hybridNotes;
    std::vector<std::string> hybridSkips;
    if (!BuildWotlkHybridCandidates(player, filtered, hybridFiltered, hybridNotes, hybridSkips, roleResolution))
    {
        for (std::string const& reason : hybridSkips)
            handler->PSendSysMessage("  skip: {}", reason);
        handler->PSendSysMessage("No safe WOTLK reward resolved for token {} ({}).", tokenItemId, tokenTemplate->Name1);
        return;
    }

    for (std::string const& reason : skipReasons)
        handler->PSendSysMessage("  skip: {}", reason);
    for (std::string const& note : notes)
        handler->PSendSysMessage("  note: {}", note);
    for (std::string const& reason : hybridSkips)
        handler->PSendSysMessage("  skip: {}", reason);
    for (std::string const& note : hybridNotes)
        handler->PSendSysMessage("  note: {}", note);

    if (!hybridFiltered.empty())
        filtered = std::move(hybridFiltered);

    handler->PSendSysMessage(
        "WOTLK resolver token {} ({}) for bot {} faction {} class {} spec {} role {} mask {}: {} staged candidates, {} passed safe filters.",
        tokenItemId,
        tokenTemplate->Name1,
        player->GetName(),
        GetTeamLabel(GetTeamIdForFiltering(player)),
        player->getClass(),
        roleResolution.detectedSpec.empty() ? "unknown" : roleResolution.detectedSpec,
        roleResolution.detectedRole.empty() ? "unknown" : roleResolution.detectedRole,
        player->getClassMask(),
        stagedCount,
        filtered.size());

    if (filtered.empty())
    {
        handler->PSendSysMessage("No safe WOTLK reward resolved for token {} ({}).", tokenItemId, tokenTemplate->Name1);
        return;
    }

    if (filtered.size() == 1)
    {
        ResolverEntry const* entry = filtered.front().entry;
        ItemTemplate const* rewardTemplate = filtered.front().rewardTemplate;
        handler->PSendSysMessage(
            "WOTLK resolved token {} ({}) to reward {} ({}) via vendor {} extcost {}.",
            tokenItemId,
            tokenTemplate->Name1,
            entry->rewardItemId,
            rewardTemplate->Name1,
            entry->vendorEntry,
            entry->extendedCostId);
        return;
    }

    std::ostringstream ss;
    ss << "WOTLK ambiguous token " << tokenItemId << " (" << tokenTemplate->Name1 << ") candidates:";
    for (FilteredCandidate const& candidate : filtered)
        ss << " [" << candidate.entry->rewardItemId << " " << candidate.rewardTemplate->Name1 << "]";
    handler->SendSysMessage(ss.str());
}

void BotTokenExchangerMgr::ResolveWotlkSelectedTokenRewards(ChatHandler* handler)
{
    if (!handler)
        return;

    Player* player = handler->getSelectedPlayer();
    if (!player)
    {
        handler->SendSysMessage("Select a Playerbot first.");
        return;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    std::unordered_map<uint32, uint32> tokenCounts;
    CollectTokenCounts(player, tokenCounts);

    uint32 scanned = 0;
    uint32 resolved = 0;
    uint32 ambiguous = 0;
    uint32 noMatch = 0;
    for (auto const& [tokenItemId, count] : tokenCounts)
    {
        (void)count;
        std::vector<ResolverEntry> const* entries = GetWotlkResolverEntries(tokenItemId);
        if (!entries || entries->empty())
            continue;

        ++scanned;
        std::vector<FilteredCandidate> filtered;
        std::vector<std::string> skipReasons;
        std::vector<std::string> notes;
        if (!BuildFilteredCandidatesFromEntriesForWotlkReadOnly(player, tokenItemId, *entries, filtered, skipReasons, notes) || filtered.empty())
        {
            ++noMatch;
            continue;
        }

        RoleResolution roleResolution = GetRoleResolution(player);
        std::vector<FilteredCandidate> hybridFiltered;
        std::vector<std::string> hybridNotes;
        std::vector<std::string> hybridSkips;
        if (!BuildWotlkHybridCandidates(player, filtered, hybridFiltered, hybridNotes, hybridSkips, roleResolution))
        {
            ++noMatch;
            continue;
        }

        if (!hybridFiltered.empty())
            filtered = std::move(hybridFiltered);

        if (filtered.size() == 1)
        {
            ++resolved;
            ResolverEntry const* entry = filtered.front().entry;
            ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
            ItemTemplate const* rewardTemplate = filtered.front().rewardTemplate;
            if (tokenTemplate && rewardTemplate)
            {
                handler->PSendSysMessage(
                    "WOTLK token {} ({}) -> reward {} ({}) vendor {} extcost {} [read-only]",
                    tokenItemId,
                    tokenTemplate->Name1,
                    entry->rewardItemId,
                    rewardTemplate->Name1,
                    entry->vendorEntry,
                    entry->extendedCostId);
            }
            continue;
        }

        ++ambiguous;
        if (_debug)
        {
            ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
            std::ostringstream ss;
            ss << "WOTLK ambiguous token " << tokenItemId << " (" << (tokenTemplate ? tokenTemplate->Name1 : "unknown") << ") candidates:";
            for (FilteredCandidate const& candidate : filtered)
                ss << " [" << candidate.entry->rewardItemId << " " << candidate.rewardTemplate->Name1 << "]";
            handler->SendSysMessage(ss.str());
        }
    }

    handler->PSendSysMessage(
        "WOTLK selected resolver scan complete: {} staged token IDs scanned, {} resolved, {} ambiguous, {} no-match.",
        scanned,
        resolved,
        ambiguous,
        noMatch);
}

void BotTokenExchangerMgr::ResolveWotlkBotTokenRewards(ChatHandler* handler, std::string const& botName)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    Player* player = ResolveOnlineBotByName(handler, botName, "resolve");
    if (!player)
        return;

    std::unordered_map<uint32, uint32> tokenCounts;
    CollectTokenCounts(player, tokenCounts);

    uint32 scanned = 0;
    for (auto const& [tokenItemId, count] : tokenCounts)
    {
        (void)count;
        if (!GetWotlkResolverEntries(tokenItemId))
            continue;
        ++scanned;
        ResolveWotlkTokenItem(handler, player, tokenItemId);
    }

    handler->PSendSysMessage("WOTLK resolve bot {} complete: {} staged WOTLK token IDs scanned.", player->GetName(), scanned);
}

void BotTokenExchangerMgr::ResolveWotlkBotTokenItem(ChatHandler* handler, std::string const& botName, uint32 tokenItemId)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    Player* player = ResolveOnlineBotByName(handler, botName, "resolve");
    if (!player)
        return;

    ResolveWotlkTokenItem(handler, player, tokenItemId);
}

void BotTokenExchangerMgr::ShowStatus(ChatHandler* handler)
{
    if (!handler)
        return;

    handler->PSendSysMessage("BotTokenExchanger status:");
    handler->PSendSysMessage("  Enable: {}", _enabled ? 1 : 0);
    handler->PSendSysMessage("  DryRun: {}", _dryRun ? 1 : 0);
    handler->PSendSysMessage("  ExchangeEnable: {}", _exchangeEnable ? 1 : 0);
    handler->PSendSysMessage("  AutoExchangeEnable: {}", _autoExchangeEnable ? 1 : 0);
    handler->PSendSysMessage("  WotlkExchangeEnable: {}", _wotlkExchangeEnable ? 1 : 0);
    handler->PSendSysMessage("  WotlkDryRun: {}", _wotlkDryRun ? 1 : 0);
    handler->PSendSysMessage("  WotlkAutoExchangeEnable: {}", _wotlkAutoExchangeEnable ? 1 : 0);
    handler->PSendSysMessage("  AllowDebugTargetCommand: {}", _allowDebugTargetCommand ? 1 : 0);
    handler->PSendSysMessage("  PlayerbotLootPassEnable: {}", _playerbotLootPassEnable ? 1 : 0);
    handler->PSendSysMessage("  LoadedMappingCount: {}", GetLoadedResolverMappingCount());
    handler->PSendSysMessage("  LoadedWotlkMappingCount: {}", GetLoadedWotlkResolverMappingCount());
    handler->PSendSysMessage("  WotlkSingleTokenChainCount: {}", GetLoadedWotlkTokenKeyCount());
    handler->PSendSysMessage("  QueueSize: {}", GetAutoExchangeQueueSize());
    handler->PSendSysMessage("  DiscoveryWriteDb: {}", _discoveryWriteDb ? 1 : 0);
    handler->PSendSysMessage("  ResolveOnly: {}", _resolveOnly ? 1 : 0);
    handler->PSendSysMessage("  OnlyPlayerbots: {}", _onlyPlayerbots ? 1 : 0);
}

void BotTokenExchangerMgr::ResolveBotTokenRewards(ChatHandler* handler, std::string const& botName)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    Player* player = ResolveOnlineBotByName(handler, botName, "resolve");
    if (!player)
        return;

    ResolveTokenRewardsForPlayer(handler, player, "debug");
}

void BotTokenExchangerMgr::ShowBotRole(ChatHandler* handler, std::string const& botName)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    Player* player = ResolveOnlineBotByName(handler, botName, "role");
    if (!player)
        return;

    DescribeRoleResolution(handler, player);
}

void BotTokenExchangerMgr::ExchangePlayerTokens(Player* player, char const* label, uint32 maxPerBotPerPass, ChatHandler* handler)
{
    if (!player)
        return;

    if (!_enabled)
    {
        if (handler)
            handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    if (_onlyPlayerbots && (!player->GetSession() || !player->GetSession()->IsBot()))
    {
        if (handler)
            handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    if (_exchangeActive)
    {
        if (handler)
            handler->SendSysMessage("Token exchange is already running for another bot.");
        return;
    }

    ExchangeScopeGuard guard(*this);

    auto sendFormatted = [&](auto const&... args)
    {
        if (handler)
            handler->PSendSysMessage(args...);
    };

    std::unordered_map<uint32, uint32> tokenCounts;
    CollectTokenCounts(player, tokenCounts);

    std::unordered_map<uint32, uint32> stagedTokenCounts;
    for (auto const& [tokenItemId, count] : tokenCounts)
    {
        std::vector<ResolverEntry> const* entries = GetResolverEntries(tokenItemId);
        if (entries && !entries->empty())
            stagedTokenCounts[tokenItemId] = count;
    }

    bool const allowRealExchange = _exchangeEnable && !_dryRun;
    bool const dryRun = !allowRealExchange;

    if (stagedTokenCounts.empty())
    {
        sendFormatted("{} bot {} has no staged token items in bags.", label, player->GetName());
        LOG_INFO("server", "BotTokenExchanger {} bot {} has no staged token items in bags.", label, player->GetName());
        return;
    }

    sendFormatted(
        "{} bot {} token scan found {} staged token item IDs. Mode: {}.",
        label,
        player->GetName(),
        stagedTokenCounts.size(),
        dryRun ? "dry-run" : "real exchange");

    LOG_INFO(
        "bot_token_exchanger",
        "{} bot {} token scan found {} staged token item IDs. Mode: {}.",
        label,
        player->GetName(),
        stagedTokenCounts.size(),
        dryRun ? "dry-run" : "real exchange");
    LOG_INFO(
        "server",
        "BotTokenExchanger {} bot {} token scan found {} staged token item IDs. Mode: {}.",
        label,
        player->GetName(),
        stagedTokenCounts.size(),
        dryRun ? "dry-run" : "real exchange");

    uint32 resolvedCount = 0;
    uint32 skippedCount = 0;

    for (auto const& [tokenItemId, tokenCount] : stagedTokenCounts)
    {
        ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
        if (!tokenTemplate)
        {
            ++skippedCount;
            std::string message = Acore::StringFormat("skip token {}: token item template missing", tokenItemId);
            LOG_INFO("bot_token_exchanger", "{}", message);
            sendFormatted("{}", message);
            continue;
        }

        std::vector<FilteredCandidate> filtered;
        std::vector<std::string> skipReasons;
        std::vector<std::string> notes;
        if (!BuildFilteredCandidates(player, tokenItemId, filtered, skipReasons, notes))
        {
            ++skippedCount;
            if (!skipReasons.empty())
            {
                for (std::string const& reason : skipReasons)
                {
                    LOG_INFO("bot_token_exchanger", "Exchange skip token {} ({}): {}", tokenItemId, tokenTemplate->Name1, reason);
                    sendFormatted("  skip: {}", reason);
                }
            }

            std::string message = Acore::StringFormat("skip token {} ({}): no safe staged resolver entries", tokenItemId, tokenTemplate->Name1);
            LOG_INFO("bot_token_exchanger", "{}", message);
            LOG_INFO("server", "BotTokenExchanger {}", message);
            sendFormatted("{}", message);
            continue;
        }

        for (std::string const& reason : skipReasons)
        {
            LOG_INFO("bot_token_exchanger", "Exchange skip token {} ({}): {}", tokenItemId, tokenTemplate->Name1, reason);
            sendFormatted("  skip: {}", reason);
        }

        for (std::string const& note : notes)
        {
            LOG_INFO("bot_token_exchanger", "Exchange note token {} ({}): {}", tokenItemId, tokenTemplate->Name1, note);
            sendFormatted("  note: {}", note);
        }

        if (filtered.empty())
        {
            ++skippedCount;
            std::string message = Acore::StringFormat("token {} ({}) has no safe reward after class/faction filtering", tokenItemId, tokenTemplate->Name1);
            LOG_INFO("bot_token_exchanger", "{}", message);
            LOG_INFO("server", "BotTokenExchanger {}", message);
            sendFormatted("{}", message);
            continue;
        }

        RoleResolution roleResolution = GetRoleResolution(player);
        std::vector<FilteredCandidate> hybridFiltered;
        std::vector<std::string> hybridNotes;
        std::vector<std::string> hybridSkips;
        if (!BuildHybridCandidates(player, filtered, hybridFiltered, hybridNotes, hybridSkips, roleResolution))
        {
            LogHybridResolutionDecision("exchange", player, tokenItemId, tokenTemplate, filtered, std::vector<FilteredCandidate>{}, roleResolution, hybridSkips, hybridNotes);
            ++skippedCount;
            for (std::string const& reason : hybridSkips)
            {
                LOG_INFO("bot_token_exchanger", "Exchange hybrid skip token {} ({}): {}", tokenItemId, tokenTemplate->Name1, reason);
                sendFormatted("  skip: {}", reason);
            }

            std::string message = Acore::StringFormat("token {} ({}) skipped after hybrid role filtering", tokenItemId, tokenTemplate->Name1);
            LOG_INFO("bot_token_exchanger", "{}", message);
            LOG_INFO("server", "BotTokenExchanger {}", message);
            sendFormatted("{}", message);
            continue;
        }

        for (std::string const& reason : hybridSkips)
        {
            LOG_INFO("bot_token_exchanger", "Exchange hybrid skip token {} ({}): {}", tokenItemId, tokenTemplate->Name1, reason);
            sendFormatted("  skip: {}", reason);
        }

        for (std::string const& note : hybridNotes)
        {
            LOG_INFO("bot_token_exchanger", "Exchange hybrid note token {} ({}): {}", tokenItemId, tokenTemplate->Name1, note);
            sendFormatted("  note: {}", note);
        }

        if (!hybridFiltered.empty())
            filtered = std::move(hybridFiltered);

        if (filtered.size() > 1)
            LogHybridResolutionDecision("exchange", player, tokenItemId, tokenTemplate, filtered, filtered, roleResolution, hybridSkips, hybridNotes);

        if (filtered.size() > 1)
        {
            ++skippedCount;
            std::ostringstream ss;
            ss << "token " << tokenItemId << " (" << tokenTemplate->Name1 << ") remains ambiguous after faction filtering:";
            for (FilteredCandidate const& candidate : filtered)
                ss << " [" << candidate.entry->rewardItemId << " " << candidate.rewardTemplate->Name1 << "]";

            std::string message = ss.str();
            LOG_INFO("bot_token_exchanger", "{}", message);
            LOG_INFO("server", "BotTokenExchanger {}", message);
            sendFormatted("{}", message);
            continue;
        }

        FilteredCandidate const& candidate = filtered.front();
        ResolverEntry const& entry = *candidate.entry;
        ItemTemplate const* rewardTemplate = candidate.rewardTemplate;

        sendFormatted(
            "token {} ({}) -> reward {} ({}) count {} vendor {} extcost {} [{}]",
            tokenItemId,
            tokenTemplate->Name1,
            rewardTemplate->ItemId,
            rewardTemplate->Name1,
            tokenCount,
            entry.vendorEntry,
            entry.extendedCostId,
            dryRun ? "dry-run" : "exchange");

        LOG_INFO(
            "bot_token_exchanger",
            "token {} ({}) -> reward {} ({}) count {} vendor {} extcost {} [{}]",
            tokenItemId,
            tokenTemplate->Name1,
            rewardTemplate->ItemId,
            rewardTemplate->Name1,
            tokenCount,
            entry.vendorEntry,
            entry.extendedCostId,
            dryRun ? "dry-run" : "exchange");

        for (uint32 i = 0; i < tokenCount; ++i)
        {
            std::string transactionLog;
            bool const success = TryExchangeToken(player, entry, tokenTemplate, rewardTemplate, dryRun, transactionLog);
            LOG_INFO("bot_token_exchanger", "{}", transactionLog);
            LOG_INFO("server", "BotTokenExchanger {}", transactionLog);
            sendFormatted("  {}", transactionLog);

            if (success)
            {
                ++resolvedCount;
                if (maxPerBotPerPass != 0 && resolvedCount >= maxPerBotPerPass)
                    break;
                continue;
            }

            ++skippedCount;
            if (allowRealExchange)
                break;
        }

        if (maxPerBotPerPass != 0 && resolvedCount >= maxPerBotPerPass)
            break;
    }

    sendFormatted(
        "{} bot {} exchange complete: {} resolved, {} skipped, mode {}.",
        label,
        player->GetName(),
        resolvedCount,
        skippedCount,
        dryRun ? "dry-run" : "real exchange");

    LOG_INFO(
        "bot_token_exchanger",
        "{} bot {} exchange complete: {} resolved, {} skipped, mode {}.",
        label,
        player->GetName(),
        resolvedCount,
        skippedCount,
        dryRun ? "dry-run" : "real exchange");
    LOG_INFO(
        "server",
        "BotTokenExchanger {} bot {} exchange complete: {} resolved, {} skipped, mode {}.",
        label,
        player->GetName(),
        resolvedCount,
        skippedCount,
        dryRun ? "dry-run" : "real exchange");
}

void BotTokenExchangerMgr::ExchangeWotlkPlayerTokens(Player* player, char const* label, ChatHandler* handler, uint32 maxPerBotPerPass)
{
    if (!player)
        return;

    if (!_enabled)
    {
        if (handler)
            handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    if (!_wotlkExchangeEnable)
    {
        if (handler)
            handler->SendSysMessage("WOTLK exchange path is disabled (BotTokenExchanger.WotlkExchangeEnable = 0).");
        return;
    }

    if (_onlyPlayerbots && (!player->GetSession() || !player->GetSession()->IsBot()))
    {
        if (handler)
            handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    auto sendFormatted = [&](auto const&... args)
    {
        if (handler)
            handler->PSendSysMessage(args...);
    };
    auto logWotlkEvent = [&](std::string const& message)
    {
        LOG_INFO("bot_token_exchanger", "WOTLK {}", message);
    };

    std::unordered_map<uint32, uint32> tokenCounts;
    CollectTokenCounts(player, tokenCounts);

    std::unordered_map<uint32, uint32> stagedTokenCounts;
    for (auto const& [tokenItemId, count] : tokenCounts)
    {
        std::vector<ResolverEntry> const* entries = GetWotlkResolverEntries(tokenItemId);
        if (entries && !entries->empty())
            stagedTokenCounts[tokenItemId] = count;
    }

    if (stagedTokenCounts.empty())
    {
        sendFormatted("{} bot {} has no staged WOTLK single-token-chain token items in bags.", label, player->GetName());
        logWotlkEvent(Acore::StringFormat("skip bot {}: no staged single-token-chain token items in bags", player->GetName()));
        return;
    }

    sendFormatted(
        "{} bot {} WOTLK token scan found {} staged token item IDs. Mode: {}.",
        label,
        player->GetName(),
        stagedTokenCounts.size(),
        _wotlkDryRun ? "dry-run" : "real exchange");

    uint32 resolvedCount = 0;
    uint32 skippedCount = 0;
    for (auto const& [tokenItemId, tokenCount] : stagedTokenCounts)
    {
        ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
        if (!tokenTemplate)
        {
            ++skippedCount;
            sendFormatted("skip WOTLK token {}: token item template missing", tokenItemId);
            logWotlkEvent(Acore::StringFormat("skip bot {} token {}: template missing", player->GetName(), tokenItemId));
            continue;
        }

        std::vector<ResolverEntry> const* entries = GetWotlkResolverEntries(tokenItemId);
        if (!entries || entries->empty())
        {
            ++skippedCount;
            sendFormatted("skip WOTLK token {} ({}): not in WOTLK single-token-chain resolver scope", tokenItemId, tokenTemplate->Name1);
            logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): excluded structure/not in resolver scope", player->GetName(), tokenItemId, tokenTemplate->Name1));
            continue;
        }

        std::vector<FilteredCandidate> filtered;
        std::vector<std::string> skipReasons;
        std::vector<std::string> notes;
        if (!BuildFilteredCandidatesFromEntriesForWotlkReadOnly(player, tokenItemId, *entries, filtered, skipReasons, notes))
        {
            ++skippedCount;
            for (std::string const& reason : skipReasons)
                sendFormatted("  skip: {}", reason);
            sendFormatted("skip WOTLK token {} ({}): no safe staged resolver entries", tokenItemId, tokenTemplate->Name1);
            logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): no safe staged resolver entries", player->GetName(), tokenItemId, tokenTemplate->Name1));
            continue;
        }

        RoleResolution roleResolution = GetRoleResolution(player);
        std::vector<FilteredCandidate> hybridFiltered;
        std::vector<std::string> hybridNotes;
        std::vector<std::string> hybridSkips;
        if (!BuildWotlkHybridCandidates(player, filtered, hybridFiltered, hybridNotes, hybridSkips, roleResolution))
        {
            ++skippedCount;
            for (std::string const& reason : skipReasons)
                sendFormatted("  skip: {}", reason);
            for (std::string const& reason : hybridSkips)
                sendFormatted("  skip: {}", reason);
            sendFormatted("skip WOTLK token {} ({}): no safe reward after role filtering", tokenItemId, tokenTemplate->Name1);
            logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): no safe reward after role filtering", player->GetName(), tokenItemId, tokenTemplate->Name1));
            continue;
        }

        for (std::string const& reason : skipReasons)
            sendFormatted("  skip: {}", reason);
        for (std::string const& note : notes)
            sendFormatted("  note: {}", note);
        for (std::string const& reason : hybridSkips)
            sendFormatted("  skip: {}", reason);
        for (std::string const& note : hybridNotes)
            sendFormatted("  note: {}", note);

        if (!hybridFiltered.empty())
            filtered = std::move(hybridFiltered);

        if (filtered.empty())
        {
            ++skippedCount;
            sendFormatted("skip WOTLK token {} ({}): no safe reward resolved", tokenItemId, tokenTemplate->Name1);
            logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): no safe reward resolved", player->GetName(), tokenItemId, tokenTemplate->Name1));
            continue;
        }

        if (filtered.size() > 1)
        {
            ++skippedCount;
            std::ostringstream ss;
            ss << "skip WOTLK token " << tokenItemId << " (" << tokenTemplate->Name1 << "): ambiguous candidates";
            for (FilteredCandidate const& candidate : filtered)
                ss << " [" << candidate.entry->rewardItemId << " " << candidate.rewardTemplate->Name1 << "]";
            sendFormatted("{}", ss.str());
            logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): ambiguous candidates ({})", player->GetName(), tokenItemId, tokenTemplate->Name1, filtered.size()));
            continue;
        }

        FilteredCandidate const& candidate = filtered.front();
        ResolverEntry const& entry = *candidate.entry;
        ItemTemplate const* rewardTemplate = candidate.rewardTemplate;

        if (HasRewardItemInEquipmentOrBags(player, rewardTemplate->ItemId))
        {
            ++skippedCount;
            sendFormatted(
                "skip WOTLK token {} ({}): reward {} ({}) already owned/equipped",
                tokenItemId,
                tokenTemplate->Name1,
                rewardTemplate->ItemId,
                rewardTemplate->Name1);
            logWotlkEvent(
                Acore::StringFormat(
                    "duplicate-prevented bot {} token {} ({}) -> reward {} ({}) already owned/equipped",
                    player->GetName(),
                    tokenItemId,
                    tokenTemplate->Name1,
                    rewardTemplate->ItemId,
                    rewardTemplate->Name1));
            continue;
        }

        for (uint32 i = 0; i < tokenCount; ++i)
        {
            std::string transactionLog;
            bool const success = TryExchangeToken(player, entry, tokenTemplate, rewardTemplate, _wotlkDryRun, transactionLog);
            if (success)
            {
                ++resolvedCount;
                sendFormatted("WOTLK {} {}", _wotlkDryRun ? "DRY RUN" : "EXCHANGE", transactionLog);
                logWotlkEvent(Acore::StringFormat("{} {}", _wotlkDryRun ? "dry-run" : "exchange", transactionLog));
                if (maxPerBotPerPass != 0 && resolvedCount >= maxPerBotPerPass)
                    break;
                continue;
            }

            ++skippedCount;
            sendFormatted("skip WOTLK token {} ({}): {}", tokenItemId, tokenTemplate->Name1, transactionLog);
            if (transactionLog.find("token ") != std::string::npos && transactionLog.find("not present in bags") != std::string::npos)
            {
                logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): no token remained during repeated execution", player->GetName(), tokenItemId, tokenTemplate->Name1));
            }
            else if (transactionLog.find("no bag space for reward") != std::string::npos)
            {
                logWotlkEvent(Acore::StringFormat("store-failure bot {} token {} ({}): {}", player->GetName(), tokenItemId, tokenTemplate->Name1, transactionLog));
            }
            else if (transactionLog.find("restored") != std::string::npos || transactionLog.find("hard failure") != std::string::npos)
            {
                logWotlkEvent(Acore::StringFormat("rollback bot {} token {} ({}): {}", player->GetName(), tokenItemId, tokenTemplate->Name1, transactionLog));
            }
            else
            {
                logWotlkEvent(Acore::StringFormat("skip bot {} token {} ({}): {}", player->GetName(), tokenItemId, tokenTemplate->Name1, transactionLog));
            }
            if (!_wotlkDryRun)
                break;
        }

        if (maxPerBotPerPass != 0 && resolvedCount >= maxPerBotPerPass)
            break;
    }

    sendFormatted(
        "{} bot {} WOTLK exchange complete: {} resolved, {} skipped, mode {}.",
        label,
        player->GetName(),
        resolvedCount,
        skippedCount,
        _wotlkDryRun ? "dry-run" : "real exchange");
    logWotlkEvent(
        Acore::StringFormat(
            "{} bot {} exchange complete: resolved={} skipped={} mode={}",
            label,
            player->GetName(),
            resolvedCount,
            skippedCount,
            _wotlkDryRun ? "dry-run" : "real exchange"));
}

void BotTokenExchangerMgr::ExchangeWotlkSelectedTokens(ChatHandler* handler)
{
    if (!handler)
        return;

    Player* player = handler->getSelectedPlayer();
    if (!player)
    {
        handler->SendSysMessage("Select a Playerbot first.");
        return;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    ExchangeWotlkPlayerTokens(player, "selected", handler);
}

void BotTokenExchangerMgr::ExchangeWotlkBotTokens(ChatHandler* handler, std::string const& botName)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    Player* player = ResolveOnlineBotByName(handler, botName, "exchange");
    if (!player)
        return;

    ExchangeWotlkPlayerTokens(player, "debug", handler);
}

void BotTokenExchangerMgr::ExchangeSelectedTokens(ChatHandler* handler)
{
    if (!handler)
        return;

    Player* player = handler->getSelectedPlayer();
    if (!player)
    {
        handler->SendSysMessage("Select a Playerbot first.");
        return;
    }

    if (!player->GetSession() || !player->GetSession()->IsBot())
    {
        handler->SendSysMessage("Selected player is not a Playerbot.");
        return;
    }

    ExchangePlayerTokens(player, "selected", 0, handler);
}

void BotTokenExchangerMgr::ExchangeBotTokens(ChatHandler* handler, std::string const& botName)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    Player* player = ResolveOnlineBotByName(handler, botName, "exchange");
    if (!player)
        return;

    ExchangePlayerTokens(player, "debug", 0, handler);
}

void BotTokenExchangerMgr::ExchangeGroupTokens(ChatHandler* handler)
{
    if (!handler)
        return;

    Player* player = handler->GetPlayer();
    if (!player)
    {
        handler->SendSysMessage("This command requires a player character.");
        return;
    }

    if (!player->GetGroup())
    {
        handler->SendSysMessage("You are not in a group.");
        return;
    }

    uint32 scannedBots = 0;
    for (GroupReference* itr = player->GetGroup()->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->GetSession() || !member->GetSession()->IsBot())
            continue;

        ++scannedBots;
        ExchangePlayerTokens(member, "group", 0, handler);
    }

    if (!scannedBots)
        handler->SendSysMessage("No Playerbots were found in your group.");
}

void BotTokenExchangerMgr::DiscoverTbcTokenMappings(ChatHandler* handler)
{
    DiscoverTokenMappings(handler, "TBC", false, false);
}

void BotTokenExchangerMgr::DiscoverWotlkTokenMappings(ChatHandler* handler)
{
    DiscoverTokenMappings(handler, "WOTLK", false, false);
}

void BotTokenExchangerMgr::DiscoverWotlkTokenMappingsNarrow(ChatHandler* handler)
{
    DiscoverTokenMappings(handler, "WOTLK", true, false);
}

void BotTokenExchangerMgr::DiscoverWotlkTokenMappingsInspect(ChatHandler* handler)
{
    DiscoverTokenMappings(handler, "WOTLK", true, true);
}

void BotTokenExchangerMgr::DiscoverWotlkTokenMappingsStage(ChatHandler* handler)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    QueryResult vendorResult = WorldDatabase.Query(
        "SELECT nv.entry, nv.slot, nv.item, nv.ExtendedCost "
        "FROM npc_vendor nv "
        "INNER JOIN item_template reward ON reward.entry = nv.item "
        "WHERE nv.ExtendedCost > 0 "
        "AND reward.class = 4 "
        "AND reward.InventoryType IN (1, 3, 5, 9, 10, 6, 7, 8, 20) "
        "AND nv.entry IN (28992, 28995, 28997, 29523, 34252, 35496, 35497, 35498, 35500, 37688, "
        "37696, 37991, 37992, 37993, 37997, 37998, 37999, 38054, 38181, 38182, "
        "38283, 38284, 38316, 38840, 38841) "
        "ORDER BY nv.entry, nv.slot ASC, nv.item ASC");

    if (!vendorResult)
    {
        handler->SendSysMessage("No matching WOTLK narrow vendor rows found.");
        return;
    }

    std::unordered_set<uint32> rewardsDiscovered;
    std::unordered_set<uint32> vendorsScanned;
    std::vector<std::string> rewardExamples;
    std::unordered_map<std::string, uint32> roleCounts;
    std::unordered_map<std::string, uint32> structureCounts;
    uint32 inspectedRows = 0;
    uint32 costRowsDiscovered = 0;
    uint32 stagedRewards = 0;
    uint32 stagedCosts = 0;

    auto inferTierRaidFromReward = [](std::string const& rewardName, std::string& tier, std::string& raid)
    {
        if (ContainsCaseInsensitive(rewardName, "sanctified"))
        {
            tier = "T10";
            raid = "Icecrown Citadel";
            return;
        }
        if (ContainsCaseInsensitive(rewardName, "triumph") || ContainsCaseInsensitive(rewardName, "triumphant"))
        {
            tier = "T9";
            raid = "Trial of the Crusader";
            return;
        }
        if (ContainsCaseInsensitive(rewardName, "conqueror's") || ContainsCaseInsensitive(rewardName, "valorous"))
            tier = "T8_like";
        else if (ContainsCaseInsensitive(rewardName, "heroes'"))
            tier = "T7_like";
    };

    do
    {
        Field* fields = vendorResult->Fetch();
        uint32 vendorEntry = fields[0].Get<uint32>();
        uint32 rewardItemId = fields[2].Get<uint32>();
        uint32 extendedCostId = fields[3].Get<uint32>();
        ++inspectedRows;
        vendorsScanned.insert(vendorEntry);

        ItemTemplate const* rewardTemplate = sObjectMgr->GetItemTemplate(rewardItemId);
        ItemExtendedCostEntry const* costEntry = sItemExtendedCostStore.LookupEntry(extendedCostId);
        if (!rewardTemplate || !costEntry)
            continue;

        std::vector<std::tuple<uint32, uint32, std::string, std::string>> reqs;
        for (uint8 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
        {
            uint32 reqId = costEntry->reqitem[i];
            uint32 reqCount = costEntry->reqitemcount[i];
            if (!reqId)
                continue;
            ItemTemplate const* reqTemplate = sObjectMgr->GetItemTemplate(reqId);
            std::string reqName = reqTemplate ? reqTemplate->Name1 : "<missing>";
            std::string role = ClassifyWotlkRequiredItemRole(reqTemplate);
            reqs.emplace_back(reqId, reqCount, reqName, role);
            ++costRowsDiscovered;
            ++roleCounts[role];
        }

        bool hasToken = false;
        bool hasPriorArmor = false;
        bool hasEmblem = false;
        bool hasUnknown = false;
        for (auto const& req : reqs)
        {
            std::string const& role = std::get<3>(req);
            hasToken = hasToken || role == "token";
            hasPriorArmor = hasPriorArmor || role == "prior_armor";
            hasEmblem = hasEmblem || role == "emblem_like";
            hasUnknown = hasUnknown || role == "unknown";
        }

        std::string structure = "unknown_structure";
        if (reqs.empty())
            structure = "no_required_items";
        else if (reqs.size() == 1 && hasToken && std::get<1>(reqs.front()) == 1)
            structure = "single_token_chain";
        else if (reqs.size() == 1 && hasEmblem)
            structure = "single_emblem_like";
        else if (hasToken && hasPriorArmor)
            structure = "token_plus_prior_armor_upgrade";
        else if (reqs.size() > 1)
            structure = "multi_item_mixed";
        else if (hasUnknown)
            structure = "single_unknown";
        ++structureCounts[structure];

        rewardsDiscovered.insert(rewardItemId);
        if (rewardExamples.size() < 12)
            rewardExamples.push_back(Acore::StringFormat("vendor {} reward {} ({}) extcost {} structure {}", vendorEntry, rewardItemId, rewardTemplate->Name1, extendedCostId, structure));

        if (_discoveryWriteDb)
        {
            EnsureWotlkDiscoveryTables();
            std::string tier;
            std::string raid;
            inferTierRaidFromReward(rewardTemplate->Name1, tier, raid);
            std::string notes = Acore::StringFormat("WOTLK stage discovery via extcost {}", extendedCostId);

            WorldDatabase.DirectExecute(
                "INSERT INTO `bot_token_exchanger_wotlk_map` "
                "(`reward_item_id`, `reward_name`, `inventory_type`, `allowable_class`, `vendor_entry`, `extended_cost_id`, `source_expansion`, `source_tier`, `source_raid`, `confidence`, `status`, `notes`) "
                "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE "
                "`reward_name`=VALUES(`reward_name`), "
                "`inventory_type`=VALUES(`inventory_type`), "
                "`allowable_class`=VALUES(`allowable_class`), "
                "`vendor_entry`=VALUES(`vendor_entry`), "
                "`extended_cost_id`=VALUES(`extended_cost_id`), "
                "`source_expansion`=VALUES(`source_expansion`), "
                "`source_tier`=VALUES(`source_tier`), "
                "`source_raid`=VALUES(`source_raid`), "
                "`confidence`=VALUES(`confidence`), "
                "`status`=VALUES(`status`), "
                "`notes`=VALUES(`notes`)",
                rewardItemId,
                SqlQuote(rewardTemplate->Name1),
                rewardTemplate->InventoryType,
                static_cast<int32>(rewardTemplate->AllowableClass),
                vendorEntry,
                extendedCostId,
                SqlQuote("WOTLK"),
                SqlNullable(tier),
                SqlNullable(raid),
                SqlQuote("runtime_dbc_inspected"),
                SqlQuote("staged"),
                SqlNullable(notes));
            ++stagedRewards;

            for (auto const& req : reqs)
            {
                WorldDatabase.DirectExecute(
                    "INSERT INTO `bot_token_exchanger_wotlk_cost` "
                    "(`reward_item_id`, `required_item_id`, `required_item_name`, `required_count`, `required_item_role`) "
                    "VALUES ({}, {}, {}, {}, {}) "
                    "ON DUPLICATE KEY UPDATE "
                    "`required_item_name`=VALUES(`required_item_name`), "
                    "`required_count`=VALUES(`required_count`), "
                    "`required_item_role`=VALUES(`required_item_role`)",
                    rewardItemId,
                    std::get<0>(req),
                    SqlQuote(std::get<2>(req)),
                    std::get<1>(req),
                    SqlQuote(std::get<3>(req)));
                ++stagedCosts;
            }
        }
    } while (vendorResult->NextRow());

    std::vector<uint32> vendorList(vendorsScanned.begin(), vendorsScanned.end());
    std::sort(vendorList.begin(), vendorList.end());
    std::ostringstream vendors;
    for (size_t i = 0; i < vendorList.size(); ++i)
    {
        if (i)
            vendors << ", ";
        vendors << vendorList[i];
    }

    handler->PSendSysMessage("WOTLK stage discovery write-to-DB: {}", _discoveryWriteDb ? "enabled" : "disabled (log-only)");
    handler->PSendSysMessage("WOTLK stage summary: vendors scanned {}, reward rows inspected {}, rewards discovered {}, cost rows discovered {}, staged reward rows {}, staged cost rows {}",
        static_cast<uint32>(vendorList.size()), inspectedRows, static_cast<uint32>(rewardsDiscovered.size()), costRowsDiscovered, stagedRewards, stagedCosts);
    handler->PSendSysMessage("WOTLK stage vendor list: {}", vendors.str());

    if (!structureCounts.empty())
    {
        handler->SendSysMessage("WOTLK stage structures:");
        std::vector<std::pair<std::string, uint32>> rows(structureCounts.begin(), structureCounts.end());
        std::sort(rows.begin(), rows.end(), [](auto const& a, auto const& b) { return a.second > b.second; });
        for (auto const& [name, count] : rows)
            handler->PSendSysMessage("  {}: {}", name, count);
    }

    if (!roleCounts.empty())
    {
        handler->SendSysMessage("WOTLK required item roles:");
        std::vector<std::pair<std::string, uint32>> rows(roleCounts.begin(), roleCounts.end());
        std::sort(rows.begin(), rows.end(), [](auto const& a, auto const& b) { return a.second > b.second; });
        for (auto const& [name, count] : rows)
            handler->PSendSysMessage("  {}: {}", name, count);
    }

    if (!rewardExamples.empty())
    {
        handler->SendSysMessage("WOTLK stage representative rows (capped):");
        for (std::string const& line : rewardExamples)
            handler->PSendSysMessage("  {}", line);
    }
}

void BotTokenExchangerMgr::DiscoverTokenMappings(ChatHandler* handler, std::string const& sourceExpansion, bool narrowVendorsOnly, bool inspectMode)
{
    if (!handler)
        return;

    if (!_enabled)
    {
        handler->SendSysMessage("BotTokenExchanger is disabled.");
        return;
    }

    std::string vendorQuery;
    if (sourceExpansion == "TBC")
    {
        vendorQuery =
            "SELECT entry, slot, item, ExtendedCost "
            "FROM npc_vendor "
            "WHERE entry IN (20613, 20616, 21905, 21906, 23381, 25976) "
            "ORDER BY entry, slot ASC, item ASC";
    }
    else if (sourceExpansion == "WOTLK")
    {
        std::string narrowVendorClause;
        if (narrowVendorsOnly)
        {
            narrowVendorClause =
                "AND nv.entry IN (28992, 28995, 28997, 29523, 34252, 35496, 35497, 35498, 35500, 37688, "
                "37696, 37991, 37992, 37993, 37997, 37998, 37999, 38054, 38181, 38182, "
                "38283, 38284, 38316, 38840, 38841) ";
        }

        vendorQuery =
            "SELECT nv.entry, nv.slot, nv.item, nv.ExtendedCost "
            "FROM npc_vendor nv "
            "INNER JOIN item_template reward ON reward.entry = nv.item "
            "WHERE nv.ExtendedCost > 0 "
            "AND reward.class = 4 "
            "AND reward.InventoryType IN (1, 3, 5, 9, 10, 6, 7, 8, 20) "
            + narrowVendorClause +
            "ORDER BY nv.entry, nv.slot ASC, nv.item ASC";
    }
    else
    {
        handler->PSendSysMessage("Unknown discovery expansion '{}'.", sourceExpansion);
        return;
    }

    QueryResult vendorResult = WorldDatabase.Query(vendorQuery);

    if (!vendorResult)
    {
        handler->PSendSysMessage("No matching {} vendor rows found.", sourceExpansion);
        return;
    }

    std::unordered_map<std::string, uint32> skipByReason;
    std::unordered_map<std::string, uint32> skipSampleCountByReason;
    std::unordered_set<uint32> scannedVendors;
    std::vector<std::string> acceptedSamples;
    uint32 candidateCount = 0;
    uint32 skippedCount = 0;
    uint32 stagedCount = 0;
    uint32 inspectedRows = 0;

    auto formatReqItem = [&](uint32 reqItemId, uint32 reqCount) -> std::string
    {
        if (!reqItemId)
            return Acore::StringFormat("item=0 count={}", reqCount);

        ItemTemplate const* reqTemplate = sObjectMgr->GetItemTemplate(reqItemId);
        if (!reqTemplate)
            return Acore::StringFormat("item={} count={} name=<missing> class=<missing> inv=<missing>", reqItemId, reqCount);

        return Acore::StringFormat(
            "item={} count={} name='{}' class={} invtype={}",
            reqItemId,
            reqCount,
            reqTemplate->Name1,
            reqTemplate->Class,
            reqTemplate->InventoryType);
    };

    auto emitInspectSample = [&](std::string const& structure, uint32 vendorEntry, ItemTemplate const* rewardTemplate, uint32 rewardItemId, uint32 extendedCostId, ItemExtendedCostEntry const* costEntry, std::string const& skipReason)
    {
        if (!inspectMode || !rewardTemplate || !costEntry)
            return;

        uint32& emitted = skipSampleCountByReason[structure];
        if (emitted >= 5)
            return;
        ++emitted;

        std::string vendorName = "<unknown>";
        if (CreatureTemplate const* vendorTemplate = sObjectMgr->GetCreatureTemplate(vendorEntry))
            vendorName = vendorTemplate->Name;

        handler->PSendSysMessage("Inspect sample [{} #{}]: vendor {} ({}) reward {} ({}) extcost {} skip {}",
            structure,
            emitted,
            vendorEntry,
            vendorName,
            rewardItemId,
            rewardTemplate->Name1,
            extendedCostId,
            skipReason);
        handler->PSendSysMessage("  reqs: [0] {}; [1] {}; [2] {}; [3] {}; [4] {}",
            formatReqItem(costEntry->reqitem[0], costEntry->reqitemcount[0]),
            formatReqItem(costEntry->reqitem[1], costEntry->reqitemcount[1]),
            formatReqItem(costEntry->reqitem[2], costEntry->reqitemcount[2]),
            formatReqItem(costEntry->reqitem[3], costEntry->reqitemcount[3]),
            formatReqItem(costEntry->reqitem[4], costEntry->reqitemcount[4]));
        handler->PSendSysMessage("  currency: honor={} arena={} rating={}",
            costEntry->reqhonorpoints,
            costEntry->reqarenapoints,
            costEntry->reqpersonalarenarating);
    };

    auto recordSkip = [&](std::string const& reason, std::string const& detail)
    {
        ++skippedCount;
        ++skipByReason[reason];
        LOG_INFO("bot_token_exchanger", "{}", detail);
        if (_debug && !inspectMode)
            handler->PSendSysMessage("{}", detail);
    };

    do
    {
        Field* fields = vendorResult->Fetch();
        uint32 vendorEntry = fields[0].Get<uint32>();
        uint32 slot = fields[1].Get<uint32>();
        uint32 rewardItemId = fields[2].Get<uint32>();
        uint32 extendedCostId = fields[3].Get<uint32>();

        ++inspectedRows;
        scannedVendors.insert(vendorEntry);

        ItemTemplate const* rewardTemplate = sObjectMgr->GetItemTemplate(rewardItemId);
        if (!rewardTemplate)
        {
            std::string message = Acore::StringFormat("skip vendor {} slot {} item {}: reward item template missing", vendorEntry, slot, rewardItemId);
            recordSkip("reward template missing", message);
            continue;
        }

        if (sourceExpansion == "TBC" && !IsKnownTbcVendor(vendorEntry))
        {
            std::string message = Acore::StringFormat("skip vendor {} item {} ({}) : vendor is not in the known TBC discovery list", vendorEntry, rewardItemId, rewardTemplate->Name1);
            recordSkip("vendor not in known TBC list", message);
            continue;
        }

        if (sourceExpansion == "WOTLK" && narrowVendorsOnly && !IsKnownWotlkNarrowVendor(vendorEntry))
        {
            std::string message = Acore::StringFormat("skip vendor {} item {} ({}): vendor is not in the verified WOTLK narrow discovery list", vendorEntry, rewardItemId, rewardTemplate->Name1);
            recordSkip("vendor not in WOTLK narrow list", message);
            continue;
        }

        ItemExtendedCostEntry const* costEntry = sItemExtendedCostStore.LookupEntry(extendedCostId);
        if (!costEntry)
        {
            std::string message = Acore::StringFormat("skip vendor {} item {} ({}): no runtime ItemExtendedCostEntry for extended cost {}", vendorEntry, rewardItemId, rewardTemplate->Name1, extendedCostId);
            recordSkip("runtime ItemExtendedCost missing", message);
            continue;
        }

        if (costEntry->reqhonorpoints || costEntry->reqarenapoints || costEntry->reqpersonalarenarating)
        {
            std::string message = Acore::StringFormat(
                "skip vendor {} item {} ({}): extended cost {} has unsupported non-item requirements (honor {}, arena {}, rating {})",
                vendorEntry,
                rewardItemId,
                rewardTemplate->Name1,
                extendedCostId,
                costEntry->reqhonorpoints,
                costEntry->reqarenapoints,
                costEntry->reqpersonalarenarating);
            recordSkip("unsupported non-item requirements", message);
            emitInspectSample("unsupported non-item requirements", vendorEntry, rewardTemplate, rewardItemId, extendedCostId, costEntry, "currency/rating present");
            continue;
        }

        uint32 tokenItemId = 0;
        uint32 tokenCount = 0;
        uint32 nonZeroRequirements = 0;
        uint32 tokenLikeRequirements = 0;
        std::string skipReason;

        for (uint8 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
        {
            uint32 reqItemId = costEntry->reqitem[i];
            uint32 reqCount = costEntry->reqitemcount[i];

            if (!reqItemId)
                continue;

            ++nonZeroRequirements;

            ItemTemplate const* reqTemplate = sObjectMgr->GetItemTemplate(reqItemId);
            if (!reqTemplate)
            {
                skipReason = Acore::StringFormat("required item {} is missing from item_template", reqItemId);
                break;
            }

            if (reqCount != 1)
            {
                skipReason = Acore::StringFormat("required item {} ({}) has unsupported count {}", reqItemId, reqTemplate->Name1, reqCount);
                break;
            }

            bool const tierCandidate = IsTierTokenCandidate(reqTemplate);
            bool const acceptedTokenForExpansion = (sourceExpansion == "TBC") ? tierCandidate : IsLikelyWotlkTierTokenCandidate(reqTemplate);
            if (!tierCandidate)
                continue;

            ++tokenLikeRequirements;
            if (!acceptedTokenForExpansion)
                continue;

            if (tokenItemId)
            {
                skipReason = Acore::StringFormat("multiple token-like requirements found, including item {} ({})", reqItemId, reqTemplate->Name1);
                break;
            }

            tokenItemId = reqItemId;
            tokenCount = reqCount;
        }

        if (!skipReason.empty())
        {
            std::string message = Acore::StringFormat(
                "skip vendor {} slot {} item {} ({}): {}",
                vendorEntry,
                slot,
                rewardItemId,
                rewardTemplate->Name1,
                skipReason);
            recordSkip("invalid required-item structure", message);
            std::string structure = "invalid required-item structure";
            if (ContainsCaseInsensitive(skipReason, "unsupported count"))
                structure = "item requirement count > 1";
            else if (ContainsCaseInsensitive(skipReason, "multiple token-like requirements"))
                structure = "multiple token-like requirements";
            else if (ContainsCaseInsensitive(skipReason, "missing from item_template"))
                structure = "required item missing from item_template";
            emitInspectSample(structure, vendorEntry, rewardTemplate, rewardItemId, extendedCostId, costEntry, skipReason);
            continue;
        }

        if (nonZeroRequirements != 1 || tokenLikeRequirements != 1 || !tokenItemId || tokenCount != 1)
        {
            std::string message = Acore::StringFormat(
                "skip vendor {} slot {} item {} ({}): expected exactly one supported token requirement with count 1, found {} non-zero requirements and {} token-like requirements",
                vendorEntry,
                slot,
                rewardItemId,
                rewardTemplate->Name1,
                nonZeroRequirements,
                tokenLikeRequirements);
            std::string structure = "not exactly one supported token requirement";
            if (nonZeroRequirements == 0)
                structure = "zero required items";
            else if (nonZeroRequirements > 1)
                structure = "two or more item requirements";
            else if (nonZeroRequirements == 1 && tokenLikeRequirements == 0)
                structure = "one required item but not supported WotLK token";
            recordSkip(structure, message);
            emitInspectSample(structure, vendorEntry, rewardTemplate, rewardItemId, extendedCostId, costEntry, message);
            continue;
        }

        if (rewardTemplate->Class != ITEM_CLASS_ARMOR)
        {
            std::string message = Acore::StringFormat("skip vendor {} item {} ({}): reward is not armor class", vendorEntry, rewardItemId, rewardTemplate->Name1);
            recordSkip("reward not armor", message);
            continue;
        }

        if (!IsValidRewardInventoryType(rewardTemplate->InventoryType))
        {
            std::string message = Acore::StringFormat(
                "skip vendor {} item {} ({}): invalid armor inventory type {}",
                vendorEntry,
                rewardItemId,
                rewardTemplate->Name1,
                rewardTemplate->InventoryType);
            recordSkip("invalid armor inventory type", message);
            continue;
        }

        if (!rewardTemplate->AllowableClass)
        {
            std::string message = Acore::StringFormat("skip vendor {} item {} ({}): reward has no allowable class restriction", vendorEntry, rewardItemId, rewardTemplate->Name1);
            recordSkip("reward has no allowable class", message);
            continue;
        }

        ItemTemplate const* tokenTemplate = sObjectMgr->GetItemTemplate(tokenItemId);
        if (!tokenTemplate)
        {
            std::string message = Acore::StringFormat("skip vendor {} item {} ({}): token item {} missing from item_template", vendorEntry, rewardItemId, rewardTemplate->Name1, tokenItemId);
            recordSkip("token template missing", message);
            continue;
        }

        ++candidateCount;

        std::string sourceTier;
        std::string sourceRaid;
        if (sourceExpansion == "WOTLK")
        {
            if (ContainsCaseInsensitive(tokenTemplate->Name1, "Sanctification"))
            {
                sourceTier = "T10";
                sourceRaid = "Icecrown Citadel";
            }
            else if (ContainsCaseInsensitive(tokenTemplate->Name1, "Triumph"))
            {
                sourceTier = "T9";
                sourceRaid = "Trial of the Crusader";
            }
        }

        std::string notes = Acore::StringFormat("vendor {} discovered from runtime DBC extended cost {}", vendorEntry, extendedCostId);
        std::string candidateMessage = Acore::StringFormat(
            "candidate [{}] vendor {} slot {} reward {} ({}) -> token {} ({}) via extended cost {} req_count {} invtype {} class {}",
            sourceExpansion,
            vendorEntry,
            slot,
            rewardItemId,
            rewardTemplate->Name1,
            tokenItemId,
            tokenTemplate->Name1,
            extendedCostId,
            tokenCount,
            rewardTemplate->InventoryType,
            rewardTemplate->AllowableClass);

        LOG_INFO("bot_token_exchanger", "{}", candidateMessage);
        if (_debug)
            handler->PSendSysMessage("{}", candidateMessage);
        if (acceptedSamples.size() < 10)
            acceptedSamples.push_back(candidateMessage);

        if (TryStageMapping(tokenItemId, rewardItemId, rewardTemplate->InventoryType, static_cast<int32>(rewardTemplate->AllowableClass), extendedCostId, vendorEntry, sourceExpansion, sourceTier, sourceRaid, notes))
            ++stagedCount;
    } while (vendorResult->NextRow());

    std::vector<uint32> vendorsSorted(scannedVendors.begin(), scannedVendors.end());
    std::sort(vendorsSorted.begin(), vendorsSorted.end());
    std::ostringstream vendorList;
    for (size_t i = 0; i < vendorsSorted.size(); ++i)
    {
        if (i != 0)
            vendorList << ", ";
        vendorList << vendorsSorted[i];
    }

    std::vector<std::pair<std::string, uint32>> skipSummary(skipByReason.begin(), skipByReason.end());
    std::sort(skipSummary.begin(), skipSummary.end(), [](auto const& left, auto const& right)
    {
        if (left.second != right.second)
            return left.second > right.second;
        return left.first < right.first;
    });

    if (_discoveryWriteDb)
    {
        EnsureDiscoveryTable();
        handler->PSendSysMessage("Discovery staging table is enabled.");
    }
    else
    {
        handler->PSendSysMessage("Discovery write-to-DB is disabled; log-only mode was used.");
    }

    handler->PSendSysMessage("Discovery [{}{}{}] complete: vendors scanned {}, reward rows inspected {}, accepted mappings {}, skipped mappings {}, staged {}.",
        sourceExpansion,
        narrowVendorsOnly ? " narrow" : "",
        inspectMode ? " inspect" : "",
        static_cast<uint32>(vendorsSorted.size()),
        inspectedRows,
        candidateCount,
        skippedCount,
        stagedCount);
    if (!vendorsSorted.empty())
        handler->PSendSysMessage("Discovery vendor list: {}", vendorList.str());

    if (!skipSummary.empty())
    {
        handler->SendSysMessage("Skipped mappings by reason:");
        for (auto const& [reason, count] : skipSummary)
            handler->PSendSysMessage("  {}: {}", reason, count);
    }

    if (!acceptedSamples.empty())
    {
        handler->SendSysMessage("Representative accepted mappings (capped at 10):");
        for (std::string const& sample : acceptedSamples)
            handler->PSendSysMessage("  {}", sample);
    }

    std::string unresolved = "none";
    if (!skipSummary.empty())
        unresolved = skipSummary.front().first;
    handler->PSendSysMessage("Unresolved category focus: {}", unresolved);
}
