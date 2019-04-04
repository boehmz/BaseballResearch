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
#include "Main.h"

using namespace std;

TeamInformation::TeamInformation() {
    opponentKey = "";
    runs = -1;
    for (int i = 0; i < 9; ++i) {
        fanduelSabrPredictor[i] = -1;
    }
}
GameTeamWinContainer::GameTeamWinContainer() {
    currentDate = currentYear = "";
}

void GameTeamWinContainer::runAnalysis() {
    if (currentDate.length() > 4) {
        currentYear = currentDate.substr(0,4);
    } else {
        return;
    }
    nextDate("");
    if (allDatesToTeamInfoMaps.size() == 0)
        return;
    
    fstream gamesRecordOverallFile;
    string gamesRecordFileName = GetGamesRecordFilename();
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
                
                for (int t = 0; t < 9; ++t) {
                    if (oppTeamItr->second.fanduelSabrPredictor[t] <= 0.01 || teamItr->second.fanduelSabrPredictor[t] <= 0.01) {
                        oppTeamItr->second.fanduelSabrPredictor[t] = 0;
                        teamItr->second.fanduelSabrPredictor[t] = 0;
                    }
                }
                
                float teamSabrAverage = AverageArrayExcludingThreshold((teamItr->second.fanduelSabrPredictor), 9, 0.0f, 5);
                float oppTeamSabrAverage = AverageArrayExcludingThreshold((oppTeamItr->second.fanduelSabrPredictor), 9, 0.0f, 5);
                
                if (teamSabrAverage > 0 && oppTeamSabrAverage > 0) {
                    gamesRecordOverallFile << dateItr->first << ";" << teamItr->first << ";" << oppTeamItr->first << ";" <<
                    teamItr->second.runs << ";" << oppTeamItr->second.runs << ";" << teamSabrAverage << ";" << oppTeamSabrAverage << endl;
                }
                
            } else {
                cout << "error could not find opponent of " << teamItr->first << " as " << teamItr->second.opponentKey << endl;
            }
        }
    }
    CompileVegasOddsIntoWinPredictionFile();
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
        cout << "Erroneous data sent to GameTeamWinContainer" << endl;
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

string GameTeamWinContainer::GetGamesRecordFilename () {
    string gamesRecordFileName = currentYear;
    gamesRecordFileName += "ResultsTracker\\TeamWinResults\\AllGamesSabrPredictions.txt";

    return gamesRecordFileName;
}

void GameTeamWinContainer::CompileVegasOddsIntoWinPredictionFile() {
    string filename = GetGamesRecordFilename();
    string vegasOddsFileName = currentYear;
    vegasOddsFileName += "ResultsTracker\\TeamWinResults\\AllGames.txt";
    string vegasOddsFileContents = GetEntireFileContents(vegasOddsFileName);
    string vegasOddsSection = "";

    string fileContents = GetEntireFileContents(filename);
    
#if PLATFORM_OSX
    filename = GetPlatformCompatibleFileNameFromRelativePath(filename);
#endif
    string filenameCombinedWithVegasOdds = filename.substr(0,filename.length()-4) +  "CombinedVegasOdds.txt";
    fstream combinedOddsFile;
    combinedOddsFile.open(filenameCombinedWithVegasOdds, std::ios::out);
    
    size_t currentLineStart = 0;
    while (currentLineStart != string::npos) {
        size_t currentLineEnd = fileContents.find("\n", currentLineStart);
        string thisLine = fileContents.substr(currentLineStart, currentLineEnd != string::npos ? (currentLineEnd - currentLineStart) : string::npos);
        vector<string> thisLineColumns = SplitStringIntoMultiple(thisLine, ";");
        if (thisLineColumns.size() != 7) {
            cout << "breaking because line has " << thisLineColumns.size() << " columns for line " << thisLine << endl;
            break;
        }
        int doubleHeaderIndex = 0;
        if (thisLineColumns[1].at(thisLineColumns[1].length()-1) == '2') {
            doubleHeaderIndex = 1;
            thisLineColumns[1] = thisLineColumns[1].substr(0,thisLineColumns[1].length()-1);
            thisLineColumns[2] = thisLineColumns[2].substr(0,thisLineColumns[2].length()-1);
        }
        if (!StringStartsWith(vegasOddsSection, thisLineColumns[0])) {
            size_t sectionStart = vegasOddsFileContents.find(thisLineColumns[0]);
            if (sectionStart == string::npos) {
                vegasOddsSection = thisLineColumns[0];
                cout << "There are no vegas odds for date " << vegasOddsSection << endl;
            } else {
                size_t sectionEnd = vegasOddsFileContents.find("\n", sectionStart);
                size_t sectionEndSearch = vegasOddsFileContents.find(thisLineColumns[0], sectionEnd);
                while (sectionEndSearch != string::npos) {
                    sectionEnd = vegasOddsFileContents.find("\n", sectionEndSearch);
                    sectionEndSearch = vegasOddsFileContents.find(thisLineColumns[0], sectionEnd);
                }
                vegasOddsSection = vegasOddsFileContents.substr(sectionStart, sectionEnd != string::npos ? sectionEnd - sectionStart : string::npos);
            }
        }
        
        string fullteamName1 = convertTeamCodeToSynonym(ConvertRotoGuruTeamCodeToStandardTeamCode(thisLineColumns[1]), 0);
        string fullteamName2 = convertTeamCodeToSynonym(ConvertRotoGuruTeamCodeToStandardTeamCode(thisLineColumns[2]), 0);
        size_t fullteamName1Index = vegasOddsSection.find(fullteamName1 + ";");
        size_t fullteamName2Index = vegasOddsSection.find(fullteamName2 + ";");
        if (doubleHeaderIndex != 0) {
            fullteamName1Index = vegasOddsSection.find(fullteamName1 + ";", fullteamName1Index + 1);
            fullteamName2Index = vegasOddsSection.find(fullteamName2 + ";", fullteamName2Index + 1);
        }
        
        if (fullteamName1Index != string::npos && fullteamName2Index != string::npos) {
            size_t vegasLineStart = vegasOddsSection.rfind("\n", fullteamName1Index);
            size_t vegasLineEnd = vegasOddsSection.find("\n", fullteamName1Index);
            if (vegasLineStart == string::npos)
                vegasLineStart = 0;
            if (vegasLineEnd == string::npos)
                vegasLineEnd = vegasOddsSection.length();
            string thisVegasLine = vegasOddsSection.substr(vegasLineStart, vegasLineEnd - vegasLineStart);
            vector<string> thisVegasLineColumns = SplitStringIntoMultiple(thisVegasLine, ";");
            if (thisVegasLineColumns.size() == 12) {
                combinedOddsFile << thisLine;
                if (fullteamName1Index < fullteamName2Index)
                    combinedOddsFile << ";" << thisVegasLineColumns[10] << ";" << thisVegasLineColumns[11];
                else
                    combinedOddsFile << ";" << thisVegasLineColumns[11] << ";" << thisVegasLineColumns[10];
                combinedOddsFile << endl;
            }
        } else {
            if (doubleHeaderIndex == 0 && vegasOddsSection != thisLineColumns[0]) {
                cout << "Could not find matching vegas odds for teams " << thisLine << endl;
            }
        }
        
        if (currentLineEnd == string::npos)
            break;
        else
            currentLineStart = currentLineEnd + 1;
    }
}

/*
 recap of stats earned
 0           1         2       3                  4           5         6          7      8       9      10     11       12         13
 Date;       GID;   MLB_ID;  Name;              Starter;  Bat order;  FD posn;  FD pts;  FD sal;  Team;  Oppt;  dblhdr;  Tm Runs;  Opp Runs
 20170404;  1524;  434378;  Verlander, Justin;  1;        0;          ;         53.0;    ;         DET;  @ chw; ;        6;        3
 */
