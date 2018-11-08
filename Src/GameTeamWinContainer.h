#ifndef GAME_TEAM_WIN_H_INCLUDED
#define GAME_TEAM_WIN_H_INCLUDED

struct TeamInformation {
    float fanduelSabrPredictor[9];
    int runs;
    std::string opponentKey;
    TeamInformation();
};

class GameTeamWinContainer {
    std::string currentDate;
    std::unordered_map<std::string, TeamInformation> teamToInfoMap;
    std::unordered_map<std::string, std::unordered_map<std::string, TeamInformation>> allDatesToTeamInfoMaps;

public:
    void nextDate(std::string newDate);
    void nextPlayer(std::vector<std::string> actualResultsLine, float sabrPredictor);
};

#endif
