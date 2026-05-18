#include "BotTokenExchangerMgr.h"

#include "Chat.h"
#include "CommandScript.h"
#include "RBAC.h"

#include <algorithm>
#include <charconv>
#include <string_view>

using namespace Acore::ChatCommands;

class bot_token_exchanger_commandscript : public CommandScript
{
public:
    bot_token_exchanger_commandscript() : CommandScript("bot_token_exchanger_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable discoverWotlkCommandTable =
        {
            { "", HandleTokenExDiscoverWotlkCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "narrow", HandleTokenExDiscoverWotlkNarrowCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "inspect", HandleTokenExDiscoverWotlkInspectCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "stage", HandleTokenExDiscoverWotlkStageCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable discoverCommandTable =
        {
            { "tbc", HandleTokenExDiscoverTbcCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "wotlk", discoverWotlkCommandTable }
        };

        static ChatCommandTable resolveWotlkCommandTable =
        {
            { "selected", HandleTokenExResolveWotlkSelectedCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "item", HandleTokenExResolveWotlkItemCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "bot", HandleTokenExResolveWotlkBotCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "botitem", HandleTokenExResolveWotlkBotItemCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable validateWotlkSingleCommandTable =
        {
            { "", HandleTokenExValidateWotlkSingleCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "verbose", HandleTokenExValidateWotlkSingleVerboseCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "unresolved", HandleTokenExValidateWotlkSingleUnresolvedCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable validateWotlkCommandTable =
        {
            { "single", validateWotlkSingleCommandTable }
        };

        static ChatCommandTable validateTbcCommandTable =
        {
            { "", HandleTokenExValidateTbcCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "verbose", HandleTokenExValidateTbcVerboseCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "unresolved", HandleTokenExValidateTbcUnresolvedCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable validateCommandTable =
        {
            { "tbc", validateTbcCommandTable },
            { "wotlk", validateWotlkCommandTable }
        };

        static ChatCommandTable resolveCommandTable =
        {
            { "wotlk", resolveWotlkCommandTable },
            { "selected", HandleTokenExResolveSelectedCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "item", HandleTokenExResolveItemCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "bot", HandleTokenExResolveBotCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable exchangeWotlkCommandTable =
        {
            { "selected", HandleTokenExExchangeWotlkSelectedCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "bot", HandleTokenExExchangeWotlkBotCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable exchangeCommandTable =
        {
            { "wotlk", exchangeWotlkCommandTable },
            { "selected", HandleTokenExExchangeSelectedCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "group", HandleTokenExExchangeGroupCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "bot", HandleTokenExExchangeBotCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable roleCommandTable =
        {
            { "selected", HandleTokenExRoleSelectedCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "bot", HandleTokenExRoleBotCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable preferSelectedCommandTable =
        {
            { "tank", HandleTokenExPreferTankCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "healer", HandleTokenExPreferHealerCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "melee_dps", HandleTokenExPreferMeleeDpsCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "caster_dps", HandleTokenExPreferCasterDpsCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "ranged_dps", HandleTokenExPreferRangedDpsCommand, SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable preferCommandTable =
        {
            { "selected", preferSelectedCommandTable }
        };

        static ChatCommandTable tokenExCommandTable =
        {
            { "status", HandleTokenExStatusCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "discover", discoverCommandTable },
            { "resolve", resolveCommandTable },
            { "exchange", exchangeCommandTable },
            { "validate", validateCommandTable },
            { "role", roleCommandTable },
            { "prefer", preferCommandTable }
        };

        static ChatCommandTable commandTable =
        {
            { "tokenex", tokenExCommandTable }
        };

        return commandTable;
    }

    static bool HandleTokenExDiscoverTbcCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.DiscoverTbcTokenMappings(handler);
        return true;
    }

    static bool HandleTokenExDiscoverWotlkCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.DiscoverWotlkTokenMappings(handler);
        return true;
    }

    static bool HandleTokenExDiscoverWotlkNarrowCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.DiscoverWotlkTokenMappingsNarrow(handler);
        return true;
    }

    static bool HandleTokenExDiscoverWotlkInspectCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.DiscoverWotlkTokenMappingsInspect(handler);
        return true;
    }

    static bool HandleTokenExDiscoverWotlkStageCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.DiscoverWotlkTokenMappingsStage(handler);
        return true;
    }

    static bool HandleTokenExStatusCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ShowStatus(handler);
        return true;
    }

    static bool HandleTokenExResolveSelectedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ResolveSelectedTokenRewards(handler);
        return true;
    }

    static bool HandleTokenExResolveWotlkSelectedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ResolveWotlkSelectedTokenRewards(handler);
        return true;
    }

    static bool HandleTokenExResolveItemCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve item <tokenItemId>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex resolve item <tokenItemId>");
            return false;
        }

        uint32 tokenItemId = 0;
        auto const* begin = input.data();
        auto const* end = input.data() + input.size();
        auto result = std::from_chars(begin, end, tokenItemId);
        if (result.ec != std::errc() || result.ptr == begin)
        {
            handler->SendSysMessage("Invalid token item id.");
            return false;
        }

        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage("Select a Playerbot first.");
            return false;
        }

        if (!player->GetSession() || !player->GetSession()->IsBot())
        {
            handler->SendSysMessage("Selected player is not a Playerbot.");
            return false;
        }

        sBotTokenExchangerMgr.ResolveTokenItem(handler, player, tokenItemId);
        return true;
    }

    static bool HandleTokenExResolveWotlkItemCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk item <tokenItemId>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk item <tokenItemId>");
            return false;
        }

        uint32 tokenItemId = 0;
        auto const* begin = input.data();
        auto const* end = input.data() + input.size();
        auto result = std::from_chars(begin, end, tokenItemId);
        if (result.ec != std::errc() || result.ptr == begin)
        {
            handler->SendSysMessage("Invalid token item id.");
            return false;
        }

        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage("Select a Playerbot first.");
            return false;
        }

        if (!player->GetSession() || !player->GetSession()->IsBot())
        {
            handler->SendSysMessage("Selected player is not a Playerbot.");
            return false;
        }

        sBotTokenExchangerMgr.ResolveWotlkTokenItem(handler, player, tokenItemId);
        return true;
    }

    static bool HandleTokenExResolveWotlkBotCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk bot <botName>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk bot <botName>");
            return false;
        }

        sBotTokenExchangerMgr.ResolveWotlkBotTokenRewards(handler, std::string(input));
        return true;
    }

    static bool HandleTokenExResolveWotlkBotItemCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk botitem <botName> <tokenItemId>");
            return false;
        }
        input.remove_prefix(first);

        size_t split = input.find_first_of(" \t");
        if (split == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk botitem <botName> <tokenItemId>");
            return false;
        }

        std::string botName(input.substr(0, split));
        std::string_view rest = input.substr(split);
        size_t second = rest.find_first_not_of(" \t");
        if (second == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve wotlk botitem <botName> <tokenItemId>");
            return false;
        }
        rest.remove_prefix(second);

        uint32 tokenItemId = 0;
        auto result = std::from_chars(rest.data(), rest.data() + rest.size(), tokenItemId);
        if (result.ec != std::errc() || result.ptr == rest.data())
        {
            handler->SendSysMessage("Invalid token item id.");
            return false;
        }

        sBotTokenExchangerMgr.ResolveWotlkBotTokenItem(handler, botName, tokenItemId);
        return true;
    }

    static bool HandleTokenExValidateWotlkSingleCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ValidateWotlkSingleTokenChain(handler, false);
        return true;
    }

    static bool HandleTokenExValidateWotlkSingleVerboseCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ValidateWotlkSingleTokenChain(handler, true);
        return true;
    }

    static bool HandleTokenExValidateWotlkSingleUnresolvedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ValidateWotlkSingleTokenChainUnresolved(handler);
        return true;
    }

    static bool HandleTokenExValidateTbcCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ValidateTbcTokenChain(handler, false);
        return true;
    }

    static bool HandleTokenExValidateTbcVerboseCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ValidateTbcTokenChain(handler, true);
        return true;
    }

    static bool HandleTokenExValidateTbcUnresolvedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ValidateTbcTokenChainUnresolved(handler);
        return true;
    }

    static bool HandleTokenExResolveBotCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex resolve bot <botName>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex resolve bot <botName>");
            return false;
        }

        sBotTokenExchangerMgr.ResolveBotTokenRewards(handler, std::string(input));
        return true;
    }

    static bool HandleTokenExExchangeSelectedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ExchangeSelectedTokens(handler);
        return true;
    }

    static bool HandleTokenExExchangeWotlkSelectedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ExchangeWotlkSelectedTokens(handler);
        return true;
    }

    static bool HandleTokenExExchangeGroupCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ExchangeGroupTokens(handler);
        return true;
    }

    static bool HandleTokenExExchangeBotCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex exchange bot <botName>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex exchange bot <botName>");
            return false;
        }

        sBotTokenExchangerMgr.ExchangeBotTokens(handler, std::string(input));
        return true;
    }

    static bool HandleTokenExExchangeWotlkBotCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex exchange wotlk bot <botName>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex exchange wotlk bot <botName>");
            return false;
        }

        sBotTokenExchangerMgr.ExchangeWotlkBotTokens(handler, std::string(input));
        return true;
    }

    static bool HandleTokenExRoleSelectedCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.ShowSelectedRole(handler);
        return true;
    }

    static bool HandleTokenExRoleBotCommand(ChatHandler* handler, char const* args)
    {
        if (!handler)
            return false;

        std::string_view input = args ? std::string_view(args) : std::string_view();
        size_t first = input.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .tokenex role bot <botName>");
            return false;
        }

        input.remove_prefix(first);
        if (input.empty())
        {
            handler->SendSysMessage("Usage: .tokenex role bot <botName>");
            return false;
        }

        sBotTokenExchangerMgr.ShowBotRole(handler, std::string(input));
        return true;
    }

    static bool HandleTokenExPreferTankCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.SetSelectedRolePreference(handler, "tank");
        return true;
    }

    static bool HandleTokenExPreferHealerCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.SetSelectedRolePreference(handler, "healer");
        return true;
    }

    static bool HandleTokenExPreferMeleeDpsCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.SetSelectedRolePreference(handler, "melee_dps");
        return true;
    }

    static bool HandleTokenExPreferCasterDpsCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.SetSelectedRolePreference(handler, "caster_dps");
        return true;
    }

    static bool HandleTokenExPreferRangedDpsCommand(ChatHandler* handler)
    {
        sBotTokenExchangerMgr.SetSelectedRolePreference(handler, "ranged_dps");
        return true;
    }
};

void AddSC_bot_token_exchanger_commandscript()
{
    new bot_token_exchanger_commandscript();
}
