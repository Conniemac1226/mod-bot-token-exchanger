#include "BotTokenExchangerMgr.h"

#include "DatabaseScript.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptDefines/WorldScript.h"

void AddSC_bot_token_exchanger_commandscript();

class BotTokenExchangerPlayerScript : public PlayerScript
{
public:
    BotTokenExchangerPlayerScript() : PlayerScript("BotTokenExchangerPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_STORE_NEW_ITEM,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGOUT
    })
    {
        sBotTokenExchangerMgr.LoadConfig();
    }

    void OnPlayerLogin(Player* player) override
    {
        sBotTokenExchangerMgr.HandlePlayerLogin(player);
    }

    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override
    {
        sBotTokenExchangerMgr.HandlePlayerStoreNewItem(player, item, count);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sBotTokenExchangerMgr.HandlePlayerUpdate(player, diff);
    }

    void OnPlayerLogout(Player* player) override
    {
        sBotTokenExchangerMgr.HandlePlayerLogout(player);
    }
};

class BotTokenExchangerDatabaseScript : public DatabaseScript
{
public:
    BotTokenExchangerDatabaseScript() : DatabaseScript("BotTokenExchangerDatabaseScript", { DATABASEHOOK_ON_AFTER_DATABASES_LOADED }) { }

    void OnAfterDatabasesLoaded(uint32 /*updateFlags*/) override
    {
        sBotTokenExchangerMgr.PreloadRuntimeCaches();
    }
};

class BotTokenExchangerWorldScript : public WorldScript
{
public:
    BotTokenExchangerWorldScript() : WorldScript("BotTokenExchangerWorldScript") { }

    void OnStartup() override
    {
        LOG_INFO("server", "BotTokenExchangerWorldScript::OnStartup invoked");
        sBotTokenExchangerMgr.PreloadRuntimeCaches();
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        sBotTokenExchangerMgr.PreloadRuntimeCaches();
    }
};

void Addmod_bot_token_exchangerScripts()
{
    new BotTokenExchangerPlayerScript();
    new BotTokenExchangerDatabaseScript();
    new BotTokenExchangerWorldScript();
    AddSC_bot_token_exchanger_commandscript();
}
