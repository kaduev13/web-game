#pragma once
#include<iostream>
#include<String>
#include<vector>
#include<Map>
#include "DBConnector.hpp"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/ParseHandler.h"
#include "User.hpp"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/WebSocket.h"
#include "WSConnection.hpp"
#include "CommonObjects.hpp"

using namespace std;
using namespace Poco::JSON;

class Matchmaking
{
public:
    static Response GetGames();
    static Response JoinToGame(int gameId, int playerId);
    static Response CreateGame(Game game, int playerId);
    static Response StartGame(int playerId);
    static Response LeaveGame(int playerId);
    static Response GetLobby(int playerId);

    static Response HandleAction(string text);

    static Response onJoinGame(Poco::JSON::Array::Ptr params);
    static Response onCreateGame(Poco::JSON::Array::Ptr params);
    static Response onLeaveGame(Poco::JSON::Array::Ptr params);
    static Response onGetLobby(Poco::JSON::Array::Ptr params);

    static vector<string> getPlayersList(int game_id);
    static void DeleteGame(int gameId);
    static void onCloseConnection(int playerId);
    static bool isInGame(int player_id);
    static void CreateConnection(User user, WebSocket& ws);
};

static class Actions
{
public:
    static enum ACTIONS
    {
        CREATE_GAME,
        GET_GAMES,
        LEAVE_GAME,
        JOIN_TO_GAME,
        START_GAME,
        GET_LOBBY
    };

    static const int getActionByName(const string name)
    {
        return actions.at(name);
    }
    static const string getActionText(const int id)
    {
        for (auto it = actions.begin(); it != actions.end(); ++it)
        {
            if (it->second == id)
            {
                return it->first;
            }
        }
        return NULL;
    }

private:
    static const std::map<string, ACTIONS> actions;
};


static void MatchmakingActionHandler(string& action, ostringstream& stream)
{
    Response r = Matchmaking::HandleAction(action);
    r.toJson().stringify(stream);
}


class MatchmakingException : std::exception
{
private:
    string msg;
public:
    MatchmakingException(const string m) :msg(m) {}
    virtual const char* what() const throw()
    {
        return msg.c_str();
    };
};
