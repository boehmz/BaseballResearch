#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <assert.h>
#include "SharedGlobals.h"
#include "StatsCollectionFunctions.h"
#include "Main.h"
using namespace std;

GameType gameType = GameType::Fanduel;
int maxTotalBudget = 35000;
// game times in Eastern and 24 hour format
int latestGameTime = 25;
int earliestGameTime = -1;
std::string todaysDate = "20170611";
int reviewDateStart = 515;
int reviewDateEnd = 609;
float percentOf2017SeasonPassed = 64.0f / 162.0f;

int dayToDayInjuredPlayersNum = 1;
string dayToDayInjuredPlayers[] = { "Polanco, Gregory" };

string pitcherOpponentTeamCode = "";

const float leagueAverageOps = 0.72f;
const int minGamesPlayed2016 = 99;


vector< vector<PlayerData> > allPlayers;
unordered_map<std::string, OpponentInformation> opponentMap;
vector<string> probableRainoutGames;

int main(void)
{
	enum ProcessType { Analyze2016, GenerateLineup, Refine, UnitTest, AnalyzeTeamWins};
	ProcessType processType = ProcessType::GenerateLineup;

	switch (processType)
	{
	case UnitTest:
		UnitTestAllStatCollectionFunctions();
		break;
	case AnalyzeTeamWins:
		AnalyzeTeamWinFactors();
		break;
	case Analyze2016:
		Analyze2016Stats();
		break;
	case Refine:
		if (gameType == GameType::BeatTheStreak)
			RefineAlgorithmForBeatTheStreak();
		else
			RefineAlgorithm();
		break;
	default:
	case GenerateLineup:
		CURL* curl = NULL;
		PopulateProbableRainoutGames(curl);
		if (gameType == GameType::BeatTheStreak)
		{
			GetBeatTheStreakCandidates(curl);
		}
		else
		{
		//	AssembleBatterSplits(curl);
			ChooseAPitcher(curl);
			GenerateNewLineup(curl);
		}
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
		vector<float> expectedPointsInputVariables;
		vector<float> seasonOpsInputVariables;
		vector<float> last30DaysOpsInputVariables;
		vector<float> last7DaysOpsInputVariables;
		vector<float> seasonOpsAdjustedInputVariables;
		vector<float> last30DayAdjustedsOpsInputVariables;
		vector<float> last7DaysOpsAdjustedInputVariables;
		vector<float> pitcherFactorInputVariables;
		vector<float> validOutputValues;
		float inputCoefficients[2] = { 0.0f, 1.0f };
		vector< float > outputValues;

		vector<float> pitcherInputValues;
		vector<float> pitcherOutputValues;

		for (int d = reviewDateStart; d <= reviewDateEnd; ++d)
		{
			if (d - ((d / 100) * 100) > 31)
			{
				d = ((d / 100)+1) * 100;
				continue;
			}
			char thisDateCStr[5];
			_itoa_s(d, thisDateCStr, 10);
			string thisDate = thisDateCStr;
			string actualResults;
			string resultsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + thisDate + "&game=fd&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941";
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

			while (getline(resultsTrackerFile, resultsLine))
			{
				vector<float> thisInputVariables;
				thisInputVariables.push_back(0);
				thisInputVariables.push_back(0);
				vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
				string thisPlayerId = lineValues[0];


				FullSeasonStats player2016Stats = GetBatterStats(thisPlayerId, "2017", curl);
				if (lineValues[2] == "L")
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
					expectedPointsInputVariables.push_back(stof(lineValues[3].c_str()));
					outputValues.push_back(stof(actualResults.substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1).c_str()));

					float seasonOps = stof(lineValues[4].c_str());
					float last30DaysOps = stof(lineValues[5].c_str());
					float last7DaysOps = stof(lineValues[6].c_str());
					if (seasonOps >= 0 && last30DaysOps >= 0 && last7DaysOps >= 0)
					{
						float adjustmentFactor = stof(lineValues[7].c_str());// *stof(lineValues[8].c_str());
						seasonOpsInputVariables.push_back(seasonOps);
						last30DaysOpsInputVariables.push_back(last30DaysOps);
						last7DaysOpsInputVariables.push_back(last7DaysOps);
						seasonOpsAdjustedInputVariables.push_back(seasonOps * adjustmentFactor);
						last30DayAdjustedsOpsInputVariables.push_back(last30DaysOps * adjustmentFactor);
						last7DaysOpsAdjustedInputVariables.push_back(last7DaysOps * adjustmentFactor);
						pitcherFactorInputVariables.push_back(stof(lineValues[7].c_str()));
						validOutputValues.push_back(outputValues[outputValues.size() - 1]);

					}
				}
			}
			resultsTrackerFile.close();

			ifstream pitcherResultsTrackerFile;
			string pitcherResultsTrackerFileName = "2017ResultsTracker\\Pitchers\\2017";
			if (d < 1000)
				pitcherResultsTrackerFileName += "0";
			pitcherResultsTrackerFileName += thisDate + ".txt";
			pitcherResultsTrackerFile.open(pitcherResultsTrackerFileName);

			while (getline(pitcherResultsTrackerFile, resultsLine))
			{
				vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
				string thisPlayerId = lineValues[0];

				size_t playerIdIndex = actualResults.find(";" + thisPlayerId + ";", 0);
				if (playerIdIndex != string::npos)
				{
					for (int i = 0; i < 6; ++i)
					{
						playerIdIndex = actualResults.find(";", playerIdIndex + 1);
					}
					size_t nextPlayerIdIndex = actualResults.find(";", playerIdIndex + 1);
					pitcherInputValues.push_back(stof(lineValues[2]));
					pitcherOutputValues.push_back(stof(actualResults.substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1).c_str()));
				}
			}
			pitcherResultsTrackerFile.close();
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

		float expectedPointsRSquared = CalculateRSquared(expectedPointsInputVariables, outputValues);
		
		float pitcherRSquared = CalculateRSquared(pitcherInputValues, pitcherOutputValues);
		float seasonOpsRSquared = CalculateRSquared(seasonOpsInputVariables, validOutputValues);
		float last30OpsRSquared = CalculateRSquared(last30DaysOpsInputVariables, validOutputValues);
		float last7OpsRSquared = CalculateRSquared(last7DaysOpsInputVariables, validOutputValues);
		float seasonOpsAdjustedRSquared = CalculateRSquared(seasonOpsAdjustedInputVariables, validOutputValues);
		float last30OpsAdjustedRSquared = CalculateRSquared(last30DayAdjustedsOpsInputVariables, validOutputValues);
		float last7OpsAdjustedRSquared = CalculateRSquared(last7DaysOpsAdjustedInputVariables, validOutputValues);
		float pitcherFactorRSquared = CalculateRSquared(pitcherFactorInputVariables, validOutputValues);
		inputCoefficients[0] = inputCoefficients[0];

	}
}

void RefineAlgorithmForBeatTheStreak()
{
	CURL *curl;

	curl = curl_easy_init();
	if (curl)
	{
		vector<BeatTheStreakPlayerProfile> playersYesHit;
		vector<BeatTheStreakPlayerProfile> playersNoHit;
		int numEligiblePlayersTotal = 0;
		int eligiblePlayersThatGotHit = 0;

		for (int d = reviewDateStart; d <= reviewDateEnd; ++d)
		{
			if (d - ((d / 100) * 100) > 31)
			{
				d = ((d / 100) + 1) * 100;
				continue;
			}
			char thisDateCStr[5];
			_itoa_s(d, thisDateCStr, 10);
			string thisDate = thisDateCStr;
			string actualResults;
			string resultsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + thisDate + "&game=fd&user=GoldenExcalibur&key=G5970032941";
			curl_easy_setopt(curl, CURLOPT_URL, resultsURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &actualResults);
			curl_easy_perform(curl);
			curl_easy_reset(curl);

			ifstream eligiblePlayersTrackerFile;
			string eligiblePlayersFileName = "2017ResultsTracker\\BeatTheStreak\\2017";
			if (d < 1000)
				eligiblePlayersFileName += "0";
			eligiblePlayersFileName += thisDate + ".txt";
			eligiblePlayersTrackerFile.open(eligiblePlayersFileName);
			string eligiblePlayerName;
			vector<string> eligiblePlayerNames;
			while (getline(eligiblePlayersTrackerFile, eligiblePlayerName))
			{
				// 0   ;1                    ;2           ;3          ;4          ;5         ;6           ;7
				// Name;HitsPerGameLast30Days;AvgLast7Days;AvgVPitcher;PitcherWhip;PitcherEra;PitcherKPer9;PitcherAvgAgainst;
				vector<string> lineValues = SplitStringIntoMultiple(eligiblePlayerName, ";");
				eligiblePlayerNames.push_back(lineValues[0]);
			}
			eligiblePlayersTrackerFile.close();
			numEligiblePlayersTotal += eligiblePlayerNames.size();

			ifstream resultsTrackerFile;
			string resultsTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\AllPlayersDaily\\2017";
			if (d < 1000)
				resultsTrackerFileName += "0";
			resultsTrackerFileName += thisDate + ".txt";
			resultsTrackerFile.open(resultsTrackerFileName);

			string resultsLine;

			for (unsigned int p = 0; p < eligiblePlayerNames.size(); ++p)
			{
				// 0   ;1                    ;2           ;3          ;4          ;5         ;6           ;7
				// Name;HitsPerGameLast30Days;AvgLast7Days;AvgVPitcher;PitcherWhip;PitcherEra;PitcherKPer9;PitcherAvgAgainst;
				size_t playerIdIndex = actualResults.find(eligiblePlayerNames[p], 0);
				if (playerIdIndex != string::npos)
				{
					for (int i = 0; i < 7; ++i)
					{
						playerIdIndex = actualResults.find("</td>", playerIdIndex + 1);
					}
					size_t prevPlayerIdIndex = actualResults.rfind(">", playerIdIndex - 1);
					prevPlayerIdIndex = actualResults.find("/", prevPlayerIdIndex + 1);
					if (prevPlayerIdIndex < playerIdIndex)
					{
						playerIdIndex = prevPlayerIdIndex;
						prevPlayerIdIndex = actualResults.rfind(" ", prevPlayerIdIndex);
						int numHits = atoi(actualResults.substr(prevPlayerIdIndex + 1, playerIdIndex - prevPlayerIdIndex - 1).c_str());

						if (numHits > 0)
							eligiblePlayersThatGotHit++;
					}
				}
			}

			while (getline(resultsTrackerFile, resultsLine))
			{
				// 0   ;1                    ;2           ;3          ;4          ;5         ;6           ;7
				// Name;HitsPerGameLast30Days;AvgLast7Days;AvgVPitcher;PitcherWhip;PitcherEra;PitcherKPer9;PitcherAvgAgainst;
				vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
				BeatTheStreakPlayerProfile thisPlayer;
				thisPlayer.playerName = lineValues[0];
				
				size_t playerIdIndex = actualResults.find(thisPlayer.playerName, 0);
				if (playerIdIndex != string::npos)
				{
					for (int i = 0; i < 7; ++i)
					{
						playerIdIndex = actualResults.find("</td>", playerIdIndex + 1);
					}
					size_t prevPlayerIdIndex = actualResults.rfind(">", playerIdIndex - 1);
					prevPlayerIdIndex = actualResults.find("/", prevPlayerIdIndex + 1);
					if (prevPlayerIdIndex < playerIdIndex)
					{
						playerIdIndex = prevPlayerIdIndex;
						prevPlayerIdIndex = actualResults.rfind(" ", prevPlayerIdIndex);
						int numHits = atoi(actualResults.substr(prevPlayerIdIndex + 1, playerIdIndex - prevPlayerIdIndex - 1).c_str());

						thisPlayer.hitsPerGameLast30Days = stof(lineValues[1].c_str());
						thisPlayer.averageLast7Days = stof(lineValues[2]);
						thisPlayer.averageVsPitcherFacing = stof(lineValues[3]);
						thisPlayer.opposingPitcherWhip = stof(lineValues[4]);
						thisPlayer.opposingPitcherEra = stof(lineValues[5]);
						thisPlayer.opposingPitcherStrikeOutsPer9 = stof(lineValues[6]);
						thisPlayer.opposingPitcherAverageAgainstHandedness = stof(lineValues[7]);

						if (numHits > 0)
							playersYesHit.push_back(thisPlayer);
						else
							playersNoHit.push_back(thisPlayer);
						if (thisPlayer.playerName.find("Anderson") != string::npos)
							int x = 0;
					}
				}

			}
			resultsTrackerFile.close();
		}

		BeatTheStreakPlayerProfile yesHitMin(1);
		BeatTheStreakPlayerProfile yesHitAvg(0);
		int eligibleVPitcherCount = 0;
		for (unsigned int p = 0; p < playersYesHit.size(); ++p)
		{
			if (playersYesHit[p].averageLast7Days < yesHitMin.averageLast7Days)
				yesHitMin.averageLast7Days = playersYesHit[p].averageLast7Days;
			if (playersYesHit[p].averageVsPitcherFacing >= 0 &&
				playersYesHit[p].averageVsPitcherFacing < yesHitMin.averageVsPitcherFacing)
				yesHitMin.averageVsPitcherFacing = playersYesHit[p].averageVsPitcherFacing;
			if (playersYesHit[p].hitsPerGameLast30Days < yesHitMin.hitsPerGameLast30Days)
				yesHitMin.hitsPerGameLast30Days = playersYesHit[p].hitsPerGameLast30Days;
			if (playersYesHit[p].opposingPitcherAverageAgainstHandedness < yesHitMin.opposingPitcherAverageAgainstHandedness)
				yesHitMin.opposingPitcherAverageAgainstHandedness = playersYesHit[p].opposingPitcherAverageAgainstHandedness;
			if (playersYesHit[p].opposingPitcherEra < yesHitMin.opposingPitcherEra)
				yesHitMin.opposingPitcherEra = playersYesHit[p].opposingPitcherEra;
			if (playersYesHit[p].opposingPitcherWhip < yesHitMin.opposingPitcherWhip)
				yesHitMin.opposingPitcherWhip = playersYesHit[p].opposingPitcherWhip;
			if (playersYesHit[p].opposingPitcherStrikeOutsPer9 > yesHitMin.opposingPitcherStrikeOutsPer9)
				yesHitMin.opposingPitcherStrikeOutsPer9 = playersYesHit[p].opposingPitcherStrikeOutsPer9;

			yesHitAvg.averageLast7Days += playersYesHit[p].averageLast7Days;
			if (playersYesHit[p].averageVsPitcherFacing >= 0)
			{
				eligibleVPitcherCount++;
				yesHitAvg.averageVsPitcherFacing += playersYesHit[p].averageVsPitcherFacing;
			}
			yesHitAvg.hitsPerGameLast30Days += playersYesHit[p].hitsPerGameLast30Days;
			yesHitAvg.opposingPitcherAverageAgainstHandedness += playersYesHit[p].opposingPitcherAverageAgainstHandedness;
			yesHitAvg.opposingPitcherEra += playersYesHit[p].opposingPitcherEra;
			yesHitAvg.opposingPitcherWhip += playersYesHit[p].opposingPitcherWhip;
			yesHitAvg.opposingPitcherStrikeOutsPer9 += playersYesHit[p].opposingPitcherStrikeOutsPer9;
		}
		yesHitAvg.averageLast7Days /= (float)playersYesHit.size();
		yesHitAvg.averageVsPitcherFacing /= (float)eligibleVPitcherCount;
		yesHitAvg.hitsPerGameLast30Days /= (float)playersYesHit.size();
		yesHitAvg.opposingPitcherAverageAgainstHandedness /= (float)playersYesHit.size();
		yesHitAvg.opposingPitcherEra /= (float)playersYesHit.size();
		yesHitAvg.opposingPitcherWhip /= (float)playersYesHit.size();
		yesHitAvg.opposingPitcherStrikeOutsPer9 /= (float)playersYesHit.size();


		BeatTheStreakPlayerProfile noHitMax(-1);
		BeatTheStreakPlayerProfile noHitAvg(0);
		eligibleVPitcherCount = 0;
		for (unsigned int p = 0; p < playersNoHit.size(); ++p)
		{
			if (playersNoHit[p].averageLast7Days > noHitMax.averageLast7Days)
				noHitMax.averageLast7Days = playersNoHit[p].averageLast7Days;
			if (playersNoHit[p].averageVsPitcherFacing >= 0 &&
				playersNoHit[p].averageVsPitcherFacing > noHitMax.averageVsPitcherFacing)
				noHitMax.averageVsPitcherFacing = playersNoHit[p].averageVsPitcherFacing;
			if (playersNoHit[p].hitsPerGameLast30Days > noHitMax.hitsPerGameLast30Days)
				noHitMax.hitsPerGameLast30Days = playersNoHit[p].hitsPerGameLast30Days;
			if (playersNoHit[p].opposingPitcherAverageAgainstHandedness > noHitMax.opposingPitcherAverageAgainstHandedness)
				noHitMax.opposingPitcherAverageAgainstHandedness = playersNoHit[p].opposingPitcherAverageAgainstHandedness;
			if (playersNoHit[p].opposingPitcherEra > noHitMax.opposingPitcherEra)
				noHitMax.opposingPitcherEra = playersNoHit[p].opposingPitcherEra;
			if (playersNoHit[p].opposingPitcherWhip > noHitMax.opposingPitcherWhip)
				noHitMax.opposingPitcherWhip = playersNoHit[p].opposingPitcherWhip;
			if (playersNoHit[p].opposingPitcherStrikeOutsPer9 < noHitMax.opposingPitcherStrikeOutsPer9)
				noHitMax.opposingPitcherStrikeOutsPer9 = playersNoHit[p].opposingPitcherStrikeOutsPer9;

			noHitAvg.averageLast7Days += playersNoHit[p].averageLast7Days;
			if (playersNoHit[p].averageVsPitcherFacing >= 0)
			{
				eligibleVPitcherCount++;
				noHitAvg.averageVsPitcherFacing += playersNoHit[p].averageVsPitcherFacing;
			}
			noHitAvg.hitsPerGameLast30Days += playersNoHit[p].hitsPerGameLast30Days;
			noHitAvg.opposingPitcherAverageAgainstHandedness += playersNoHit[p].opposingPitcherAverageAgainstHandedness;
			noHitAvg.opposingPitcherEra += playersNoHit[p].opposingPitcherEra;
			noHitAvg.opposingPitcherWhip += playersNoHit[p].opposingPitcherWhip;
			noHitAvg.opposingPitcherStrikeOutsPer9 += playersNoHit[p].opposingPitcherStrikeOutsPer9;
		}
		noHitAvg.averageLast7Days /= (float)playersNoHit.size();
		noHitAvg.averageVsPitcherFacing /= (float)eligibleVPitcherCount;
		noHitAvg.hitsPerGameLast30Days /= (float)playersNoHit.size();
		noHitAvg.opposingPitcherAverageAgainstHandedness /= (float)playersNoHit.size();
		noHitAvg.opposingPitcherEra /= (float)playersNoHit.size();
		noHitAvg.opposingPitcherWhip /= (float)playersNoHit.size();
		noHitAvg.opposingPitcherStrikeOutsPer9 /= (float)playersNoHit.size();
		
		yesHitMin = yesHitMin;

		ofstream yesHitTrackerFile;
		string yesHitTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\PlayersYesHit.txt";
		yesHitTrackerFile.open(yesHitTrackerFileName);
		for (unsigned int y = 0; y < playersYesHit.size(); ++y)
		{
			yesHitTrackerFile << playersYesHit[y].ToString() << endl;
		}
		yesHitTrackerFile.close();
		ofstream noHitTrackerFile;
		string noHitTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\PlayersNoHit.txt";
		noHitTrackerFile.open(noHitTrackerFileName);
		for (unsigned int n = 0; n < playersNoHit.size(); ++n)
		{
			noHitTrackerFile << playersNoHit[n].ToString() << endl;
		}
		noHitTrackerFile.close();
	}
}

void ChooseAPitcher(CURL *curl)
{
	if (curl == NULL)
		curl = curl_easy_init();

	if (curl)
	{
		std::string readBuffer;
		string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=1&sort=4&game=d&colA=0&daypt=0&denom=3&xavg=0&inact=0&maxprc=99999&sched=1&starters=1&hithand=0&numlist=c&user=GoldenExcalibur&key=G5970032941";
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
		curl_easy_setopt(curl, CURLOPT_URL, "https://www.teamrankings.com/mlb/stat/on-base-plus-slugging-pct");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &team2017RunsPerGameData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

	
		ofstream pitcherStatsArchiveFile;
		string pitcherStatsArchiveFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\" + todaysDate + ".txt";
		pitcherStatsArchiveFile.open(pitcherStatsArchiveFileName);

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
			singlePlayerData.teamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str();

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
			auto opponent = opponentMap.find(singlePlayerData.teamCode);
			if (opponent != opponentMap.end())
			{
				opponentTeamCode = opponent->second.teamCodeRotoGuru;
				auto myTeam = opponentMap.find(opponentTeamCode);
				if (myTeam != opponentMap.end())
				{
					myTeam->second.pitcherAdvancedStats = pitcherVBatterCareerStats;
				}
			}
			else
			{
				assert("No opponent information for pitcher found" == "");
			}

			// now look up 2016 points per game
			singlePlayerData.playerPointsPerGame = 0;
			FullSeasonPitcherStats newPitcherStats = GetPitcherStats(singlePlayerData.playerId, "2017", curl);
			FullSeasonPitcherStats pitcherStats = GetPitcherStats(singlePlayerData.playerId, "2016", curl);
			FullSeasonPitcherStats pitcherCareerStats = GetPitcherStats(singlePlayerData.playerId, "Total", curl);
			// default to average
			float opponentRunsPerGame = 4.4f;
			float opponentStrikeoutsPerGame = 8.1f;

			auto opponentsInfo = opponentMap.find(singlePlayerData.teamCode);

			if (opponentsInfo != opponentMap.end())
			{
				size_t opponentTeamIndex = team2016OffensiveData.find(";" + opponentsInfo->second.teamCodeRankingsSite + ";", 0);
				opponentTeamIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
				size_t opponentTeamNextIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
				opponentRunsPerGame = stof(team2016OffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());

				opponentTeamIndex = team2016OffensiveData.find(";", opponentTeamIndex + 1);
				opponentTeamNextIndex = team2016OffensiveData.find("\n", opponentTeamIndex + 1);
				opponentStrikeoutsPerGame = stof(team2016OffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
			}
			else
			{
				assert("No opponent information for pitcher found" == "");
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

			float parkHomerFactor = 1;
			float parkRunsFactor = 1;
			if (opponentsInfo != opponentMap.end())
			{
				opponentRunsPerGame *= max(0.0f, 1.0f - (percentOf2017SeasonPassed * 2.0f));
				opponentStrikeoutsPerGame *= max(0.0f, 1.0f - (percentOf2017SeasonPassed * 2.0f));

				size_t opponentTeamIndex = team2017RunsPerGameData.find(">" + opponentsInfo->second.rankingsSiteTeamName + "<", 0);
				opponentTeamIndex = team2017RunsPerGameData.find("data-sort=", opponentTeamIndex + 1);
				opponentTeamIndex = team2017RunsPerGameData.find(">", opponentTeamIndex + 1);
				size_t opponentTeamNextIndex = team2017RunsPerGameData.find("<", opponentTeamIndex + 1);
				float ops = stof(team2017RunsPerGameData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
				// ops to runs per game is
				// 13.349 * ops - 5.379
				opponentRunsPerGame += (13.349f * ops - 5.379f) * min(1.0f, percentOf2017SeasonPassed * 2.0f);

				opponentTeamIndex = team2017StrikeoutData.find(">" + opponentsInfo->second.rankingsSiteTeamName + "<", 0);
				opponentTeamIndex = team2017StrikeoutData.find("data-sort=", opponentTeamIndex + 1);
				opponentTeamIndex = team2017StrikeoutData.find(">", opponentTeamIndex + 1);
				opponentTeamNextIndex = team2017StrikeoutData.find("<", opponentTeamIndex + 1);
				opponentStrikeoutsPerGame += stof(team2017StrikeoutData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str()) * min(1.0f, percentOf2017SeasonPassed * 2.0f);
			
				// ballpark factors
				float pitcherBallparkHomerRateVsRighty, pitcherBallparkHomerRateVsLefty;
				float pitcherBallparkRunsRateVsRighty, pitcherBallparkRunsRateVsLefty;
				GetBallparkFactors(opponentsInfo->second.ballParkPlayedIn, "HR", pitcherBallparkHomerRateVsLefty, pitcherBallparkHomerRateVsRighty);
				GetBallparkFactors(opponentsInfo->second.ballParkPlayedIn, "R", pitcherBallparkRunsRateVsLefty, pitcherBallparkRunsRateVsRighty);
				parkRunsFactor = (pitcherBallparkRunsRateVsLefty + pitcherBallparkRunsRateVsRighty) * 0.5f;
				parkHomerFactor = (pitcherBallparkHomerRateVsLefty + pitcherBallparkHomerRateVsRighty) * 0.5f;
			}
			

			pitcherStats.era *= parkRunsFactor;
			pitcherStats.fip *= parkHomerFactor;

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
			
			bool bRainedOut = false;
			int gameStartTime = 99;
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
				gameStartTime = opponentsInfo->second.gameTime;

				string opponentTeamCode = opponent->second.teamCodeRotoGuru;
				auto myTeam = opponentMap.find(opponentTeamCode);
				if (myTeam != opponentMap.end())
				{
					myTeam->second.pitcherEstimatedPpg = singlePlayerData.playerPointsPerGame;
				}
			}
			if (newPitcherStats.xfip > -0.1f)
			{
				pitcherStatsArchiveFile << singlePlayerData.teamCode << ";" << singlePlayerData.playerId << ";" << singlePlayerData.playerName << ";" << newPitcherStats.ToString();
				pitcherStatsArchiveFile << endl;
			}
			// throw this guy out if his game will most likely be rained out
			if (pitcherStats.strikeOutsPer9 >= 0 && gameStartTime <= latestGameTime && gameStartTime >= earliestGameTime && !bRainedOut)
				positionalPlayerData.push_back(singlePlayerData);
			if (placeHolderIndex == string::npos)
				break;
			else
				placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
		}
		pitcherStatsArchiveFile.close();

		sort(positionalPlayerData.begin(), positionalPlayerData.end(), comparePlayerByPointsPerGame);

		ofstream playerResultsTrackerFile;
		string playerResultsTrackerFileName = "2017ResultsTracker\\Pitchers\\" + todaysDate + ".txt";
		playerResultsTrackerFile.open(playerResultsTrackerFileName);

		ofstream teamWinTrackerFile;
		string teamWinTrackerFileName = "2017ResultsTracker\\TeamWinResults\\" + todaysDate + ".txt";
		teamWinTrackerFile.open(teamWinTrackerFileName);
		for (unsigned int i = 0; i < positionalPlayerData.size(); ++i)
		{
			string alreadyWrittenData = GetEntireFileContents(teamWinTrackerFileName);
			if (alreadyWrittenData.find(positionalPlayerData[i].teamCode) == string::npos)
			{
				auto opponentsInfo = opponentMap.find(positionalPlayerData[i].teamCode);
				if (opponentsInfo != opponentMap.end())
				{
					if (positionalPlayerData[i].playerPointsPerGame > 0 && opponentsInfo->second.pitcherEstimatedPpg > 0)
					{
						teamWinTrackerFile << positionalPlayerData[i].teamCode << ";" << positionalPlayerData[i].playerPointsPerGame << ";" << opponentsInfo->second.teamCodeRotoGuru << ";" << opponentsInfo->second.pitcherEstimatedPpg << ";";
						teamWinTrackerFile << endl;
					}
				}
			}
			playerResultsTrackerFile << positionalPlayerData[i].playerId << ";" << positionalPlayerData[i].playerName << ";" << positionalPlayerData[i].playerPointsPerGame;
			playerResultsTrackerFile << endl;
		}
		teamWinTrackerFile.close();
		playerResultsTrackerFile.close();

		for (unsigned int i = 0; i < positionalPlayerData.size() && i < 10; ++i)
		{
			cout << i << ".  " << positionalPlayerData[i].playerName << "  " << positionalPlayerData[i].playerPointsPerGame << "  " << positionalPlayerData[i].playerSalary << endl;
		}
		cout << "Choose between pitcher 0 and 9." << endl;
		int pitcherSelected = -1;
		cin >> pitcherSelected;
		while (!cin || pitcherSelected < 0 || pitcherSelected > 9)
		{
			cout << "Must select between 0 and 9." << endl;
			cin.clear();
			cin.ignore();
			cin >> pitcherSelected;
		}
		maxTotalBudget = 35000 - positionalPlayerData[pitcherSelected].playerSalary;
		auto opponentInformation = opponentMap.find(positionalPlayerData[pitcherSelected].teamCode);
		if (opponentInformation != opponentMap.end())
		{
			pitcherOpponentTeamCode = opponentInformation->second.teamCodeRotoGuru;
		}
		curl_easy_cleanup(curl);
	}
}

void GenerateNewLineup(CURL *curl)
{
	if (curl == NULL)
		curl = curl_easy_init();
	if (curl)
	{
	  DetermineProbableStarters(curl);
	
	  for (int p = 2; p <= 7; ++p)
	  {
		  // first basemen
		  std::string readBuffer;
		  char pAsString[5];
		  _itoa_s(p, pAsString, 10);
		  string pAsStringString(pAsString);
		  string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=" + pAsStringString + "&sort=6&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=0&hithand=1&numlist=c&user=GoldenExcalibur&key=G5970032941";

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
			singlePlayerData.teamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);

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
				// two thirds of the season becomes the point where we disregard last season
				thisSeasonPercent /= (162.0f * 0.2f);
			}
			else
			{
				singlePlayerData.isFacingLefthander = false;
				// about 70% of starters are right handed
				// two thirds of the season becomes the point where we disregard last season
				thisSeasonPercent /= (162.0f * 0.46f);
			}
		//	if (thisSeasonPercent > 1)
				thisSeasonPercent = 1;

			// game name
			for (int i = 0; i < 1; ++i)
			{
				placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
			}

			// now look up 2016 points per game
			singlePlayerData.playerPointsPerGame *= thisSeasonPercent;
			FullSeasonStats player2016Stats = GetBatterStats(singlePlayerData.playerId, "2016", curl);
			if (singlePlayerData.isFacingLefthander)
				singlePlayerData.playerPointsPerGame += player2016Stats.averagePpgVsLefty * (1.0f - thisSeasonPercent);
			else
				singlePlayerData.playerPointsPerGame += player2016Stats.averagePpgVsRighty * (1.0f - thisSeasonPercent);
			
			// now factor in opposing pitcher
			float pitcherFactor = 1.0f;
			float ballParkFactor = 1.0f;
			FullSeasonStatsAdvanced opposingPitcherAdvancedStats;
			auto opponentInformation = opponentMap.find(singlePlayerData.teamCode);
			if (opponentInformation != opponentMap.end())
			{
				opposingPitcherAdvancedStats = opponentInformation->second.pitcherAdvancedStats;
				float ballParkFactorAsLefty, ballParkFactorAsRighty;
				GetBallparkFactors(opponentInformation->second.ballParkPlayedIn, "SLG", ballParkFactorAsLefty, ballParkFactorAsRighty);
				if (singlePlayerData.batsLeftHanded)
					ballParkFactor = ballParkFactorAsLefty;
				else
					ballParkFactor = ballParkFactorAsRighty;
			}
			else
			{	
				string playerHasNoOpponentInformation = singlePlayerData.teamCode;
				assert(playerHasNoOpponentInformation == "nope");
			}
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
				if (singlePlayerData.batsLeftHanded)
					pitcherFactor = opposingPitcherAdvancedStats.opsVersusLefty / leagueAverageOps;
				else
					pitcherFactor = opposingPitcherAdvancedStats.opsVersusRighty / leagueAverageOps;
			}
			singlePlayerData.playerPointsPerGame *= pitcherFactor * ballParkFactor;
			singlePlayerData.pitcherFactor = pitcherFactor;
			singlePlayerData.parkFactor = ballParkFactor;
			auto playerSplits = allBattersSplits.find(ConvertLFNameToFLName(singlePlayerData.playerName));
			if (playerSplits != allBattersSplits.end())
			{
				singlePlayerData.playerPointsPerGame = playerSplits->second.opsLast30Days * 1000.0f;
				singlePlayerData.playerPointsPerGame *= pitcherFactor;
			}
			else
			{
				singlePlayerData.playerPointsPerGame = 0;
			}
			
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
			for (int inj = 0; inj < dayToDayInjuredPlayersNum; ++inj)
			{
				if (singlePlayerData.playerName.find(dayToDayInjuredPlayers[inj]) != string::npos)
				{
					bRainedOut = true;
					break;
				}
			}
			bool bFacingChosenPitcher = singlePlayerData.teamCode == pitcherOpponentTeamCode;
			// throw this guy out if he's not a starter or his game will most likely be rained out
			//if (!bFacingChosenPitcher && numGamesPlayed2016 >= minGamesPlayed2016 && gameStartTime <= latestGameTime && gameStartTime >= earliestGameTime && !bRainedOut)
			bool bIsProbableStarter = probableStarters.find(singlePlayerData.playerId) != probableStarters.end();
			if (!bFacingChosenPitcher && bIsProbableStarter && gameStartTime <= latestGameTime && gameStartTime >= earliestGameTime && !bRainedOut)
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
		  resultsTrackerFile << allPlayers[i][p].playerPointsPerGame << ";";
		  auto playerSplits = allBattersSplits.find(ConvertLFNameToFLName(allPlayers[i][p].playerName));
		  if (playerSplits != allBattersSplits.end())
		  {
			  resultsTrackerFile << playerSplits->second.opsSeason << ";" << playerSplits->second.opsLast30Days << ";" << playerSplits->second.opsLast7Days << ";";
		  }
		  else
		  {
			  resultsTrackerFile << "-1;-1;-1;";
		  }
		  resultsTrackerFile << allPlayers[i][p].pitcherFactor << ";" << allPlayers[i][p].parkFactor << "; ";
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
					if (swappee == idealPlayerPerPosition.size() - 3)
					{
						if (bs == idealPlayerPerPosition[idealPlayerPerPosition.size() - 2])
							continue;
						if (bs == idealPlayerPerPosition[idealPlayerPerPosition.size() - 1])
							continue;
					}
					else if (swappee == idealPlayerPerPosition.size() - 2)
					{
						if (bs == idealPlayerPerPosition[idealPlayerPerPosition.size() - 1])
							continue;
					}
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

void DetermineProbableStarters(CURL* curl)
{
	if (curl == NULL)
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
			string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=" + pAsStringString + "&sort=4&game=d&colA=0&daypt=1&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=0&hithand=1&numlist=c&user=GoldenExcalibur&key=G5970032941";

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
				singlePlayerData.teamCode = readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1);

				// number of games started this season
				for (int i = 0; i < 4; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}
				nextIndex = readBuffer.find(";", placeHolderIndex + 1);
				int gamesStartedLast30Days = atoi(readBuffer.substr(placeHolderIndex + 1, nextIndex - placeHolderIndex - 1).c_str());

				// now look for teammates who might have more starts
				int maxNumPlayersWithMoreStarts = 0;
				// should probably be for AL only when p == 3
				if (p == 3)
					maxNumPlayersWithMoreStarts = 1;
				if (p == 7)
					maxNumPlayersWithMoreStarts = 2;

				int numPlayersWithMoreStarts = 0;
				size_t teammateIndex = readBuffer.find(";" + singlePlayerData.teamCode + ";", 0);
				size_t teammateNextLine = readBuffer.find("\n", teammateIndex);
				for (int i = 0; i < 4; ++i)
				{
					if (teammateIndex == string::npos)
						break;
					teammateIndex = readBuffer.find(";", teammateIndex + 1);
				}
				while (teammateIndex != string::npos)
				{
					if (teammateIndex < teammateNextLine && teammateIndex != placeHolderIndex)
					{
						size_t nextTeammateIndex = readBuffer.find(";", teammateIndex + 1);
						int numGamesTeammateStarted = atoi(readBuffer.substr(teammateIndex + 1, nextTeammateIndex - teammateIndex - 1).c_str());
						if (numGamesTeammateStarted > gamesStartedLast30Days)
							numPlayersWithMoreStarts++;
					}
					teammateIndex = readBuffer.find(";" + singlePlayerData.teamCode + ";", teammateIndex + 1);
					teammateNextLine = readBuffer.find("\n", teammateIndex);
					for (int i = 0; i < 4; ++i)
					{
						if (teammateIndex == string::npos)
							break;
						teammateIndex = readBuffer.find(";", teammateIndex + 1);
					}
				}
				// we are a starter
				if ( gamesStartedLast30Days > 3 && numPlayersWithMoreStarts <= maxNumPlayersWithMoreStarts)
					probableStarters.insert({singlePlayerData.playerId, true});

				// go to next player
				for (int i = 0; i < 16; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}
				if (placeHolderIndex == string::npos)
					break;
				else
					placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
			}
		}
	}
}

void PopulateProbableRainoutGames(CURL *curl)
{
	if (curl == NULL)
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
				size_t windIndex = weatherData.find("Wind:", precipPercentEnd);
				for (int i = 0; i < 4; ++i)
				{
					precipPercentEnd = weatherData.find("%", precipPercentEnd + 1);
				}
				if (precipPercentEnd < windIndex)
				{
					for (int i = 0; i < 6; ++i)
					{
						size_t precipPercentStart = weatherData.rfind(">", precipPercentEnd);
						gameTimePercipByHour[i] = atoi(weatherData.substr(precipPercentStart + 1, precipPercentEnd - precipPercentStart - 1).c_str());
						precipPercentEnd = weatherData.find("%", precipPercentEnd + 1);
					}
					for (int i = 0; i < 4; ++i)
					{
						if (gameTimePercipByHour[i] + gameTimePercipByHour[i + 1] + gameTimePercipByHour[i + 2] > 135)
						{
							bProbableRainout = true;
							break;
						}
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
			
			size_t colonIndex = weatherData.find(":", dashIndex);
			int gameStartTime = atoi(weatherData.substr(dashIndex + 3, colonIndex - dashIndex - 3).c_str());
			if (gameStartTime < 10)
				gameStartTime += 12;
			if (weatherData.find("PDT", dashIndex) < timeIndex)
				gameStartTime += 3;
			else if (weatherData.find("MDT", dashIndex) < timeIndex)
				gameStartTime += 2;
			else if (weatherData.find("CDT", dashIndex) < timeIndex)
				gameStartTime += 1;

			
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
			homeTeamInformation.teamCodeRankingsSite = homeTeamCode;
			homeTeamInformation.teamCodeRotoGuru = homeTeamCode;
			homeTeamInformation.rankingsSiteTeamName = homeTeamAlternativeName;
			homeTeamInformation.gameTime = gameStartTime;

			OpponentInformation awayTeamInformation;
			awayTeamInformation.ballParkPlayedIn = ballparkName;
			awayTeamInformation.weatherSiteTeamName = awayTeam;
			awayTeamInformation.teamCodeRankingsSite = awayTeamCode;
			awayTeamInformation.teamCodeRotoGuru = awayTeamCode;
			awayTeamInformation.rankingsSiteTeamName = awayTeamAlternativeName;
			awayTeamInformation.gameTime = gameStartTime;

			// rotogur1.com uses different team codes than standard...
			if (awayTeamInformation.teamCodeRotoGuru == "laa")
				awayTeamInformation.teamCodeRotoGuru = "ana";
			if (homeTeamInformation.teamCodeRotoGuru == "laa")
				homeTeamInformation.teamCodeRotoGuru = "ana";
			if (awayTeamInformation.teamCodeRotoGuru == "lad")
				awayTeamInformation.teamCodeRotoGuru = "los";
			if (homeTeamInformation.teamCodeRotoGuru == "lad")
				homeTeamInformation.teamCodeRotoGuru = "los";
			if (awayTeamInformation.teamCodeRotoGuru == "mia")
				awayTeamInformation.teamCodeRotoGuru = "fla";
			if (homeTeamInformation.teamCodeRotoGuru == "mia")
				homeTeamInformation.teamCodeRotoGuru = "fla";

			opponentMap.insert({ { homeTeamInformation.teamCodeRotoGuru,awayTeamInformation },{ awayTeamInformation.teamCodeRotoGuru,homeTeamInformation } });

			weatherDataBeginIndex = nextWeatherDataRow;
		}
	}
}

void UnitTestAllStatCollectionFunctions()
{
	CURL *curl;

	curl = curl_easy_init();
	if (curl)
	{
		FullSeasonStats batterStats = GetBatterStats("3215", "2016", curl);
		FullSeasonStatsAdvanced batterCareerAdvancedStats = GetBatterAdvancedStats("3215", "Total", curl);
		//FullSeasonStatsAdvanced batter2017AdvancedStats = GetBatterAdvancedStats("3215", "2017", curl);
		FullSeasonStatsAdvanced batter2016AdvancedStats = GetBatterAdvancedStats("3215", "2016", curl);
		
		FullSeasonStatsAdvanced pitcherAdvancedCareerStats = GetPitcherAdvancedStats("1580", "Total", curl);
		//FullSeasonStatsAdvanced pitcherAdvanced2017Stats = GetPitcherAdvancedStats("1580", "2017", curl);
		FullSeasonStatsAdvanced pitcherAdvanced2016Stats = GetPitcherAdvancedStats("1580", "2016", curl);
		FullSeasonPitcherStats pitcherCareerStats = GetPitcherStats("1580", "Total", curl);
		//FullSeasonPitcherStats pitcher2017Stats = GetPitcherStats("1580", "2017", curl);
		FullSeasonPitcherStats pitcher2016Stats = GetPitcherStats("1580", "2016", curl);
		
		// cache greinke 2016 stats for easier testing
		FullSeasonPitcherStats expectedPitcher2016Stats;
		expectedPitcher2016Stats.era = 4.37f;
		expectedPitcher2016Stats.fip = 4.12f;
		expectedPitcher2016Stats.numInnings = 158.6666666f;
		expectedPitcher2016Stats.strikeOutsPer9 = 7.6f;
		expectedPitcher2016Stats.whip = 1.27f;
		expectedPitcher2016Stats.xfip = 3.98f;
		expectedPitcher2016Stats.wobaAllowed = 0.319f;
		expectedPitcher2016Stats.opsAllowed = 0.750f;
		FullSeasonStatsAdvanced expectedPitcherAdvanced2016Stats;
		expectedPitcherAdvanced2016Stats.averageVersusLefty = 0.251f;
		expectedPitcherAdvanced2016Stats.isoVersusLefty = 0.2f;
		expectedPitcherAdvanced2016Stats.opsVersusLefty = 0.745f;
		expectedPitcherAdvanced2016Stats.sluggingVersusLefty = 0.451f;
		expectedPitcherAdvanced2016Stats.wobaVersusLefty = 0.315f;
		expectedPitcherAdvanced2016Stats.averageVersusRighty = 0.265f;
		expectedPitcherAdvanced2016Stats.isoVersusRighty = 0.17f;
		expectedPitcherAdvanced2016Stats.opsVersusRighty = 0.756f;
		expectedPitcherAdvanced2016Stats.sluggingVersusRighty = 0.435f;
		expectedPitcherAdvanced2016Stats.wobaVersusRighty = 0.323f;
		// cache hosmer 2016 stats for easier testing
		FullSeasonStats expectedBatterStats;
		expectedBatterStats.averagePpg = 10.2f;
		expectedBatterStats.averagePpgVsLefty = 9.3f;
		expectedBatterStats.averagePpgVsRighty = 10.5f;
		expectedBatterStats.totalGamesStarted = 158;
		FullSeasonStatsAdvanced expectedBatter2016AdvancedStats;
		expectedBatter2016AdvancedStats.averageVersusLefty = 0.233f;
		expectedBatter2016AdvancedStats.sluggingVersusLefty = 0.381f;
		expectedBatter2016AdvancedStats.isoVersusLefty = 0.148f;
		expectedBatter2016AdvancedStats.opsVersusLefty = 0.656f;
		expectedBatter2016AdvancedStats.wobaVersusLefty = 0.280f;
		expectedBatter2016AdvancedStats.averageVersusRighty = 0.283f;
		expectedBatter2016AdvancedStats.sluggingVersusRighty = 0.459f;
		expectedBatter2016AdvancedStats.opsVersusRighty = 0.813f;
		expectedBatter2016AdvancedStats.isoVersusRighty = 0.176f;
		expectedBatter2016AdvancedStats.wobaVersusRighty = 0.348f;

		assert(expectedPitcher2016Stats == pitcher2016Stats);
		assert(expectedPitcherAdvanced2016Stats == pitcherAdvanced2016Stats);
		assert(expectedBatterStats == batterStats);
		assert(expectedBatter2016AdvancedStats == batter2016AdvancedStats);

		int iBreakpoint = 0;
		iBreakpoint = iBreakpoint;
	}
	string zackGreinkeName = "Zack Greinke";
	string greinkeZackName = "Greinke, Zack";
	string ericHosmerName = "Eric Hosmer";
	string hosmerEricName = "Hosmer, Eric";
	assert(ConvertLFNameToFLName(greinkeZackName) == zackGreinkeName);
	assert(ConvertFLNameToLFName(zackGreinkeName) == greinkeZackName);
	assert(ConvertLFNameToFLName(hosmerEricName) == ericHosmerName);
	assert(ConvertFLNameToLFName(ericHosmerName) == hosmerEricName);
	// Zack Greinke
	// http://rotoguru1.com/cgi-bin/player16.cgi?1580x

	// Eric Hosmer
	// http://rotoguru1.com/cgi-bin/player16.cgi?3215x
	
		
	float fenwayHomeRunFactorLeftyBatter;
	float fenwayHomeRunFactorRightyBatter;
	float petcoSluggingFactorLeftyBatter;
	float petcoSluggingFactorRightyBatter;
	float kauffmanAverageFactorLeftyBatter;
	float kauffmanAverageFactorRightyBatter;
	float coorsRunsFactorLeftyBatter;
	float coorsRunsFactorRightyBatter;

	GetBallparkFactors("Fenway Park", "HR", fenwayHomeRunFactorLeftyBatter, fenwayHomeRunFactorRightyBatter);
	GetBallparkFactors("Petco Park", "SLG", petcoSluggingFactorLeftyBatter, petcoSluggingFactorRightyBatter);
	GetBallparkFactors("Kauffman Stadium", "AVG", kauffmanAverageFactorLeftyBatter, kauffmanAverageFactorRightyBatter);
	GetBallparkFactors("Coors Field", "R", coorsRunsFactorLeftyBatter, coorsRunsFactorRightyBatter);

	assert(abs(fenwayHomeRunFactorLeftyBatter - 0.79f) < 0.01f);
	assert(abs(fenwayHomeRunFactorRightyBatter - 1.15f) < 0.01f);
	assert(abs(petcoSluggingFactorLeftyBatter - 0.96f) < 0.01f);
	assert(abs(petcoSluggingFactorRightyBatter - 1.00f) < 0.01f);
	assert(abs(kauffmanAverageFactorLeftyBatter - 1.04f) < 0.01f);
	assert(abs(kauffmanAverageFactorRightyBatter - 1.03f) < 0.01f);
	assert(abs(coorsRunsFactorLeftyBatter - 1.32f) < 0.01f);
	assert(abs(coorsRunsFactorRightyBatter - 1.37f) < 0.01f);
}

string ConvertOddsPortalNameToTeamRankingsName(string oddsportalTeamName)
{
	string teamCodesData = GetEntireFileContents("TeamCodes.txt");

	size_t teamNameIndex = teamCodesData.find(oddsportalTeamName, 0);
	if (teamNameIndex == string::npos && oddsportalTeamName.find("Cardinals") != string::npos)
	{
		teamNameIndex = teamCodesData.find("Cardinals");
	}
	for (int i = 0; i < 2; ++i)
	{
		teamNameIndex = teamCodesData.find(";", teamNameIndex + 1);
	}
	size_t teamNameEndIndex = teamCodesData.find(";", teamNameIndex + 1);
	return teamCodesData.substr(teamNameIndex + 1, teamNameEndIndex - teamNameIndex - 1);
}

void AnalyzeTeamWinFactors()
{
	CURL* curl = NULL;
	//GatherTeamWins();
	string pitcherPage1Data;
	CurlGetSiteContents(curl, "http://www.fangraphs.com/leaderssplits.aspx?splitArr=42&strgroup=season&statgroup=2&startDate=2017-03-01&endDate=2017-11-01&filter=&position=P&statType=player&autoPt=false&sort=17,-1&pg=0", pitcherPage1Data);
	size_t noahIndex = pitcherPage1Data.find(">Noah Syndergaard<");
	ofstream noahInfo("NoahInfo");
	noahInfo << pitcherPage1Data;
	if (noahIndex != string::npos)
	{
		for (int i = 0; i < 7; ++i)
			noahIndex = pitcherPage1Data.find("</td>", noahIndex + 1);
		size_t prevIndex = pitcherPage1Data.rfind(">", noahIndex - 1);
		float kbb = stof(pitcherPage1Data.substr(prevIndex + 1, noahIndex - prevIndex - 1));
		kbb = kbb;
	}
	return;
	fstream allGamesFile;
	allGamesFile.open("2017ResultsTracker\\OddsWinsResults\\AllGamesResults.txt");
	ofstream gamesFactorsFile;
	gamesFactorsFile.open("2017ResultsTracker\\OddsWinsResults\\AllGamesFactors.txt");
	string resultsLine;
	string currentDate = "";
	string currentDateOpsStats = "";
	string currentDateRunsStats = "";
	while (getline(allGamesFile, resultsLine))
	{
		vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
		string dateWithDashes = lineValues[0].substr(0,4) + "-" + lineValues[0].substr(4,2) + "-" + lineValues[0].substr(6,2);
		if (dateWithDashes != currentDate)
		{
			currentDate = dateWithDashes;
			string opsFileName = "2017ResultsTracker\\OddsWinsResults\\TeamOpsCachedData\\" + currentDate + ".txt";
			currentDateOpsStats = GetEntireFileContents(opsFileName);
			if (currentDateOpsStats == "")
			{
				CurlGetSiteContents(curl, "https://www.teamrankings.com/mlb/stat/on-base-plus-slugging-pct?date=" + currentDate, currentDateOpsStats);
				ofstream opsOutputFile;
				opsOutputFile.open(opsFileName);
				opsOutputFile << currentDateOpsStats;
				opsOutputFile.close();
			}
			string runsFileName = "2017ResultsTracker\\OddsWinsResults\\TeamRunsCachedData\\" + currentDate + ".txt";
			currentDateRunsStats = GetEntireFileContents(runsFileName);
			if (currentDateRunsStats == "")
			{
				CurlGetSiteContents(curl, "https://www.teamrankings.com/mlb/stat/runs-per-game?date=" + currentDate, currentDateRunsStats);
				ofstream runsOutputFile;
				runsOutputFile.open(runsFileName);
				runsOutputFile << currentDateRunsStats;
				runsOutputFile.close();
			}
		}
		if (lineValues[4].find(" - ") != string::npos || lineValues[5].find(" - ") != string::npos)
			continue;
		
		string winningTeamRankingsName = ConvertOddsPortalNameToTeamRankingsName(lineValues[1]);
		string losingTeamRankingsName = ConvertOddsPortalNameToTeamRankingsName(lineValues[2]);
		vector<string> winningTeamOpsColumns = GetRankingsRowColumns(winningTeamRankingsName, currentDateOpsStats, 6);
		vector<string> losingTeamOpsColumns = GetRankingsRowColumns(losingTeamRankingsName, currentDateOpsStats, 6);
		float opsDiff = stof(winningTeamOpsColumns[0]) - stof(losingTeamOpsColumns[0]);
		float last3OpsDiff = stof(winningTeamOpsColumns[1]) - stof(losingTeamOpsColumns[1]);
		float last1OpsDiff = stof(winningTeamOpsColumns[2]) - stof(losingTeamOpsColumns[2]);
		float t2016OpsDiff = stof(winningTeamOpsColumns[5]) - stof(losingTeamOpsColumns[5]);
		float haOpsDiff = 0;
		if (lineValues[3] == "H")
			haOpsDiff = stof(winningTeamOpsColumns[3]) - stof(losingTeamOpsColumns[4]);
		else
			haOpsDiff = stof(winningTeamOpsColumns[4]) - stof(losingTeamOpsColumns[3]);
		float hOnlyOpsDiff = 0;
		if (lineValues[3] == "H")
			hOnlyOpsDiff = stof(winningTeamOpsColumns[3]) - stof(losingTeamOpsColumns[0]);
		else
			hOnlyOpsDiff = stof(winningTeamOpsColumns[0]) - stof(losingTeamOpsColumns[3]);

		size_t teamRunsEnd = currentDateRunsStats.find(">" + winningTeamRankingsName + "<");
		for (int i = 0; i < 2; ++i)
		{
			teamRunsEnd = currentDateRunsStats.find("</td>", teamRunsEnd + 1);
		}
		size_t teamRunsBegin = currentDateRunsStats.rfind(">", teamRunsEnd - 1);
		float winningTeamRuns = stof(currentDateRunsStats.substr(teamRunsBegin + 1, teamRunsEnd - teamRunsBegin - 1));

		teamRunsEnd = currentDateRunsStats.find(">" + losingTeamRankingsName + "<");
		for (int i = 0; i < 2; ++i)
		{
			teamRunsEnd = currentDateRunsStats.find("</td>", teamRunsEnd + 1);
		}
		teamRunsBegin = currentDateRunsStats.rfind(">", teamRunsEnd - 1);
		float losingTeamRuns = stof(currentDateRunsStats.substr(teamRunsBegin + 1, teamRunsEnd - teamRunsBegin - 1));
		float runsDiff = winningTeamRuns - losingTeamRuns;


		int winningMoneyLine = atoi(lineValues[4].c_str());
		int losingMoneyLine = atoi(lineValues[5].c_str());
		//if (winningMoneyLine <= -150 || losingMoneyLine <= -150)
		//if ((opsDiff >= 0 && winningMoneyLine > losingMoneyLine) ||
		//	(opsDiff <= 0 && winningMoneyLine < losingMoneyLine))
		{
			gamesFactorsFile << lineValues[0] << ";";
			gamesFactorsFile << winningMoneyLine << ";" << losingMoneyLine << ";";
			
			/*
			if (winningMoneyLine > losingMoneyLine)
				gamesFactorsFile << losingMoneyLine << ";";
			else
			{
				gamesFactorsFile << "10;";// (-1000.0f / (float)winningMoneyLine) << "; ";
			}
			if (opsDiff > 0)
			{
				if (winningMoneyLine > 0)
					gamesFactorsFile << winningMoneyLine << ";";
				else
					gamesFactorsFile << (-10000.0f / (float)winningMoneyLine) << "; ";
			}
			else
			{
				gamesFactorsFile << "-10;";
			}
			*/
			gamesFactorsFile << opsDiff << ";" << runsDiff << ";";
			gamesFactorsFile << last3OpsDiff << ";" << last1OpsDiff << ";" << t2016OpsDiff << ";" << haOpsDiff << ";" << hOnlyOpsDiff << ";";
			gamesFactorsFile << endl;
		}
	}
	gamesFactorsFile.close();
	allGamesFile.close();
}

void GatherTeamWins()
{
	CURL* curl = NULL;
	string pageData = "";
	for (int i = -1; i <= 12; ++i)
	{
		char iCStr[4];
		_itoa_s(i, iCStr, 10);
		string iStr = iCStr;
		pageData += GetEntireFileContents("2017ResultsTracker\\OddsWinsResults\\CachedPage" + iStr + ".txt");
	}
	fstream allGamesFile;
	allGamesFile.open("2017ResultsTracker\\OddsWinsResults\\AllGamesResults.txt");

	for (int d = 415; d <= 604; ++d)
	{
		int monthInteger = (d / 100) * 100;
		int isolatedDay = d - (monthInteger);
		char thisDayCStr[3];
		_itoa_s(isolatedDay, thisDayCStr, 10);
		string thisDay = thisDayCStr;
		if (isolatedDay < 10)
			thisDay = "0" + thisDay;
		string thisMonth = "";
		switch (d / 100)
		{
		case 3:
			thisMonth = "March";
			break;
		case 4:
			thisMonth = "Apr";
			break;
		case 5:
			thisMonth = "May";
			break;
		case 6:
			thisMonth = "Jun";
			break;
		case 7:
			thisMonth = "Jul";
			break;
		case 8:
			thisMonth = "Aug";
			break;
		case 9:
			thisMonth = "Sep";
			break;
		case 10:
			thisMonth = "Oct";
			break;
		case 11:
			thisMonth = "Nov";
			break;
		}


		ofstream winResultsOutputFile;
		string winResultsFileName = "2017ResultsTracker\\OddsWinsResults\\";
		winResultsFileName += IntToDateYMD(d) +".txt";
		winResultsOutputFile.open(winResultsFileName);

		string dateSearchString = thisDay + " " + thisMonth + " 2017";
		size_t dateIndex = pageData.find(dateSearchString);
		while (dateIndex != string::npos)
		{
			size_t nextDateIndex = pageData.find(" 2017", dateIndex + 9);
			size_t gameGroup = pageData.find("table-time datet", dateIndex);
			size_t gameGroupPrev = gameGroup;
			gameGroup = pageData.find("table-time datet", gameGroupPrev + 1);
			while (gameGroup != string::npos)
			{
				string gameString = pageData.substr(gameGroupPrev, gameGroup - gameGroupPrev);
				if (gameString.find("abandon") == string::npos)
				{
					size_t timeIndex = gameString.find(":");
					size_t prevTimeIndex = gameString.find(">");
					string timeString = gameString.substr(prevTimeIndex + 1, timeIndex - prevTimeIndex - 1);
					size_t winningTeamNameIndex = gameString.find("</span>", 0);
					size_t teamNamePrevIndex = gameString.rfind(">", winningTeamNameIndex);
					string winningTeamName = gameString.substr(teamNamePrevIndex + 1, winningTeamNameIndex - teamNamePrevIndex - 1);
					size_t losingTeamNameIndex = gameString.find(" - ");
					if (losingTeamNameIndex > winningTeamNameIndex)
					{
						teamNamePrevIndex = losingTeamNameIndex + 2;
						losingTeamNameIndex = gameString.find("<", teamNamePrevIndex);
					}
					else
					{
						teamNamePrevIndex = gameString.rfind(">", losingTeamNameIndex);
					}
					string losingTeamName = gameString.substr(teamNamePrevIndex + 1, losingTeamNameIndex - teamNamePrevIndex - 1);

					size_t payoutIndex = gameString.find("</a></td>", winningTeamNameIndex);
					if (gameString.find("extra inning") == string::npos)
						payoutIndex = gameString.find("</a></td>", payoutIndex + 1);
					string losingTeamPayoutString;
					size_t payoutIndexPrev;
					if (winningTeamNameIndex > losingTeamNameIndex)
					{
						payoutIndexPrev = gameString.rfind(">", payoutIndex);
						losingTeamPayoutString = gameString.substr(payoutIndexPrev + 1, payoutIndex - payoutIndexPrev - 1);
						payoutIndex = gameString.find("</a></td>", payoutIndex + 1);
					}
					payoutIndexPrev = gameString.rfind(">", payoutIndex);
					string payoutString = gameString.substr(payoutIndexPrev + 1, payoutIndex - payoutIndexPrev - 1);
					if (winningTeamNameIndex < losingTeamNameIndex)
					{
						payoutIndex = gameString.find("</a></td>", payoutIndex + 1);
						payoutIndexPrev = gameString.rfind(">", payoutIndex);
						losingTeamPayoutString = gameString.substr(payoutIndexPrev + 1, payoutIndex - payoutIndexPrev - 1);
					}

					if (winningTeamName.find_first_of("&<=01234") == string::npos && losingTeamName.find("&<=01234") == string::npos)
					{
						string yearMonthDate = IntToDateYMD(atoi(timeString.c_str()) <= 7 ? d - 1 : d);
						winResultsOutputFile << yearMonthDate << ";";
						winResultsOutputFile << winningTeamName << ";" << losingTeamName << ";";
						if (winningTeamNameIndex < losingTeamNameIndex)
							winResultsOutputFile << "H;";
						else
							winResultsOutputFile << "A;";
						winResultsOutputFile << payoutString << ";" << losingTeamPayoutString << ";";
						winResultsOutputFile << endl;
					}
				}
				if (gameGroup < nextDateIndex)
				{
					gameGroupPrev = gameGroup;
					gameGroup = pageData.find("table-time datet", gameGroupPrev + 1);
					if (gameGroup >= nextDateIndex)
						gameGroup = nextDateIndex;
				}
				else
					break;
			}
			dateIndex = pageData.find(dateSearchString, dateIndex + 1);
			if (dateIndex != string::npos)
				int x = 0;
		}
		winResultsOutputFile.close();
		allGamesFile << GetEntireFileContents(winResultsFileName);
	}
	allGamesFile.close();
}

std::vector<string> GetRankingsRowColumns(std::string teamName, std::string allData, int numColumns)
{
	vector<string> allColumns;
	size_t teamColumnEnd = allData.find(">" + teamName + "<");
	teamColumnEnd = allData.find("</td>", teamColumnEnd + 1);
	while (numColumns > 0)
	{
		teamColumnEnd = allData.find("</td>", teamColumnEnd + 1);
		size_t teamColumnBegin = allData.rfind(">", teamColumnEnd - 1);
		allColumns.push_back(allData.substr(teamColumnBegin + 1, teamColumnEnd - teamColumnBegin - 1));
		numColumns--;
	}
	return allColumns;
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
					FullSeasonStats batter2016Stats = GetBatterStats(playerId, "2016", curl);
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

void GetBeatTheStreakCandidates(CURL *curl)
{
	if (curl == NULL)
		curl = curl_easy_init();
	if (curl)
	{
		string readURL;
		vector<BeatTheStreakPlayerProfile> allPlayers;
		vector<BeatTheStreakPlayerProfile> eligiblePlayers;

		// do the top x pages
		for (int page = 0; page < 4; ++page)
		{
			std::string last30DaysStats;
			char pageCStr[3];
			_itoa_s(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
			readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=y&type=0&season=2017&month=3&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=6,d&page=" + pageStr + "_50";
			curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &last30DaysStats);
			curl_easy_perform(curl);
			curl_easy_reset(curl);

			size_t playerPositionIndex = last30DaysStats.find("LeaderBoard1_dg1_ctl00__", 0);
			while (playerPositionIndex != string::npos)
			{
				BeatTheStreakPlayerProfile playerProfile;

				for (int i = 0; i < 2; ++i)
				{
					playerPositionIndex = last30DaysStats.find("</td>", playerPositionIndex + 1);
				}
				playerPositionIndex -= 4;
				size_t playerPositionPrevIndex = last30DaysStats.rfind(">", playerPositionIndex);
				playerProfile.playerName = last30DaysStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1);

				for (int i = 0; i < 3; ++i)
				{
					playerPositionIndex = last30DaysStats.find("</td>", playerPositionIndex + 1);
				}
				playerPositionPrevIndex = last30DaysStats.rfind(">", playerPositionIndex);
				int numGamesPlayed = atoi(last30DaysStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1).c_str());

				for (int i = 0; i < 3; ++i)
				{
					playerPositionIndex = last30DaysStats.find("</td>", playerPositionIndex + 1);
				}
				playerPositionPrevIndex = last30DaysStats.rfind(">", playerPositionIndex);
				int numHitsGot = atoi(last30DaysStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1).c_str());

				playerProfile.hitsPerGameLast30Days = (float)numHitsGot / (float)numGamesPlayed;
				if (playerProfile.hitsPerGameLast30Days > 1.05f)
					eligiblePlayers.push_back(playerProfile);
				allPlayers.push_back(playerProfile);
				playerPositionIndex = last30DaysStats.find("LeaderBoard1_dg1_ctl00__", playerPositionIndex + 1);
			}
		}

		std::string last7DaysStats;
		for (int page = 0; page < 6; ++page)
		{
			std::string last30DaysStats;
			char pageCStr[3];
			_itoa_s(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
			readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=10&type=0&season=2017&month=1&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=22,d&page=" + pageStr + "_50";
			curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &last7DaysStats);
			curl_easy_perform(curl);
			curl_easy_reset(curl);
		}		

		for (int i = allPlayers.size() - 1; i >= 0; --i)
		{
			size_t positionOn7DayList = last7DaysStats.find(allPlayers[i].playerName, 0);
			if (positionOn7DayList != string::npos)
			{
				for (int i = 0; i < 22; ++i)
				{
					positionOn7DayList = last7DaysStats.find("</td>", positionOn7DayList + 1);
				}
				size_t prevPositionOn7DayList = last7DaysStats.rfind(">", positionOn7DayList);
				allPlayers[i].averageLast7Days = stof(last7DaysStats.substr(prevPositionOn7DayList + 1, positionOn7DayList - prevPositionOn7DayList - 1));
			}
			else
				allPlayers.erase(allPlayers.begin() + i);
		}

		for (int i = eligiblePlayers.size() - 1; i >= 0; --i)
		{
			size_t positionOn7DayList = last7DaysStats.find(eligiblePlayers[i].playerName, 0);
			bool bKeep = false;
			if (positionOn7DayList != string::npos)
			{
				for (int i = 0; i < 22; ++i)
				{
					positionOn7DayList = last7DaysStats.find("</td>", positionOn7DayList + 1);
				}
				size_t prevPositionOn7DayList = last7DaysStats.rfind(">", positionOn7DayList);
				eligiblePlayers[i].averageLast7Days = stof(last7DaysStats.substr(prevPositionOn7DayList + 1, positionOn7DayList - prevPositionOn7DayList - 1));
				bKeep = eligiblePlayers[i].averageLast7Days >= 0.299f;
			}
			if (!bKeep)
				eligiblePlayers.erase(eligiblePlayers.begin() + i);
		}

		std::string versusPitcherDirectStats;
		readURL = "http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=0&showdfs=&sort=woba&r40=0&scsv=1&nohead=1&user=GoldenExcalibur&key=G5970032941";
		curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &versusPitcherDirectStats);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		for (int i = allPlayers.size() - 1; i >= 0; --i)
		{
			size_t positionOnVPitcherList = versusPitcherDirectStats.find(allPlayers[i].playerName, 0);
			if (positionOnVPitcherList != string::npos)
			{
				for (int i = 0; i < 4; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				size_t prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				allPlayers[i].batterHandedness = versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1);

				for (int i = 0; i < 12; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				int numABsVPitcher = atoi(versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1).c_str());

				for (int i = 0; i < 1; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				int numHitsVPitcher = atoi(versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1).c_str());

				for (int i = 0; i < 19; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				allPlayers[i].opposingPitcherName = versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1);

				if (numABsVPitcher >= 3)
				{
					allPlayers[i].averageVsPitcherFacing = (float)numHitsVPitcher / (float)numABsVPitcher;
				}
				else
				{
					allPlayers[i].averageVsPitcherFacing = -1;
				}
			}
			else
			{
				allPlayers.erase(allPlayers.begin() + i);
			}
			
		}

		for (int i = eligiblePlayers.size() - 1; i >= 0; --i)
		{
			size_t positionOnVPitcherList = versusPitcherDirectStats.find(eligiblePlayers[i].playerName, 0);
			bool bKeep = false;
			if (positionOnVPitcherList != string::npos)
			{
				for (int i = 0; i < 4; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				size_t prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				eligiblePlayers[i].batterHandedness = versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1);

				for (int i = 0; i < 12; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				int numABsVPitcher = atoi(versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1).c_str());

				for (int i = 0; i < 1; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				int numHitsVPitcher = atoi(versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1).c_str());

				for (int i = 0; i < 19; ++i)
				{
					positionOnVPitcherList = versusPitcherDirectStats.find(";", positionOnVPitcherList + 1);
				}
				prevPositionOnVPitcherList = versusPitcherDirectStats.rfind(";", positionOnVPitcherList - 1);
				eligiblePlayers[i].opposingPitcherName = versusPitcherDirectStats.substr(prevPositionOnVPitcherList + 1, positionOnVPitcherList - prevPositionOnVPitcherList - 1);

				if (numABsVPitcher >= 3)
				{
					eligiblePlayers[i].averageVsPitcherFacing = (float)numHitsVPitcher / (float)numABsVPitcher;
					if (eligiblePlayers[i].averageVsPitcherFacing >= 0.299f)
						bKeep = true;
				}
				else
				{
					eligiblePlayers[i].averageVsPitcherFacing = -1;
					bKeep = true;
				}
			}
			else
			{
				cout << eligiblePlayers[i].playerName << " not found on player versus batter page." << endl;
			}
			if (!bKeep)
				eligiblePlayers.erase(eligiblePlayers.begin() + i);
		}


		std::string startingPitcherData;
		readURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=1&sort=4&game=d&colA=0&daypt=0&denom=3&xavg=0&inact=0&maxprc=99999&sched=1&starters=1&hithand=0&numlist=c&user=GoldenExcalibur&key=G5970032941";
		curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &startingPitcherData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		for (int i = allPlayers.size() - 1; i >= 0; --i)
		{
			size_t pitcherIndex = startingPitcherData.find(allPlayers[i].opposingPitcherName, 0);
			size_t prevPitcherIndex = startingPitcherData.rfind("\n", pitcherIndex);
			pitcherIndex = startingPitcherData.find(";", prevPitcherIndex + 1);
			if (pitcherIndex != string::npos)
			{
				string pitcherGID = startingPitcherData.substr(prevPitcherIndex + 1, pitcherIndex - prevPitcherIndex - 1);

				//FullSeasonStatsAdvanced pitcherVBatter2017Stats = GetPitcherAdvancedStats(singlePlayerData.playerId, "2017", curl);
				FullSeasonPitcherStats pitcher2017Stats = GetPitcherStats(pitcherGID, "2017", curl);
				for (int i = 0; i < 19; ++i)
				{
					pitcherIndex = startingPitcherData.find(";", pitcherIndex + 1);
				}
				prevPitcherIndex = startingPitcherData.rfind(";", pitcherIndex - 1);
				string pitcherHandedness = startingPitcherData.substr(prevPitcherIndex + 1, pitcherIndex - prevPitcherIndex - 1);
				FullSeasonStatsAdvanced pitcher2017AdvancedStats = GetPitcherAdvancedStats(pitcherGID, "2017", curl);

				allPlayers[i].opposingPitcherEra = pitcher2017Stats.era;
				allPlayers[i].opposingPitcherStrikeOutsPer9 = pitcher2017Stats.strikeOutsPer9;
				allPlayers[i].opposingPitcherWhip = pitcher2017Stats.whip;
				allPlayers[i].opposingPitcherAverageAgainstHandedness = (pitcher2017AdvancedStats.averageVersusLefty + pitcher2017AdvancedStats.averageVersusRighty) * 0.5f;
				if (allPlayers[i].batterHandedness == "L")
					allPlayers[i].opposingPitcherAverageAgainstHandedness = pitcher2017AdvancedStats.averageVersusLefty;
				else if (allPlayers[i].batterHandedness == "R")
					allPlayers[i].opposingPitcherAverageAgainstHandedness = pitcher2017AdvancedStats.averageVersusRighty;

				if (allPlayers[i].opposingPitcherEra < 0)
					allPlayers.erase(allPlayers.begin() + i);
			}
			else
			{
				allPlayers.erase(allPlayers.begin() + i);
			}
		}

		for (int i = eligiblePlayers.size() - 1; i >= 0; --i)
		{
			size_t pitcherIndex = startingPitcherData.find(eligiblePlayers[i].opposingPitcherName, 0);
			size_t prevPitcherIndex = startingPitcherData.rfind("\n", pitcherIndex);
			pitcherIndex = startingPitcherData.find(";", prevPitcherIndex + 1);
			bool bKeep = false;
			if (pitcherIndex != string::npos)
			{
				string pitcherGID = startingPitcherData.substr(prevPitcherIndex + 1, pitcherIndex - prevPitcherIndex - 1);
				
				//FullSeasonStatsAdvanced pitcherVBatter2017Stats = GetPitcherAdvancedStats(singlePlayerData.playerId, "2017", curl);
				FullSeasonPitcherStats pitcher2017Stats = GetPitcherStats(pitcherGID, "2017", curl);
				
				if ( pitcher2017Stats.strikeOutsPer9 < 7.69f && pitcher2017Stats.era >= 4.15f && pitcher2017Stats.whip >= 1.308f)
				{
					for (int i = 0; i < 19; ++i)
					{
						pitcherIndex = startingPitcherData.find(";", pitcherIndex + 1);
					}
					prevPitcherIndex = startingPitcherData.rfind(";", pitcherIndex - 1);
					string pitcherHandedness = startingPitcherData.substr(prevPitcherIndex + 1, pitcherIndex - prevPitcherIndex - 1);
					FullSeasonStatsAdvanced pitcher2017AdvancedStats = GetPitcherAdvancedStats(pitcherGID, "2017", curl);
					
					eligiblePlayers[i].opposingPitcherEra = pitcher2017Stats.era;
					eligiblePlayers[i].opposingPitcherStrikeOutsPer9 = pitcher2017Stats.strikeOutsPer9;
					eligiblePlayers[i].opposingPitcherWhip = pitcher2017Stats.whip;
					eligiblePlayers[i].opposingPitcherAverageAgainstHandedness = (pitcher2017AdvancedStats.averageVersusLefty + pitcher2017AdvancedStats.averageVersusRighty) * 0.5f;
					if (eligiblePlayers[i].batterHandedness == "L")
						eligiblePlayers[i].opposingPitcherAverageAgainstHandedness = pitcher2017AdvancedStats.averageVersusLefty;
					else if (eligiblePlayers[i].batterHandedness == "R")
						eligiblePlayers[i].opposingPitcherAverageAgainstHandedness = pitcher2017AdvancedStats.averageVersusRighty;

					if (eligiblePlayers[i].opposingPitcherAverageAgainstHandedness >= 0.280f)
						bKeep = true;
				}
			}
			else
			{
				cout << eligiblePlayers[i].playerName << " opposing pitcher not found on starting pitchers page." << endl;
			}
			if (!bKeep)
				eligiblePlayers.erase(eligiblePlayers.begin() + i);
		}

		// park avg factor above 0.99

		sort(eligiblePlayers.begin(), eligiblePlayers.end(),
			[](const BeatTheStreakPlayerProfile& s1, const BeatTheStreakPlayerProfile& s2) -> bool
		{
			return s1.hitsPerGameLast30Days > s2.hitsPerGameLast30Days;
		});

		ofstream resultsTrackerFile;
		string resultsTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\" + todaysDate + ".txt";
		resultsTrackerFile.open(resultsTrackerFileName);
		for (unsigned int i = 0; i < eligiblePlayers.size(); ++i)
		{
			resultsTrackerFile << eligiblePlayers[i].playerName << ";" << eligiblePlayers[i].hitsPerGameLast30Days << ";" << eligiblePlayers[i].averageLast7Days << ";" << eligiblePlayers[i].averageVsPitcherFacing << ";";
			resultsTrackerFile << endl;
		}
		resultsTrackerFile.close();

		ofstream allResultsTrackerFile;
		string allResultsTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\AllPlayersDaily\\" + todaysDate + ".txt";
		allResultsTrackerFile.open(allResultsTrackerFileName);

		for (unsigned int i = 0; i < allPlayers.size(); ++i)
		{
			allResultsTrackerFile << allPlayers[i].ToString();
			allResultsTrackerFile << endl;
		}
		allResultsTrackerFile.close();
		int x = 0;
	}
}

static string ballParkFactorData = "";
void GetBallparkFactors(string ballparkName, string statName, float& outFactorLeftyBatter, float& outFactorRightyBatter)
{
	if (ballParkFactorData == "")
		ballParkFactorData = GetEntireFileContents("BallparkFactors.txt");

	size_t ballParkIndex = ballParkFactorData.find(ballparkName, 0);
	if (ballParkIndex != string::npos)
	{
		ballParkIndex = ballParkFactorData.find(";" + statName + ";", ballParkIndex);
		size_t ballParkEndIndex;
		size_t leftHandedBatterIndex = ballParkIndex + statName.length() + 1;
		ballParkEndIndex = ballParkFactorData.find("\n", leftHandedBatterIndex);
		if (leftHandedBatterIndex != string::npos && ballParkEndIndex != string::npos)
			outFactorLeftyBatter = stof(ballParkFactorData.substr(leftHandedBatterIndex + 1, ballParkEndIndex - leftHandedBatterIndex - 1));

		ballParkEndIndex = ballParkIndex;
		size_t rightHandedBatterIndex = ballParkFactorData.rfind("\n", ballParkEndIndex);
		if (rightHandedBatterIndex != string::npos && ballParkEndIndex != string::npos)
			outFactorRightyBatter = stof(ballParkFactorData.substr(rightHandedBatterIndex + 1, ballParkEndIndex - rightHandedBatterIndex - 1));

	}
}

std::vector<string> SplitStringIntoMultiple(std::string wholeString, std::string tokens)
{
	vector<string> stringArray;
	std::vector<char> writableWholeString(wholeString.begin(), wholeString.end());
	writableWholeString.push_back('\0');
	char * pch;
	pch = strtok(&writableWholeString[0], tokens.c_str());
	while (pch != NULL)
	{
		stringArray.push_back(pch);
		pch = strtok(NULL, tokens.c_str());
	}
	return stringArray;
}

string BeatTheStreakPlayerProfile::ToString()
{
	// Name;HitsPerGameLast30Days;AvgLast7Days;AvgVPitcher;PitcherWhip;PitcherEra;PitcherKPer9;PitcherAvgAgainst;
	return playerName + ";" + to_string(hitsPerGameLast30Days) + ";" + to_string(averageLast7Days) + ";" + to_string(averageVsPitcherFacing) + ";" + to_string(opposingPitcherWhip) + ";" + to_string(opposingPitcherEra) + ";" + to_string(opposingPitcherStrikeOutsPer9) + ";" + to_string(opposingPitcherAverageAgainstHandedness) + ";";
}

void AssembleBatterSplits(CURL *curl)
{
	if (curl == NULL)
		curl = curl_easy_init();
	if (curl)
	{
		for (int page = 0; page < 6; ++page)
		{
			std::string seasonStats;
			char pageCStr[3];
			_itoa_s(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
			string readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=80&type=1&season=2017&month=0&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=10,d&page=" + pageStr + "_50";
			CurlGetSiteContents(curl, readURL, seasonStats);

			size_t playerPositionIndex = seasonStats.find("LeaderBoard1_dg1_ctl00__", 0);
			while (playerPositionIndex != string::npos)
			{
				BatterSplitsData playerProfile;

				for (int i = 0; i < 2; ++i)
				{
					playerPositionIndex = seasonStats.find("</td>", playerPositionIndex + 1);
				}
				playerPositionIndex -= 4;
				size_t playerPositionPrevIndex = seasonStats.rfind(">", playerPositionIndex);
				string playerName = seasonStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1);
				
				for (int i = 0; i < 10; ++i)
				{
					playerPositionIndex = seasonStats.find("</td>", playerPositionIndex + 1);
				}
				playerPositionPrevIndex = seasonStats.rfind(">", playerPositionIndex);
				playerProfile.opsSeason = stof(seasonStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1).c_str());
				
				if (allBattersSplits.find(playerName) == allBattersSplits.end())
					allBattersSplits.insert({ playerName,playerProfile });

				playerPositionIndex = seasonStats.find("LeaderBoard1_dg1_ctl00__", playerPositionIndex + 1);
			}
		}

		std::string last30DaysStats;
		for (int page = 0; page < 6; ++page)
		{
			char pageCStr[3];
			_itoa_s(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
			string readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=60&type=1&season=2017&month=3&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=10,d&page=" + pageStr + "_50";
			CurlGetSiteContents(curl, readURL, last30DaysStats);
			
		}
		for (auto& it : allBattersSplits)
		{
			size_t playerPositionIndex = last30DaysStats.find("LeaderBoard1_dg1_ctl00__", 0);
			playerPositionIndex = last30DaysStats.find(it.first, playerPositionIndex);
			if (playerPositionIndex != string::npos)
			{
				for (int i = 0; i < 10; ++i)
				{
					playerPositionIndex = last30DaysStats.find("</td>", playerPositionIndex + 1);
				}
				size_t playerPositionPrevIndex = last30DaysStats.rfind(">", playerPositionIndex);
				it.second.opsLast30Days = stof(last30DaysStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1).c_str());
			}
		}

		std::string last7DaysStats;
		for (int page = 0; page < 6; ++page)
		{
			char pageCStr[3];
			_itoa_s(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
			string readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=10&type=1&season=2017&month=1&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=10,d&page=" + pageStr + "_50";
			CurlGetSiteContents(curl, readURL, last7DaysStats);
		}
		for (auto& it : allBattersSplits)
		{
			size_t playerPositionIndex = last7DaysStats.find("LeaderBoard1_dg1_ctl00__", 0);
			playerPositionIndex = last7DaysStats.find(it.first, playerPositionIndex);
			if (playerPositionIndex != string::npos)
			{
				for (int i = 0; i < 10; ++i)
				{
					playerPositionIndex = last7DaysStats.find("</td>", playerPositionIndex + 1);
				}
				size_t playerPositionPrevIndex = last7DaysStats.rfind(">", playerPositionIndex);
				it.second.opsLast7Days = stof(last7DaysStats.substr(playerPositionPrevIndex + 1, playerPositionIndex - playerPositionPrevIndex - 1).c_str());
			}
		}
	}
}

std::string ConvertFLNameToLFName(std::string firstLast)
{
	string convertedName = firstLast;
	size_t spaceIndex = firstLast.find(" ", 0);
	if (spaceIndex != string::npos)
	{
		convertedName = firstLast.substr(spaceIndex + 1);
		convertedName += ", ";
		convertedName += firstLast.substr(0, spaceIndex);
	}
	return convertedName;
}
std::string ConvertLFNameToFLName(std::string lastFirst)
{
	string convertedName = lastFirst;
	size_t commaIndex = lastFirst.find(", ", 0);
	if (commaIndex != string::npos)
	{
		convertedName = lastFirst.substr(commaIndex + 2);
		convertedName += " ";
		convertedName += lastFirst.substr(0, commaIndex);
	}
	return convertedName;
}
std::string IntToDateYMD(int date, bool roundUp)
{
	int monthInteger = (date / 100);
	int isolatedDay = date - (monthInteger * 100);
	if (isolatedDay == 0)
	{
		monthInteger--;
		date -= 100;
		switch (monthInteger)
		{
		case 4:
		case 6:
		case 9:
			isolatedDay = 30;
			break;
		default:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
			isolatedDay = 31;
			break;
		}
		date = monthInteger * 100 + isolatedDay;
	}
	
	char thisDateCStr[5];
	_itoa_s(date, thisDateCStr, 10);
	string thisDate = thisDateCStr;

	string dateFormatted = "2017";
	if (date < 1000)
		dateFormatted += "0";
	dateFormatted += thisDate;
	return dateFormatted;
}

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer)
{
	if (curl == NULL)
		curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeBuffer);
		curl_easy_perform(curl);
		curl_easy_reset(curl);
	}
}

/*
http://rotoguru1.com/cgi-bin/stats.cgi?pos=6&sort=6&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=1&hithand=1&numlist=c&user=GoldenExcalibur&key=G5970032941
0    1    2               3     4         5             6      7    8     9        10             11       12           13      14      15      16        17     18    19       20    21     22          23
GID; Pos; Name;           Team; Salary; Salary Change; Points; GS;  GP; Active; Pts / Game; Pts / G / $; Pts / G(alt); Last; Days ago; MLBID;  ESPNID; YahooID; Bats; Throws; H / A; Oppt; Oppt hand; Game title
5125; 3; Cabrera, Miguel; det;  4000;      0;             0;     0; 0;    1;     0;           0;             0;         0;      0;     408234; 5544;   7163;     R;      R;     A;    chw;    L;       Jose Quintana(L) chw vs.det - 4:10 PM EDT - U.S.Cellular Field



http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=0&user=GoldenExcalibur&key=G5970032941
http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=1&nohead=1&user=GoldenExcalibur&key=G5970032941
pitcher vs batter matchups
MLB_ID;  ESPN_ID;  Name(LF);          Name(FL);         Team;  H/A;  Bats;  Active;  FD_pos;  DK_pos;  DD_pos;  YH_pos;  FD_sal;  DK_sal;  DD_sal;  YH_sal;  NP;  PA;  AB;  Hits;  2B;  3B;  HR;  Runs;  RBI;  BB;  IBB;  SO;  HBP;  SB;  CS;  AVG;   OBP;   SLG;   OPS;     wOBA;  MLB_ID(p);  ESPN_ID(p);  Pitcher_name(LF);  Pitcher_name(FL);  P_Team;  Throws;  game_time;    Stadium;        FD_sal;  DK_sal;  DD_sal;  YH_sal
453056;  28637;    Ellsbury, Jacoby;  Jacoby Ellsbury;  nyy;   H;    L;     1;       7;       7;       7;       7;        ;       ;        ;        ;        16;  4;   4;   2;     0;   0;   0;   0;     1;    0;   0;    2;   0;    0;   1;   .500;  .500;  .500;  1.000;  .450;   502009;     30196;       Latos, Mat;        Mat Latos;         tor;     R;       7:05 PM EDT;  Yankee Stadium; ;        ;        ;


http://rotoguru1.com/cgi-bin/byday.pl?date=404&game=fd&scsv=1&user=GoldenExcalibur&key=G5970032941
http://rotoguru1.com/cgi-bin/byday.pl?date=404&game=fd&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941
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


http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=y&type=0&season=2017&month=3&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=6,d&page=1_50
top batters sorted by total hits last 30 days
http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=10&type=0&season=2017&month=1&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=22,d&page=1_50
top batters sorted by average last 7 days

http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=y&type=0&season=2017&month=13&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&page=1_50
http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=y&type=0&season=2017&month=14&season1=2017&ind=0&team=0&rost=0&age=0&filter=&players=0&page=1_50
top batters vs leftys, 2017 season
top batters vs rightys, 2017 season

*/