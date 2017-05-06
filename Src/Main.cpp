#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include "SharedGlobals.h"
#include "StatsCollectionFunctions.h"
#include "Main.h"
using namespace std;

GameType gameType = Fanduel;
int maxTotalBudget = 35000 - 10400;
// game times in Eastern and 24 hour format
int latestGameTime = 25;
int earliestGameTime = -1;
std::string todaysDate = "20170506";
int reviewDateStart = 406;
int reviewDateEnd = 504;
float percentOf2017SeasonPassed = 26.0f / 162.0f;


const float leagueAverageOps = 0.72f;
const int minGamesPlayed2016 = 99;


vector< vector<PlayerData> > allPlayers;
unordered_map<std::string, OpponentInformation> opponentMap;
vector<string> probableRainoutGames;

int main(void)
{
	enum ProcessType { Analyze2016, GenerateLineup, Refine};
	ProcessType processType = GenerateLineup;

	switch (processType)
	{
	case Analyze2016:
		Analyze2016Stats();
		break;
	case Refine:
		RefineAlgorithm();
		break;
	default:
	case GenerateLineup:
		PopulateProbableRainoutGames();
		ChooseAPitcher();
		GenerateNewLineup();
		break;
	}

	cout << "program has finished" << endl;
	getchar();
	return 0;
}

bool comparePlayerByPointsPerGame(PlayerData i, PlayerData j)
{
	if (abs(i.playerPointsPerGame - j.playerPointsPerGame) < 0.05f)
		return i.playerSalary < j.playerSalary;
	else
		return (i.playerPointsPerGame > j.playerPointsPerGame);
}

bool comparePlayersBySalary(PlayerData i, PlayerData j)
{
	return i.playerSalary < j.playerSalary;
}

void RefineAlgorithm()
{
	CURL *curl;  

	curl = curl_easy_init();
	if (curl)
	{
		vector< vector<float> > inputVariables;
		float inputCoefficients[2] = { 0.0f, 1.0f };
		vector< float > outputValues;

		for (int d = reviewDateStart; d <= reviewDateEnd; ++d)
		{
			char thisDateCStr[5];
			_itoa_s(d, thisDateCStr, 10);
			string thisDate = thisDateCStr;
			string actualResults;
			string resultsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + thisDate + "&game=fd&scsv=1&nowrap=1";
			curl_easy_setopt(curl, CURLOPT_URL, resultsURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &actualResults);
			curl_easy_perform(curl);
			curl_easy_reset(curl);

			ifstream resultsTrackerFile;
			string resultsTrackerFileName = "2017ResultsTracker\\2017";
			if (d < 1000)
				resultsTrackerFileName += "0";
			resultsTrackerFileName += thisDate + ".txt";
			resultsTrackerFile.open(resultsTrackerFileName);

			string resultsLine;
			string finalWriteString;

			while (getline(resultsTrackerFile, resultsLine))
			{
				vector<float> thisInputVariables;
				thisInputVariables.push_back(0);
				thisInputVariables.push_back(0);
				size_t columnStartIndex = resultsLine.find(";", 0);
				string thisPlayerId = resultsLine.substr(0, columnStartIndex);


				FullSeasonStats player2016Stats = GetBatter2016Stats(thisPlayerId, curl);
				columnStartIndex = resultsLine.find(";", columnStartIndex + 1);
				size_t nextIndex = resultsLine.find(";", columnStartIndex + 1);
				string opposingHandedness = resultsLine.substr(columnStartIndex + 1, nextIndex - columnStartIndex - 1);
				if (opposingHandedness == "L")
					thisInputVariables[1] = player2016Stats.averagePpgVsLefty;
				else
					thisInputVariables[1] = player2016Stats.averagePpgVsRighty;
				thisInputVariables[0] = player2016Stats.averagePpg;

				size_t playerIdIndex = actualResults.find(";" + thisPlayerId + ";", 0);
				if (playerIdIndex != string::npos && thisInputVariables[1] > 0)
				{
					inputVariables.push_back(thisInputVariables);
					for (int i = 0; i < 6; ++i)
					{
						playerIdIndex = actualResults.find(";", playerIdIndex + 1);
					}
					size_t nextPlayerIdIndex = actualResults.find(";", playerIdIndex + 1);
					outputValues.push_back(stof(actualResults.substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1).c_str()));
				}

				finalWriteString += resultsLine;
				finalWriteString += "\n";
			}
			resultsTrackerFile.close();
		}

		float fCoefficientStep = 0.01f;
		float mostAccurateCoefficients[2] = { 0,0 };
		float mostAccurateWrongAmount = INFINITY;
		float mostAccurateRSquaredCoefficients[2] = { 0,0 };
		float bestRSquared = 0;
		while (inputCoefficients[0] <= 1.0f + fCoefficientStep * 0.5f)
		{
			float wrongAmount = 0.0f;
			for (unsigned int i = 0; i < inputVariables.size(); ++i)
			{
				wrongAmount += (outputValues[i] - (inputCoefficients[0] * inputVariables[i][0] + inputCoefficients[1] * inputVariables[i][1])) * (outputValues[i] - (inputCoefficients[0] * inputVariables[i][0] + inputCoefficients[1] * inputVariables[i][1]));
				//wrongAmount -= (outputValues[i] - (inputCoefficients[0] * inputVariables[i][0] + inputCoefficients[1] * inputVariables[i][1]));
			}
			if (wrongAmount < mostAccurateWrongAmount)
			{
				mostAccurateWrongAmount = wrongAmount;
				mostAccurateCoefficients[0] = inputCoefficients[0];
				mostAccurateCoefficients[1] = inputCoefficients[1];
			}

			vector< float > finalInputs;
			for (unsigned int i = 0; i < inputVariables.size(); ++i)
			{
				finalInputs.push_back((inputCoefficients[0] * inputVariables[i][0] + inputCoefficients[1] * inputVariables[i][1]));
			}
			float thisRSquared = CalculateRSquared(finalInputs, outputValues);
			if (thisRSquared > bestRSquared)
			{
				bestRSquared = thisRSquared;
				mostAccurateRSquaredCoefficients[0] = inputCoefficients[0];
				mostAccurateRSquaredCoefficients[1] = inputCoefficients[1];
			}
			inputCoefficients[0] += fCoefficientStep;
			inputCoefficients[1] -= fCoefficientStep;
		}

		inputCoefficients[0] = inputCoefficients[0];

	//	ofstream writeResultsTrackerFile(resultsTrackerFileName);
	//	writeResultsTrackerFile << finalWriteString;
	//	writeResultsTrackerFile.close();
	}
}

void ChooseAPitcher()
{
	CURL *curl;
	//CURLcode res;  

	curl = curl_easy_init();
	if (curl)
	{
		std::string readBuffer;
		string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=1&sort=4&game=d&colA=0&daypt=0&denom=3&xavg=0&inact=0&maxprc=99999&sched=1&starters=1&hithand=0&numlist=c";
		curl_easy_setopt(curl, CURLOPT_URL, thisPositionURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		string team2016OffensiveData = GetEntireFileContents("Team2016DataCached\\TeamOffense.txt");
		string team2017StrikeoutData;
		curl_easy_setopt(curl, CURLOPT_URL, "https://www.teamrankings.com/mlb/stat/strikeouts-per-game");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &team2017StrikeoutData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);
		string team2017RunsPerGameData;
		curl_easy_setopt(curl, CURLOPT_URL, "https://www.teamrankings.com/mlb/stat/runs-per-game");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &team2017RunsPerGameData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

	
		vector<PlayerData> positionalPlayerData;

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

			// player's team code
			placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			string playerTeamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str();

			// player salary
			placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			singlePlayerData.playerSalary = atoi(readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str());

			// game name
			for (int i = 0; i < 19; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}

			FullSeasonStatsAdvanced pitcherVBatter2017Stats = GetPitcherAdvancedStats(singlePlayerData.playerId, "2017", curl);
			FullSeasonStatsAdvanced pitcherVBatter2016Stats = GetPitcherAdvancedStats(singlePlayerData.playerId, "2016", curl);
			FullSeasonStatsAdvanced pitcherVBatterCareerStats = GetPitcherAdvancedStats(singlePlayerData.playerId, "Total", curl);
			
			if (pitcherVBatter2016Stats.opsVersusLefty >= 0)
			{
				pitcherVBatterCareerStats.opsVersusLefty = 0.5f * pitcherVBatterCareerStats.opsVersusLefty + 0.5f * pitcherVBatter2016Stats.opsVersusLefty;
				pitcherVBatterCareerStats.isoVersusLefty = 0.5f * pitcherVBatterCareerStats.isoVersusLefty + 0.5f * pitcherVBatter2016Stats.isoVersusLefty;
				pitcherVBatterCareerStats.wobaVersusLefty = 0.5f * pitcherVBatterCareerStats.wobaVersusLefty + 0.5f * pitcherVBatter2016Stats.wobaVersusLefty;
				pitcherVBatterCareerStats.sluggingVersusLefty = 0.5f * pitcherVBatterCareerStats.sluggingVersusLefty + 0.5f * pitcherVBatter2016Stats.sluggingVersusLefty;
			}
			if (pitcherVBatter2016Stats.opsVersusRighty >= 0)
			{
				pitcherVBatterCareerStats.opsVersusRighty = 0.5f * pitcherVBatterCareerStats.opsVersusRighty + 0.5f * pitcherVBatter2016Stats.opsVersusRighty;
				pitcherVBatterCareerStats.isoVersusRighty = 0.5f * pitcherVBatterCareerStats.isoVersusRighty + 0.5f * pitcherVBatter2016Stats.isoVersusRighty;
				pitcherVBatterCareerStats.wobaVersusRighty = 0.5f * pitcherVBatterCareerStats.wobaVersusRighty + 0.5f * pitcherVBatter2016Stats.wobaVersusRighty;
				pitcherVBatterCareerStats.sluggingVersusRighty = 0.5f * pitcherVBatterCareerStats.sluggingVersusRighty + 0.5f * pitcherVBatter2016Stats.sluggingVersusRighty;
			}
			if (pitcherVBatter2017Stats.opsVersusLefty >= 0)
			{
				pitcherVBatterCareerStats.opsVersusLefty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.opsVersusLefty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.opsVersusLefty;
				pitcherVBatterCareerStats.isoVersusLefty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.isoVersusLefty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.isoVersusLefty;
				pitcherVBatterCareerStats.wobaVersusLefty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.wobaVersusLefty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.wobaVersusLefty;
				pitcherVBatterCareerStats.sluggingVersusLefty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.sluggingVersusLefty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.sluggingVersusLefty;
			}
			if (pitcherVBatter2017Stats.opsVersusRighty >= 0)
			{
				pitcherVBatterCareerStats.opsVersusRighty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.opsVersusRighty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.opsVersusRighty;
				pitcherVBatterCareerStats.isoVersusRighty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.isoVersusRighty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.isoVersusRighty;
				pitcherVBatterCareerStats.wobaVersusRighty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.wobaVersusRighty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.wobaVersusRighty;
				pitcherVBatterCareerStats.sluggingVersusRighty = (1.0f - percentOf2017SeasonPassed) * pitcherVBatterCareerStats.sluggingVersusRighty + percentOf2017SeasonPassed * pitcherVBatter2017Stats.sluggingVersusRighty;
			}
			string opponentTeamCode = "";
			auto opponent = opponentMap.find(playerTeamCode);
			if (opponent != opponentMap.end())
			{
				opponentTeamCode = opponent->second.teamCode;
				auto myTeam = opponentMap.find(opponentTeamCode);
				if (myTeam != opponentMap.end())
				{
					myTeam->second.pitcherAdvancedStats = pitcherVBatterCareerStats;
				}
			}

			// now look up 2016 points per game
			singlePlayerData.playerPointsPerGame = 0;
			FullSeasonPitcherStats newPitcherStats = GetPitcherStats(singlePlayerData.playerId, "2017", curl);
			FullSeasonPitcherStats pitcherStats = GetPitcherStats(singlePlayerData.playerId, "2016", curl);
			FullSeasonPitcherStats pitcherCareerStats = GetPitcherStats(singlePlayerData.playerId, "Total", curl);
			// default to average
			float opponentRunsPerGame = 4.4f;
			float opponentStrikeoutsPerGame = 8.1f;

			auto opponentsInfo = opponentMap.find(playerTeamCode);

			if (opponentsInfo != opponentMap.end())
			{
				size_t opponentTeamIndex = team2016OffensiveData.find(";" + opponentsInfo->second.teamCode + ";", 0);
				opponentTeamIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
				size_t opponentTeamNextIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
				opponentRunsPerGame = stof(team2016OffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());

				opponentTeamIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
				opponentTeamNextIndex = team2016OffensiveData.find("\n", opponentTeamIndex + 1);
				opponentStrikeoutsPerGame = stof(team2016OffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
			}
			if (pitcherStats.strikeOutsPer9 >= 0 && pitcherCareerStats.strikeOutsPer9 >= 0)
			{
				pitcherStats.era = 0.5f * pitcherStats.era + 0.5f * pitcherCareerStats.era;
				pitcherStats.fip = 0.5f * pitcherStats.fip + 0.5f * pitcherCareerStats.fip;
				pitcherStats.strikeOutsPer9 = 0.5f * pitcherStats.strikeOutsPer9 + 0.5f * pitcherCareerStats.strikeOutsPer9;
			}
			else if (pitcherCareerStats.strikeOutsPer9 >= 0)
			{
				pitcherStats = pitcherCareerStats;
			}

			if (pitcherStats.strikeOutsPer9 >= 0)
			{
				if (newPitcherStats.strikeOutsPer9 >= 0)
				{
					pitcherStats.era *= 1.0f - percentOf2017SeasonPassed;
					pitcherStats.fip *= 1.0f - percentOf2017SeasonPassed;
					pitcherStats.strikeOutsPer9 *= 1.0f - percentOf2017SeasonPassed;

					pitcherStats.era += newPitcherStats.era * percentOf2017SeasonPassed;
					pitcherStats.fip += newPitcherStats.fip * percentOf2017SeasonPassed;
					pitcherStats.strikeOutsPer9 += newPitcherStats.strikeOutsPer9 * percentOf2017SeasonPassed;
				}
			}
			else
			{
				// for now, no rookies.
				//pitcherStats = newPitcherStats;
			}

			if (opponentsInfo != opponentMap.end())
			{
				opponentRunsPerGame *= max(0.0f, 1.0f - (percentOf2017SeasonPassed * 2.0f));
				opponentStrikeoutsPerGame *= max(0.0f, 1.0f - (percentOf2017SeasonPassed * 2.0f));

				size_t opponentTeamIndex = team2017RunsPerGameData.find(">" + opponentsInfo->second.rankingsSiteTeamName + "<", 0);
				opponentTeamIndex = team2017RunsPerGameData.find("data-sort=", opponentTeamIndex + 1);
				opponentTeamIndex = team2017RunsPerGameData.find(">", opponentTeamIndex + 1);
				size_t opponentTeamNextIndex = team2017RunsPerGameData.find("<", opponentTeamIndex + 1);
				float temp = stof(team2017RunsPerGameData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
				opponentRunsPerGame += stof(team2017RunsPerGameData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str()) * min(1.0f, percentOf2017SeasonPassed * 2.0f);

				opponentTeamIndex = team2017StrikeoutData.find(">" + opponentsInfo->second.rankingsSiteTeamName + "<", 0);
				opponentTeamIndex = team2017StrikeoutData.find("data-sort=", opponentTeamIndex + 1);
				opponentTeamIndex = team2017StrikeoutData.find(">", opponentTeamIndex + 1);
				opponentTeamNextIndex = team2017StrikeoutData.find("<", opponentTeamIndex + 1);
				temp = stof(team2017StrikeoutData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
				opponentStrikeoutsPerGame += stof(team2017StrikeoutData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str()) * min(1.0f, percentOf2017SeasonPassed * 2.0f);
			}

			//fip,			era,			kper9,	oppRunsPerGame,	oppKPer9
			//0.200000003, 0.600000024, 0.500000000, 1.00000012, 0.500000000
			//0.200000033, 0.539999962, 0.479999930, 0.959999919, 0.429999977
			//0.300000012, 0.600000024, 0.500000000, 1.00000012, 0.400000006
			float pitcherInputCoefficients[5] = { 0.2f, 0.6f, 0.5f, 1.0f, 0.4f };
			float pitcherInputStats[5] = { 0,0,0,0,0 };
			pitcherInputStats[0] = 9.0f / max(pitcherStats.fip, 1.0f);
			pitcherInputStats[1] = 9.0f / max(pitcherStats.era, 1.0f);
			pitcherInputStats[2] = pitcherStats.strikeOutsPer9 / 9.0f;
			pitcherInputStats[3] = 9.0f / max(opponentRunsPerGame, 1.0f);
			pitcherInputStats[4] = opponentStrikeoutsPerGame / 9.0f;

			// attempt to normalize variables to be average fantasy score of 16.5
			pitcherInputStats[0] *= 6.4166667f;
			pitcherInputStats[1] *= 6.4166667f;
			pitcherInputStats[2] *= 27.0f;
			pitcherInputStats[3] *= 6.4166667f;
			pitcherInputStats[4] *= 27.0f;

			singlePlayerData.playerPointsPerGame = 0;
			for (int i = 0; i < 5; ++i)
			{
				singlePlayerData.playerPointsPerGame += pitcherInputStats[i] * pitcherInputCoefficients[i];
			}
			/*
			int gameStartTime = 24;
			size_t colonIndex = readBuffer.find(":", placeHolderIndex + 1);
			size_t nextSemiColonIndex = readBuffer.find(";", placeHolderIndex + 1);
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
			}
			else if (readBuffer.find("Final", placeHolderIndex + 1) != string::npos && readBuffer.find("Final", placeHolderIndex + 1) < nextSemiColonIndex)
			{
				// game has gone final
				gameStartTime = 999;
			}
			*/
			
			bool bRainedOut = false;
			if (opponentsInfo != opponentMap.end())
			{
				size_t closestRainOutPark = string::npos;
				for (unsigned int i = 0; i < probableRainoutGames.size(); ++i)
				{
					if (opponentsInfo->second.ballParkPlayedIn == probableRainoutGames[i])
					{
						bRainedOut = true;
						break;
					}
				}
			}
			
			/*
			// throw this guy out if his game will most likely be rained out
			if (gameStartTime <= latestGameTime && gameStartTime >= earliestGameTime && !bRainedOut)
			*/
			if (pitcherStats.strikeOutsPer9 >= 0 && !bRainedOut)
				positionalPlayerData.push_back(singlePlayerData);
			if (placeHolderIndex == string::npos)
				break;
			else
				placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
		}

		sort(positionalPlayerData.begin(), positionalPlayerData.end(), comparePlayerByPointsPerGame);
		curl_easy_cleanup(curl);
	}
}

void GenerateNewLineup()
{
	CURL *curl;
	//CURLcode res;

	string ballParkFactorData = GetEntireFileContents("BallparkFactors.txt");

	curl = curl_easy_init();
	if (curl)
	{
	  for (int p = 2; p <= 7; ++p)
	  {
		  // first basemen
		  std::string readBuffer;
		  char pAsString[5];
		  _itoa_s(p, pAsString, 10);
		  string pAsStringString(pAsString);
		  string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=" + pAsStringString + "&sort=6&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=0&hithand=1&numlist=c";

		  curl_easy_setopt(curl, CURLOPT_URL, thisPositionURL.c_str());
		  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		  curl_easy_perform(curl);
		  curl_easy_reset(curl);

		  vector<PlayerData> positionalPlayerData;

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
			string playerTeamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);

			// player salary
			for (int i = 0; i < 1; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			singlePlayerData.playerSalary = atoi(readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str());

			// number of games started this season
			for (int i = 0; i < 3; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			float thisSeasonPercent = (float)atoi(readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str());

			// player ppg this season
			for (int i = 0; i < 3; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			singlePlayerData.playerPointsPerGame = 0;
			singlePlayerData.playerPointsPerGame = stof((readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str()));

			// player ppg last 30 days
			for (int i = 0; i < 2; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			float last30DaysPointsPerGame = stof((readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str()));

			// batter handedness
			for (int i = 0; i < 6; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			if (readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1) == "L")
				singlePlayerData.batsLeftHanded = true;
			else
				singlePlayerData.batsLeftHanded = false;

			// opposing pitcher handedness
			for (int i = 0; i < 4; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}
			nextIndex = readBuffer.find(";", placeHolderIndex + 1);
			if (readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1) == "L")
			{
				singlePlayerData.isFacingLefthander = true;
				// about 30% of starters are left handed
				thisSeasonPercent /= (162.0f * 0.3f);
			}
			else
			{
				singlePlayerData.isFacingLefthander = false;
				// about 70% of starters are right handed
				thisSeasonPercent /= (162.0f * 0.5f);
			}
			if (thisSeasonPercent > 1)
				thisSeasonPercent = 1;

			// game name
			for (int i = 0; i < 1; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}

			// now look up 2016 points per game
			singlePlayerData.playerPointsPerGame *= thisSeasonPercent;
			FullSeasonStats player2016Stats = GetBatter2016Stats(singlePlayerData.playerId, curl);
			if (singlePlayerData.isFacingLefthander)
				singlePlayerData.playerPointsPerGame += player2016Stats.averagePpgVsLefty * (1.0f - thisSeasonPercent);
			else
				singlePlayerData.playerPointsPerGame += player2016Stats.averagePpgVsRighty * (1.0f - thisSeasonPercent);
			
			// now factor in opposing pitcher
			float pitcherFactor = 1.0f;
			float ballParkFactor = 1.0f;
			FullSeasonStatsAdvanced opposingPitcherAdvancedStats;
			auto opponentInformation = opponentMap.find(playerTeamCode);
			if (opponentInformation != opponentMap.end())
			{
				opposingPitcherAdvancedStats = opponentInformation->second.pitcherAdvancedStats;
				size_t ballParkIndex = ballParkFactorData.find(opponentInformation->second.ballParkPlayedIn, 0);
				if (ballParkIndex != string::npos)
				{
					ballParkIndex = ballParkFactorData.find(";SLG;", ballParkIndex);
					size_t ballParkEndIndex;
					if (singlePlayerData.batsLeftHanded)
					{
						ballParkIndex += 4;
						ballParkEndIndex = ballParkFactorData.find("\n", ballParkIndex);
					}
					else
					{
						ballParkEndIndex = ballParkIndex;
						ballParkIndex = ballParkFactorData.rfind("\n", ballParkEndIndex);
					}
					if (ballParkIndex != string::npos && ballParkEndIndex != string::npos)
						ballParkFactor = stof(ballParkFactorData.substr(ballParkIndex + 1, ballParkEndIndex - ballParkIndex - 1));
				}
			}
	//		if (singlePlayerData.playerName.find("Kinsler") != string::npos)
	//			singlePlayerData = singlePlayerData;
			if ((singlePlayerData.batsLeftHanded && opposingPitcherAdvancedStats.opsVersusLefty >= 0) ||
				(!singlePlayerData.batsLeftHanded && opposingPitcherAdvancedStats.opsVersusRighty >= 0))
			{
				//ops to points = 5.89791155 * ops + 3.76171160
				float averagePointsPerGame = 5.89791155f * leagueAverageOps + 3.76171160f;
				float opposingPitcherAveragePointsAllowed = 3.76171160f;
				if (singlePlayerData.batsLeftHanded)
					opposingPitcherAveragePointsAllowed += 5.89791155f * opposingPitcherAdvancedStats.opsVersusLefty;
				else
					opposingPitcherAveragePointsAllowed += 5.89791155f * opposingPitcherAdvancedStats.opsVersusRighty;
				pitcherFactor = opposingPitcherAveragePointsAllowed / averagePointsPerGame;
			}
			singlePlayerData.playerPointsPerGame *= pitcherFactor * ballParkFactor;
			

			
			int gameStartTime = 24;
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
			}
			else if (readBuffer.find("Final", placeHolderIndex + 1) != string::npos && readBuffer.find("Final", placeHolderIndex + 1) < nextSemiColonIndex)
			{
				// game has gone final
				gameStartTime = 999;
			}
			else if (readBuffer.find("Mid", placeHolderIndex + 1) != string::npos && readBuffer.find("Mid", placeHolderIndex + 1) < nextSemiColonIndex)
			{
				// game is in progress
				gameStartTime = 999;
			}
			else if (readBuffer.find("Top", placeHolderIndex + 1) != string::npos && readBuffer.find("Top", placeHolderIndex + 1) < nextSemiColonIndex)
			{
				// game is in progress
				gameStartTime = 999;
			}
			else if (readBuffer.find("Bot", placeHolderIndex + 1) != string::npos && readBuffer.find("Bot", placeHolderIndex + 1) < nextSemiColonIndex)
			{
				// game is in progress
				gameStartTime = 999;
			}

			int numGamesPlayed2016 = player2016Stats.totalGamesStarted;
				
			// go to next player
			size_t closestRainOutPark = string::npos;
			for (unsigned int i = 0; i < probableRainoutGames.size(); ++i)
			{
				size_t thisRainoutPark = readBuffer.find(probableRainoutGames[i], placeHolderIndex);
				if (thisRainoutPark < closestRainOutPark)
					closestRainOutPark = thisRainoutPark;
			}
			bool bRainedOut = closestRainOutPark != string::npos && closestRainOutPark < readBuffer.find("\n", placeHolderIndex + 1);
			// throw this guy out if he's not a starter or his game will most likely be rained out
			if (numGamesPlayed2016 >= minGamesPlayed2016 && gameStartTime <= latestGameTime && gameStartTime >= earliestGameTime && !bRainedOut)
				positionalPlayerData.push_back(singlePlayerData);
			
			if (placeHolderIndex == string::npos)
				break;
			else
				placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
		  }

		  sort(positionalPlayerData.begin(), positionalPlayerData.end(), comparePlayerByPointsPerGame);
		  
		  allPlayers.push_back(positionalPlayerData);
	  }
	  curl_easy_cleanup(curl);
  }

  ofstream resultsTrackerFile;
  string resultsTrackerFileName = "2017ResultsTracker\\" + todaysDate + ".txt";
  resultsTrackerFile.open(resultsTrackerFileName);
  for (unsigned int i = 0; i < allPlayers.size(); ++i)
  {
	  for (unsigned int p = 0; p < allPlayers[i].size(); ++p)
	  {
		  resultsTrackerFile << allPlayers[i][p].playerId << ";" << allPlayers[i][p].playerName;
		  if (allPlayers[i][p].isFacingLefthander)
			  resultsTrackerFile << ";L;";
		  else
			  resultsTrackerFile << ";R;";
		  resultsTrackerFile << endl;
	  }
  }
  resultsTrackerFile.close();

  for (unsigned int ap = 0; ap < allPlayers.size(); ++ap)
  {
	  for (int i = allPlayers[ap].size() - 1; i > 0; --i)
	  {
		  bool bDeleteThisPlayer = false;
		  int numBetterValuePlayers = 0;
		  for (int x = i - 1; x >= 0; --x)
		  {
			  if (allPlayers[ap][i].playerSalary >= allPlayers[ap][x].playerSalary)
			  {
				  numBetterValuePlayers++;
				  if (ap != 5 || numBetterValuePlayers >= 3)
				  {
					  bDeleteThisPlayer = true;
					  break;
				  }
			  }
		  }

		  if (bDeleteThisPlayer)
		  {
			  allPlayers[ap].erase(allPlayers[ap].begin() + i);
		  }
	  }
  }
  for (unsigned int i = 0; i < allPlayers.size(); ++i)
  {
	  if (allPlayers[i].size() == 0)
	  {
		  cout << "Position " << i << " has no available players." << endl;
		  return;
	  }
  }
  vector<PlayerData> chosenLineup = OptimizeLineupToFitBudget();
  
}

vector<PlayerData> OptimizeLineupToFitBudget()
{
	vector<unsigned int> idealPlayerPerPosition;
	for (unsigned int i = 0; i < allPlayers.size(); ++i)
	{
		idealPlayerPerPosition.push_back(0);
	}
	idealPlayerPerPosition.push_back(1);
	idealPlayerPerPosition.push_back(2);

	int totalSalary = 0;
	for (unsigned int i = 0; i < idealPlayerPerPosition.size(); ++i)
	{
		unsigned int positionIndex = i;
		if (positionIndex >= allPlayers.size())
			positionIndex = allPlayers.size() - 1;
		totalSalary += allPlayers[positionIndex][idealPlayerPerPosition[i]].playerSalary;
	}

	while (totalSalary > maxTotalBudget)
	{
		int playerIndexToDrop = -1;
		int biggestSalaryDrop = 0;
		float smallestPointDrop = 99999;
		for (unsigned int i = 0; i < idealPlayerPerPosition.size(); ++i)
		{
			bool bIsOutfield = false;
			unsigned int positionIndex = i;
			if (positionIndex >= allPlayers.size() - 1)
			{
				positionIndex = allPlayers.size() - 1;
				bIsOutfield = true;
			}
			if (idealPlayerPerPosition[i] < allPlayers[positionIndex].size() - 1)
			{
				if (!bIsOutfield || i == idealPlayerPerPosition.size() - 1 || idealPlayerPerPosition[i + 1] - idealPlayerPerPosition[i] > 1)
				{
					int salaryDrop = allPlayers[positionIndex][idealPlayerPerPosition[i]].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].playerSalary;
					float pointDrop = allPlayers[positionIndex][idealPlayerPerPosition[i]].playerPointsPerGame - allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].playerPointsPerGame;
					if ((pointDrop < smallestPointDrop) || (abs(pointDrop - smallestPointDrop) < 0.1f && salaryDrop > biggestSalaryDrop))
					{
						playerIndexToDrop = i;
						smallestPointDrop = pointDrop;
						biggestSalaryDrop = salaryDrop;
					}
				}
			}
		}
		idealPlayerPerPosition[playerIndexToDrop]++;
		totalSalary -= biggestSalaryDrop;
	}

	// the outfielders can get out of order, maximize them at the end
	for (unsigned int i = idealPlayerPerPosition.size() - 3; i < idealPlayerPerPosition.size(); ++i)
	{
		int startingIndex = 0;
		if (i > idealPlayerPerPosition.size() - 3)
		{
			startingIndex = idealPlayerPerPosition[i - 1] + 1;
		}
		for (unsigned int pl = 0; pl < idealPlayerPerPosition[i]; ++pl)
		{
			if (i == idealPlayerPerPosition.size() - 2)
			{
				if (pl == idealPlayerPerPosition[i - 1])
					continue;
			}
			else if (i == idealPlayerPerPosition.size() - 1)
			{
				if (pl == idealPlayerPerPosition[i - 1] || pl == idealPlayerPerPosition[i - 2])
					continue;
			}
			int salaryIncrease = allPlayers[allPlayers.size() - 1][pl].playerSalary - allPlayers[allPlayers.size() - 1][idealPlayerPerPosition[i]].playerSalary;
			if (salaryIncrease <= 0)
			{
				idealPlayerPerPosition[i] = pl;
				totalSalary += salaryIncrease;
				break;
			}
		}
	}
	// make sure we keep the outfielders in best to worst order
	if (idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] < idealPlayerPerPosition[idealPlayerPerPosition.size() - 3])
	{
		int temp = idealPlayerPerPosition[idealPlayerPerPosition.size() - 1];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] = idealPlayerPerPosition[idealPlayerPerPosition.size() - 3];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 3] = temp;
	}
	if (idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] < idealPlayerPerPosition[idealPlayerPerPosition.size() - 2])
	{
		int temp = idealPlayerPerPosition[idealPlayerPerPosition.size() - 1];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] = idealPlayerPerPosition[idealPlayerPerPosition.size() - 2];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 2] = temp;
	}
	if (idealPlayerPerPosition[idealPlayerPerPosition.size() - 2] < idealPlayerPerPosition[idealPlayerPerPosition.size() - 3])
	{
		int temp = idealPlayerPerPosition[idealPlayerPerPosition.size() - 2];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 2] = idealPlayerPerPosition[idealPlayerPerPosition.size() - 3];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 3] = temp;
	}
	// we might have freed some salary, see if we are now able to get a better player
	unsigned int positionIndex = 1;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 3;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 2;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 0;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 4;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}

	for (unsigned int i = 0; i < idealPlayerPerPosition.size(); ++i)
	{
		positionIndex = i;
		if (positionIndex >= allPlayers.size())
		{
			positionIndex = allPlayers.size() - 1;
		}
		for (unsigned int b = 0; b < idealPlayerPerPosition[i]; ++b)
		{
			if (i == idealPlayerPerPosition.size() - 2)
			{
				if (b == idealPlayerPerPosition[i - 1])
					continue;
			}
			else if (i == idealPlayerPerPosition.size() - 1)
			{
				if (b == idealPlayerPerPosition[i - 1] || b == idealPlayerPerPosition[i - 2])
					continue;
			}

			bool bSwappedPlayers = false;
			float pointsGained = allPlayers[positionIndex][b].playerPointsPerGame - allPlayers[positionIndex][idealPlayerPerPosition[i]].playerPointsPerGame;
			int salaryNeeded = allPlayers[positionIndex][b].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[i]].playerSalary;
			for (unsigned int swappee = 0; swappee < idealPlayerPerPosition.size(); ++swappee)
			{
				unsigned swappeePositionIndex = swappee;
				if (swappeePositionIndex >= allPlayers.size())
					swappeePositionIndex = allPlayers.size() - 1;
				if (swappeePositionIndex == positionIndex)
					continue;
				for (unsigned int bs = idealPlayerPerPosition[swappee] + 1; bs < allPlayers[swappeePositionIndex].size(); ++bs)
				{
					int swappedSalaryGained = allPlayers[swappeePositionIndex][idealPlayerPerPosition[swappee]].playerSalary - allPlayers[swappeePositionIndex][bs].playerSalary;
					float pointsLost = allPlayers[swappeePositionIndex][idealPlayerPerPosition[swappee]].playerPointsPerGame - allPlayers[swappeePositionIndex][bs].playerPointsPerGame;
					if (pointsGained > pointsLost && totalSalary - swappedSalaryGained + salaryNeeded <= maxTotalBudget)
					{
						// we should swap to gain more points for equal or less salary
						idealPlayerPerPosition[i] = b;
						idealPlayerPerPosition[swappee] = bs;
						totalSalary = totalSalary - swappedSalaryGained + salaryNeeded;
						bSwappedPlayers = true;
						break;
					}
				}
				if (bSwappedPlayers)
					break;
			}
			if (bSwappedPlayers)
				break;
		}
	}

	// now again after doing swapping
	// the outfielders can get out of order, maximize them at the end
	for (unsigned int i = idealPlayerPerPosition.size() - 3; i < idealPlayerPerPosition.size(); ++i)
	{
		int startingIndex = 0;
		if (i > idealPlayerPerPosition.size() - 3)
		{
			startingIndex = idealPlayerPerPosition[i - 1] + 1;
		}
		for (unsigned int pl = 0; pl < idealPlayerPerPosition[i]; ++pl)
		{
			if (i == idealPlayerPerPosition.size() - 2)
			{
				if (pl == idealPlayerPerPosition[i - 1])
					continue;
			}
			else if (i == idealPlayerPerPosition.size() - 1)
			{
				if (pl == idealPlayerPerPosition[i - 1] || pl == idealPlayerPerPosition[i - 2])
					continue;
			}
			int salaryIncrease = allPlayers[allPlayers.size() - 1][pl].playerSalary - allPlayers[allPlayers.size() - 1][idealPlayerPerPosition[i]].playerSalary;
			if (salaryIncrease <= 0)
			{
				idealPlayerPerPosition[i] = pl;
				totalSalary += salaryIncrease;
				break;
			}
		}
	}
	// make sure we keep the outfielders in best to worst order
	if (idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] < idealPlayerPerPosition[idealPlayerPerPosition.size() - 3])
	{
		int temp = idealPlayerPerPosition[idealPlayerPerPosition.size() - 1];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] = idealPlayerPerPosition[idealPlayerPerPosition.size() - 3];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 3] = temp;
	}
	if (idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] < idealPlayerPerPosition[idealPlayerPerPosition.size() - 2])
	{
		int temp = idealPlayerPerPosition[idealPlayerPerPosition.size() - 1];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 1] = idealPlayerPerPosition[idealPlayerPerPosition.size() - 2];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 2] = temp;
	}
	if (idealPlayerPerPosition[idealPlayerPerPosition.size() - 2] < idealPlayerPerPosition[idealPlayerPerPosition.size() - 3])
	{
		int temp = idealPlayerPerPosition[idealPlayerPerPosition.size() - 2];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 2] = idealPlayerPerPosition[idealPlayerPerPosition.size() - 3];
		idealPlayerPerPosition[idealPlayerPerPosition.size() - 3] = temp;
	}
	// we might have freed some salary, see if we are now able to get a better player
	positionIndex = 1;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 3;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 2;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 0;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	positionIndex = 4;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
		if (salaryIncrease + totalSalary <= maxTotalBudget)
		{
			totalSalary += salaryIncrease;
			idealPlayerPerPosition[positionIndex] = pl;
			break;
		}
	}
	

	vector<PlayerData> playersToReturn;
	for (unsigned int i = 0; i < idealPlayerPerPosition.size(); ++i)
	{
		unsigned int positionIndex = i;
		if (positionIndex >= allPlayers.size())
			positionIndex = allPlayers.size() - 1;
		playersToReturn.push_back(allPlayers[positionIndex][idealPlayerPerPosition[i]]);
	}

	return playersToReturn;
}

void PopulateProbableRainoutGames()
{
	CURL *curl;

	curl = curl_easy_init();
	if (curl)
	{
		string weatherData;
		string weatherURL = "http://dailybaseballdata.com/cgi-bin/weather.pl?scsv=1";
		curl_easy_setopt(curl, CURLOPT_URL, weatherURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &weatherData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		string teamCodesData = GetEntireFileContents("TeamCodes.txt");

		size_t weatherDataBeginIndex = weatherData.find("Air Density Index:", 0);
		while (weatherDataBeginIndex != string::npos)
		{
			bool bProbableRainout = false;
			size_t nextWeatherDataRow = weatherData.find("Air Density Index:", weatherDataBeginIndex + 1);
			size_t roofCovered = weatherData.find("retractable roof", weatherDataBeginIndex + 1);
			size_t domeCovered = weatherData.find("dome", weatherDataBeginIndex + 1);
			if (roofCovered == string::npos || domeCovered < roofCovered)
				roofCovered = domeCovered;
			
			if (roofCovered == string::npos || roofCovered > nextWeatherDataRow)
			{
				int gameTimePercipByHour[6] { 0,0,0,0,0 };
				size_t precipPercentEnd = weatherData.find("Precip%:", weatherDataBeginIndex + 1);
				precipPercentEnd += 8;
				for (int i = 0; i < 4; ++i)
				{
					precipPercentEnd = weatherData.find("%", precipPercentEnd + 1);
				}
				for (int i = 0; i < 6; ++i)
				{
					size_t precipPercentStart = weatherData.rfind(">", precipPercentEnd);
					gameTimePercipByHour[i] = atoi(weatherData.substr(precipPercentStart + 1, precipPercentEnd - precipPercentStart - 1).c_str());
					precipPercentEnd = weatherData.find("%", precipPercentEnd + 1);
				}
				for (int i = 0; i < 4; ++i)
				{
					if (gameTimePercipByHour[i] + gameTimePercipByHour[i+1] + gameTimePercipByHour[i+2] > 135)
					{
						bProbableRainout = true;
						break;
					}
				}				
			}
			size_t timeIndex = weatherData.rfind("DT - ", weatherDataBeginIndex);
			size_t markupIndex = weatherData.find("<", timeIndex);
			string ballparkName = weatherData.substr(timeIndex + 5, markupIndex - timeIndex - 5);
			if (bProbableRainout)
			{
				probableRainoutGames.push_back(ballparkName);
			}


			size_t dashIndex = weatherData.rfind(" – ", timeIndex);
			size_t atIndex = weatherData.rfind(" at ", dashIndex);
			string homeTeam = weatherData.substr(atIndex + 4, dashIndex - atIndex - 4);
			dashIndex = weatherData.rfind(">", atIndex);
			string awayTeam = weatherData.substr(dashIndex + 1, atIndex - dashIndex - 1);
			
			size_t teamNameIndex = teamCodesData.find(homeTeam, 0);
			for (int i = 0; i < 2; ++i)
			{
				teamNameIndex = teamCodesData.find(";", teamNameIndex + 1);
			}
			size_t teamNameEndIndex = teamCodesData.find(";", teamNameIndex + 1);
			string homeTeamAlternativeName = teamCodesData.substr(teamNameIndex + 1, teamNameEndIndex - teamNameIndex - 1);
			teamNameIndex = teamNameEndIndex;
			teamNameEndIndex = teamCodesData.find(";", teamNameIndex + 1);
			string homeTeamCode = teamCodesData.substr(teamNameIndex + 1, teamNameEndIndex - teamNameIndex - 1);

			teamNameIndex = teamCodesData.find(awayTeam, 0);
			for (int i = 0; i < 2; ++i)
			{
				teamNameIndex = teamCodesData.find(";", teamNameIndex + 1);
			}
			teamNameEndIndex = teamCodesData.find(";", teamNameIndex + 1);
			string awayTeamAlternativeName = teamCodesData.substr(teamNameIndex + 1, teamNameEndIndex - teamNameIndex - 1);
			teamNameIndex = teamNameEndIndex;
			teamNameEndIndex = teamCodesData.find(";", teamNameIndex + 1);
			string awayTeamCode = teamCodesData.substr(teamNameIndex + 1, teamNameEndIndex - teamNameIndex - 1);

			OpponentInformation homeTeamInformation;
			homeTeamInformation.ballParkPlayedIn = ballparkName;
			homeTeamInformation.weatherSiteTeamName = homeTeam;
			homeTeamInformation.teamCode = homeTeamCode;
			homeTeamInformation.rankingsSiteTeamName = homeTeamAlternativeName;

			OpponentInformation awayTeamInformation;
			awayTeamInformation.ballParkPlayedIn = ballparkName;
			awayTeamInformation.weatherSiteTeamName = awayTeam;
			awayTeamInformation.teamCode = awayTeamCode;
			awayTeamInformation.rankingsSiteTeamName = awayTeamAlternativeName;
			
			opponentMap.insert({ { homeTeamCode,awayTeamInformation },{ awayTeamCode,homeTeamInformation } });
			weatherDataBeginIndex = nextWeatherDataRow;
		}
	}
}

void Analyze2016Stats()
{
	bool bShouldAnalyzePitchers = false;
	bool bShoulAnalyzeBatters = true;
	CURL *curl;

	curl = curl_easy_init();
	if (curl)
	{
		if (bShouldAnalyzePitchers)
		{
			bool bShouldGatherPitcherData = false;
			bool bShouldTrainPitcherData = true;
			if (bShouldGatherPitcherData)
			{
				ofstream pitchersDataFile;
				string pitchersDataFileName = "Player2016AnalysisCached\\Pitchers.txt";
				pitchersDataFile.open(pitchersDataFileName);

				string total2016Data;
				/*
					curl_easy_setopt(curl, CURLOPT_URL, "http://rotoguru1.com/cgi-bin/mlb-dbd-2016.pl?user=GoldenExcalibur&key=G5970032941");
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &total2016Data);
					curl_easy_perform(curl);
					curl_easy_reset(curl);
			*/
					
				total2016Data = "";
				total2016Data = GetEntireFileContents("Player2016AnalysisCached\\RawData.txt");

				string team2016OffensiveData = GetEntireFileContents("Team2016DataCached\\TeamOffense.txt");

				size_t pitcherIndex = total2016Data.find("GID:MLB_ID", 0);
				pitcherIndex = total2016Data.find(":P:", pitcherIndex + 1);
				while (pitcherIndex != string::npos)
				{
					for (int i = 0; i < 3; ++i)
					{
						pitcherIndex = total2016Data.rfind(":", pitcherIndex - 1);
					}
					size_t previousIndex = total2016Data.rfind("\n", pitcherIndex);
					string playerId = total2016Data.substr(previousIndex + 1, pitcherIndex - previousIndex - 1);

					for (int i = 0; i < 2; ++i)
					{
						pitcherIndex = total2016Data.find(":", pitcherIndex + 1);
					}
					size_t nextIndex = total2016Data.find(":", pitcherIndex + 1);
					string playerName = total2016Data.substr(pitcherIndex + 1, nextIndex - pitcherIndex - 1);

					for (int i = 0; i < 5; ++i)
					{
						pitcherIndex = total2016Data.find(":", pitcherIndex + 1);
					}
					nextIndex = total2016Data.find(":", pitcherIndex + 1);
					string opposingTeamCode = total2016Data.substr(pitcherIndex + 1, nextIndex - pitcherIndex - 1);

					for (int i = 0; i < 25; ++i)
					{
						pitcherIndex = total2016Data.find(":", pitcherIndex + 1);
					}
					nextIndex = total2016Data.find(":", pitcherIndex + 1);
					string winString = total2016Data.substr(pitcherIndex + 1, nextIndex - pitcherIndex - 1);

					for (int i = 0; i < 1; ++i)
					{
						pitcherIndex = total2016Data.find(":", pitcherIndex + 1);
					}
					nextIndex = total2016Data.find(":", pitcherIndex + 1);
					string qualityStartString = total2016Data.substr(pitcherIndex + 1, nextIndex - pitcherIndex - 1);

					for (int i = 0; i < 1; ++i)
					{
						pitcherIndex = total2016Data.find(":", pitcherIndex + 1);
					}
					nextIndex = total2016Data.find(":", pitcherIndex + 1);
					string pointsInGameString = total2016Data.substr(pitcherIndex + 1, nextIndex - pitcherIndex - 1);
					float pointsInGame = 0;

					if (pointsInGameString != "")
					{
						pointsInGame = stof(pointsInGameString);
						if (winString == "W")
							pointsInGame -= 12.0f;
						if (qualityStartString == "1")
							pointsInGame += 4.0f;

						FullSeasonPitcherStats pitcher2016Stats = GetPitcherStats(playerId, "2016", curl);
						if (pitcher2016Stats.strikeOutsPer9 >= 0)
						{
							// Name GID inningsPerStart fip era k/9 oppR/G oppK/9 FDPcorrected

							size_t opponentTeamIndex = team2016OffensiveData.find(";" + opposingTeamCode + ";", 0);
							opponentTeamIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
							size_t opponentTeamNextIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
							float opponentRunsPerGame = stof(team2016OffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());

							opponentTeamIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
							opponentTeamNextIndex = team2016OffensiveData.find("\n", opponentTeamIndex + 1);
							float opponentStrikeoutsPerGame = stof(team2016OffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());

							int numPitcher2016Starts = 0;
							size_t rowStart = total2016Data.rfind("\n", pitcherIndex);
							for (int i = 0; i < 2; ++i)
							{
								rowStart = total2016Data.find(":", rowStart + 1);
							}
							size_t nameEndIndex = rowStart;
							for (int i = 0; i < 2; ++i)
							{
								nameEndIndex = total2016Data.find(":", nameEndIndex + 1);
							}
							string nameStringInFile = total2016Data.substr(rowStart, nameEndIndex - rowStart);
							size_t foundNameIndex = total2016Data.find(nameStringInFile, 0);
							while (foundNameIndex != string::npos)
							{
								numPitcher2016Starts++;
								foundNameIndex = total2016Data.find(nameStringInFile, foundNameIndex + 1);
							}

							// Name GID inningsPerStart fip era k/9 oppR/G oppK/9 FDPcorrected
							pitchersDataFile << playerName << ";" << playerId << ";" << pitcher2016Stats.numInnings / (float)numPitcher2016Starts << ";" << pitcher2016Stats.fip << ";" << pitcher2016Stats.era << ";" << pitcher2016Stats.strikeOutsPer9 << ";" << opponentRunsPerGame << ";" << opponentStrikeoutsPerGame << ";" << pointsInGame << endl;
						}
					}
					
					for (int i = 0; i < 11; ++i)
					{
						pitcherIndex = total2016Data.find(":", pitcherIndex + 1);
					}
					pitcherIndex = total2016Data.find(":P:", pitcherIndex + 1);
				}
			}
			if (bShouldTrainPitcherData)
			{
				vector< vector<float> > inputVariables;
				float inputCoefficients[5] = { 0.0f };
				vector< float > outputValues;
				string pitcherProcessedData = GetEntireFileContents("Player2016AnalysisCached\\Pitchers.txt");
				size_t pitcherIndex = pitcherProcessedData.find(";", 0);
				while (pitcherIndex != string::npos)
				{
					vector<float> thisInputVariables;
					for (int i = 0; i < 2; ++i)
					{
						pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					}
					size_t nextPitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					thisInputVariables.push_back(stof(pitcherProcessedData.substr(pitcherIndex + 1, nextPitcherIndex - pitcherIndex - 1).c_str()));

					pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					nextPitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					thisInputVariables.push_back(stof(pitcherProcessedData.substr(pitcherIndex + 1, nextPitcherIndex - pitcherIndex - 1).c_str()));

					pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					nextPitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					thisInputVariables.push_back(stof(pitcherProcessedData.substr(pitcherIndex + 1, nextPitcherIndex - pitcherIndex - 1).c_str()));

					pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					nextPitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					thisInputVariables.push_back(stof(pitcherProcessedData.substr(pitcherIndex + 1, nextPitcherIndex - pitcherIndex - 1).c_str()));

					pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					nextPitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					thisInputVariables.push_back(stof(pitcherProcessedData.substr(pitcherIndex + 1, nextPitcherIndex - pitcherIndex - 1).c_str()));

					thisInputVariables[0] = 9.0f / max(thisInputVariables[0], 1.0f);

					thisInputVariables[1] = 9.0f / max(thisInputVariables[1], 1.0f);

					thisInputVariables[2] = thisInputVariables[2] / 9.0f;

					thisInputVariables[3] = 9.0f / max(thisInputVariables[3], 1.0f);
					thisInputVariables[4] = thisInputVariables[4] / 9.0f;

					// attempt to normalize variables to be average fantasy score of 16.5
					thisInputVariables[0] *= 6.4166667f;
					thisInputVariables[1] *= 6.4166667f;
					thisInputVariables[2] *= 27.0f;
					thisInputVariables[3] *= 6.4166667f;
					thisInputVariables[4] *= 27.0f;

					inputVariables.push_back(thisInputVariables);

					pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
					nextPitcherIndex = pitcherProcessedData.find("\n", pitcherIndex + 1);
					outputValues.push_back(stof(pitcherProcessedData.substr(pitcherIndex + 1, nextPitcherIndex - pitcherIndex - 1).c_str()));
					//outputValues[outputValues.size() - 1] /= 9.0f;

					pitcherIndex = pitcherProcessedData.find(";", pitcherIndex + 1);
				}

				float outputAverage = 0;
				for (unsigned int i = 0; i < outputValues.size(); ++i)
				{
					outputAverage += outputValues[i] * 9.0f;
				}
				outputAverage /= (float)(outputValues.size());
				float fCoefficientStep = 0.1f;
				float mostAccurateCoefficients[5] = { 0 };
				float mostAccurateWrongAmount = INFINITY;
				float mostAccurateRSquared = -1;
				//		for (int g = 0; g < 5; ++g)
				{
					for (int assign = 0; assign < 5; ++assign)
					{
					//	inputCoefficients[assign] = 0.6f;
					}
					//	inputCoefficients[0] = 0.3f;
					//	inputCoefficients[1] = 0.4f;
					//	inputCoefficients[2] = 0.06f;
					//	inputCoefficients[3] = 0.21f;
					//	inputCoefficients[4] = 0.21f;
						// 3942

					//	inputCoefficients[0] = 0.14f;
					//	inputCoefficients[1] = 0.49f;
					//	inputCoefficients[2] = 0.1f;
					//	inputCoefficients[3] = 0.17f;
					//	inputCoefficients[4] = 0.23f;
					/*
					inputCoefficients[0] = 0.2f;
					inputCoefficients[1] = 0.97f;
					inputCoefficients[2] = 0.23f;
					inputCoefficients[3] = 0.08f;
					inputCoefficients[4] = 0.1f;
					// 13%
					*/
					//	inputCoefficients[1] = 0.36f;
					//	inputCoefficients[4] = 0.25f;
						// 15%

				//	inputCoefficients[0] = 0.13f;
				//	inputCoefficients[1] = 0.53f;
				//	inputCoefficients[2] = 0.45f;
				//	inputCoefficients[3] = 0.2f;
				//	inputCoefficients[4] = 0.15f;
					// 15.8%
					//	inputCoefficients[g] = 0.0f;

					//mostAccurateCoefficients = {0.200000003, 0.600000024, 0.500000000, 1.00000012, 0.500000000}
					//mostAccurateCoefficients = {0.200000033, 0.539999962, 0.479999930, 0.959999919, 0.429999977}
					//mostAccurateCoefficients = {0.300000012, 0.600000024, 0.500000000, 1.00000012, 0.400000006}
					inputCoefficients[0] = 0;
					while (inputCoefficients[0] <= 1.0f + fCoefficientStep * 0.5f)
					{
						inputCoefficients[1] = 0;
						while (inputCoefficients[1] <= 1.0f + fCoefficientStep * 0.5f)
						{
							inputCoefficients[2] = 0;
							while (inputCoefficients[2] <= 1.0f + fCoefficientStep * 0.5f)
							{
								inputCoefficients[3] = 0;
								while (inputCoefficients[3] <= 1.0f + fCoefficientStep * 0.5f)
								{
									inputCoefficients[4] = 0;

									while (inputCoefficients[4] <= 1.0f + fCoefficientStep * 0.5f)
									{
										/*	float wrongAmount = 0.0f;
											for (unsigned int i = 0; i < inputVariables.size(); ++i)
											{
												float expectedValue = 0;
												for (int e = 0; e < 5; ++e)
												{
													expectedValue += inputCoefficients[e] * inputVariables[i][e];
												}
												wrongAmount += (outputValues[i] - expectedValue) * (outputValues[i] - expectedValue);
												//wrongAmount -= (outputValues[i] - expectedValue);
											}

											if (wrongAmount < mostAccurateWrongAmount)
											{
												mostAccurateWrongAmount = wrongAmount;
												for (int a = 0; a < 5; ++a)
												{
													mostAccurateCoefficients[a] = inputCoefficients[a];
												}
											}
											*/
										vector<float> finalInputs;
										for (unsigned int i = 0; i < inputVariables.size(); ++i)
										{
											float expectedValue = 0;
											for (int e = 0; e < 5; ++e)
											{
												expectedValue += inputCoefficients[e] * inputVariables[i][e];
											}
											finalInputs.push_back(expectedValue);
										}
										float rSquared = CalculateRSquared(finalInputs, outputValues);
										if (rSquared > mostAccurateRSquared)
										{
											mostAccurateRSquared = rSquared;
											for (int a = 0; a < 5; ++a)
											{
												mostAccurateCoefficients[a] = inputCoefficients[a];
											}
										}
										//	inputCoefficients[g] += fCoefficientStep;
										//}

										inputCoefficients[4] += fCoefficientStep;
									}
									inputCoefficients[3] += fCoefficientStep;
								}
								inputCoefficients[2] += fCoefficientStep;
							}
							inputCoefficients[1] += fCoefficientStep;
						}
						inputCoefficients[0] += fCoefficientStep;
						//}
						//}

					}
					
					mostAccurateRSquared = mostAccurateRSquared;
				}
			}
		}

		if (bShoulAnalyzeBatters)
		{
			bool bShouldGatherBatterData = false;
			bool bShouldTrainBatterData = true;
			if (bShouldGatherBatterData)
			{
				ofstream battersDataFile;
				string battersDataFileName = "Player2016AnalysisCached\\BattersOverall.txt";
				battersDataFile.open(battersDataFileName);

				string total2016Data;
				total2016Data = GetEntireFileContents("Player2016AnalysisCached\\RawData.txt");

				unordered_map<std::string, int> battersAnalyzed;

				size_t batterIndex = total2016Data.find("GID:MLB_ID", 0);
				batterIndex = total2016Data.find(":H:", batterIndex + 1);
				while (batterIndex != string::npos)
				{
					for (int i = 0; i < 3; ++i)
					{
						batterIndex = total2016Data.rfind(":", batterIndex - 1);
					}
					size_t previousIndex = total2016Data.rfind("\n", batterIndex);
					string playerId = total2016Data.substr(previousIndex + 1, batterIndex - previousIndex - 1);

					if (battersAnalyzed.find(playerId) != battersAnalyzed.end())
					{
						for (int i = 0; i < 45; ++i)
						{
							batterIndex = total2016Data.find(":", batterIndex + 1);
						}
						batterIndex = total2016Data.find(":H:", batterIndex + 1);
						continue;
					}
					battersAnalyzed.insert({ playerId, 1 });

					for (int i = 0; i < 2; ++i)
					{
						batterIndex = total2016Data.find(":", batterIndex + 1);
					}
					size_t nextIndex = total2016Data.find(":", batterIndex + 1);
					string playerName = total2016Data.substr(batterIndex + 1, nextIndex - batterIndex - 1);

					for (int i = 0; i < 5; ++i)
					{
						batterIndex = total2016Data.find(":", batterIndex + 1);
					}
					nextIndex = total2016Data.find(":", batterIndex + 1);
					string opposingTeamCode = total2016Data.substr(batterIndex + 1, nextIndex - batterIndex - 1);

					for (int i = 0; i < 27; ++i)
					{
						batterIndex = total2016Data.find(":", batterIndex + 1);
					}
					if (playerName.find("Giovanny Urshela") != string::npos)
						nextIndex = nextIndex;
					nextIndex = total2016Data.find(":", batterIndex + 1);
					string pointsInGameString = total2016Data.substr(batterIndex + 1, nextIndex - batterIndex - 1);
					float pointsInGame = 0;

					if (pointsInGameString != "")
					{
						pointsInGame = stof(pointsInGameString);

						//	FullSeasonPitcherStats pitcher2016Stats = GetPitcher2016Stats(playerId, curl);
						//	if (pitcher2016Stats.strikeOutsPer9 >= 0)
						{
						}
					}
					// Name GID slgVL opsVL wobaVL isoVL ppgVL slgVR opsVR wobaVR isoVR ppgVR
					FullSeasonStats batter2016Stats = GetBatter2016Stats(playerId, curl);
					FullSeasonStatsAdvanced batter2016AdvancedStats = GetBatterAdvancedStats(playerId, "2016", curl);
					battersDataFile << playerName << ";" << playerId << ";" << batter2016AdvancedStats.sluggingVersusLefty << ";" << batter2016AdvancedStats.opsVersusLefty << ";" << batter2016AdvancedStats.wobaVersusLefty << ";" << batter2016AdvancedStats.isoVersusLefty << ";" << batter2016Stats.averagePpgVsLefty << "; " << batter2016AdvancedStats.sluggingVersusRighty << "; " << batter2016AdvancedStats.opsVersusRighty << "; " << batter2016AdvancedStats.wobaVersusRighty << "; " << batter2016AdvancedStats.isoVersusRighty << ";" << batter2016Stats.averagePpgVsRighty << endl;
					batter2016Stats = batter2016Stats;
					for (int i = 0; i < 11; ++i)
					{
						batterIndex = total2016Data.find(":", batterIndex + 1);
					}
					batterIndex = total2016Data.find(":H:", batterIndex + 1);
				}
			}
			if (bShouldTrainBatterData)
			{
				vector< vector<float> > inputVariables;
				for (int i = 0; i < 4; ++i)
				{
					vector<float> blankInputVariables;
					inputVariables.push_back(blankInputVariables);
				}
				vector< float > outputValues;
				string batterProcessedData = GetEntireFileContents("Player2016AnalysisCached\\BattersOverall.txt");
				size_t batterIndex = batterProcessedData.find(";", 0);
				while (batterIndex != string::npos)
				{
					for (int lr = 0; lr < 2; ++lr)
					{
						bool bValid = true;
						for (int i = 0; i < 4; ++i)
						{
							batterIndex = batterProcessedData.find(";", batterIndex + 1);
							size_t nextBatterIndex = batterProcessedData.find(";", batterIndex + 1);
							float inputValue = stof(batterProcessedData.substr(batterIndex + 1, nextBatterIndex - batterIndex - 1).c_str());
							if (i == 0 && inputValue < 0)
							{
								bValid = false;
							}
							if (bValid)
								inputVariables[i].push_back(inputValue);
						}

						batterIndex = batterProcessedData.find(";", batterIndex + 1);
						size_t nextBatterIndex = batterProcessedData.find(";", batterIndex + 1);
						if (lr == 1)
							nextBatterIndex = batterProcessedData.find("\n", batterIndex + 1);
						if (bValid)
							outputValues.push_back(stof(batterProcessedData.substr(batterIndex + 1, nextBatterIndex - batterIndex - 1).c_str()));
					}

					batterIndex = batterProcessedData.find(";", batterIndex + 1);
				}
				float rSquaredSlugging = CalculateRSquared(inputVariables[0], outputValues);
				float rSquaredOps = CalculateRSquared(inputVariables[1], outputValues);
				float rSquaredWoba = CalculateRSquared(inputVariables[2], outputValues);
				float rSquaredIso = CalculateRSquared(inputVariables[3], outputValues);
				rSquaredWoba = rSquaredWoba;
				//ops to points = 5.89791155 * ops + 3.76171160
			}
		}
	}
}

/*
http://rotoguru1.com/cgi-bin/stats.cgi?pos=6&sort=6&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=1&hithand=1&numlist=c
0    1    2               3     4         5             6      7    8     9        10             11       12           13      14      15      16        17     18    19       20    21     22          23
GID; Pos; Name;           Team; Salary; Salary Change; Points; GS;  GP; Active; Pts / Game; Pts / G / $; Pts / G(alt); Last; Days ago; MLBID;  ESPNID; YahooID; Bats; Throws; H / A; Oppt; Oppt hand; Game title
5125; 3; Cabrera, Miguel; det;  4000;      0;             0;     0; 0;    1;     0;           0;             0;         0;      0;     408234; 5544;   7163;     R;      R;     A;    chw;    L;       Jose Quintana(L) chw vs.det - 4:10 PM EDT - U.S.Cellular Field



http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=0
http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=1&nohead=1
pitcher vs batter matchups
MLB_ID;  ESPN_ID;  Name(LF);          Name(FL);         Team;  H/A;  Bats;  Active;  FD_pos;  DK_pos;  DD_pos;  YH_pos;  FD_sal;  DK_sal;  DD_sal;  YH_sal;  NP;  PA;  AB;  Hits;  2B;  3B;  HR;  Runs;  RBI;  BB;  IBB;  SO;  HBP;  SB;  CS;  AVG;   OBP;   SLG;   OPS;     wOBA;  MLB_ID(p);  ESPN_ID(p);  Pitcher_name(LF);  Pitcher_name(FL);  P_Team;  Throws;  game_time;    Stadium;        FD_sal;  DK_sal;  DD_sal;  YH_sal
453056;  28637;    Ellsbury, Jacoby;  Jacoby Ellsbury;  nyy;   H;    L;     1;       7;       7;       7;       7;        ;       ;        ;        ;        16;  4;   4;   2;     0;   0;   0;   0;     1;    0;   0;    2;   0;    0;   1;   .500;  .500;  .500;  1.000;  .450;   502009;     30196;       Latos, Mat;        Mat Latos;         tor;     R;       7:05 PM EDT;  Yankee Stadium; ;        ;        ;


http://rotoguru1.com/cgi-bin/byday.pl?date=404&game=fd&scsv=1
http://rotoguru1.com/cgi-bin/byday.pl?date=404&game=fd&scsv=1&nowrap=1
recap of stats earned
0           1         2       3                  4           5         6          7      8       9      10     11       12         13
Date;       GID;   MLB_ID;  Name;              Starter;  Bat order;  FD posn;  FD pts;  FD sal;  Team;  Oppt;  dblhdr;  Tm Runs;  Opp Runs
20170404;  1524;  434378;  Verlander, Justin;  1;        0;          ;         53.0;    ;         DET;  @ chw; ;        6;        3


http://rotoguru1.com/cgi-bin/player16.cgi?3124x
change 3124 with 4 digit player id
x (starting) is the 2016 ppg


http://dailybaseballdata.com/cgi-bin/weather.pl?scsv=1
weather


http://rotoguru1.com/cgi-bin/mlb-dbd-2016.pl?user=GoldenExcalibur&key=G5970032941
0     1        2                 3                 4     5      6         7      8      9      10                              11                 12            13           14           15            16     17          18        19          20      21          22   23   24    25      26                27              28                  29                30   31         32    33      34   35          36          37          38          39          40          41          42          43       44       45       46
GID:  MLB_ID:  Name_Last_First:  Name_First_Last:  P/H:  Hand:  Date:     Team:  Oppt:  H/A:  Game#(1 unless double header):  Game_ID:            Gametime_ET:  Team_score:  Oppt_score:  Home_Ump:     Temp:  Condition:  W_speed:  W_dir:      ADI:    prior_ADI:  GS:  GP:  Pos:  Order:  Oppt_pitch_hand:  Oppt_pich_GID:  Oppt_pitch_MLB_ID:  Oppt_pitch_Name:  PA:  wOBA_num:  IP:   W/L/S:  QS:  FD_points:  DK_points:  DD_points:  YH_points:  FD_salary:  DK_salary:  DD_salary:  YH_salary:  FD_pos:  DK_pos:  DD_pos:  YH_pos
2407: 547989:  Abreu, Jose:      Jose Abreu:       H:    R:     20161002: chw:   min:   h:    1:                              20161002-min-chw-1: 15.10:        3:           6:           Nic Lentz:    65:    cloudy:     6:        Out to LF:  65.90:  65.56:      1:   1:   1B:   4:      R:                136p:           621244:             Berrios, Jose:    4:   2.3:       :     :       :    12.5:       9.00:       19:         8:          3200:       3900:       8350:       19:         3:       3:       3:       3:
136r: 534947:  Adleman, Timothy: Tim Adleman:      P:    R:     20161001: cin:   chc:   h:    1:                              20161001-chc-cin-1: 16.10:        7:           4:           Tom Hallion:  65:    overcast:   8:        R to L:     65.05:  66.30:      1:   1:   P:    9:      L:                1506:           452657:             Lester, Jon:      2:   0.9:       5.0:  W:      1:   30:         12.45:      20:         13:         6100:       5500:       9000:       28:         1:       1:       1:       1:

*/