#include <jni.h>
#include <string>
#include <curl/curl.h>
#include <android/log.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
#include <thread>
#include "SharedGlobals.h"
#include "GameTeamWinContainer.h"
#include "StatsCollectionFunctions.h"
#include "StringUtils.h"


int latestGameTime =   2050;
int earliestGameTime = 2005;
std::string todaysDate = "20180924";
bool skipStatsCollection = false;
int reviewDateStart = 515;
int reviewDateEnd = 609;


#define TRUE 1
#define FALSE 0

using namespace std;

struct TeamVegasInfo {
    string teamName;
    string gameTime;
    int bovadaMoneyline;
    string pitcherName;
};

string allOfTodaysGames(vector<string> sabrPredictorOdds, unordered_map<string,TeamVegasInfo> teamToVegasInfoMap);
unordered_map<string,TeamVegasInfo> getTodaysMoneyLines(CURL* curl);

string getSabrPredictorFileContents(CURL* curl, string date, bool bPitchers) {
	if (curl == NULL) {
		curl = curl_easy_init();
	}
	string entireContents = "";
    string htmlContents = "";
    if (bPitchers) {
    	CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=all&stats=pit&type=sabersim&team=0&lg=all&players=0", entireContents, true);
    	CutStringToOnlySectionBetweenKeywords(entireContents, "class=\"rgMasterTable\"", "</table>");
    } else {
   		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=c&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=1b&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=2b&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=ss&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=3b&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=rf&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=cf&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=lf&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=dh&stats=bat&type=sabersim&team=0&lg=al&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=c&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=1b&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=2b&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=ss&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=3b&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=rf&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=cf&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=lf&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
		
		CurlGetSiteContents(curl, "https://www.fangraphs.com/dailyprojections.aspx?pos=dh&stats=bat&type=sabersim&team=0&lg=nl&players=0", htmlContents, true);
   		CutStringToOnlySectionBetweenKeywords(htmlContents, "class=\"rgMasterTable\"", "</table>");
		entireContents += htmlContents;
		htmlContents = "";
    }
    //LOGI("length = %u contents = %s", (unsigned int)htmlContents.length(), htmlContents.c_str());
    return entireContents;
}

string GenerateLineups()
{
	CURL* curl = curl_easy_init();
	if (curl)
	{
		unordered_map<string,TeamVegasInfo> vegasInfo = getTodaysMoneyLines(curl);

        string todaysLineups;
        curl_easy_setopt(curl, CURLOPT_URL, "http://www.fantasyalarm.com/mlb/lineups/");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &todaysLineups);
        curl_easy_perform(curl);
        curl_easy_reset(curl);
        todaysLineups = ConvertSpecialCharactersToEnglish26(todaysLineups);

		string sabrPredictorText = getSabrPredictorFileContents(curl, todaysDate, false);
		string sabrPredictorTextPitchers = getSabrPredictorFileContents(curl, todaysDate, true);
        GameTeamWinContainer gameTeamWinContainer;

		for (int p = 2; p <= 7; ++p)
		{
			int positionIndex = p - 2;
			std::string readBuffer;
			char pAsString[5];
			itoa(p, pAsString, 10);
			string pAsStringString(pAsString);
			string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=" + pAsStringString + "&sort=6";
			thisPositionURL += "&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=0&hithand=0&numlist=c&user=GoldenExcalibur&key=G5970032941";

			curl_easy_setopt(curl, CURLOPT_URL, thisPositionURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
			curl_easy_perform(curl);
			curl_easy_reset(curl);

			size_t placeHolderIndex = readBuffer.find("GID;", 0);
			size_t endOfPlayerDataIndex = readBuffer.find("Statistical data provided", placeHolderIndex);

			for (int i = 0; i < 23; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
			while (placeHolderIndex != string::npos && readBuffer.find(";", placeHolderIndex + 1) < endOfPlayerDataIndex - 1)
			{
				PlayerData singlePlayerData;

				// player id
				size_t nextIndex = readBuffer.find(";", placeHolderIndex + 1);
				singlePlayerData.playerId = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);

				// player name
				for (int i = 0; i < 2; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}
				nextIndex = readBuffer.find(";", placeHolderIndex + 1);
				singlePlayerData.playerName = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);
				// team name code
				for (int i = 0; i < 1; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}
				nextIndex = readBuffer.find(";", placeHolderIndex + 1);
				singlePlayerData.teamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);

				// player salary
				for (int i = 0; i < 1; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}
				nextIndex = readBuffer.find(";", placeHolderIndex + 1);
				singlePlayerData.playerSalary = atoi(readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str());

                // number of games started this season
                for (int i = 0; i < 17; ++i)
                {
                    placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
                }
                nextIndex = readBuffer.find(";", placeHolderIndex + 1);
                string opponentTeamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);

				// number of games started this season
				for (int i = 0; i < 2; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}

				singlePlayerData.playerPointsPerGame = -1;

                float expectedFdPointsOpposingPitcher = -1;
                float expectedYahooPointsOpposingPitcher = -1;
                float expectedFdPoints = -1;
                float expectedPitcherOpsAllowed = -1;
				size_t playerNameIndex = sabrPredictorText.find(ConvertLFNameToFLName(singlePlayerData.playerName));
                if (playerNameIndex == string::npos)
                    playerNameIndex = FindPlayerNameIndexInList(singlePlayerData.playerName, sabrPredictorText);

				if (playerNameIndex != string::npos) {
					size_t htmlSectionStart = sabrPredictorText.rfind("<td class", playerNameIndex);
					size_t htmlSectionEnd = sabrPredictorText.find("</tr>", playerNameIndex);
					string htmlSection = sabrPredictorText.substr(htmlSectionStart, htmlSectionEnd - htmlSectionStart);

					vector<string> thisSabrLine = MultineRegex(htmlSection, ".*?>(.*?)<.*?");
					//vector<string> thisSabrLine = SplitStringIntoMultiple(sabrPredictorText.substr(playerNameIndex, nextNewLine - playerNameIndex), ",", "\"");
                    if (thisSabrLine.size() == 19) {
	                    expectedFdPoints = stof(thisSabrLine[17]);
						singlePlayerData.playerPointsPerGame = expectedFdPoints;
						if (static_cast <float> (rand()) > 0.7f)
							LOGI("%s expected fd points of %f", singlePlayerData.playerName.c_str(), singlePlayerData.playerPointsPerGame);
					} else {
						LOGI("%s has %u lines in predictor file", singlePlayerData.playerName.c_str(), (unsigned int)thisSabrLine.size());
					}

                /*
					if (sabrPredictorTextPitchers != "") {
						string playerTeamName = thisSabrLine[1];
						string playerGameName = thisSabrLine[2];
						size_t gameNameIndex = sabrPredictorTextPitchers.find(playerGameName);
                        if (playerGameName == "")
                            gameNameIndex = string::npos;
						if (gameNameIndex != string::npos) {
							size_t prevNewLinePitchers = sabrPredictorTextPitchers.rfind("\n", gameNameIndex);
							size_t nextNewLinePitchers = sabrPredictorTextPitchers.find("\n", gameNameIndex);
							vector<string> thisSabrLinePitchers = SplitStringIntoMultiple(sabrPredictorTextPitchers.substr(prevNewLinePitchers, nextNewLinePitchers - prevNewLinePitchers), ",", "\"");
							if (thisSabrLinePitchers[1] == playerTeamName) {
								gameNameIndex = sabrPredictorTextPitchers.find(playerGameName, nextNewLinePitchers);
								if (gameNameIndex != string::npos) {
									prevNewLinePitchers = sabrPredictorTextPitchers.rfind("\n", gameNameIndex);
									nextNewLinePitchers = sabrPredictorTextPitchers.find("\n", gameNameIndex);
									thisSabrLinePitchers.clear();
									thisSabrLinePitchers = SplitStringIntoMultiple(sabrPredictorTextPitchers.substr(prevNewLinePitchers, nextNewLinePitchers - prevNewLinePitchers), ",", "\"");
								}
							}
                            expectedFdPointsOpposingPitcher = stof(thisSabrLinePitchers[14]);
                            expectedYahooPointsOpposingPitcher = stof(thisSabrLinePitchers[13]);
                            float pitcherOnBaseAllowed = stof(thisSabrLinePitchers[6]) + stof(thisSabrLinePitchers[11]);
                            float pitcherBattersFaced = stof(thisSabrLinePitchers[5]);
                            float pitcherTotalBasesAllowed = stof(thisSabrLinePitchers[7]) + stof(thisSabrLinePitchers[8]) * 2 + stof(thisSabrLinePitchers[9]) * 3 + stof(thisSabrLinePitchers[10]) * 4;
                            expectedPitcherOpsAllowed = pitcherOnBaseAllowed / pitcherBattersFaced + pitcherTotalBasesAllowed / pitcherBattersFaced;
						}
					}
					*/
				} else {
					LOGI("Could not find sabr prediction for %s on team %s", singlePlayerData.playerName.c_str(), singlePlayerData.teamCode.c_str());
                }

				int gameStartTime = 99999;
				size_t colonIndex = readBuffer.find(":", placeHolderIndex + 1);
				size_t nextSemiColonIndex = readBuffer.find("\n", placeHolderIndex + 1);
				if (colonIndex != string::npos && colonIndex < nextSemiColonIndex)
				{
					size_t spaceIndex = readBuffer.rfind(" ", colonIndex);
					gameStartTime = atoi(readBuffer.substr(spaceIndex + 1, colonIndex - spaceIndex - 1).c_str());
					size_t pmIndex = readBuffer.find("PM", spaceIndex);
					size_t edtIndex = readBuffer.find("EDT", spaceIndex);
					if (pmIndex != string::npos && pmIndex < edtIndex)
					{
						gameStartTime += 12;
					}
                    gameStartTime *= 100;
                    gameStartTime += atoi(readBuffer.substr(colonIndex + 1, 2).c_str());
				}
				else if (readBuffer.find("Final", placeHolderIndex + 1) != string::npos && readBuffer.find("Final", placeHolderIndex + 1) < nextSemiColonIndex)
				{
					// game has gone final
					gameStartTime = 999999;
				}
				else if (readBuffer.find("Mid", placeHolderIndex + 1) != string::npos && readBuffer.find("Mid", placeHolderIndex + 1) < nextSemiColonIndex)
				{
					// game is in progress
					gameStartTime = 99999;
				}
				else if (readBuffer.find("Top", placeHolderIndex + 1) != string::npos && readBuffer.find("Top", placeHolderIndex + 1) < nextSemiColonIndex)
				{
					// game is in progress
					gameStartTime = 99999;
				}
				else if (readBuffer.find("Bot", placeHolderIndex + 1) != string::npos && readBuffer.find("Bot", placeHolderIndex + 1) < nextSemiColonIndex)
				{
					// game is in progress
					gameStartTime = 99999;
                } else if (readBuffer.find("Postponed", placeHolderIndex + 1) != string::npos && readBuffer.find("Postponed", placeHolderIndex + 1) < nextSemiColonIndex)
                {
                    // game is in progress
                    gameStartTime = 99999;
                } else if (readBuffer.find("End", placeHolderIndex + 1) != string::npos && readBuffer.find("End", placeHolderIndex + 1) < nextSemiColonIndex)
                {
                    // game is in progress
                    gameStartTime = 99999;
                }

				int actualBattingOrder = -1;
                size_t playerIndexInTodaysLineups = todaysLineups.find(">" + ConvertLFNameToFLName(singlePlayerData.playerName) + " ");
				if (playerIndexInTodaysLineups == string::npos) {
					playerIndexInTodaysLineups = todaysLineups.find(">" + ConvertNameToFirstInitialLastName(singlePlayerData.playerName) + " ");

				}
				if (playerIndexInTodaysLineups != string::npos) {
                    size_t prevLineupOrderIndex = todaysLineups.rfind("lineup-large-pos", playerIndexInTodaysLineups);
                    size_t lineupOrderStartIndex = todaysLineups.find(">", prevLineupOrderIndex);
                    size_t lineupOrderEndIndex = todaysLineups.find("<", prevLineupOrderIndex);

                    string lineupOrderString = todaysLineups.substr(lineupOrderStartIndex + 1, lineupOrderEndIndex - lineupOrderStartIndex - 1);
                    // if this player's lineup is not out yet, base it off previous/general lineup orders
                    if (lineupOrderString != "-")
                    {
                        actualBattingOrder = atoi(lineupOrderString.c_str());
                    }
				}
              //  LOGI("%s is starting at %d with salary %d and batting order %d", singlePlayerData.playerName.c_str(), gameStartTime, singlePlayerData.playerSalary, actualBattingOrder);

				// throw this guy out if he's not a starter or his game will most likely be rained out
				if (gameStartTime < 999999
                    && actualBattingOrder >= 0
                    && expectedFdPoints > 0) {
						auto vegasGame = vegasInfo.find(singlePlayerData.teamCode);
						if (vegasGame != vegasInfo.end()) {
							size_t dashStart = readBuffer.find("- ", placeHolderIndex);
							size_t dashEnd = readBuffer.find(" -", dashStart + 1);
							string gameTime = readBuffer.substr(dashStart + 2, dashEnd - dashStart - 2);
							vegasGame->second.gameTime = gameTime;
						}
                        singlePlayerData.playerPointsPerGame = expectedFdPoints;
                        singlePlayerData.battingOrder = actualBattingOrder;
                        gameTeamWinContainer.nextPlayer(singlePlayerData, opponentTeamCode);
                        //LOGI("adding player %s to win container", singlePlayerData.playerName.c_str());
				}
				if (placeHolderIndex == string::npos)
					break;
				else
					placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
			}
		}
		

	    string ret = allOfTodaysGames(gameTeamWinContainer.getStringsFromTodaysDate(), vegasInfo);
	    curl_easy_cleanup(curl);
	    curl = NULL;
	    return ret;
	}
	int breakpoint = 0;
	return "";
}

unordered_map<string,TeamVegasInfo> getTodaysMoneyLines(CURL* curl) {
    unordered_map<string,TeamVegasInfo> teamToMoneyLinesInfo;
    if (curl == NULL) {
    	curl = curl_easy_init();
	}
    string gameMoneyLinesURL = "https://www.bovada.lv/services/sports/event/v2/events/A/description/baseball/mlb?marketFilterId=def&preMatchOnly=false&lang=en";
    string gameMoneyLines = "";
    CurlGetSiteContents(curl, gameMoneyLinesURL, gameMoneyLines, true);
    size_t oddsOpenerBegin = gameMoneyLines.find("Moneyline");
    while (oddsOpenerBegin != string::npos) {
    	size_t outcomesArrayBegin = gameMoneyLines.find("outcomes", oddsOpenerBegin);
    	if (outcomesArrayBegin != string::npos) {
    		outcomesArrayBegin = gameMoneyLines.find("[", outcomesArrayBegin);
    		size_t outcomesArrayEnd = gameMoneyLines.find("]", outcomesArrayBegin);
    		size_t nextArrayOpener = gameMoneyLines.find("[", outcomesArrayBegin);
    		while (nextArrayOpener < outcomesArrayEnd) {
    			outcomesArrayEnd = gameMoneyLines.find("]", outcomesArrayEnd + 1);
    			nextArrayOpener = gameMoneyLines.find("[", nextArrayOpener + 1);
    		}
    		string gameSection = gameMoneyLines.substr(outcomesArrayBegin, outcomesArrayEnd - outcomesArrayBegin);
    		size_t description1 = gameSection.find("description");
    		string teamName1 = getJsonValueFromKey(gameSection, "description");
    		string americanOdds1 = getJsonValueFromKey(gameSection, "american", description1);

    		size_t description2 = gameSection.find("description", description1 + 1);
    		string teamName2 = getJsonValueFromKey(gameSection, "description", description2);
    		string americanOdds2 = getJsonValueFromKey(gameSection, "american", description2);

    		if (americanOdds1 == "EVEN") {
    			americanOdds1 = "100";
    		}
    		if (americanOdds2 == "EVEN") {
    			americanOdds2 = "100";
    		}

    		teamName1 = ConvertStandardTeamCodeToRotoGuruTeamCode(convertTeamCodeToSynonym(teamName1,3));
    		teamName2 = ConvertStandardTeamCodeToRotoGuruTeamCode(convertTeamCodeToSynonym(teamName2,3));

    		if (teamToMoneyLinesInfo.find(teamName1) == teamToMoneyLinesInfo.end()) {
	    		TeamVegasInfo team1Info;
	            //team1Info.gameTime = gameTime;
	            team1Info.teamName = teamName1;
	            //size_t rightMostSpace = awayPitcher.rfind(" ", awayPitcher.length() - 1);
	            //awayPitcher = awayPitcher.substr(0, rightMostSpace);
	            //awayTeamInfo.pitcherName = awayPitcher;
	            team1Info.bovadaMoneyline = atoi(americanOdds1.c_str());
	            teamToMoneyLinesInfo.insert({teamName1,team1Info});
	            LOGI("adding %s with moneyline %d", teamName1.c_str(), team1Info.bovadaMoneyline);

	            TeamVegasInfo team2Info;
	            //team1Info.gameTime = gameTime;
	            team2Info.teamName = teamName1;
	            //size_t rightMostSpace = awayPitcher.rfind(" ", awayPitcher.length() - 1);
	            //awayPitcher = awayPitcher.substr(0, rightMostSpace);
	            //awayTeamInfo.pitcherName = awayPitcher;
	            team2Info.bovadaMoneyline = atoi(americanOdds2.c_str());
	            teamToMoneyLinesInfo.insert({teamName2,team2Info});
	            LOGI("adding %s with moneyline %d", teamName2.c_str(), team2Info.bovadaMoneyline);
        	}
    	}
        oddsOpenerBegin = gameMoneyLines.find("Moneyline", oddsOpenerBegin + 5);
    }
    return teamToMoneyLinesInfo;
}

string uiTest() {
    GameTeamWinContainer gameTeamWinContainer;
    PlayerData pd;
    pd.teamCode = "kan";
    pd.battingOrder = 0;
    pd.playerPointsPerGame = 8;
    gameTeamWinContainer.nextPlayer(pd, "min");
    pd.teamCode = "min";
    gameTeamWinContainer.nextPlayer(pd, "kan");

    pd.teamCode = "nyy";
    pd.playerPointsPerGame = 10;
    gameTeamWinContainer.nextPlayer(pd, "bos");

    pd.playerPointsPerGame = 12;
    pd.battingOrder = 1;
    gameTeamWinContainer.nextPlayer(pd, "bos");

    pd.teamCode = "bos";
    gameTeamWinContainer.nextPlayer(pd, "nyy");

    pd.teamCode = "was";
    pd.playerPointsPerGame = 12;
    pd.battingOrder = 1;
    gameTeamWinContainer.nextPlayer(pd, "los");

    pd.teamCode = "los";
    gameTeamWinContainer.nextPlayer(pd, "was");

    pd.teamCode = "cle";
    pd.playerPointsPerGame = 12;
    pd.battingOrder = 1;
    gameTeamWinContainer.nextPlayer(pd, "col");

    pd.teamCode = "col";
     pd.playerPointsPerGame = 10;
    gameTeamWinContainer.nextPlayer(pd, "cle");

    return allOfTodaysGames(gameTeamWinContainer.getStringsFromTodaysDate(), getTodaysMoneyLines(NULL));
}

string allOfTodaysGames(vector<string> sabrPredictorOdds, unordered_map<string,TeamVegasInfo> teamToVegasInfoMap) {

    list<string> underDogsToBetOn;
    list<float> underDogsFloats;
    list<string> allOthers;
    list<float> allOthersFloats;
    for (int i = 0; i < sabrPredictorOdds.size(); ++i) {
        vector<string> singleGameSabrOdds = SplitStringIntoMultiple(sabrPredictorOdds[i], ";");
        if (singleGameSabrOdds.size() == 4) {
            string teamA = singleGameSabrOdds[0];
            string teamB = singleGameSabrOdds[1];
            float teamASabrPoints = stof(singleGameSabrOdds[2]);
            float teamBSabrPoints = stof(singleGameSabrOdds[3]);
            auto teamAVegasInfo = teamToVegasInfoMap.find(teamA);
            auto teamBVegasInfo = teamToVegasInfoMap.find(teamB);
            float teamAMyPercent = abs(teamASabrPoints - teamBSabrPoints) * 0.07933333333f;
            if (teamASabrPoints > teamBSabrPoints) {
                teamAMyPercent += 0.5f;
            } else {
                teamAMyPercent = 0.5f - teamAMyPercent;
            }
            float teamBMyPercent = 1.0f - teamAMyPercent;
            teamAMyPercent *= 100;
            teamBMyPercent *= 100;

            string gameString = "";
            float teamAVegasPercent = 100;
            float teamBVegasPercent = 100;

            if (teamAVegasInfo != teamToVegasInfoMap.end() && teamBVegasInfo != teamToVegasInfoMap.end()) {
                teamAVegasPercent = (teamAVegasInfo->second.bovadaMoneyline > 0) ? (1.0f - ((float)teamAVegasInfo->second.bovadaMoneyline / ((float)teamAVegasInfo->second.bovadaMoneyline + 100))) : ((float)teamAVegasInfo->second.bovadaMoneyline / ((float)teamAVegasInfo->second.bovadaMoneyline - 100));
                teamBVegasPercent = (teamBVegasInfo->second.bovadaMoneyline > 0) ? (1.0f - ((float)teamBVegasInfo->second.bovadaMoneyline / ((float)teamBVegasInfo->second.bovadaMoneyline + 100))) : ((float)teamBVegasInfo->second.bovadaMoneyline / ((float)teamBVegasInfo->second.bovadaMoneyline - 100));
                teamAVegasPercent *= 100;
                teamBVegasPercent *= 100;

                if (teamASabrPoints > teamBSabrPoints) {
                    gameString += teamA + ";" + to_string(teamAMyPercent).substr(0,4) + ";" + to_string(teamAVegasPercent).substr(0,4) + ";" + teamAVegasInfo->second.pitcherName + ";";
                    gameString += teamAVegasInfo->second.gameTime + ";";
                    gameString += teamB + ";" + to_string(teamBMyPercent).substr(0,4) + ";" + to_string(teamBVegasPercent).substr(0,4) + ";" + teamBVegasInfo->second.pitcherName + ";";
                } else {
                    gameString += teamB + ";" + to_string(teamBMyPercent).substr(0,4) + ";" + to_string(teamBVegasPercent).substr(0,4) + ";" + teamBVegasInfo->second.pitcherName + ";";
                    gameString += teamAVegasInfo->second.gameTime + ";";
                    gameString += teamA + ";" + to_string(teamAMyPercent).substr(0,4) + ";" + to_string(teamAVegasPercent).substr(0,4) + ";" + teamAVegasInfo->second.pitcherName + ";";
                }
            } else {
                // for some reason odds could not be found, still put it in the string
                if (teamASabrPoints > teamBSabrPoints) {
                    gameString += teamA + ";" + to_string(teamAMyPercent).substr(0,4) + ";;;";
                    gameString += ";";
                    gameString += teamB + ";" + to_string(teamBMyPercent).substr(0,4) + ";;;";
                } else {
                    gameString += teamB + ";" + to_string(teamBMyPercent).substr(0,4) + ";;;";
                    gameString += ";";
                    gameString += teamA + ";" + to_string(teamAMyPercent).substr(0,4) + ";;;";
                }
            }
            float sortedPercent = (teamASabrPoints > teamBSabrPoints) ? teamAMyPercent : teamBMyPercent;
            float sortedVegasPercent = (teamASabrPoints > teamBSabrPoints) ? teamAVegasPercent : teamBVegasPercent;
            if (sortedPercent >= 50 && sortedVegasPercent < 50) {
                list<float>::iterator dogsFloatItr = underDogsFloats.begin();
                for (list<string>::iterator dogsItr = underDogsToBetOn.begin(); ; ++dogsItr, ++dogsFloatItr) {
                    if (dogsItr == underDogsToBetOn.end() || sortedPercent > *dogsFloatItr) {
                        underDogsFloats.insert(dogsFloatItr, sortedPercent);
                        underDogsToBetOn.insert(dogsItr, gameString);
                        break;
                    }
                }
            } else {
                list<float>::iterator floatsItr = allOthersFloats.begin();
                for (list<string>::iterator itr = allOthers.begin(); ; ++itr, ++floatsItr) {
                    if (itr == allOthers.end() || sortedPercent > *floatsItr) {
                        allOthersFloats.insert(floatsItr, sortedPercent);
                        allOthers.insert(itr, gameString);
                        break;
                    }
                }

            }
        }
    }

    string finalString = "";
    for (list<string>::iterator dogsItr = underDogsToBetOn.begin(); dogsItr != underDogsToBetOn.end(); ++dogsItr) {
        finalString += *dogsItr + "\n\n";
    }
    for (list<string>::iterator itr = allOthers.begin(); itr != allOthers.end(); ++itr) {
        finalString += *itr + "\n\n";
    }
    return finalString;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_predictor_mlb_mlbpredictor_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
	    return env->NewStringUTF(GenerateLineups().c_str());
}
