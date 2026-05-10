#ifndef MOD_BOT_TOKEN_EXCHANGER_MGR_H
#define MOD_BOT_TOKEN_EXCHANGER_MGR_H

#include "Define.h"
#include "Group.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Item;
class ItemTemplate;
class Player;

class BotTokenExchangerMgr
{
public:
    static BotTokenExchangerMgr& instance();

    void LoadConfig();
    void PreloadRuntimeCaches();
    void HandlePlayerStoreNewItem(Player* player, Item* item, uint32 count);
    void HandlePlayerLogin(Player* player);
    void HandlePlayerUpdate(Player* player, uint32 diff);
    void HandlePlayerLogout(Player* player);
    void DiscoverTbcTokenMappings(class ChatHandler* handler);
    void ResolveSelectedTokenRewards(class ChatHandler* handler);
    void ResolveBotTokenRewards(class ChatHandler* handler, std::string const& botName);
    void ResolveTokenItem(class ChatHandler* handler, Player* player, uint32 tokenItemId);
    void ShowStatus(class ChatHandler* handler);
    void ExchangeSelectedTokens(class ChatHandler* handler);
    void ExchangeGroupTokens(class ChatHandler* handler);
    void ExchangeBotTokens(class ChatHandler* handler, std::string const& botName);
    void ShowBotRole(class ChatHandler* handler, std::string const& botName);
    void ShowSelectedRole(class ChatHandler* handler);
    void SetSelectedRolePreference(class ChatHandler* handler, std::string const& role);

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsDebugEnabled() const { return _debug; }
    [[nodiscard]] bool OnlyPlayerbots() const { return _onlyPlayerbots; }
    [[nodiscard]] uint32 GetExchangeDelayMs() const { return _exchangeDelayMs; }
    [[nodiscard]] bool AnnounceToBotOwner() const { return _announceToBotOwner; }
    [[nodiscard]] bool DiscoveryWriteDb() const { return _discoveryWriteDb; }
    [[nodiscard]] bool ResolveOnly() const { return _resolveOnly; }
    [[nodiscard]] bool DryRun() const { return _dryRun; }
    [[nodiscard]] bool ExchangeEnable() const { return _exchangeEnable; }
    [[nodiscard]] bool AllowDebugTargetCommand() const { return _allowDebugTargetCommand; }
    [[nodiscard]] bool AutoExchangeEnable() const { return _autoExchangeEnable; }
    [[nodiscard]] bool PlayerbotLootPassEnable() const { return _playerbotLootPassEnable; }
    [[nodiscard]] uint32 AutoExchangeDelayMs() const { return _autoExchangeDelayMs; }
    [[nodiscard]] bool AutoExchangeOnLoot() const { return _autoExchangeOnLoot; }
    [[nodiscard]] bool AutoExchangeOnLogin() const { return _autoExchangeOnLogin; }
    [[nodiscard]] uint32 AutoExchangeMaxPerBotPerPass() const { return _autoExchangeMaxPerBotPerPass; }
    [[nodiscard]] size_t GetLoadedResolverMappingCount() const
    {
        size_t count = 0;
        for (auto const& [tokenId, entries] : _resolverEntriesByToken)
            count += entries.size();
        return count;
    }

    [[nodiscard]] size_t GetAutoExchangeQueueSize() const { return _autoExchangeQueueByBotGuid.size(); }

private:
    struct ResolverEntry
    {
        uint32 tokenItemId = 0;
        uint32 rewardItemId = 0;
        std::string tokenName;
        std::string rewardName;
        uint32 inventoryType = 0;
        int32 allowableClass = 0;
        uint32 extendedCostId = 0;
        uint32 vendorEntry = 0;
        std::string sourceTier;
        std::string sourceRaid;
        std::string confidence;
        std::string status;
        std::string notes;
    };

    struct FilteredCandidate
    {
        ResolverEntry const* entry = nullptr;
        ItemTemplate const* rewardTemplate = nullptr;
    };

    struct RoleResolution
    {
        std::string detectedRole;
        std::string detectedSpec;
        bool detectedReliable = false;
        std::string manualRole;
        bool manualConfigured = false;
        std::string decisionSource;
    };

    struct ExchangeScopeGuard
    {
        explicit ExchangeScopeGuard(BotTokenExchangerMgr& mgr) : _mgr(mgr)
        {
            _mgr._exchangeActive = true;
        }

        ~ExchangeScopeGuard()
        {
            _mgr._exchangeActive = false;
        }

    private:
        BotTokenExchangerMgr& _mgr;
    };

    struct AutoExchangeQueueEntry
    {
        uint64 botGuid = 0;
        std::string botName;
        std::unordered_set<uint32> tokenItemIds;
        uint64 scheduledTimeMs = 0;
    };

    BotTokenExchangerMgr() = default;

    void EnsureDiscoveryTable();
    void EnsurePreferenceTable();
    void UpdatePlayerbotLootPassCallback();
    bool HandlePlayerbotBeforeLootRoll(Player* bot, ItemTemplate const* itemTemplate, RollVote& rollVote);
    Player* ResolveOnlineBotByName(class ChatHandler* handler, std::string const& botName, char const* action);
    void ResolveTokenRewardsForPlayer(class ChatHandler* handler, Player* player, char const* label);
    bool TryStageMapping(uint32 tokenItemId, uint32 rewardItemId, uint32 inventoryType, int32 allowableClass, uint32 extendedCostId, uint32 vendorEntry, std::string const& sourceTier, std::string const& sourceRaid, std::string const& notes);
    void LoadResolverMappings();
    void LoadPreferenceMappings();
    void QueueAutoExchange(Player* player, uint32 tokenItemId, bool queueAllStagedTokens, bool logWhenEmpty, char const* reason);
    void ProcessAutoExchangeQueue(Player* player);
    std::vector<ResolverEntry> const* GetResolverEntries(uint32 tokenItemId);
    std::vector<ResolverEntry> const* GetResolverEntriesCached(uint32 tokenItemId) const;
    bool BuildFilteredCandidatesFromEntries(Player* player, uint32 tokenItemId, std::vector<ResolverEntry> const& entries, std::vector<FilteredCandidate>& filtered, std::vector<std::string>& skipReasons, std::vector<std::string>& notes) const;
    bool BuildFilteredCandidates(Player* player, uint32 tokenItemId, std::vector<FilteredCandidate>& filtered, std::vector<std::string>& skipReasons, std::vector<std::string>& notes);
    bool BuildHybridCandidates(Player* player, std::vector<FilteredCandidate> const& input, std::vector<FilteredCandidate>& output, std::vector<std::string>& notes, std::vector<std::string>& skipReasons, RoleResolution const& roleResolution) const;
    RoleResolution GetRoleResolution(Player* player);
    RoleResolution GetLootPassRoleResolution(Player* player) const;
    bool TryExchangeToken(Player* player, ResolverEntry const& entry, ItemTemplate const* tokenTemplate, ItemTemplate const* rewardTemplate, bool dryRun, std::string& transactionLog);
    bool TryResolveUniqueCandidate(Player* player, uint32 tokenItemId, FilteredCandidate& resolvedCandidate) const;
    bool HasRewardItemEquipped(Player* player, uint32 itemId) const;
    bool HasRewardItemInBags(Player* player, uint32 itemId) const;
    bool HasRewardItemInEquipmentOrBags(Player* player, uint32 itemId) const;
    bool HasStagedResolverEntries(uint32 tokenItemId) const;
    void ExchangePlayerTokens(Player* player, char const* label, uint32 maxPerBotPerPass = 0, class ChatHandler* handler = nullptr);
    void DescribeRoleResolution(class ChatHandler* handler, Player* player);
    static bool IsKnownTbcVendor(uint32 vendorEntry);
    static bool IsTierTokenCandidate(ItemTemplate const* item);
    static bool IsValidRewardInventoryType(uint32 inventoryType);
    static bool IsFactionPreferred(ItemTemplate const* item, uint32 teamId);
    static bool IsFactionRewardMatch(ItemTemplate const* item, uint32 teamId);
    static bool IsHybridClass(uint32 classId);
    static bool UsesRoleFiltering(uint32 classId);
    static std::string NormalizeRole(std::string role);
    static bool IsRoleAllowedForClass(uint32 classId, std::string const& role);
    static std::string RoleFromBotRoles(uint32 classId, uint32 roles);
    static std::string ClassifyRewardRole(ItemTemplate const* item);
    static std::string ClassifyRewardRole(Player const* player, ItemTemplate const* item, RoleResolution const& roleResolution);
    static bool RoleMatchesHint(std::string const& hint, std::string const& role);
    void LogHybridResolutionDecision(char const* phase, Player* player, uint32 tokenItemId, ItemTemplate const* tokenTemplate, std::vector<FilteredCandidate> const& input, std::vector<FilteredCandidate> const& output, RoleResolution const& roleResolution, std::vector<std::string> const& skipReasons, std::vector<std::string> const& notes) const;
    static std::string FormatResolverCandidate(ResolverEntry const& entry);
    static std::string EscapeSql(std::string value);
    static std::string SqlQuote(std::string value);
    static std::string SqlNullable(std::string const& value);

    bool _enabled = true;
    bool _debug = false;
    bool _onlyPlayerbots = true;
    uint32 _exchangeDelayMs = 1000;
    bool _announceToBotOwner = false;
    bool _discoveryWriteDb = false;
    bool _resolveOnly = true;
    bool _dryRun = true;
    bool _exchangeEnable = false;
    bool _allowDebugTargetCommand = false;
    bool _playerbotLootPassEnable = false;
    bool _autoExchangeEnable = false;
    uint32 _autoExchangeDelayMs = 1500;
    bool _autoExchangeOnLoot = true;
    bool _autoExchangeOnLogin = false;
    uint32 _autoExchangeMaxPerBotPerPass = 1;
    bool _discoveryTableEnsured = false;
    bool _preferenceTableEnsured = false;
    bool _resolverLoaded = false;
    bool _preferenceLoaded = false;
    bool _exchangeActive = false;
    std::unordered_map<uint32, std::vector<ResolverEntry>> _resolverEntriesByToken;
    std::unordered_map<uint64, RoleResolution> _preferenceByBotGuid;
    std::unordered_map<uint64, AutoExchangeQueueEntry> _autoExchangeQueueByBotGuid;
};

#define sBotTokenExchangerMgr BotTokenExchangerMgr::instance()

#endif
