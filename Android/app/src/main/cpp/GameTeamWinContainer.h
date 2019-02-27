#ifndef GAME_TEAM_WIN_H_INCLUDED
#define GAME_TEAM_WIN_H_INCLUDED

struct TeamInformation {
    float fanduelSabrPredictor[9];
    int runs;
    std::string opponentKey;
    TeamInformation();
};

class GameTeamWinContainer {
    std::string currentYear;
    std::string currentDate;
    std::unordered_map<std::string, TeamInformation> teamToInfoMap;
    std::unordered_map<std::string, std::unordered_map<std::string, TeamInformation>> allDatesToTeamInfoMaps;

    std::string GetGamesRecordFilename();

public:
    GameTeamWinContainer();
    void nextDate(std::string newDate);
    void nextPlayer(std::vector<std::string> actualResultsLine, float sabrPredictor);
    void nextPlayer(struct PlayerData singlePlayerData, std::string opponentTeamCode);
    void runAnalysis();
    std::vector<std::string> getStringsFromTodaysDate();
};



#endif
