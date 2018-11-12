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
#include "StatsCollectionFunctions.h"

using namespace std;
TeamInformation::TeamInformation() {
    opponentKey = "";
    runs = -1;
    for (int i = 0; i < 9; ++i) {
        fanduelSabrPredictor[i] = -1;
    }
}

void GameTeamWinContainer::runAnalysis() {
    nextDate("");
    if (allDatesToTeamInfoMaps.size() == 0)
        return;
    
    fstream gamesRecordOverallFile;
    string gamesRecordFileName = CURRENT_YEAR;
    gamesRecordFileName += "ResultsTracker\\TeamWinResults\\AllGamesSabrPredictions.txt";
#if PLATFORM_OSX
    gamesRecordFileName = GetPlatformCompatibleFileNameFromRelativePath(gamesRecordFileName);
#endif
    gamesRecordOverallFile.open(gamesRecordFileName, std::ios::out);
            
    for (auto dateItr = allDatesToTeamInfoMaps.begin(); dateItr != allDatesToTeamInfoMaps.end(); ++dateItr) {
        unordered_set<string> teamsWritten;
        for (auto teamItr = dateItr->second.begin(); teamItr != dateItr->second.end(); ++teamItr) {
            if (teamsWritten.find(teamItr->first) != teamsWritten.end())
                continue;
            auto oppTeamItr = dateItr->second.find(teamItr->second.opponentKey);
            if (oppTeamItr != dateItr->second.end()) {
                teamsWritten.insert(oppTeamItr->first);
                
                float teamSabrAverage = AverageArrayExcludingThreshold((teamItr->second.fanduelSabrPredictor), 9, 0.0f);
                float oppTeamSabrAverage = AverageArrayExcludingThreshold((oppTeamItr->second.fanduelSabrPredictor), 9, 0.0f);
                
                gamesRecordOverallFile << dateItr->first << ";" << teamItr->first << ";" << oppTeamItr->first << ";" <<
                teamItr->second.runs << ";" << oppTeamItr->second.runs << ";" << teamSabrAverage << ";" << oppTeamSabrAverage << endl;
                
            } else {
                cout << "error could not find opponent of " << teamItr->first << " as " << teamItr->second.opponentKey << endl;
            }
        }
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
    std::transform(actualResultsLine[9].begin(), actualResultsLine[9].end(), actualResultsLine[9].begin(), ::tolower);
    string teamNameKey =  actualResultsLine[9] + actualResultsLine[11];
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
