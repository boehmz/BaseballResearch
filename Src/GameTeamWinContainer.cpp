#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
#include <thread>
#include "float.h"

#include "GameTeamWinContainer.h"
#include "SharedGlobals.h"
#include "StringUtils.h"

using namespace std;
TeamInformation::TeamInformation() {
    opponentKey = "";
    runs = -1;
    for (int i = 0; i < 9; ++i) {
        fanduelSabrPredictor[i] = -1;
    }
}

void GameTeamWinContainer::nextDate(std::string newDate) {
    if (currentDate != "") {
        allDatesToTeamInfoMaps.insert({currentDate,teamToInfoMap});
    }
    teamToInfoMap.clear();
    currentDate = newDate;
}

void GameTeamWinContainer::nextPlayer(std::vector<std::string> actualResultsLine, float sabrPredictor) {
    if (actualResultsLine.size() != 14) {
        cout << "Erroenous data sent to GameTeamWinContainer" << endl;
        return;
    }
    if (actualResultsLine[0] != currentDate) {
        nextDate(actualResultsLine[0]);
    }
    string teamNameKey = actualResultsLine[9] + actualResultsLine[11];
    auto teamInfo = teamToInfoMap.find(teamNameKey);
    if (teamInfo == teamToInfoMap.end()) {
        TeamInformation ti;
        ti.runs = atoi(actualResultsLine[12].c_str());
        ti.opponentKey = actualResultsLine[10].substr(actualResultsLine[10].length()-3) + actualResultsLine[11];
        teamToInfoMap.insert({ teamNameKey, ti });
        teamInfo = teamToInfoMap.find(teamNameKey);
    }
    if (actualResultsLine[4] != "1")
        return;
    teamInfo->second.fanduelSabrPredictor[atoi(actualResultsLine[5].c_str()) - 1] = sabrPredictor;
}

/*
 recap of stats earned
 0           1         2       3                  4           5         6          7      8       9      10     11       12         13
 Date;       GID;   MLB_ID;  Name;              Starter;  Bat order;  FD posn;  FD pts;  FD sal;  Team;  Oppt;  dblhdr;  Tm Runs;  Opp Runs
 20170404;  1524;  434378;  Verlander, Justin;  1;        0;          ;         53.0;    ;         DET;  @ chw; ;        6;        3
 */
