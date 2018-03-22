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
#include "StringUtils.h"
#include "Main.h"
using namespace std;

// BeatTheStreak not supported past 2017, RIP
GameType gameType = GameType::Fanduel;
int maxTotalBudget = 35000;
// game times in Eastern and 24 hour format
int latestGameTime = 99;
int earliestGameTime = -1;
std::string todaysDate = "20171001";
int reviewDateStart = 515;
int reviewDateEnd = 609;
float percentOfSeasonPassed = 161.0f / 162.0f;
// tournament is:
// any batting order
// applies team stacks
// max 4 per team
bool bTournamentLineup = true;
// regular (non-tournament) is:
// batting order 2-5 (6 for catchers)
// applies team stacks
// max 3 per team (to avoid if a team happens to be cold)

int dayToDayInjuredPlayersNum = 0;
string dayToDayInjuredPlayers[] = { "" };

string pitcherTeamCode = "";
string pitcherOpponentTeamCode = "";

const float leagueAverageOps = 0.72f;


vector< vector<PlayerData> > allPlayers;
unordered_map<std::string, OpponentInformation> opponentMap;
vector<string> probableRainoutGames;

int main(void)
{
	enum ProcessType { Analyze2016, GenerateLineup, Refine, UnitTest, AnalyzeTeamWins};
	ProcessType processType = ProcessType::Refine;
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
			//AssembleBatterSplits(curl);
			ChooseAPitcher(curl);
			//GenerateNewLineup(curl);
			GenerateNewLineupFromSabrPredictor(curl);
		}
		break;
	}

	cout << "program has finished" << endl;
	getchar();
	return 0;
}

bool compareTeamsByAveragePlayerPointsPerGame(TeamStackTracker a, TeamStackTracker b) {
	if (b.numPlayersAdded <= 0)
		return true;
	if (a.numPlayersAdded <= 0)
		return false;
	return (a.teamTotalExpectedPoints / (float)a.numPlayersAdded) > (b.teamTotalExpectedPoints / (float)b.numPlayersAdded);
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

float getActualPoints(PlayerData playerData, string actualResults, string dayWithoutYear) {
	size_t playerIdIndex = actualResults.find(dayWithoutYear + ";" + playerData.playerId + ";", 0);
	if (playerIdIndex != string::npos)
	{
		for (int n = 0; n < 7; ++n) {
			playerIdIndex = actualResults.find(";", playerIdIndex + 1);
		}
		size_t nextPlayerIdIndex = actualResults.find(";", playerIdIndex + 1);
		string actualPointsString = actualResults.substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1);
		float actualPoints = stof(actualPointsString.c_str());
		if (actualPoints > 100)
			cout << "likely error" << std::endl;
		return actualPoints;
	}
	return 0;
}
float tallyLineupTotals(vector<PlayerData> chosenLineup, string actualResults, string dayWithoutYear) {
	float totalPoints = 0;
	for (unsigned int cp = 0; cp < chosenLineup.size(); ++cp) {
		totalPoints += getActualPoints(chosenLineup[cp], actualResults, dayWithoutYear);
	}
	return totalPoints;
}

string getSabrPredictorFileContents(string date, bool bPitchers) {
	int dateInt = atoi(date.c_str());
	if (dateInt < 19410101) {
		cout << "Incorrect date passed into getSabrPredictorFileContents, assuming current year." << endl;
		dateInt += CurrentYearAsInt() * 10000;
		char thisDateCStr[9];
		itoa(dateInt, thisDateCStr, 10);
		date = thisDateCStr;
	}
	string sabrPredictorFileName = "FangraphsSABRPredictions\\";
	if (bPitchers)
		sabrPredictorFileName += "Pitchers\\";
	else
		sabrPredictorFileName += "Batters\\";
	sabrPredictorFileName += date;
	sabrPredictorFileName += ".csv";
	string sabrPredictorText = GetEntireFileContents(sabrPredictorFileName);
	return sabrPredictorText;
}

void RefineAlgorithm()
{
	bool bRefineForPitchers = true;
	bool bRefineForBatters = true;
	bool bCombinePitcherIntoLineup = true;
	bool bRefineForGames = false;
	bool bKeepLineupDatesInSync = false;

	fstream gamesRecordOverallFile;
	if (bRefineForGames) {
        string gamesRecordFileName = "2017ResultsTracker\\TeamWinResults\\AllGames.txt";
#if PLATFORM_OSX
        gamesRecordFileName = GetPlatformCompatibleFileNameFromRelativePath(gamesRecordFileName);
#endif
		gamesRecordOverallFile.open(gamesRecordFileName);
	}
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
		vector<float> sabrPredictorValues;
		vector<float> sabrPredictorOutputValues;
		float inputCoefficients[2] = { 0.0f, 1.0f };
		vector< float > outputValues;

		vector< vector<float>> chosenLineupsList(39);
		string batters2017SeasonProjections = GetEntireFileContents("ZiPSBatterProjections2017.txt");
		string pitchers2017SeasonProjections = GetEntireFileContents("ZiPSPitcherProjections2017.txt");

		vector<float> pitcherInputValues;
		vector<float> pitcherOutputValues;
		vector<float> sabrPredictorPitcherInputValues;
		vector<float> sabrPredictorPitcherOutputValues;
		reviewDateStart = 20170410;
		reviewDateEnd = 20170704;
		percentOfSeasonPassed = 10.0f / 160.0f;
        string top10PitchersTrainingFileName = "Top10PitchersTrainingFile.csv";
        string top25BattersTrainingFileName = "Top25Order25BattersTrainingFile.csv";
        string top30BattersWithPitcherTrainingFileName = "Top30Order25BattersWithPitcherTrainingFile.csv";
#if PLATFORM_OSX
        top10PitchersTrainingFileName = GetPlatformCompatibleFileNameFromRelativePath(top10PitchersTrainingFileName);
        top25BattersTrainingFileName = GetPlatformCompatibleFileNameFromRelativePath(top25BattersTrainingFileName);
        top30BattersWithPitcherTrainingFileName = GetPlatformCompatibleFileNameFromRelativePath(top30BattersWithPitcherTrainingFileName);
#endif
		fstream top10PitchersTrainingFile(top10PitchersTrainingFileName, std::ios::out);
		fstream top25BattersTrainingFile(top25BattersTrainingFileName, std::ios::out);
		fstream top30BattersWithPitcherTrainingFile(top30BattersWithPitcherTrainingFileName, std::ios::out);
		int numBattersPutInTrainingFileToday = 0;
		int numPitchersPutInTrainingFileToday = 0;
		int numBattersWithPitcherPutInTrainingFileToday = 0;
		FullSeasonPitcherStats playersOver25PointsSumPitcher;
		FullSeasonStatsAdvancedNoHandedness playersOver25PointsSum;
		int numPlayersOver25Points = 0;
		playersOver25PointsSumPitcher.era = playersOver25PointsSumPitcher.fip = playersOver25PointsSumPitcher.numInnings = playersOver25PointsSumPitcher.opsAllowed = playersOver25PointsSumPitcher.strikeOutsPer9 = playersOver25PointsSumPitcher.whip = playersOver25PointsSumPitcher.xfip = playersOver25PointsSumPitcher.wobaAllowed = 0;
		playersOver25PointsSum.average = playersOver25PointsSum.iso = playersOver25PointsSum.onBaseAverage = playersOver25PointsSum.ops = playersOver25PointsSum.slugging = playersOver25PointsSum.strikeoutPercent = playersOver25PointsSum.walkPercent = playersOver25PointsSum.woba = playersOver25PointsSum.wrcPlus = 0;
		for (int d = reviewDateStart; d <= reviewDateEnd; ++d)
		{
			int yearInt = d / 10000;
			int monthInt = (d - yearInt * 10000) / 100;
			int dayInt = (d - yearInt * 10000 - monthInt * 100);
			if (dayInt > 31) {
				d = yearInt * 10000 + (monthInt + 1) * 100;
				continue;
			}
			if (monthInt > 10) {
				d = (yearInt + 1) * 10000 + 315;
				continue;
			}
			percentOfSeasonPassed += 1.0f / 162.0f;
			if (percentOfSeasonPassed > 1)
				percentOfSeasonPassed = 1;
			char thisDateCStr[9];
			itoa(d, thisDateCStr, 10);
			string thisDate = thisDateCStr;
			string thisDateWithoutYear = thisDate.substr(4);
			if (thisDateWithoutYear.at(0) == '0') {
				thisDateWithoutYear = thisDateWithoutYear.substr(1);
			}
			string thisDateOnePrevious = IntToDateYMD(d - 1);
			string actualResults;
			string resultsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + thisDateWithoutYear + "&game=fd&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941";
			curl_easy_setopt(curl, CURLOPT_URL, resultsURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &actualResults);
			curl_easy_perform(curl);
			curl_easy_reset(curl);

			string resultsLine;

			numBattersPutInTrainingFileToday = 0;
			numPitchersPutInTrainingFileToday = 0;
			numBattersWithPitcherPutInTrainingFileToday = 0;
			if (bRefineForBatters) {
				ifstream resultsTrackerFile;
				string resultsTrackerFileName = "2017ResultsTracker\\";
				resultsTrackerFileName += thisDate + ".txt";
#if PLATFORM_OSX
                resultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(resultsTrackerFileName);
#endif
				resultsTrackerFile.open(resultsTrackerFileName);
				string sabrPredictorText = getSabrPredictorFileContents(thisDate, false);
				string batterVSpecificPitcherTextFileName = "2017ResultsTracker\\BatterVPitcherLogs\\" + thisDate + ".txt";
				string batterVSpecificPitcherText = GetEntireFileContents(batterVSpecificPitcherTextFileName);
				string sabrPredictorTextPitchers = getSabrPredictorFileContents(thisDate, true);

				vector< vector<PlayerData> > allPlayersAll(6);
				vector< vector<PlayerData> > allPlayersHomeRuns(6);
				vector< vector<PlayerData> > allPlayersStackingTeams(6);
				vector< vector<PlayerData> > allPlayers27StackingTeams(6);
				vector< vector<PlayerData> > allPlayers26StackingTeams(6);
				vector< vector<PlayerData> > allPlayers25StackingTeams(6);
				vector< vector<PlayerData> > allPlayersTwoThruFive(6);
				vector< vector<PlayerData> > allPlayers24(6);
				vector< vector<PlayerData> > allPlayers35(6);
				vector< vector<PlayerData> > allPlayers25AvoidPitchers30(6);
				vector< vector<PlayerData> > allPlayers25AvoidPitchers40(6);
				vector< vector<PlayerData> > allPlayers25PitcherMultiply(6);
				vector< vector<PlayerData> > allPlayers25PitcherDkMultiply(6);
				vector< vector<PlayerData> > allPlayers25PitcherYahooMultiply(6);
				vector< vector<PlayerData> > allPlayers25PitcherOpsMultiply(6);
				vector< vector<PlayerData> > allPlayers25OpsBatterVSpecificPitcher(6);
				vector< vector<PlayerData> > allPlayers25MachineLearning(6);
				vector< vector<PlayerData> > allPlayers25MachineLearningPitcherMultiply(6);
				vector< vector<PlayerData> > allPlayersHighScoreThreshold(6);
				vector< vector<PlayerData> > allPlayersHighScoreThresholdOrder25(6);
				vector< vector<PlayerData> > allPlayersProjectionsIso25(6);
				vector< vector<PlayerData> > allPlayersProjectionsOps25(6);
				vector< vector<PlayerData> > allPlayersProjectionsSlugging25(6);
				vector< vector<PlayerData> > allPlayersProjectionsIso25PitcherMultiply(6);
				vector< vector<PlayerData> > allPlayersProjectionsOps25PitcherMultiply(6);
				vector< vector<PlayerData> > allPlayersProjectionsSlugging25PitcherMultiply(6);
				vector< vector<PlayerData> > allPlayersActualScores(6);

				vector< vector<PlayerData> > allPlayers25SeasonOps(6);
				vector< vector<PlayerData> > allPlayers25SeasonOpsPitcherMultiply(6);
				vector< vector<PlayerData> > allPlayers25SeasonIso(6);
				vector< vector<PlayerData> > allPlayers25SeasonIsoPitcherMultiply(6);
				vector< vector<PlayerData> > allPlayers25SeasonIsoPitcherMultiplyEra(6);
				vector< vector<PlayerData> > allPlayers25SeasonIsoPitcherMultiplyFip(6);
				vector< vector<PlayerData> > allPlayers25SeasonIsoPitcherMultiplyXFip(6);
				vector< vector<PlayerData> > allPlayers25SeasonIsoPitcherMultiplyKPer9(6);
				vector< vector<PlayerData> > allPlayers25SeasonIsoPitcherMultiplyWhip(6);

				vector< vector<PlayerData> > allPlayers25SeasonWrc(6);
				vector< vector<PlayerData> > allPlayers25SeasonWrcPitcherMultiply(6);

				vector<TeamStackTracker> teamStackList;

				size_t currentIndex = actualResults.find("</script>", 0);
				currentIndex = actualResults.find("\n", currentIndex + 1);
				size_t nextIndex = actualResults.find("\n", currentIndex + 1);

				unordered_map<std::string, FullSeasonPitcherStats> opponentPitcherScoreMap;
				unordered_map<std::string, FullSeasonPitcherStats> opponentPitcherProjectionsMap;

				while (nextIndex != string::npos) {
					vector<string> thisLineActualResults = SplitStringIntoMultiple(actualResults.substr(currentIndex, nextIndex - currentIndex), ";");
					if (thisLineActualResults.size() < 10)
						continue;
					if (thisLineActualResults[4] == "1") {
						PlayerData singlePlayerData;
						singlePlayerData.playerId = thisLineActualResults[1];
						singlePlayerData.playerName = thisLineActualResults[3];
						singlePlayerData.playerSalary = atoi(thisLineActualResults[8].c_str());
						singlePlayerData.teamCode = thisLineActualResults[9];
						int battingOrder = atoi(thisLineActualResults[5].c_str());
						singlePlayerData.battingOrder = battingOrder;
						float actualPlayerPoints = stof(thisLineActualResults[7]);
						std::transform(singlePlayerData.teamCode.begin(), singlePlayerData.teamCode.end(), singlePlayerData.teamCode.begin(), ::tolower);
						singlePlayerData.playerPointsPerGame = -1;

						int playerPosition = atoi(thisLineActualResults[6].c_str());
						playerPosition -= 2;
						if (playerPosition >= 0) {
							
							FullSeasonStatsAdvancedNoHandedness batterStats = GetBatterCumulativeStatsUpTo(singlePlayerData.playerId, curl, thisDateOnePrevious);
							FullSeasonStatsAdvancedNoHandedness batterStats2016 = GetBatterStatsSeason(singlePlayerData.playerId, curl, "2016");
							FullSeasonStatsAdvancedNoHandedness batterStatsCareer = GetBatterCumulativeStatsUpTo(singlePlayerData.playerId, curl, thisDateOnePrevious, true);
							FullSeasonStatsAdvancedNoHandedness combinedBatterStats = batterStats;
							if (batterStats2016.average >= 0) {
								combinedBatterStats = batterStatsCareer * 0.5f + batterStats2016 * 0.5f;
							}
							combinedBatterStats = combinedBatterStats * (1.0f - percentOfSeasonPassed) + percentOfSeasonPassed * batterStats;
                         
							if (battingOrder >= 2 && battingOrder <= 5 && combinedBatterStats.average > 0.21f) {
								singlePlayerData.playerPointsPerGame = combinedBatterStats.ops * 100.0f;
								allPlayers25SeasonOps[playerPosition].push_back(singlePlayerData);
								singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f;
								allPlayers25SeasonIso[playerPosition].push_back(singlePlayerData);
								singlePlayerData.playerPointsPerGame = combinedBatterStats.wrcPlus;
								allPlayers25SeasonWrc[playerPosition].push_back(singlePlayerData);
								singlePlayerData.playerPointsPerGame = combinedBatterStats.iso * 10.8216856682926313f + combinedBatterStats.wrcPlus * .00302694128934278411f;
								singlePlayerData.playerPointsPerGame *= 10.0f;
								allPlayers25MachineLearning[playerPosition].push_back(singlePlayerData);
								if (numBattersPutInTrainingFileToday < 25) {
									top25BattersTrainingFile << combinedBatterStats.ops * 100.0f << "," << combinedBatterStats.iso * 100.0f << "," << combinedBatterStats.wrcPlus << "," << actualPlayerPoints << endl;
									numBattersPutInTrainingFileToday++;
								}
							}

							size_t playerNameIndex = sabrPredictorText.find(ConvertLFNameToFLName(singlePlayerData.playerName));
							if (playerNameIndex != string::npos) {
								size_t nextNewLine = sabrPredictorText.find("\n", playerNameIndex);
								vector<string> thisSabrLine = SplitStringIntoMultiple(sabrPredictorText.substr(playerNameIndex, nextNewLine - playerNameIndex), ",", "\"");
								float expectedFdPoints = stof(thisSabrLine[17]);
								singlePlayerData.playerPointsPerGame = expectedFdPoints;
								bool teamStackTrackerExists = false;
								for (unsigned int texp = 0; texp < teamStackList.size(); ++texp) {
									if (teamStackList[texp].teamCode == singlePlayerData.teamCode) {
										teamStackList[texp].numPlayersAdded++;
										teamStackList[texp].teamTotalExpectedPoints += singlePlayerData.playerPointsPerGame;
										teamStackTrackerExists = true;
										break;
									}
								}
								if (!teamStackTrackerExists) {
									TeamStackTracker tst;
									tst.numPlayersAdded = 1;
									tst.teamCode = singlePlayerData.teamCode;
									tst.teamTotalExpectedPoints = singlePlayerData.playerPointsPerGame;
									teamStackList.push_back(tst);
								}

								allPlayersAll[playerPosition].push_back(singlePlayerData);
								allPlayersStackingTeams[playerPosition].push_back(singlePlayerData);
								
								if (battingOrder >= 2 && (battingOrder <= 7 || (playerPosition == 0 && battingOrder <= 7))) {
									allPlayers27StackingTeams[playerPosition].push_back(singlePlayerData);
								}
								if (battingOrder >= 2 && (battingOrder <= 6 || (playerPosition == 0 && battingOrder <= 6))) {
									allPlayers26StackingTeams[playerPosition].push_back(singlePlayerData);
								}
								if (battingOrder >= 2 && (battingOrder <= 5 || (playerPosition == 0 && battingOrder <= 6))) {
									allPlayers25StackingTeams[playerPosition].push_back(singlePlayerData);
								}
								if (battingOrder >= 2 && (battingOrder <= 5 || (playerPosition == 0 && battingOrder <= 5))) {
									allPlayersTwoThruFive[playerPosition].push_back(singlePlayerData);
								}
								if (battingOrder >= 2 && (battingOrder <= 4 || (playerPosition == 0 && battingOrder <= 5)))
									allPlayers24[playerPosition].push_back(singlePlayerData);
								if (battingOrder >= 3 && (battingOrder <= 5 || (playerPosition == 0 && battingOrder <= 5)))
									allPlayers35[playerPosition].push_back(singlePlayerData);

								float playerPointsCached = singlePlayerData.playerPointsPerGame;
								singlePlayerData.playerPointsPerGame = stof(thisSabrLine[9]);
								singlePlayerData.playerPointsPerGame *= 100.0f;
								allPlayersHomeRuns[playerPosition].push_back(singlePlayerData);
								singlePlayerData.playerPointsPerGame = playerPointsCached;

								

								if (sabrPredictorTextPitchers != "") {
									string playerTeamName = thisSabrLine[1];
									string playerGameName = thisSabrLine[2];
									size_t gameNameIndex = sabrPredictorTextPitchers.find(playerGameName);
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
										float expectedFdPointsPitcher = stof(thisSabrLinePitchers[14]);
										float expectedDkPointsPitcher = stof(thisSabrLinePitchers[13]);
										float expectedYahooPointsPitcher = stof(thisSabrLinePitchers[12]);
										float pitcherOnBaseAllowed = stof(thisSabrLinePitchers[6]) + stof(thisSabrLinePitchers[11]);
										float pitcherBattersFaced = stof(thisSabrLinePitchers[5]);
										float pitcherTotalBasesAllowed = stof(thisSabrLinePitchers[7]) + stof(thisSabrLinePitchers[8]) * 2 + stof(thisSabrLinePitchers[9]) * 3 + stof(thisSabrLinePitchers[10]) * 4;
										float pitcherOpsAllowed = pitcherOnBaseAllowed / pitcherBattersFaced + pitcherTotalBasesAllowed / pitcherBattersFaced;
										if (battingOrder >= 2 && (battingOrder <= 5 || (playerPosition == 0 && battingOrder <= 5))
											&& expectedFdPointsPitcher < 30)
											allPlayers25AvoidPitchers30[playerPosition].push_back(singlePlayerData);
										if (battingOrder >= 2 && (battingOrder <= 5 || (playerPosition == 0 && battingOrder <= 5))
											&& expectedFdPointsPitcher < 40)
											allPlayers25AvoidPitchers40[playerPosition].push_back(singlePlayerData);
										if (battingOrder >= 2 && battingOrder <= 5) {
											float storedPoints = singlePlayerData.playerPointsPerGame;
											singlePlayerData.playerPointsPerGame *= 160.0f / expectedFdPointsPitcher;
											allPlayers25PitcherMultiply[playerPosition].push_back(singlePlayerData);
											singlePlayerData.playerPointsPerGame = storedPoints * (60.0f / expectedDkPointsPitcher);
											allPlayers25PitcherDkMultiply[playerPosition].push_back(singlePlayerData);
											singlePlayerData.playerPointsPerGame = storedPoints * (120.0f / expectedYahooPointsPitcher);
											allPlayers25PitcherYahooMultiply[playerPosition].push_back(singlePlayerData);
											singlePlayerData.playerPointsPerGame = storedPoints * (1.7f * pitcherOpsAllowed / leagueAverageOps);
											allPlayers25PitcherOpsMultiply[playerPosition].push_back(singlePlayerData);
												
											if (combinedBatterStats.average > 0.21f) {
												playerPointsCached = singlePlayerData.playerPointsPerGame;
												singlePlayerData.playerPointsPerGame = combinedBatterStats.ops * 100.0f * (160.0f / expectedFdPointsPitcher);
												allPlayers25SeasonOpsPitcherMultiply[playerPosition].push_back(singlePlayerData);
												singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f * (160.0f / expectedFdPointsPitcher);
												allPlayers25SeasonIsoPitcherMultiply[playerPosition].push_back(singlePlayerData);
												singlePlayerData.playerPointsPerGame = combinedBatterStats.wrcPlus * (160.0f / expectedFdPointsPitcher);
												allPlayers25SeasonWrcPitcherMultiply[playerPosition].push_back(singlePlayerData);
												singlePlayerData.playerPointsPerGame = playerPointsCached;
											}

											singlePlayerData.playerPointsPerGame = storedPoints;
										}

									}
								}
							}
							size_t playerNameProjectionsIndex = batters2017SeasonProjections.find(ConvertLFNameToFLName(singlePlayerData.playerName));
							if (playerNameProjectionsIndex != string::npos && battingOrder >= 2 && battingOrder <= 5) {
								size_t prevLineIndex = batters2017SeasonProjections.rfind("\n", playerNameProjectionsIndex);
								prevLineIndex++;
								size_t nextLineIndex = batters2017SeasonProjections.find("\n", playerNameProjectionsIndex);
								vector<string> batterProjectionsColumns = SplitStringIntoMultiple(batters2017SeasonProjections.substr(prevLineIndex, nextLineIndex - prevLineIndex), "\t");
								if (batterProjectionsColumns.size() == 10) {
									float projectedPlayerIso = stof(batterProjectionsColumns[4]);
									float projectedPlayerOps = (stof(batterProjectionsColumns[7]) + stof(batterProjectionsColumns[8]));
									float projectedPlayerSlugging = stof(batterProjectionsColumns[8]);
                                    float projectedPlayerOnBaseAverage = stof(batterProjectionsColumns[7]);
                                    if (batterStats.iso >= 0) {
                                        projectedPlayerIso = percentOfSeasonPassed * batterStats.iso + (1.0f - percentOfSeasonPassed) * projectedPlayerIso;
                                        projectedPlayerSlugging = percentOfSeasonPassed * batterStats.slugging + (1.0f - percentOfSeasonPassed) * projectedPlayerSlugging;
                                        projectedPlayerOps = percentOfSeasonPassed * batterStats.ops + (1.0f - percentOfSeasonPassed) * projectedPlayerOps;
                                    }
                                    if (projectedPlayerOnBaseAverage >= 0.31) {
                                        singlePlayerData.playerPointsPerGame = projectedPlayerIso * 100.0f;
                                        allPlayersProjectionsIso25[playerPosition].push_back(singlePlayerData);
                                        singlePlayerData.playerPointsPerGame = projectedPlayerOps * 100.0f;
                                        allPlayersProjectionsOps25[playerPosition].push_back(singlePlayerData);
                                        singlePlayerData.playerPointsPerGame = projectedPlayerSlugging * 100.0f;
                                        allPlayersProjectionsSlugging25[playerPosition].push_back(singlePlayerData);
                                        auto opponentPitcherProjected = opponentPitcherProjectionsMap.find(singlePlayerData.teamCode);
                                        if (opponentPitcherProjected != opponentPitcherProjectionsMap.end()) {
                                            singlePlayerData.playerPointsPerGame = projectedPlayerIso * 100.0f;
                                            singlePlayerData.playerPointsPerGame *= opponentPitcherProjected->second.era;
                                            allPlayersProjectionsIso25PitcherMultiply[playerPosition].push_back(singlePlayerData);
                                            singlePlayerData.playerPointsPerGame = projectedPlayerOps * 100.0f;
                                            singlePlayerData.playerPointsPerGame *= opponentPitcherProjected->second.era;
                                            allPlayersProjectionsOps25PitcherMultiply[playerPosition].push_back(singlePlayerData);
                                            singlePlayerData.playerPointsPerGame = projectedPlayerSlugging * 100.0f;
                                            singlePlayerData.playerPointsPerGame *= opponentPitcherProjected->second.era;
                                            allPlayersProjectionsSlugging25PitcherMultiply[playerPosition].push_back(singlePlayerData);
                                        }
                                    }
								}
								else {
									int breakpoint = 0;
								}
							}
							if (true || sabrPredictorTextPitchers == "") {
								if (combinedBatterStats.average > 0.21f && battingOrder >= 2 && battingOrder <= 5) {
									auto opponentPitcher = opponentPitcherScoreMap.find(singlePlayerData.teamCode);
									if (opponentPitcher != opponentPitcherScoreMap.end()) {
										float pitcherPointsNumerator = 15.0f;
										float pitcherPoints = opponentPitcher->second.era * -0.352834158133307318f + opponentPitcher->second.xfip * -1.50744966177988493f + opponentPitcher->second.strikeOutsPer9 * 1.44486530250260237f;
										singlePlayerData.playerPointsPerGame = combinedBatterStats.ops * 100.0f * (pitcherPointsNumerator / pitcherPoints);
										allPlayers25SeasonOpsPitcherMultiply[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f * (pitcherPointsNumerator / pitcherPoints);
										allPlayers25SeasonIsoPitcherMultiply[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.wrcPlus * (pitcherPointsNumerator / pitcherPoints);
										allPlayers25SeasonWrcPitcherMultiply[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 10.8216856682926313f + combinedBatterStats.wrcPlus * .00302694128934278411f;
										singlePlayerData.playerPointsPerGame *= (pitcherPointsNumerator / pitcherPoints);
										allPlayers25MachineLearningPitcherMultiply[playerPosition].push_back(singlePlayerData);
										
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f;
										singlePlayerData.playerPointsPerGame *= opponentPitcher->second.era;
										allPlayers25SeasonIsoPitcherMultiplyEra[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f;
										singlePlayerData.playerPointsPerGame *= opponentPitcher->second.fip;
										allPlayers25SeasonIsoPitcherMultiplyFip[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f;
										singlePlayerData.playerPointsPerGame *= opponentPitcher->second.xfip;
										allPlayers25SeasonIsoPitcherMultiplyXFip[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 3000.0f;
										singlePlayerData.playerPointsPerGame *= 1.0f / opponentPitcher->second.strikeOutsPer9;
										allPlayers25SeasonIsoPitcherMultiplyKPer9[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.slugging * 100.0f;
										singlePlayerData.playerPointsPerGame *= opponentPitcher->second.whip;
										allPlayers25SeasonIsoPitcherMultiplyWhip[playerPosition].push_back(singlePlayerData);
										
									//	singlePlayerData.playerPointsPerGame = combinedBatterStats.iso * 14.0676013585639081f;
									//	singlePlayerData.playerPointsPerGame += opponentPitcher->second.era * 0.0405585668255538101f + opponentPitcher->second.xfip * 0.535854100213696949f;
									//	allPlayers25MachineLearningPitcherMultiply[playerPosition].push_back(singlePlayerData);
									/*	if (combinedBatterStats.onBaseAverage >= 0.35f && combinedBatterStats.slugging >= 0.49f && combinedBatterStats.wrcPlus >= 120 && opponentPitcher->second.era > 4.3f && opponentPitcher->second.fip > 4.4f) {
											singlePlayerData.playerPointsPerGame = combinedBatterStats.ops * 100.0f;
											allPlayersHighScoreThreshold[playerPosition].push_back(singlePlayerData);
											if (battingOrder >= 2 && battingOrder <= 5) {
												allPlayersHighScoreThresholdOrder25[playerPosition].push_back(singlePlayerData);
											}
										}	*/
										if (numBattersWithPitcherPutInTrainingFileToday < 30) {
										//	top30BattersWithPitcherTrainingFile << combinedBatterStats.iso * 100.0f << "," << combinedBatterStats.wrcPlus;
										//	top30BattersWithPitcherTrainingFile << "," << opponentPitcher->second.era << "," << opponentPitcher->second.xfip;
										//	top30BattersWithPitcherTrainingFile  << "," << actualPlayerPoints << endl;
											numBattersWithPitcherPutInTrainingFileToday++;
										}
										if (actualPlayerPoints >= 25) {
											playersOver25PointsSum += combinedBatterStats;
										//	playersOver25PointsSumPitcher += opponentPitcher->second;
											numPlayersOver25Points++;
										}
									} else {
									/*	float standardMultiplier = 3.0f;
										singlePlayerData.playerPointsPerGame = combinedBatterStats.ops * 100.0f * standardMultiplier;
										allPlayers25SeasonOpsPitcherMultiply[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.iso * 100.0f * standardMultiplier;
										allPlayers25SeasonIsoPitcherMultiply[playerPosition].push_back(singlePlayerData);
										singlePlayerData.playerPointsPerGame = combinedBatterStats.wrcPlus * standardMultiplier;
										allPlayers25SeasonWrcPitcherMultiply[playerPosition].push_back(singlePlayerData);
								*/	}
									singlePlayerData.playerPointsPerGame = combinedBatterStats.ops * 100.0f;
									size_t indexInVSpecificPitcherFile = batterVSpecificPitcherText.find(singlePlayerData.playerName);
									if (indexInVSpecificPitcherFile != string::npos) {
										size_t prevLineIndex = batterVSpecificPitcherText.rfind("\n", indexInVSpecificPitcherFile);
										size_t nextLineIndex = batterVSpecificPitcherText.find("\n", indexInVSpecificPitcherFile);
										vector<string> thisPlayerVSpecificPitcherLines = SplitStringIntoMultiple(batterVSpecificPitcherText.substr(prevLineIndex, nextLineIndex - prevLineIndex), ";");
										if (thisPlayerVSpecificPitcherLines.size() > 47) {
											int numPlateAppearances = atoi(thisPlayerVSpecificPitcherLines[17].c_str());
											if (numPlateAppearances >= 5) {
												singlePlayerData.playerPointsPerGame = stof(thisPlayerVSpecificPitcherLines[34]);
											}
										}
									}
									allPlayers25OpsBatterVSpecificPitcher[playerPosition].push_back(singlePlayerData);
								}
							}
							singlePlayerData.playerPointsPerGame = actualPlayerPoints;
							allPlayersActualScores[playerPosition].push_back(singlePlayerData);
						} else if (playerPosition == -1) {
							if (bRefineForPitchers) {
								FullSeasonPitcherStats pitcherStatsThisYearSoFar = GetPitcherCumulativeStatsUpTo(thisLineActualResults[1], curl, thisDateOnePrevious);
								string opponentTeamCode = thisLineActualResults[10].substr(thisLineActualResults[10].size() - 3);
								auto opponentPitcher = opponentPitcherScoreMap.find(opponentTeamCode);
								if (opponentPitcher == opponentPitcherScoreMap.end()) {
									FullSeasonPitcherStats careerPitcherStats = GetPitcherCumulativeStatsUpTo(thisLineActualResults[1], curl, thisDateOnePrevious, true);
									FullSeasonPitcherStats lastYearPitcherStats = GetPitcherStats(thisLineActualResults[1], "2016", curl);
									FullSeasonPitcherStats combinedPitcherStats(lastYearPitcherStats);
									if (lastYearPitcherStats.numInnings > 0) {
										combinedPitcherStats = careerPitcherStats * 0.5f + lastYearPitcherStats * 0.5f;
										if (pitcherStatsThisYearSoFar.numInnings > 0) {
											combinedPitcherStats = combinedPitcherStats * (1.0f - percentOfSeasonPassed) + percentOfSeasonPassed * pitcherStatsThisYearSoFar;
										}
									}
									else {
										combinedPitcherStats = pitcherStatsThisYearSoFar;
									}
									if (combinedPitcherStats.numInnings > 0) {
										//float pitcherPoints = GetExpectedFanduelPointsFromPitcherStats(combinedPitcherStats);
										float pitcherPoints = combinedPitcherStats.era * -0.352834158133307318f + combinedPitcherStats.xfip * -1.50744966177988493f + combinedPitcherStats.strikeOutsPer9 * 1.44486530250260237f;
										opponentPitcherScoreMap.insert({ opponentTeamCode,combinedPitcherStats });
										pitcherInputValues.push_back(pitcherPoints);
										pitcherOutputValues.push_back(actualPlayerPoints);
										if (numPitchersPutInTrainingFileToday < 10) {
											top10PitchersTrainingFile << combinedPitcherStats.era << "," << combinedPitcherStats.fip << "," << combinedPitcherStats.xfip << "," << combinedPitcherStats.strikeOutsPer9 << "," << actualPlayerPoints << endl;
											numPitchersPutInTrainingFileToday++;
										}
									}
								}
								FullSeasonPitcherStats projectionsPitcherStats;
								auto opponentPitcherProjected = opponentPitcherProjectionsMap.find(opponentTeamCode);
								if (opponentPitcherProjected == opponentPitcherProjectionsMap.end()) {
									size_t playerNameProjectionsIndex = pitchers2017SeasonProjections.find(ConvertLFNameToFLName(singlePlayerData.playerName));
									if (playerNameProjectionsIndex != string::npos) {
										size_t prevLineIndex = pitchers2017SeasonProjections.rfind("\n", playerNameProjectionsIndex);
										prevLineIndex++;
										size_t nextLineIndex = pitchers2017SeasonProjections.find("\n", playerNameProjectionsIndex);
										vector<string> pitcherProjectionsColumns = SplitStringIntoMultiple(pitchers2017SeasonProjections.substr(prevLineIndex, nextLineIndex - prevLineIndex), "\t");
										if (pitcherProjectionsColumns.size() == 10) {
											projectionsPitcherStats.era = stof(pitcherProjectionsColumns[6]);
											projectionsPitcherStats.fip = stof(pitcherProjectionsColumns[7]);
											projectionsPitcherStats.numInnings = stof(pitcherProjectionsColumns[1]);
                                            projectionsPitcherStats.strikeOutsPer9 = 7;
										}
                                        if (pitcherStatsThisYearSoFar.numInnings > 0) {
                                            projectionsPitcherStats = percentOfSeasonPassed * pitcherStatsThisYearSoFar + (1.0f - percentOfSeasonPassed) * projectionsPitcherStats;
                                        }
										opponentPitcherProjectionsMap.insert({ opponentTeamCode, projectionsPitcherStats });
									}
								}
							}
						}
					} 
					if (nextIndex == string::npos)
						break;
					currentIndex = nextIndex + 1;
					nextIndex = actualResults.find("\n", currentIndex + 1);
				}
				
				/*
				vector< vector<PlayerData> > allPlayers25SeasonOps(6);
				vector< vector<PlayerData> > allPlayers25SeasonOpsAdjusted(6);
				vector< vector<PlayerData> > allPlayers25Last30Ops(6);
				vector< vector<PlayerData> > allPlayers25Last30OpsAdjusted(6);
				*/
			/*	while (getline(resultsTrackerFile, resultsLine))
				{
					vector<float> thisInputVariables;
					thisInputVariables.push_back(0);
					thisInputVariables.push_back(0);
					vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
					string thisPlayerId = lineValues[0];

					size_t playerIdIndex = actualResults.find(thisDateWithoutYear + ";" + thisPlayerId + ";", 0);
					if (playerIdIndex != string::npos)// && thisInputVariables[1] > 0)
					{
						inputVariables.push_back(thisInputVariables);
						size_t prevActualResultsNewLine = actualResults.rfind("\n", playerIdIndex);
						size_t nextActualResultsNewLine = actualResults.find("\n", playerIdIndex);
						vector<string> thisLineActualResults = SplitStringIntoMultiple(actualResults.substr(prevActualResultsNewLine + 1, nextActualResultsNewLine - prevActualResultsNewLine - 1), ";");
						if (thisLineActualResults.size() < 10)
							continue;
						string playerName = thisLineActualResults[3];
						playerName = ConvertLFNameToFLName(playerName);
						for (int i = 0; i < 4; ++i)
						{
							playerIdIndex = actualResults.find(";", playerIdIndex + 1);
						}
						expectedPointsInputVariables.push_back(stof(lineValues[3].c_str()));
						outputValues.push_back(stof(thisLineActualResults[7].c_str()));

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
							PlayerData singlePlayerData;
							int playerPosition = atoi(thisLineActualResults[6].c_str());
							playerPosition -= 2;
							if (playerPosition >= 0) {
								singlePlayerData.playerId = thisLineActualResults[1];
								singlePlayerData.playerName = thisLineActualResults[3];
								singlePlayerData.playerSalary = atoi(thisLineActualResults[8].c_str());
								singlePlayerData.playerPointsPerGame = -1;
								int battingOrder = atoi(thisLineActualResults[5].c_str());
								if (battingOrder >= 2 && battingOrder <= 5) {
									singlePlayerData.playerPointsPerGame = seasonOps * 100;
									allPlayers25SeasonOps[playerPosition].push_back(singlePlayerData);
									singlePlayerData.playerPointsPerGame = seasonOps * 100 * adjustmentFactor;
									allPlayers25SeasonOpsAdjusted[playerPosition].push_back(singlePlayerData);
									singlePlayerData.playerPointsPerGame = last30DaysOps * 100;
									allPlayers25Last30Ops[playerPosition].push_back(singlePlayerData);
									singlePlayerData.playerPointsPerGame = last30DaysOps * 100 * adjustmentFactor;
									allPlayers25Last30OpsAdjusted[playerPosition].push_back(singlePlayerData);

								}
							}
							
						}
						if (sabrPredictorText != "") {
							size_t playerNameIndex = sabrPredictorText.find(playerName);
							if (playerNameIndex != string::npos) {
								size_t nextNewLine = sabrPredictorText.find("\n", playerNameIndex);
								vector<string> thisSabrLine = SplitStringIntoMultiple(sabrPredictorText.substr(playerNameIndex, nextNewLine - playerNameIndex), ",", "\"");
								float expectedFdPoints = stof(thisSabrLine[17]);
								sabrPredictorValues.push_back(expectedFdPoints);
								sabrPredictorOutputValues.push_back(outputValues[outputValues.size() - 1]);
							}
						}
					}
				}
			*/
				sort(teamStackList.begin(), teamStackList.end(), compareTeamsByAveragePlayerPointsPerGame);
				
				for (unsigned int pos = 0; pos < allPlayersStackingTeams.size(); ++pos) {
					for (unsigned int player = 0; player < allPlayersStackingTeams[pos].size(); ++player) {
						int teamStackRank = -1;
						for (unsigned int team = 0; team < teamStackList.size(); ++team) {
							if (teamStackList[team].teamCode == allPlayersStackingTeams[pos][player].teamCode) {
								teamStackRank = team;
								break;
							}
						}
						if (teamStackRank < 10) {
							allPlayersStackingTeams[pos][player].playerPointsPerGame += (float)(10 - teamStackRank);
						}
					}
				}
				for (unsigned int pos = 0; pos < allPlayers27StackingTeams.size(); ++pos) {
					for (unsigned int player = 0; player < allPlayers27StackingTeams[pos].size(); ++player) {
						int teamStackRank = -1;
						for (unsigned int team = 0; team < teamStackList.size(); ++team) {
							if (teamStackList[team].teamCode == allPlayers27StackingTeams[pos][player].teamCode) {
								teamStackRank = team;
								break;
							}
						}
						if (teamStackRank < 10) {
							allPlayers27StackingTeams[pos][player].playerPointsPerGame += (float)(10 - teamStackRank);
						}
					}
				}
				for (unsigned int pos = 0; pos < allPlayers26StackingTeams.size(); ++pos) {
					for (unsigned int player = 0; player < allPlayers26StackingTeams[pos].size(); ++player) {
						int teamStackRank = -1;
						for (unsigned int team = 0; team < teamStackList.size(); ++team) {
							if (teamStackList[team].teamCode == allPlayers26StackingTeams[pos][player].teamCode) {
								teamStackRank = team;
								break;
							}
						}
						if (teamStackRank < 10) {
							allPlayers26StackingTeams[pos][player].playerPointsPerGame += (float)(10 - teamStackRank);
						}
					}
				}
				for (unsigned int pos = 0; pos < allPlayers25StackingTeams.size(); ++pos) {
					for (unsigned int player = 0; player < allPlayers25StackingTeams[pos].size(); ++player) {
						int teamStackRank = -1;
						for (unsigned int team = 0; team < teamStackList.size(); ++team) {
							if (teamStackList[team].teamCode == allPlayers25StackingTeams[pos][player].teamCode) {
								teamStackRank = team;
								break;
							}
						}
						if (teamStackRank < 10) {
							allPlayers25StackingTeams[pos][player].playerPointsPerGame += (float)(10 - teamStackRank);
						}
					}
				}

				vector< vector< vector<PlayerData> > > allPlayersLineupOrder;
				allPlayersLineupOrder.push_back(allPlayersAll);		//0
				allPlayersLineupOrder.push_back(allPlayersAll);
				allPlayersLineupOrder.push_back(allPlayersTwoThruFive);
				allPlayersLineupOrder.push_back(allPlayers24);
				allPlayersLineupOrder.push_back(allPlayers25AvoidPitchers30);
				allPlayersLineupOrder.push_back(allPlayers25AvoidPitchers40);	//5
				allPlayersLineupOrder.push_back(allPlayers35);
				allPlayersLineupOrder.push_back(allPlayers25PitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayers25SeasonOps);
				allPlayersLineupOrder.push_back(allPlayers25SeasonOpsPitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayers25SeasonIso);	//10
				allPlayersLineupOrder.push_back(allPlayers25SeasonIsoPitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayers25SeasonIsoPitcherMultiplyEra);
				allPlayersLineupOrder.push_back(allPlayers25SeasonIsoPitcherMultiplyFip);
				allPlayersLineupOrder.push_back(allPlayers25SeasonIsoPitcherMultiplyXFip);
				allPlayersLineupOrder.push_back(allPlayers25SeasonIsoPitcherMultiplyKPer9);	//15
				allPlayersLineupOrder.push_back(allPlayers25SeasonIsoPitcherMultiplyWhip);
				allPlayersLineupOrder.push_back(allPlayersStackingTeams);
				allPlayersLineupOrder.push_back(allPlayers27StackingTeams);
				allPlayersLineupOrder.push_back(allPlayers26StackingTeams);
				allPlayersLineupOrder.push_back(allPlayers25StackingTeams);	//20
				allPlayersLineupOrder.push_back(allPlayersHomeRuns);
				allPlayersLineupOrder.push_back(allPlayers25PitcherDkMultiply);
				allPlayersLineupOrder.push_back(allPlayers25PitcherYahooMultiply);
				allPlayersLineupOrder.push_back(allPlayers25PitcherOpsMultiply);
				allPlayersLineupOrder.push_back(allPlayers25SeasonWrc);	//25
				allPlayersLineupOrder.push_back(allPlayers25SeasonWrcPitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayers25MachineLearning);
				allPlayersLineupOrder.push_back(allPlayers25MachineLearningPitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayers25OpsBatterVSpecificPitcher);
				allPlayersLineupOrder.push_back(allPlayersProjectionsIso25); //30
				allPlayersLineupOrder.push_back(allPlayersProjectionsSlugging25);
				allPlayersLineupOrder.push_back(allPlayersProjectionsOps25);
				allPlayersLineupOrder.push_back(allPlayersProjectionsIso25PitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayersProjectionsSlugging25PitcherMultiply);
				allPlayersLineupOrder.push_back(allPlayersProjectionsOps25PitcherMultiply);	//35
				allPlayersLineupOrder.push_back(allPlayersHighScoreThreshold);
				allPlayersLineupOrder.push_back(allPlayersHighScoreThresholdOrder25);
				allPlayersLineupOrder.push_back(allPlayersActualScores);
				float battingOrderBonus = 10.0f;
				for (unsigned int line = 0; line < chosenLineupsList.size(); ++line) {
					allPlayers.clear();
					unsigned int lineIndex = line;
					unsigned int uniqueLines = chosenLineupsList.size();// (chosenLineupsList.size() / 3) - 1;
					while (lineIndex > uniqueLines) {
						lineIndex -= (uniqueLines + 1);
					}
					allPlayers = allPlayersLineupOrder[lineIndex];
					bool bValidLineup = allPlayers.size() > 0;
					for (unsigned int a = 0; a < allPlayers.size(); ++a) {
						if (allPlayers[a].size() == 0) {
							bValidLineup = false;
							break;
						}
						for (unsigned int pi = 0; pi < allPlayers[a].size(); ++pi) {
							float battingOrderDistFrom3 = 6 - abs(allPlayers[a][pi].battingOrder - 3.0f);
							allPlayers[a][pi].playerPointsPerGame += battingOrderBonus * battingOrderDistFrom3;
						}
					}
					if (!bValidLineup)
						continue;
					if (line > uniqueLines * 2 + 1) {
						for (unsigned int a = 0; a < allPlayers.size(); ++a) {
							for (unsigned int ap = 0; ap < allPlayers[a].size(); ++ap) {
								allPlayers[a][ap].playerPointsPerGame = allPlayers[a][ap].playerPointsPerGame * allPlayers[a][ap].playerPointsPerGame;
							}
						}
					}
					else if (line > uniqueLines) {
						for (unsigned int a = 0; a < allPlayers.size(); ++a) {
							for (unsigned int ap = 0; ap < allPlayers[a].size(); ++ap) {
								allPlayers[a][ap].playerPointsPerGame = sqrtf(allPlayers[a][ap].playerPointsPerGame);
							}
						}
					}
					for (int a = 0; a < 6; ++a) {
						sort(allPlayers[a].begin(), allPlayers[a].end(), comparePlayerByPointsPerGame);
					}
					maxTotalBudget = 25000;
					vector<PlayerData> chosenLineup = OptimizeLineupToFitBudget();
					if (chosenLineup.size() > 0) {
						float totalPoints = tallyLineupTotals(chosenLineup, actualResults, thisDateWithoutYear);
                        if (totalPoints > 200 && line != (chosenLineupsList.size()-1)) {
                            cout << "Got over 200 points on " << d << " with lineup formula " << line << endl;
                        }
						chosenLineupsList[line].push_back(totalPoints);
					}
				}
				if (bKeepLineupDatesInSync) {
					while (chosenLineupsList[6].size() > chosenLineupsList[20].size()) {
						chosenLineupsList[6].pop_back();
					}
					while (chosenLineupsList[20].size() > chosenLineupsList[6].size()) {
						chosenLineupsList[20].pop_back();
					}
				}
				resultsTrackerFile.close();
			}

			if (bRefineForPitchers) {
				ifstream pitcherResultsTrackerFile;
				string pitcherResultsTrackerFileName = "2017ResultsTracker\\Pitchers\\";
				pitcherResultsTrackerFileName += thisDate + ".txt";
#if PLATFORM_OSX
                pitcherResultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(pitcherResultsTrackerFileName);
#endif
				pitcherResultsTrackerFile.open(pitcherResultsTrackerFileName);
				string sabrPredictorText = getSabrPredictorFileContents(thisDate, true);

				while (getline(pitcherResultsTrackerFile, resultsLine))
				{
					vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
					string thisPlayerId = lineValues[0];

					size_t playerIdIndex = actualResults.find(thisDateWithoutYear + ";" + thisPlayerId + ";", 0);
					if (playerIdIndex != string::npos)
					{
						for (int i = 0; i < 3; ++i)
						{
							playerIdIndex = actualResults.find(";", playerIdIndex + 1);
						}
						size_t nextPlayerIdIndex = actualResults.find(";", playerIdIndex + 1);
						string playerName = actualResults.substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1);
						playerName = ConvertLFNameToFLName(playerName);
						for (int i = 0; i < 4; ++i)
						{
							playerIdIndex = actualResults.find(";", playerIdIndex + 1);
						}
						nextPlayerIdIndex = actualResults.find(";", playerIdIndex + 1);
						if (sabrPredictorText != "") {
							size_t playerNameIndex = sabrPredictorText.find(playerName);
							if (playerNameIndex != string::npos) {
								size_t nextNewLine = sabrPredictorText.find("\n", playerNameIndex);
								vector<string> thisSabrLine = SplitStringIntoMultiple(sabrPredictorText.substr(playerNameIndex, nextNewLine - playerNameIndex), ",", "\"");
								float expectedFdPoints = stof(thisSabrLine[14]);
								sabrPredictorPitcherInputValues.push_back(expectedFdPoints);
								sabrPredictorPitcherOutputValues.push_back(stof(actualResults.substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1).c_str()));
							}
						}
					}
				}
				pitcherResultsTrackerFile.close();
			}

			if (bRefineForGames) {
				string actualGamesResults = GetEntireFileContents("2017ResultsTracker\\OddsWinsResults\\AllGamesResults.txt");
				ifstream gamesPredictorFile;
				string gamesPredictorFileName = "2017ResultsTracker\\TeamWinResults\\";
				gamesPredictorFileName += thisDate + ".txt";
#if PLATFORM_OSX
                gamesPredictorFileName = GetPlatformCompatibleFileNameFromRelativePath(gamesPredictorFileName);
#endif
				gamesPredictorFile.open(gamesPredictorFileName);

				while (getline(gamesPredictorFile, resultsLine)) {
					vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
					size_t curentActualGamesIndex = actualGamesResults.find(thisDate);
					vector<string> oddsResultsValues;
					while (curentActualGamesIndex < string::npos) {
						size_t nextActualGamesIndex = actualGamesResults.find("\n", curentActualGamesIndex + 1);
						size_t teamNameIndex = actualGamesResults.find(ConvertTeamCodeToOddsPortalName(lineValues[0], false), curentActualGamesIndex);
						if (teamNameIndex < nextActualGamesIndex) {
							oddsResultsValues = SplitStringIntoMultiple(actualGamesResults.substr(curentActualGamesIndex, nextActualGamesIndex - curentActualGamesIndex), ";");
						}
						curentActualGamesIndex = actualGamesResults.find(thisDateWithoutYear, curentActualGamesIndex + 1);
					}
					if (oddsResultsValues.size() > 0) {
						string correctPrediction = "0";
						if (oddsResultsValues[1] == ConvertTeamCodeToOddsPortalName(lineValues[0], false))
							correctPrediction = "1";
						else if (oddsResultsValues[2] == ConvertTeamCodeToOddsPortalName(lineValues[0], false))
							correctPrediction = "-1";
						else {
							cout << "something went wrong with a game on " << thisDate << " expecting " << lineValues[0] << " and " << lineValues[2] << endl;
							continue;
						}
						gamesRecordOverallFile << thisDateWithoutYear << ";";
						gamesRecordOverallFile << resultsLine << correctPrediction << ";";
						if (correctPrediction == "1")
							gamesRecordOverallFile << oddsResultsValues[4] << ";" << oddsResultsValues[5];
						else if (correctPrediction == "-1")
							gamesRecordOverallFile << oddsResultsValues[5] << ";" << oddsResultsValues[4];
						gamesRecordOverallFile << endl;
					}
				}
			}
		}
		top10PitchersTrainingFile.close();
		top25BattersTrainingFile.close();
		top30BattersWithPitcherTrainingFile.close();
		if (bRefineForBatters) {
			vector<int> lineupsOver80;
			vector<float> lineupsTotalPointsPer;
			for (unsigned int i = 0; i < chosenLineupsList.size(); ++i) {
				float totalLineupPoints = 0;
				int numOver80 = 0;
				for (unsigned int line = 0; line < chosenLineupsList[i].size(); ++line) {
					totalLineupPoints += chosenLineupsList[i][line];
					if (chosenLineupsList[i][line] > 80)
						numOver80++;
				}
				lineupsOver80.push_back(numOver80);
				lineupsTotalPointsPer.push_back(totalLineupPoints / (float)chosenLineupsList[i].size());
				sort(chosenLineupsList[i].begin(), chosenLineupsList[i].end(), greaterSortingFunction());
			}

			float fCoefficientStep = 0.05f;
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

			inputCoefficients[0] = 0.0f;
			inputCoefficients[1] = 0.0f;
			float bestRSquaredMultiplied = -1;
			while (inputCoefficients[0] < 1.0f + fCoefficientStep * 0.5f)
			{
				inputCoefficients[1] = 0.0f;
				while (inputCoefficients[1] < 1.0f + fCoefficientStep * 0.5f)
				{
					vector<float> adjustedInputValues;
					for (unsigned int i = 0; i < last30DaysOpsInputVariables.size(); ++i)
					{
						adjustedInputValues.push_back(last30DaysOpsInputVariables[i] * inputCoefficients[0] + pitcherFactorInputVariables[i] * leagueAverageOps * inputCoefficients[1]);
					}
					float thisRSquared = CalculateRSquared(adjustedInputValues, validOutputValues);
					if (thisRSquared > bestRSquaredMultiplied)
					{
						bestRSquaredMultiplied = thisRSquared;
						mostAccurateRSquaredCoefficients[0] = inputCoefficients[0];
						mostAccurateRSquaredCoefficients[1] = inputCoefficients[1];
					}
					inputCoefficients[1] += fCoefficientStep;
				}
				inputCoefficients[0] += fCoefficientStep;
			}

			float seasonOpsRSquared = CalculateRSquared(seasonOpsInputVariables, validOutputValues);
			float last30OpsRSquared = CalculateRSquared(last30DaysOpsInputVariables, validOutputValues);
			float last7OpsRSquared = CalculateRSquared(last7DaysOpsInputVariables, validOutputValues);
			float seasonOpsAdjustedRSquared = CalculateRSquared(seasonOpsAdjustedInputVariables, validOutputValues);
			float last30OpsAdjustedRSquared = CalculateRSquared(last30DayAdjustedsOpsInputVariables, validOutputValues);
			float last7OpsAdjustedRSquared = CalculateRSquared(last7DaysOpsAdjustedInputVariables, validOutputValues);
			float pitcherFactorRSquared = CalculateRSquared(pitcherFactorInputVariables, validOutputValues);
			float sabrBatterRSquared = CalculateRSquared(sabrPredictorValues, sabrPredictorOutputValues);
			inputCoefficients[0] = inputCoefficients[0];
		}

		if (bRefineForPitchers) {
			float pitcherRSquared = CalculateRSquared(pitcherInputValues, pitcherOutputValues);
			float sabrPitcherRSquared = CalculateRSquared(sabrPredictorPitcherInputValues, sabrPredictorPitcherOutputValues);
			int breakpoint = 0;
		}
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
		int numPredictedOneHitOrMore = 0;
		int actualOneHitOrMore = 0;
		int numPredictedOneHitOrMoreAwayTop3 = 0;
		int actualOneHitOrMoreAwayTop3 = 0;
		reviewDateStart = 806;
		reviewDateEnd = 1001;
		for (int d = reviewDateStart; d <= reviewDateEnd; ++d)
		{
			if (d - ((d / 100) * 100) > 31)
			{
				d = ((d / 100) + 1) * 100;
				continue;
			}
			char thisDateCStr[5];
			itoa(d, thisDateCStr, 10);
			string thisDate = thisDateCStr;
			string actualResults;
			string resultsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + thisDate + "&game=fd&user=GoldenExcalibur&key=G5970032941";
			curl_easy_setopt(curl, CURLOPT_URL, resultsURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &actualResults);
			curl_easy_perform(curl);
			curl_easy_reset(curl);

			string sabrPredictorText = getSabrPredictorFileContents(thisDate, false);
			
			if (sabrPredictorText != "") {
				string tableResults;
				string tableResultsUrl = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + thisDate + "&game=fd&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941";
				curl_easy_setopt(curl, CURLOPT_URL, tableResultsUrl.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tableResults);
				curl_easy_perform(curl);
				curl_easy_reset(curl);

				size_t sabrNewLine = sabrPredictorText.find("\n", 1);
				size_t sabrNextNewLine = sabrPredictorText.find("\n", sabrNewLine + 1);
				while (sabrNewLine != string::npos) {
					string sabrLine = sabrPredictorText.substr(sabrNewLine + 1, sabrNextNewLine - sabrNewLine - 1);
					vector<string> sabrColumns = SplitStringIntoMultiple(sabrLine, ",", "\"");
					size_t playerIdIndex = actualResults.find(sabrColumns[0], 0);
					if (playerIdIndex != string::npos) {
						size_t nextAtSign = actualResults.find("@ ", playerIdIndex);
						for (int i = 0; i < 7; ++i) {
							playerIdIndex = actualResults.find("</td>", playerIdIndex + 1);
						}
						size_t prevPlayerIdIndex = actualResults.rfind(">", playerIdIndex - 1);
						prevPlayerIdIndex = actualResults.find("/", prevPlayerIdIndex + 1);
						if (prevPlayerIdIndex < playerIdIndex) {
							playerIdIndex = prevPlayerIdIndex;
							prevPlayerIdIndex = actualResults.rfind(" ", prevPlayerIdIndex);
							int numHits = atoi(actualResults.substr(prevPlayerIdIndex + 1, playerIdIndex - prevPlayerIdIndex - 1).c_str());
							float predictedHits = stof(sabrColumns[5].c_str());
							
							size_t tablePlayerNameIndex = tableResults.find(ConvertFLNameToLFName(sabrColumns[0]));
							vector<string> tableColumns;
							if (tablePlayerNameIndex != string::npos) {
								size_t prevNewLineTable = tableResults.rfind("\n", tablePlayerNameIndex);
								size_t nextNewLineTable = tableResults.find("\n", tablePlayerNameIndex);
								tableColumns = SplitStringIntoMultiple(tableResults.substr(prevNewLineTable + 1, nextNewLineTable - prevNewLineTable - 1), ";");
							}

							if (predictedHits > 1.0f && tableColumns.size() > 9 && tableColumns[4] == "1") {
								numPredictedOneHitOrMore++;
								if (numHits > 0)
									actualOneHitOrMore++;
								int battingOrder = atoi(tableColumns[5].c_str());
								if (nextAtSign < prevPlayerIdIndex && battingOrder < 4) {
										numPredictedOneHitOrMoreAwayTop3++;
										if (numHits > 0)
											actualOneHitOrMoreAwayTop3++;
								}
							}
						}
					}
					sabrNewLine = sabrNextNewLine;
					sabrNextNewLine = sabrPredictorText.find("\n", sabrNewLine + 1);
				}
			}

			ifstream eligiblePlayersTrackerFile;
			string eligiblePlayersFileName = "2017ResultsTracker\\BeatTheStreak\\2017";
			if (d < 1000)
				eligiblePlayersFileName += "0";
			eligiblePlayersFileName += thisDate + ".txt";
#if PLATFORM_OSX
            eligiblePlayersFileName = GetPlatformCompatibleFileNameFromRelativePath(eligiblePlayersFileName);
#endif
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
#if PLATFORM_OSX
            resultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(resultsTrackerFileName);
#endif
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
#if PLATFORM_OSX
        yesHitTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(yesHitTrackerFileName);
#endif
		yesHitTrackerFile.open(yesHitTrackerFileName);
		for (unsigned int y = 0; y < playersYesHit.size(); ++y)
		{
			yesHitTrackerFile << playersYesHit[y].ToString() << endl;
		}
		yesHitTrackerFile.close();
		ofstream noHitTrackerFile;
		string noHitTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\PlayersNoHit.txt";
#if PLATFORM_OSX
        noHitTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(noHitTrackerFileName);
#endif
		noHitTrackerFile.open(noHitTrackerFileName);
		for (unsigned int n = 0; n < playersNoHit.size(); ++n)
		{
			noHitTrackerFile << playersNoHit[n].ToString() << endl;
		}
		noHitTrackerFile.close();
	}
}

float GetExpectedFanduelPointsFromPitcherStats(FullSeasonPitcherStats pitcherStats, float opponentRunsPerGame, float opponentStrikeoutsPerGame) {
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

	float playerPointsPerGame = 0;
	for (int i = 0; i < 5; ++i)
	{
		playerPointsPerGame += pitcherInputStats[i] * pitcherInputCoefficients[i];
	}
	return playerPointsPerGame;
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

        string lastYearOffenseCachedDataFileName = "Team";
        lastYearOffenseCachedDataFileName += LAST_YEAR;
        lastYearOffenseCachedDataFileName += "DataCached\\TeamOffense.txt";
		string teamLastYearOffensiveData = GetEntireFileContents(lastYearOffenseCachedDataFileName);
		string teamThisYearStrikeoutData;
		curl_easy_setopt(curl, CURLOPT_URL, "https://www.teamrankings.com/mlb/stat/strikeouts-per-game");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &teamThisYearStrikeoutData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);
		string teamThisYearRunsPerGameData;
		curl_easy_setopt(curl, CURLOPT_URL, "https://www.teamrankings.com/mlb/stat/on-base-plus-slugging-pct");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &teamThisYearStrikeoutData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

	
		ofstream pitcherStatsArchiveFile;
        string pitcherStatsArchiveFileName = CURRENT_YEAR;
        pitcherStatsArchiveFileName += "ResultsTracker\\TeamWinResults\\PitcherData\\" + todaysDate + ".txt";
#if PLATFORM_OSX
        pitcherStatsArchiveFileName = GetPlatformCompatibleFileNameFromRelativePath(pitcherStatsArchiveFileName);
#endif
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

			FullSeasonStatsAdvanced pitcherVBatterThisYearStats = GetPitcherAdvancedStats(singlePlayerData.playerId, CURRENT_YEAR, curl);
			FullSeasonStatsAdvanced pitcherVBatterLastYearStats = GetPitcherAdvancedStats(singlePlayerData.playerId, LAST_YEAR, curl);
			FullSeasonStatsAdvanced pitcherVBatterCareerStats = GetPitcherAdvancedStats(singlePlayerData.playerId, "Total", curl);
			
			if (pitcherVBatterLastYearStats.opsVersusLefty >= 0 && pitcherVBatterLastYearStats.opsVersusRighty >= 0) {
				pitcherVBatterCareerStats = 0.5f * pitcherVBatterCareerStats + 0.5f * pitcherVBatterLastYearStats;
			}
			
			if (pitcherVBatterThisYearStats.opsVersusLefty >= 0 && pitcherVBatterThisYearStats.opsVersusRighty >= 0) {
				pitcherVBatterCareerStats = (1.0f - percentOfSeasonPassed) * pitcherVBatterCareerStats + percentOfSeasonPassed * pitcherVBatterThisYearStats;
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

			// now look up last year points per game
			singlePlayerData.playerPointsPerGame = 0;
			FullSeasonPitcherStats thisYearPitcherStats = GetPitcherStats(singlePlayerData.playerId, CURRENT_YEAR, curl);
			FullSeasonPitcherStats lastYearPitcherStats = GetPitcherStats(singlePlayerData.playerId, LAST_YEAR, curl);
			FullSeasonPitcherStats pitcherCareerStats = GetPitcherStats(singlePlayerData.playerId, "Total", curl);
			// default to average
			float opponentRunsPerGame = 4.4f;
			float opponentStrikeoutsPerGame = 8.1f;

			auto opponentsInfo = opponentMap.find(singlePlayerData.teamCode);

			if (opponentsInfo != opponentMap.end())
			{
				size_t opponentTeamIndex = teamLastYearOffensiveData.find(";" + opponentsInfo->second.teamCodeRankingsSite + ";", 0);
				opponentTeamIndex = teamLastYearOffensiveData.find(";", opponentTeamIndex + 1);
				size_t opponentTeamNextIndex = teamLastYearOffensiveData.find(";", opponentTeamIndex + 1);
				opponentRunsPerGame = stof(teamLastYearOffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());

				opponentTeamIndex = teamLastYearOffensiveData.find(";", opponentTeamIndex + 1);
				opponentTeamNextIndex = teamLastYearOffensiveData.find("\n", opponentTeamIndex + 1);
				opponentStrikeoutsPerGame = stof(teamLastYearOffensiveData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
			}
			else
			{
				assert("No opponent information for pitcher found" == "");
			}

			FullSeasonPitcherStats combinedPitcherStats(lastYearPitcherStats);
			if (lastYearPitcherStats.strikeOutsPer9 >= 0 && pitcherCareerStats.strikeOutsPer9 >= 0) {
				combinedPitcherStats = 0.5f * lastYearPitcherStats + 0.5f * pitcherCareerStats;
			}
			else if (pitcherCareerStats.strikeOutsPer9 >= 0) {
				// no rookies sorry
				//combinedPitcherStats = pitcherCareerStats;
			}

			if (combinedPitcherStats.strikeOutsPer9 >= 0 && thisYearPitcherStats.strikeOutsPer9 >= 0) {
				combinedPitcherStats = combinedPitcherStats * (1.0f - percentOfSeasonPassed) + thisYearPitcherStats * percentOfSeasonPassed;
			}

			float parkHomerFactor = 1;
			float parkRunsFactor = 1;
			float opponentOps = 0;
			if (opponentsInfo != opponentMap.end())
			{
				opponentRunsPerGame *= max(0.0f, 1.0f - (percentOfSeasonPassed * 2.0f));
				opponentStrikeoutsPerGame *= max(0.0f, 1.0f - (percentOfSeasonPassed * 2.0f));

				size_t opponentTeamIndex = teamThisYearStrikeoutData.find(">" + opponentsInfo->second.rankingsSiteTeamName + "<", 0);
				opponentTeamIndex = teamThisYearStrikeoutData.find("data-sort=", opponentTeamIndex + 1);
				opponentTeamIndex = teamThisYearStrikeoutData.find(">", opponentTeamIndex + 1);
				size_t opponentTeamNextIndex = teamThisYearStrikeoutData.find("<", opponentTeamIndex + 1);
				opponentOps = stof(teamThisYearStrikeoutData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str());
				// ops to runs per game is
				// 13.349 * ops - 5.379
				opponentRunsPerGame += (13.349f * opponentOps - 5.379f) * min(1.0f, percentOfSeasonPassed * 2.0f);

				opponentTeamIndex = teamThisYearStrikeoutData.find(">" + opponentsInfo->second.rankingsSiteTeamName + "<", 0);
				opponentTeamIndex = teamThisYearStrikeoutData.find("data-sort=", opponentTeamIndex + 1);
				opponentTeamIndex = teamThisYearStrikeoutData.find(">", opponentTeamIndex + 1);
				opponentTeamNextIndex = teamThisYearStrikeoutData.find("<", opponentTeamIndex + 1);
				opponentStrikeoutsPerGame += stof(teamThisYearStrikeoutData.substr(opponentTeamIndex + 1, opponentTeamNextIndex - opponentTeamIndex - 1).c_str()) * min(1.0f, percentOfSeasonPassed * 2.0f);
			
				// ballpark factors
				float pitcherBallparkHomerRateVsRighty, pitcherBallparkHomerRateVsLefty;
				float pitcherBallparkRunsRateVsRighty, pitcherBallparkRunsRateVsLefty;
				GetBallparkFactors(opponentsInfo->second.ballParkPlayedIn, "HR", pitcherBallparkHomerRateVsLefty, pitcherBallparkHomerRateVsRighty);
				GetBallparkFactors(opponentsInfo->second.ballParkPlayedIn, "R", pitcherBallparkRunsRateVsLefty, pitcherBallparkRunsRateVsRighty);
				parkRunsFactor = (pitcherBallparkRunsRateVsLefty + pitcherBallparkRunsRateVsRighty) * 0.5f;
				parkHomerFactor = (pitcherBallparkHomerRateVsLefty + pitcherBallparkHomerRateVsRighty) * 0.5f;
			}
			

			combinedPitcherStats.era *= parkRunsFactor;
			combinedPitcherStats.fip *= parkHomerFactor;

			singlePlayerData.playerPointsPerGame = GetExpectedFanduelPointsFromPitcherStats(combinedPitcherStats, opponentRunsPerGame, opponentStrikeoutsPerGame);
			
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
					myTeam->second.teamWinEstimatedScore = 0;
					myTeam->second.teamWinEstimatedScore -= thisYearPitcherStats.fip * 0.1f;
					myTeam->second.teamWinEstimatedScore -= thisYearPitcherStats.whip * 0.7f;
					myTeam->second.teamWinEstimatedScore -= opponentOps * 0.9f;
				}
			}
			if (thisYearPitcherStats.xfip > -0.1f)
			{
				pitcherStatsArchiveFile << singlePlayerData.teamCode << ";" << singlePlayerData.playerId << ";" << singlePlayerData.playerName << ";" << thisYearPitcherStats.ToString();
				pitcherStatsArchiveFile << endl;
			}
			// throw this guy out if his game will most likely be rained out
			if (combinedPitcherStats.strikeOutsPer9 >= 0 && gameStartTime <= latestGameTime && gameStartTime >= earliestGameTime && !bRainedOut)
				positionalPlayerData.push_back(singlePlayerData);
			if (placeHolderIndex == string::npos)
				break;
			else
				placeHolderIndex = readBuffer.find("\n", placeHolderIndex + 1);
		}
		pitcherStatsArchiveFile.close();

		sort(positionalPlayerData.begin(), positionalPlayerData.end(), comparePlayerByPointsPerGame);

		ofstream playerResultsTrackerFile;
        string playerResultsTrackerFileName = CURRENT_YEAR;
        playerResultsTrackerFileName += "ResultsTracker\\Pitchers\\" + todaysDate + ".txt";
#if PLATFORM_OSX
        playerResultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(playerResultsTrackerFileName);
#endif
		playerResultsTrackerFile.open(playerResultsTrackerFileName);

		ofstream teamWinTrackerFile;
        string teamWinTrackerFileName = CURRENT_YEAR;
        teamWinTrackerFileName += "ResultsTracker\\TeamWinResults\\" + todaysDate + ".txt";
#if PLATFORM_OSX
        teamWinTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(teamWinTrackerFileName);
#endif
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
						auto myTeamInfo = opponentMap.find(opponentsInfo->second.teamCodeRotoGuru);
						if (myTeamInfo != opponentMap.end())
						{
							teamWinTrackerFile << myTeamInfo->second.teamWinEstimatedScore - opponentsInfo->second.teamWinEstimatedScore << ";";
						}
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
		pitcherTeamCode = positionalPlayerData[pitcherSelected].teamCode;
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
		  itoa(p, pAsString, 10);
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

			// now look up last year points per game
			singlePlayerData.playerPointsPerGame *= thisSeasonPercent;
			FullSeasonStats playerLastYearStats = GetBatterStats(singlePlayerData.playerId, LAST_YEAR, curl);
			if (singlePlayerData.isFacingLefthander)
				singlePlayerData.playerPointsPerGame += playerLastYearStats.averagePpgVsLefty * (1.0f - thisSeasonPercent);
			else
				singlePlayerData.playerPointsPerGame += playerLastYearStats.averagePpgVsRighty * (1.0f - thisSeasonPercent);
			
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
  string resultsTrackerFileName = CURRENT_YEAR;
  resultsTrackerFileName += "ResultsTracker\\" + todaysDate + ".txt";
#if PLATFORM_OSX
    resultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(resultsTrackerFileName);
#endif
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

  
  vector<PlayerData> chosenLineup = OptimizeLineupToFitBudget();
  
}

void GenerateNewLineupFromSabrPredictor(CURL *curl)
{
	string sabrPredictorText = getSabrPredictorFileContents(todaysDate, false);
	if (sabrPredictorText == "")
		return;

	if (curl == NULL)
		curl = curl_easy_init();
	vector<TeamStackTracker> teamStackList;
	if (curl)
	{
		// previous days results, to get the likely batting order
		int numDaysPreviousResults = 5;
		vector<string> previousDayResults;
		for (int i = 1; i <= numDaysPreviousResults; ++i) {
			string previousResults;
			string thisDayAbbreviated = todaysDate.substr(4);
			int thisDayAbbreviatedInt = atoi(thisDayAbbreviated.c_str());
			thisDayAbbreviatedInt -= i;
			int thisDayAbbreviatedIntWithoutMonth = thisDayAbbreviatedInt - (thisDayAbbreviatedInt / 100) * 100;
			if (thisDayAbbreviatedIntWithoutMonth == 0 || thisDayAbbreviatedIntWithoutMonth > 30)
				thisDayAbbreviatedInt -= 70;

			char prevDayCStr[5];
			itoa(thisDayAbbreviatedInt, prevDayCStr, 10);
			string prevDay = prevDayCStr;
			string resultsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=" + prevDay + "&game=fd&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941";
			curl_easy_setopt(curl, CURLOPT_URL, resultsURL.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &previousResults);
			curl_easy_perform(curl);
			curl_easy_reset(curl);
			previousResults = prevDay + "\n" + previousResults;
			previousDayResults.push_back(previousResults);
		}

		for (int p = 2; p <= 7; ++p)
		{
			// first basemen
			std::string readBuffer;
			char pAsString[5];
			itoa(p, pAsString, 10);
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
				for (int i = 0; i < 19; ++i)
				{
					placeHolderIndex = readBuffer.find(";", placeHolderIndex + 1);
				}
				
				singlePlayerData.playerPointsPerGame = -1;

				size_t playerNameIndex = sabrPredictorText.find(ConvertLFNameToFLName(singlePlayerData.playerName));
				if (playerNameIndex != string::npos) {
					size_t nextNewLine = sabrPredictorText.find("\n", playerNameIndex);
					vector<string> thisSabrLine = SplitStringIntoMultiple(sabrPredictorText.substr(playerNameIndex, nextNewLine - playerNameIndex), ",", "\"");
					float expectedFdPoints = stof(thisSabrLine[17]);
					singlePlayerData.playerPointsPerGame = expectedFdPoints;

					bool teamStackTrackerExists = false;
					for (unsigned int texp = 0; texp < teamStackList.size(); ++texp) {
						if (teamStackList[texp].teamCode == singlePlayerData.teamCode) {
							teamStackList[texp].numPlayersAdded++;
							teamStackList[texp].teamTotalExpectedPoints += singlePlayerData.playerPointsPerGame;
							teamStackTrackerExists = true;
							break;
						}
					}
					if (!teamStackTrackerExists) {
						TeamStackTracker tst;
						tst.numPlayersAdded = 1;
						tst.teamCode = singlePlayerData.teamCode;
						tst.teamTotalExpectedPoints = singlePlayerData.playerPointsPerGame;
						teamStackList.push_back(tst);
					}
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
				bool bAcceptableBattingOrder = false;
				int minBattingOrder = 2;
				int maxBattingOrder = 5;
				// catchers usually bat later anyway
				if (p == 2)
					maxBattingOrder = 6;
				if (bTournamentLineup) {
				//	minBattingOrder = -1;
				//	maxBattingOrder = 99;
				}
				for (unsigned int i = 0; i < previousDayResults.size(); ++i) {
					size_t playerIdIndex = previousDayResults[i].find(previousDayResults[i].substr(0,3) + ";" + singlePlayerData.playerId + ";", 0);
					if (playerIdIndex != string::npos)
					{
						for (int m = 0; m < 5; ++m) {
							playerIdIndex = previousDayResults[i].find(";", playerIdIndex + 1);
						}
						size_t nextPlayerIdIndex = previousDayResults[i].find(";", playerIdIndex + 1);
						int prevBattingOrder = atoi(previousDayResults[i].substr(playerIdIndex + 1, nextPlayerIdIndex - playerIdIndex - 1).c_str());
						if (prevBattingOrder >= minBattingOrder && prevBattingOrder <= maxBattingOrder) {
							bAcceptableBattingOrder = true;
							break;
						}
					}
				}
				
				// throw this guy out if he's not a starter or his game will most likely be rained out
				if (bAcceptableBattingOrder 
					&& !bFacingChosenPitcher 
					&& gameStartTime <= latestGameTime 
					&& gameStartTime >= earliestGameTime 
					&& !bRainedOut)
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
	sort(teamStackList.begin(), teamStackList.end(), compareTeamsByAveragePlayerPointsPerGame);
	// always use stacks
	if (true) {
		for (unsigned int pos = 0; pos < allPlayers.size(); ++pos) {
			for (unsigned int player = 0; player < allPlayers[pos].size(); ++player) {
				int teamStackRank = -1;
				for (unsigned int team = 0; team < teamStackList.size(); ++team) {
					if (teamStackList[team].teamCode == allPlayers[pos][player].teamCode) {
						teamStackRank = team;
						break;
					}
				}
				if (teamStackRank < 10 && teamStackRank >= 0) {
					allPlayers[pos][player].playerPointsPerGame += (float)(10 - teamStackRank);
				}
			}
			sort(allPlayers[pos].begin(), allPlayers[pos].end(), comparePlayerByPointsPerGame);
		}
	}
	vector<PlayerData> chosenLineup = OptimizeLineupToFitBudget();
	int breakpoint = 0;
}

vector<string> teamsWithNumPlayersAboveThreshold(unordered_map<string, int> &numPlayersTeamMap, int maxThreshold) {
	vector<string> teamsWithExcessPlayers;
	for (auto it = numPlayersTeamMap.begin(); it != numPlayersTeamMap.end(); ++it) {
		if (it->second > maxThreshold)
			teamsWithExcessPlayers.push_back(it->first);
	}
	return teamsWithExcessPlayers;
}

void addPlayerToTeam(unordered_map<string, int> &numPlayersTeamMap, string teamCode) {
	if (teamCode == "")
		return;
	auto team = numPlayersTeamMap.find(teamCode);
	if (team != numPlayersTeamMap.end()) {
		team->second = team->second + 1;
	}
	else {
		numPlayersTeamMap.insert({ teamCode,1 });
	}
}
void removePlayerFromTeam(unordered_map<string, int> &numPlayersTeamMap, string teamCode) {
	auto team = numPlayersTeamMap.find(teamCode);
	if (team != numPlayersTeamMap.end()) {
		team->second = team->second - 1;
	}
}

vector<PlayerData> OptimizeLineupToFitBudget()
{
	vector<unsigned int> idealPlayerPerPosition;
	int maxPlayersPerTeam = 3;
	if (bTournamentLineup) {
		maxPlayersPerTeam = 4;
	}

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
		if (allPlayers[i].size() == 0) {
			cout << "Position " << i << " has no available players." << endl;
			vector<PlayerData> playersToReturn;
			return playersToReturn;
		}
		if (i == 5 && allPlayers[i].size() < 3) {
			cout << "Position " << i << " does not have enough available players." << endl;
			vector<PlayerData> playersToReturn;
			return playersToReturn;
		}
	}


	for (unsigned int i = 0; i < allPlayers.size(); ++i)
	{
		idealPlayerPerPosition.push_back(0);
	}
	idealPlayerPerPosition.push_back(1);
	idealPlayerPerPosition.push_back(2);
	unordered_map<string, int> numPlayersFromTeam;

	int totalSalary = 0;
	for (unsigned int i = 0; i < idealPlayerPerPosition.size(); ++i)
	{
		unsigned int positionIndex = i;
		if (positionIndex >= allPlayers.size())
			positionIndex = allPlayers.size() - 1;
		totalSalary += allPlayers[positionIndex][idealPlayerPerPosition[i]].playerSalary;
		addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[i]].teamCode);
	}
	if (bTournamentLineup) {
		addPlayerToTeam(numPlayersFromTeam, pitcherTeamCode);
	}
	vector<string> teamsWithTooManyPlayers = teamsWithNumPlayersAboveThreshold(numPlayersFromTeam, maxPlayersPerTeam);
	while (teamsWithTooManyPlayers.size() > 0) {
		int playerIndexToDrop = -1;
		int biggestSalaryDrop = 0;
		int positionToDrop = -1;
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
			if (std::find(teamsWithTooManyPlayers.begin(), teamsWithTooManyPlayers.end(), allPlayers[positionIndex][idealPlayerPerPosition[i]].teamCode) != teamsWithTooManyPlayers.end()) {
				if (idealPlayerPerPosition[i] < allPlayers[positionIndex].size() - 1) {
					if (!bIsOutfield || i == idealPlayerPerPosition.size() - 1 || idealPlayerPerPosition[i + 1] - idealPlayerPerPosition[i] > 1)
					{
						int salaryDrop = allPlayers[positionIndex][idealPlayerPerPosition[i]].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].playerSalary;
						float pointDrop = allPlayers[positionIndex][idealPlayerPerPosition[i]].playerPointsPerGame - allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].playerPointsPerGame;
						if ((pointDrop < smallestPointDrop) || (abs(pointDrop - smallestPointDrop) < 0.1f && salaryDrop > biggestSalaryDrop))
						{
							playerIndexToDrop = i;
							smallestPointDrop = pointDrop;
							biggestSalaryDrop = salaryDrop;
							positionToDrop = positionIndex;
						}
					}
				}
			}
		}
		if (playerIndexToDrop < 0) {
			vector<PlayerData> playersToReturn;
			return playersToReturn;
		}
		removePlayerFromTeam(numPlayersFromTeam,allPlayers[positionToDrop][idealPlayerPerPosition[playerIndexToDrop]].teamCode);
		idealPlayerPerPosition[playerIndexToDrop]++;
		addPlayerToTeam(numPlayersFromTeam, allPlayers[positionToDrop][idealPlayerPerPosition[playerIndexToDrop]].teamCode);
		totalSalary -= biggestSalaryDrop;
		teamsWithTooManyPlayers = teamsWithNumPlayersAboveThreshold(numPlayersFromTeam, maxPlayersPerTeam);
	}
	
	while (totalSalary > maxTotalBudget && teamsWithNumPlayersAboveThreshold(numPlayersFromTeam, maxPlayersPerTeam).size() == 0)
	{
		int positionIndexToDrop = -1;
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
					bool teamEligible = true;
					auto team = numPlayersFromTeam.find(allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].teamCode);
					if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
						teamEligible = false;
					}
					if (teamEligible) {
						int salaryDrop = allPlayers[positionIndex][idealPlayerPerPosition[i]].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].playerSalary;
						float pointDrop = allPlayers[positionIndex][idealPlayerPerPosition[i]].playerPointsPerGame - allPlayers[positionIndex][idealPlayerPerPosition[i] + 1].playerPointsPerGame;
						if ((pointDrop < smallestPointDrop) || (abs(pointDrop - smallestPointDrop) < 0.1f && salaryDrop > biggestSalaryDrop))
						{
							positionIndexToDrop = positionIndex;
							playerIndexToDrop = i;
							smallestPointDrop = pointDrop;
							biggestSalaryDrop = salaryDrop;
						}
					}
				}
			}
		}
		if (playerIndexToDrop < 0) {
			vector<PlayerData> playersToReturn;
			return playersToReturn;
		}
		removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndexToDrop][idealPlayerPerPosition[playerIndexToDrop]].teamCode);
		idealPlayerPerPosition[playerIndexToDrop]++;
		addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndexToDrop][idealPlayerPerPosition[playerIndexToDrop]].teamCode);
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
			bool teamEligible = true;
			auto team = numPlayersFromTeam.find(allPlayers[allPlayers.size() - 1][pl].teamCode);
			if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
				teamEligible = false;
			}
			if (teamEligible) {
				int salaryIncrease = allPlayers[allPlayers.size() - 1][pl].playerSalary - allPlayers[allPlayers.size() - 1][idealPlayerPerPosition[i]].playerSalary;
				if (salaryIncrease <= 0)
				{
					removePlayerFromTeam(numPlayersFromTeam, allPlayers[allPlayers.size() - 1][idealPlayerPerPosition[i]].teamCode);
					addPlayerToTeam(numPlayersFromTeam, allPlayers[allPlayers.size() - 1][pl].teamCode);
					idealPlayerPerPosition[i] = pl;
					totalSalary += salaryIncrease;
					break;
				}
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
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 3;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 2;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 0;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 4;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
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
			if (positionIndex == allPlayers.size() - 1) {
				bool alreadyUsingThisOutfielder = false;
				for (unsigned int outfieldIndex = idealPlayerPerPosition.size() - 3; outfieldIndex < idealPlayerPerPosition.size(); ++outfieldIndex) {
					if (b == idealPlayerPerPosition[outfieldIndex]) {
						alreadyUsingThisOutfielder = true;
						break;
					}
				}
				if (alreadyUsingThisOutfielder) {
					continue;
				}
			}
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

					bool teamEligible = true;
					auto team = numPlayersFromTeam.find(allPlayers[swappeePositionIndex][bs].teamCode);
					if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
						teamEligible = false;
					}
					team = numPlayersFromTeam.find(allPlayers[positionIndex][b].teamCode);
					if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
						teamEligible = false;
					}
					if (teamEligible) {
						int swappedSalaryGained = allPlayers[swappeePositionIndex][idealPlayerPerPosition[swappee]].playerSalary - allPlayers[swappeePositionIndex][bs].playerSalary;
						float pointsLost = allPlayers[swappeePositionIndex][idealPlayerPerPosition[swappee]].playerPointsPerGame - allPlayers[swappeePositionIndex][bs].playerPointsPerGame;
						if (pointsGained > pointsLost && totalSalary - swappedSalaryGained + salaryNeeded <= maxTotalBudget)
						{
							// we should swap to gain more points for equal or less salary
							removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[i]].teamCode);
							addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][b].teamCode);
							removePlayerFromTeam(numPlayersFromTeam, allPlayers[swappeePositionIndex][idealPlayerPerPosition[swappee]].teamCode);
							addPlayerToTeam(numPlayersFromTeam, allPlayers[swappeePositionIndex][bs].teamCode);
							idealPlayerPerPosition[i] = b;
							idealPlayerPerPosition[swappee] = bs;
							totalSalary = totalSalary - swappedSalaryGained + salaryNeeded;
							bSwappedPlayers = true;
							break;
						}
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
			bool teamEligible = true;
			auto team = numPlayersFromTeam.find(allPlayers[allPlayers.size() - 1][pl].teamCode);
			if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
				teamEligible = false;
			}
			if (teamEligible) {
				int salaryIncrease = allPlayers[allPlayers.size() - 1][pl].playerSalary - allPlayers[allPlayers.size() - 1][idealPlayerPerPosition[i]].playerSalary;
				if (salaryIncrease <= 0)
				{
					removePlayerFromTeam(numPlayersFromTeam, allPlayers[allPlayers.size() - 1][idealPlayerPerPosition[i]].teamCode);
					addPlayerToTeam(numPlayersFromTeam, allPlayers[allPlayers.size() - 1][pl].teamCode);
					idealPlayerPerPosition[i] = pl;
					totalSalary += salaryIncrease;
					break;
				}
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
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 3;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 2;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 0;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
		}
	}
	positionIndex = 4;
	for (unsigned int pl = 0; pl < idealPlayerPerPosition[positionIndex]; ++pl)
	{
		bool teamEligible = true;
		auto team = numPlayersFromTeam.find(allPlayers[positionIndex][pl].teamCode);
		if (team != numPlayersFromTeam.end() && team->second >= maxPlayersPerTeam) {
			teamEligible = false;
		}
		if (teamEligible) {
			int salaryIncrease = allPlayers[positionIndex][pl].playerSalary - allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].playerSalary;
			if (salaryIncrease + totalSalary <= maxTotalBudget)
			{
				totalSalary += salaryIncrease;
				removePlayerFromTeam(numPlayersFromTeam, allPlayers[positionIndex][idealPlayerPerPosition[positionIndex]].teamCode);
				addPlayerToTeam(numPlayersFromTeam, allPlayers[positionIndex][pl].teamCode);
				idealPlayerPerPosition[positionIndex] = pl;
				break;
			}
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
	/*if (teamsWithNumPlayersAboveThreshold(numPlayersFromTeam, maxPlayersPerTeam).size() > 0) {
		playersToReturn.clear();
		allPlayers.clear();
	}*/
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
			itoa(p, pAsString, 10);
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


			size_t dashIndex = weatherData.rfind(" <96> ", timeIndex);
			
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
		FullSeasonStatsAdvanced batter2016AdvancedStats = GetBatterAdvancedStats("3215", "2016", curl);
		
		FullSeasonStatsAdvanced pitcherAdvanced2016Stats = GetPitcherAdvancedStats("1580", "2016", curl);
		FullSeasonPitcherStats pitcher2016Stats = GetPitcherStats("1580", "2016", curl);
		
		FullSeasonStatsAdvancedNoHandedness jeddGyorko2016Stats = GetBatterStatsSeason("5214", curl, "2016");
		FullSeasonStatsAdvancedNoHandedness expectedJeddGyorko2016Stats;
		expectedJeddGyorko2016Stats.average = 0.243f;
		expectedJeddGyorko2016Stats.onBaseAverage = 0.306f;
		expectedJeddGyorko2016Stats.slugging = 0.495f;
		expectedJeddGyorko2016Stats.ops = 0.801f;
		expectedJeddGyorko2016Stats.iso = 0.253f;
		expectedJeddGyorko2016Stats.woba = 0.339f;
		expectedJeddGyorko2016Stats.wrcPlus = 112;

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
		pitcher2016Stats *= 0.345f;
		assert(expectedPitcher2016Stats * 0.345f == pitcher2016Stats);
		expectedPitcher2016Stats.era = 4.37f * 0.345f;
		expectedPitcher2016Stats.fip = 4.12f * 0.345f;
		expectedPitcher2016Stats.numInnings = 158.6666666f * 0.345f;
		expectedPitcher2016Stats.strikeOutsPer9 = 7.6f * 0.345f;
		expectedPitcher2016Stats.whip = 1.27f * 0.345f;
		expectedPitcher2016Stats.xfip = 3.98f * 0.345f;
		expectedPitcher2016Stats.wobaAllowed = 0.319f * 0.345f;
		expectedPitcher2016Stats.opsAllowed = 0.750f * 0.345f;
		assert(expectedPitcher2016Stats == pitcher2016Stats);
		expectedPitcher2016Stats += pitcher2016Stats;
		assert(expectedPitcher2016Stats == pitcher2016Stats * 2.0f);
		assert(expectedPitcherAdvanced2016Stats == pitcherAdvanced2016Stats);
		assert(expectedBatterStats == batterStats);
		assert(expectedBatter2016AdvancedStats == batter2016AdvancedStats);
		batter2016AdvancedStats *= 0.5f;
		assert(batter2016AdvancedStats == (expectedBatter2016AdvancedStats * 0.5f));
		expectedBatter2016AdvancedStats.averageVersusLefty = 0.233f * 0.5f;;
		expectedBatter2016AdvancedStats.sluggingVersusLefty = 0.381f * 0.5f;;
		expectedBatter2016AdvancedStats.isoVersusLefty = 0.148f * 0.5f;;
		expectedBatter2016AdvancedStats.opsVersusLefty = 0.656f * 0.5f;;
		expectedBatter2016AdvancedStats.wobaVersusLefty = 0.280f * 0.5f;;
		expectedBatter2016AdvancedStats.averageVersusRighty = 0.283f * 0.5f;;
		expectedBatter2016AdvancedStats.sluggingVersusRighty = 0.459f * 0.5f;;
		expectedBatter2016AdvancedStats.opsVersusRighty = 0.813f * 0.5f;
		expectedBatter2016AdvancedStats.isoVersusRighty = 0.176f * 0.5f;;
		expectedBatter2016AdvancedStats.wobaVersusRighty = 0.348f * 0.5f;;
		assert(expectedBatter2016AdvancedStats == batter2016AdvancedStats);
		assert(expectedBatter2016AdvancedStats + batter2016AdvancedStats == batter2016AdvancedStats * 2.0f);
		
		assert(expectedJeddGyorko2016Stats == jeddGyorko2016Stats);
		jeddGyorko2016Stats *= 0.5f;
		expectedJeddGyorko2016Stats.average = 0.243f * 0.5f;
		expectedJeddGyorko2016Stats.onBaseAverage = 0.306f * 0.5f;
		expectedJeddGyorko2016Stats.slugging = 0.495f * 0.5f;
		expectedJeddGyorko2016Stats.ops = 0.801f * 0.5f;
		expectedJeddGyorko2016Stats.iso = 0.253f * 0.5f;
		expectedJeddGyorko2016Stats.woba = 0.339f * 0.5f;
		expectedJeddGyorko2016Stats.wrcPlus = 112 * 0.5f;
		assert(expectedJeddGyorko2016Stats == jeddGyorko2016Stats);
		assert((expectedJeddGyorko2016Stats + expectedJeddGyorko2016Stats) == (jeddGyorko2016Stats * 2.0f));

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
	
	assert(ConvertTeamCodeToOddsPortalName("stl", false) == "St.Louis Cardinals");
	assert(ConvertTeamCodeToOddsPortalName("kan", false) == "Kansas City Royals");

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

	// string functions
	assert(IntToDateYMD(20170801 - 1) == "20170731");
	assert(IntToDateYMD(801 - 1, "2017") == "20170731");
	assert(IntToDateYMD(20170801 - 25) == "20170707");
	assert(IntToDateYMD(801 - 30, "2017") == "20170702");
	assert(IntToDateYMD(20170730 + 45, "2017", true) == "20170913");
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
string ConvertTeamCodeToTeamRankingsName(string teamCode)
{
	string teamCodesData = GetEntireFileContents("TeamCodes.txt");
	size_t teamNameIndex = teamCodesData.find(";" + teamCode + ";", 0);	
	size_t teamNamePrevIndex = teamCodesData.rfind(";", teamNameIndex - 1);

	return teamCodesData.substr(teamNamePrevIndex + 1, teamNameIndex - teamNamePrevIndex - 1);
}
std::string ConvertTeamCodeToOddsPortalName(std::string teamCode, bool standardTeamCode = true) {
	string teamCodesData = GetEntireFileContents("TeamCodes.txt");
	if (!standardTeamCode)
		teamCode = ConvertRotoGuruTeamCodeToStandardTeamCode(teamCode);
	size_t teamCodeIndex = teamCodesData.find(";" + teamCode + ";", 0);
	if (teamCodeIndex == string::npos)
		assert(false);
	teamCodeIndex = teamCodesData.rfind("\n", teamCodeIndex);
	teamCodeIndex++;
	size_t oddsportalNameEndIndex = teamCodesData.find(";", teamCodeIndex);
	string oddsportalTeamName = teamCodesData.substr(teamCodeIndex, oddsportalNameEndIndex - teamCodeIndex);
	if ( oddsportalTeamName.find("Cardinals") != string::npos)
	{
		oddsportalTeamName = "St.Louis Cardinals";
	}
	return oddsportalTeamName;
}
std::string ConvertOddsPortalNameToTeamCodeName(std::string oddsportalTeamName, bool standardTeamCode = true)
{
	string teamCodesData = GetEntireFileContents("TeamCodes.txt");

	size_t teamNameIndex = teamCodesData.find(oddsportalTeamName, 0);
	if (teamNameIndex == string::npos && oddsportalTeamName.find("Cardinals") != string::npos)
	{
		teamNameIndex = teamCodesData.find("Cardinals");
	}
	for (int i = 0; i < 3; ++i)
	{
		teamNameIndex = teamCodesData.find(";", teamNameIndex + 1);
	}
	size_t teamNameEndIndex = teamCodesData.find(";", teamNameIndex + 1);
	string teamCode = teamCodesData.substr(teamNameIndex + 1, teamNameEndIndex - teamNameIndex - 1);
	if (!standardTeamCode)
		teamCode = ConvertRotoGuruTeamCodeToStandardTeamCode(teamCode);
	return teamCode;
}
std::string ConvertStandardTeamCodeToRotoGuruTeamCode(std::string standardCode) {
	// rotogur1.com uses different team codes than standard...
	if (standardCode == "laa")
		standardCode = "ana";
	if (standardCode == "lad")
		standardCode = "los";
	if (standardCode == "mia")
		standardCode = "fla";
	return standardCode;
}
std::string ConvertRotoGuruTeamCodeToStandardTeamCode(std::string rotoGuruCode) {
	// rotogur1.com uses different team codes than standard...
	if (rotoGuruCode == "ana")
		rotoGuruCode = "laa";
	if (rotoGuruCode == "los")
		rotoGuruCode = "lad";
	if (rotoGuruCode == "fla")
		rotoGuruCode = "mia";
	return rotoGuruCode;
}

void AnalyzeTeamWinFactors()
{
	CURL* curl = NULL;
	Gather2016TeamWins();
	return;
	//GatherPitcher2016CumulativeData();
	//Analyze2016TeamWins();
	//Analyze2016TeamWinFactors();
	//Refine2016TeamWinFactors();
	//return;
	fstream allGamesFile;
    string allGamesFileName = "2017ResultsTracker\\OddsWinsResults\\AllGamesResults.txt";
#if PLATFORM_OSX
    allGamesFileName = GetPlatformCompatibleFileNameFromRelativePath(allGamesFileName);
#endif
	allGamesFile.open(allGamesFileName);
	ofstream gamesFactorsFile;
    string gamesFactorsFileName = "2017ResultsTracker\\OddsWinsResults\\AllGamesFactors.txt";
#if PLATFORM_OSX
    gamesFactorsFileName = GetPlatformCompatibleFileNameFromRelativePath(gamesFactorsFileName);
#endif
	gamesFactorsFile.open(gamesFactorsFileName);
	string resultsLine;
	string currentDate = "";
	string currentDateOpsStats = "";
	string currentDateRunsStats = "";
	string currentDatePitcherStats = "";
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
#if PLATFORM_OSX
                opsFileName = GetPlatformCompatibleFileNameFromRelativePath(opsFileName);
#endif
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
#if PLATFORM_OSX
                runsFileName = GetPlatformCompatibleFileNameFromRelativePath(runsFileName);
#endif
				runsOutputFile.open(runsFileName);
				runsOutputFile << currentDateRunsStats;
				runsOutputFile.close();
			}

			string pitcherFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\";
			pitcherFileName += lineValues[0] + ".txt";
			currentDatePitcherStats = "";
			currentDatePitcherStats = GetEntireFileContents(pitcherFileName);
			if (currentDatePitcherStats == "")
				cout << "No pitcher data for the date of " << dateWithDashes << endl;
		}
		if (lineValues[4].find(" - ") != string::npos || lineValues[5].find(" - ") != string::npos)
			continue;
		
		string winningTeamCode = ConvertOddsPortalNameToTeamCodeName(lineValues[1]);
		string losingTeamCode = ConvertOddsPortalNameToTeamCodeName(lineValues[2]);
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

		size_t teamPitchingBegin = currentDatePitcherStats.find(winningTeamCode);
		size_t teamPitchingEnd = currentDatePitcherStats.find("\n", teamPitchingBegin);
		for (int skip = 0; skip < 3; ++skip)
		{
			teamPitchingBegin = currentDatePitcherStats.find(";", teamPitchingBegin + 1);
		}
		FullSeasonPitcherStats winningTeamPitchingStats(currentDatePitcherStats.substr(teamPitchingBegin + 1, teamPitchingEnd - teamPitchingBegin - 1));

		teamPitchingBegin = currentDatePitcherStats.find(losingTeamCode);
		teamPitchingEnd = currentDatePitcherStats.find("\n", teamPitchingBegin);
		for (int skip = 0; skip < 3; ++skip)
		{
			teamPitchingBegin = currentDatePitcherStats.find(";", teamPitchingBegin + 1);
		}
		FullSeasonPitcherStats losingTeamPitchingStats(currentDatePitcherStats.substr(teamPitchingBegin + 1, teamPitchingEnd - teamPitchingBegin - 1));


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
			gamesFactorsFile << lineValues[0] << ";" << lineValues[1] << ";" << lineValues[2] << ";";
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
			if (winningTeamPitchingStats.whip > -1 && losingTeamPitchingStats.whip > -1)
			{
				float expectedValue = 0;
				expectedValue += opsDiff * 1.7f;
				if (lineValues[3] == "H")
					expectedValue += 0.4f;
				else
					expectedValue -= 0.4f;
				expectedValue -= (winningTeamPitchingStats.whip - losingTeamPitchingStats.whip) * 0.2f;
				gamesFactorsFile << expectedValue << ";";
			}
			if (winningTeamPitchingStats.whip > -1 && losingTeamPitchingStats.whip > -1)
			{
				float expectedValue = 0;
				expectedValue += opsDiff * 0.9f;
				expectedValue -= (winningTeamPitchingStats.fip - losingTeamPitchingStats.fip) * 0.1f;
				expectedValue -= (winningTeamPitchingStats.whip - losingTeamPitchingStats.whip) * 0.7f;
				gamesFactorsFile << expectedValue << ";";
			}
			//gamesFactorsFile << last3OpsDiff << ";" << last1OpsDiff << ";" << t2016OpsDiff << ";" << haOpsDiff << ";" << hOnlyOpsDiff << ";";
			gamesFactorsFile << endl;
		}
	}
	gamesFactorsFile.close();
	allGamesFile.close();
}

void Analyze2016TeamWinFactors()
{
	CURL *curl = NULL;
	fstream all2016GamesFile;
    string allGamesFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\2016\\2016Totals.txt";
    string allGamesFactorsFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\2016\\2016TotalsWithOps.txt";
#if PLATFORM_OSX
    allGamesFileName = GetPlatformCompatibleFileNameFromRelativePath(allGamesFileName);
    allGamesFactorsFileName = GetPlatformCompatibleFileNameFromRelativePath(allGamesFactorsFileName);
#endif
	all2016GamesFile.open(allGamesFileName);
	ofstream gamesFactorsFile;
	gamesFactorsFile.open(allGamesFactorsFileName);
	string resultsLine;
	string currentDate = "";
	string currentDateOpsStats = "";
	string currentDateRunsStats = "";
	while (getline(all2016GamesFile, resultsLine))
	{
		vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
		string dateWithDashes = lineValues[0].substr(0, 4) + "-" + lineValues[0].substr(4, 2) + "-" + lineValues[0].substr(6, 2);
		if (dateWithDashes != currentDate)
		{
			currentDate = dateWithDashes;
			string opsFileName = "2017ResultsTracker\\OddsWinsResults\\TeamOpsCachedData\\2016\\" + currentDate + ".txt";
			currentDateOpsStats = GetEntireFileContents(opsFileName);
			if (currentDateOpsStats == "")
			{
				CurlGetSiteContents(curl, "https://www.teamrankings.com/mlb/stat/on-base-plus-slugging-pct?date=" + currentDate, currentDateOpsStats);
				ofstream opsOutputFile;
#if PLATFORM_OSX
                opsFileName = GetPlatformCompatibleFileNameFromRelativePath(opsFileName);
#endif
				opsOutputFile.open(opsFileName);
				opsOutputFile << currentDateOpsStats;
				opsOutputFile.close();
			}
		}

		string winningTeamRankingsName = ConvertTeamCodeToTeamRankingsName(lineValues[1]);
		string losingTeamRankingsName = ConvertTeamCodeToTeamRankingsName(lineValues[2]);
		vector<string> winningTeamOpsColumns = GetRankingsRowColumns(winningTeamRankingsName, currentDateOpsStats, 6);
		vector<string> losingTeamOpsColumns = GetRankingsRowColumns(losingTeamRankingsName, currentDateOpsStats, 6);
		float opsDiff = stof(winningTeamOpsColumns[0]) - stof(losingTeamOpsColumns[0]);

		gamesFactorsFile << resultsLine << opsDiff << ";";
		gamesFactorsFile << endl;
		
	}
	gamesFactorsFile.close();
	all2016GamesFile.close();
}

void Refine2016TeamWinFactors()
{
	CURL *curl = NULL;
	ifstream gamesFactorsFile;
    string gamesFactorsFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\2016\\2016TotalsWithOps.txt";
    string guessFileName = "2016TestGuesses.txt";
#if PLATFORM_OSX
    gamesFactorsFileName = GetPlatformCompatibleFileNameFromRelativePath(gamesFactorsFileName);
    guessFileName = GetPlatformCompatibleFileNameFromRelativePath(guessFileName);
#endif
	gamesFactorsFile.open(gamesFactorsFileName);
	ofstream guessFile;
	guessFile.open(guessFileName);
	string resultsLine;
	string currentDate = "";
	string currentDateOpsStats = "";
	string currentDateRunsStats = "";
	struct GameProfile
	{
		float eraDiff;
		float fipDiff;
		float xfipDiff;
		float k9Diff;
		float whipDiff;
		float opsDiff;
		int homeValue;
	};
	vector<GameProfile> allGameProfiles;
	while (getline(gamesFactorsFile, resultsLine))
	{
		vector<string> lineValues = SplitStringIntoMultiple(resultsLine, ";");
		GameProfile thisGame;
		thisGame.eraDiff = stof(lineValues[3]);
		thisGame.fipDiff = stof(lineValues[4]);
		thisGame.xfipDiff = stof(lineValues[5]);
		thisGame.k9Diff = stof(lineValues[6]);
		thisGame.whipDiff = stof(lineValues[7]);
		thisGame.opsDiff = stof(lineValues[11]);
		thisGame.homeValue = atoi(lineValues[10].c_str());
	//	if (stof(lineValues[8]) > 20 && stof(lineValues[9]) > 20)
			allGameProfiles.push_back(thisGame);
	}
	gamesFactorsFile.close();


	for (unsigned int i = 0; i < allGameProfiles.size(); ++i)
	{
		float expectedValue = 0;
		//expectedValue -= allGameProfiles[i].eraDiff * inputCoefficients[0];
		expectedValue -= allGameProfiles[i].fipDiff * 0.1f;
		//expectedValue -= allGameProfiles[i].xfipDiff * inputCoefficients[2];
		//expectedValue += allGameProfiles[i].k9Diff * inputCoefficients[3];
		expectedValue -= allGameProfiles[i].whipDiff * 0.7f;
		expectedValue += allGameProfiles[i].opsDiff * 0.9f;
		guessFile << expectedValue;
		guessFile << endl;
	}
	//return;

	float fCoefficientStep = 0.1f;
	float inputCoefficients[4] = { 0.0f, 0.0f, 0.0f, 0.0f };// , 0.0f, 0.0f, 0.0f};
	float mostAccurateCoefficients[4] = { 0.0f, 0.0f, 0.0f, 0.0f };// , 0.0f, 0.0f, 0.0f};
	int bestNumberCorrect = -1;
	inputCoefficients[0] = 0;
	while (inputCoefficients[0] <= 2.0f + fCoefficientStep * 0.5f)
	{
		inputCoefficients[1] = 0;
		while (inputCoefficients[1] <= 2.0f + fCoefficientStep * 0.5f)
		{
			inputCoefficients[2] = 0;
			while (inputCoefficients[2] <= 2.0f + fCoefficientStep * 0.5f)
			{
				inputCoefficients[3] = 0;
				while (inputCoefficients[3] <= 2.0f + fCoefficientStep * 0.5f)
				{
				/*	inputCoefficients[4] = 0;
					while (inputCoefficients[4] <= 1.0f + fCoefficientStep * 0.5f)
					{
						inputCoefficients[5] = 0;
						while (inputCoefficients[5] <= 1.0f + fCoefficientStep * 0.5f)
						{
						*/
							int numCorrect = 0;
							int numTies = 0;
							for (unsigned int i = 0; i < allGameProfiles.size(); ++i)
							{
								float expectedValue = 0;
								//expectedValue -= allGameProfiles[i].eraDiff * inputCoefficients[0];
								expectedValue -= allGameProfiles[i].fipDiff * inputCoefficients[0];
								//expectedValue -= allGameProfiles[i].xfipDiff * inputCoefficients[2];
								//expectedValue += allGameProfiles[i].k9Diff * inputCoefficients[3];
								expectedValue -= allGameProfiles[i].whipDiff * inputCoefficients[1];
								expectedValue += allGameProfiles[i].opsDiff * inputCoefficients[2];
								expectedValue += allGameProfiles[i].homeValue * inputCoefficients[3];
								if (expectedValue > 0)
									numCorrect++;
								else if (expectedValue == 0)
									numTies++;
							}
							if (numCorrect > bestNumberCorrect)
							{
								bestNumberCorrect = numCorrect;
								for (int a = 0; a < 4; ++a)
								{
									mostAccurateCoefficients[a] = inputCoefficients[a];
								}
							}
						/*	inputCoefficients[5] += fCoefficientStep;
						}
						inputCoefficients[4] += fCoefficientStep;
					}
					*/
					inputCoefficients[3] += fCoefficientStep;
				}
				inputCoefficients[2] += fCoefficientStep;
			}
			inputCoefficients[1] += fCoefficientStep;
		}
		inputCoefficients[0] += fCoefficientStep;
	}

	int best = bestNumberCorrect;
}

void Analyze2016TeamWins()
{
	CURL* curl = NULL;
	string totalsFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\2016\\2016Totals.txt";
	ofstream totalsFile;
#if PLATFORM_OSX
    totalsFileName = GetPlatformCompatibleFileNameFromRelativePath(totalsFileName);
#endif
	totalsFile.open(totalsFileName);
	for (int d = 415; d <= 930; ++d)
	{
		int monthInteger = (d / 100) * 100;
		int isolatedDay = d - (monthInteger);
		if (isolatedDay > 31)
		{
			d = monthInteger + 100;
			continue;
		}
		char thisDateCStr[5];
		itoa(d, thisDateCStr, 10);
		string thisDate = thisDateCStr;
		string thisDateWithYear = IntToDateYMD(d, "2016");

		string daysPitcherStats = "";
		string dayStatsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=";
		dayStatsURL += thisDate;
		dayStatsURL += "&game=fd&year=2016&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941";
		CurlGetSiteContents(curl, dayStatsURL, daysPitcherStats);
		size_t lineEndIndex = daysPitcherStats.find(thisDateWithYear);
		size_t linePrevIndex = lineEndIndex;
		lineEndIndex = daysPitcherStats.find("\n", linePrevIndex);

		string todaysPitcherStats = GetEntireFileContents("2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\2016\\" + thisDateWithYear + ".txt");
		
		while (lineEndIndex != string::npos)
		{
			string currentLine = daysPitcherStats.substr(linePrevIndex, lineEndIndex - linePrevIndex);
			vector<string> lineValues = SplitStringIntoMultiple(currentLine, ";");
			if (lineValues.size() != 14)
				break;
			if (lineValues[6] != "1" && lineValues[6] != "")
				break;
			if (lineValues[4] == "1")
			{
				string teamCode = lineValues[9];
				std::transform(teamCode.begin(), teamCode.end(), teamCode.begin(), ::tolower);

				string opposingTeamCode = lineValues[10].substr(lineValues[10].length() - 3, 3);
				bool bIsHomeTeam = lineValues[10].find("@") == string::npos;
				int teamRuns = atoi(lineValues[12].c_str());
				int opposingTeamRuns = atoi(lineValues[13].c_str());
				if (teamRuns > opposingTeamRuns)
				{
					size_t teamPitcherIndex = todaysPitcherStats.find(teamCode + ";");
					size_t teamPitcherEndIndex = todaysPitcherStats.find("\n", teamPitcherIndex);
					for (int skip = 0; skip < 3; ++skip)
					{
						teamPitcherIndex = todaysPitcherStats.find(";", teamPitcherIndex + 1);
					}
					FullSeasonPitcherStats teamPitcherStats(todaysPitcherStats.substr(teamPitcherIndex + 1, teamPitcherEndIndex - teamPitcherIndex - 1));

					teamPitcherIndex = todaysPitcherStats.find(opposingTeamCode + ";");
					teamPitcherEndIndex = todaysPitcherStats.find("\n", teamPitcherIndex);
					for (int skip = 0; skip < 3; ++skip)
					{
						teamPitcherIndex = todaysPitcherStats.find(";", teamPitcherIndex + 1);
					}
					FullSeasonPitcherStats opposingTeamPitcherStats(todaysPitcherStats.substr(teamPitcherIndex + 1, teamPitcherEndIndex - teamPitcherIndex - 1));

					if (teamPitcherStats.era > -1 && opposingTeamPitcherStats.era > -1)
					{
						totalsFile << thisDateWithYear << ";";
						totalsFile << teamCode << ";" << opposingTeamCode << ";";
						totalsFile << teamPitcherStats.era - opposingTeamPitcherStats.era << ";";
						totalsFile << teamPitcherStats.fip - opposingTeamPitcherStats.fip << ";";
						totalsFile << teamPitcherStats.xfip - opposingTeamPitcherStats.xfip << ";";
						totalsFile << teamPitcherStats.strikeOutsPer9 - opposingTeamPitcherStats.strikeOutsPer9 << ";";
						totalsFile << teamPitcherStats.whip - opposingTeamPitcherStats.whip << ";";
						totalsFile << teamPitcherStats.numInnings << ";" << opposingTeamPitcherStats.numInnings << ";";
						if (bIsHomeTeam)
							totalsFile << "1;";
						else
							totalsFile << "-1;";
						totalsFile << endl;
					}
				}
			}
			linePrevIndex = lineEndIndex;
			lineEndIndex = daysPitcherStats.find("\n", lineEndIndex + 1);
		}
	}
	totalsFile.close();
}

void GatherPitcher2016CumulativeData()
{
	CURL* curl = NULL;
	for (int d = 415; d <= 930; ++d)
	{
		int monthInteger = (d / 100) * 100;
		int isolatedDay = d - (monthInteger);
		if (isolatedDay > 31)
		{
			d = monthInteger + 100;
			continue;
		}
		char thisDateCStr[5];
		itoa(d, thisDateCStr, 10);
		string thisDate = thisDateCStr;
		string thisDateWithYear = IntToDateYMD(d, "2016");
		string prevDateWithYear = IntToDateYMD(d - 1, "2016");

		string daysPitcherStats = "";
		string dayStatsURL = "http://rotoguru1.com/cgi-bin/byday.pl?date=";
		dayStatsURL += thisDate;
		dayStatsURL += "&game=fd&year=2016&scsv=1&nowrap=1&user=GoldenExcalibur&key=G5970032941";
		CurlGetSiteContents(curl, dayStatsURL, daysPitcherStats);
		size_t lineEndIndex = daysPitcherStats.find(thisDateWithYear);
		size_t linePrevIndex = lineEndIndex;
		lineEndIndex = daysPitcherStats.find("\n", linePrevIndex);

		ofstream pitcherStatsArchiveFile;
		string pitcherStatsArchiveFileName = "2017ResultsTracker\\TeamWinResults\\PitcherData\\Historical\\2016\\" + thisDateWithYear + ".txt";
#if PLATFORM_OSX
        pitcherStatsArchiveFileName = GetPlatformCompatibleFileNameFromRelativePath(pitcherStatsArchiveFileName);
#endif
		pitcherStatsArchiveFile.open(pitcherStatsArchiveFileName);

		while (lineEndIndex != string::npos)
		{
			string currentLine = daysPitcherStats.substr(linePrevIndex, lineEndIndex - linePrevIndex);
			vector<string> lineValues = SplitStringIntoMultiple(currentLine, ";");
			if (lineValues.size() != 14)
				break;
			if (lineValues[6] != "1" && lineValues[6] != "")
				break;
			if (lineValues[4] == "1")
			{
				FullSeasonPitcherStats cumulativeStats = GetPitcherCumulativeStatsUpTo(lineValues[1], curl, prevDateWithYear);
				std::transform(lineValues[9].begin(), lineValues[9].end(), lineValues[9].begin(), ::tolower);
				pitcherStatsArchiveFile << lineValues[9]  << ";" << lineValues[1] << ";" << lineValues[3] << ";" << cumulativeStats.ToString();
				pitcherStatsArchiveFile << endl;
			}
			linePrevIndex = lineEndIndex;
			lineEndIndex = daysPitcherStats.find("\n", lineEndIndex + 1);
		}
		pitcherStatsArchiveFile.close();
	}
}
void Gather2016TeamWins()
{
	CURL* curl = NULL;
	string pageData = "";
	for (int i = -12; i <= -2; ++i)
	{
		char iCStr[4];
		itoa(i, iCStr, 10);
		string iStr = iCStr;
		pageData += GetEntireFileContents("2017ResultsTracker\\OddsWinsResults\\CachedPage" + iStr + ".txt");
	}
	fstream allGamesFile;
    string allGamesFileName = "2017ResultsTracker\\OddsWinsResults\\AllGamesResults.txt";
#if PLATFORM_OSX
    allGamesFileName = GetPlatformCompatibleFileNameFromRelativePath(allGamesFileName);
#endif
	allGamesFile.open(allGamesFileName);

	for (int d = 706; d <= 816; ++d)
	{
		int monthInteger = (d / 100) * 100;
		int isolatedDay = d - (monthInteger);
		if (isolatedDay > 31)
		{
			d = ((d / 100) + 1) * 100;
			continue;
		}
		char thisDayCStr[3];
		itoa(isolatedDay, thisDayCStr, 10);
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
		winResultsFileName += IntToDateYMD(d, "2017") +".txt";
#if PLATFORM_OSX
        winResultsFileName = GetPlatformCompatibleFileNameFromRelativePath(winResultsFileName);
#endif
		winResultsOutputFile.open(winResultsFileName);

		string dateSearchString = thisDay + " " + thisMonth + " 2017";
		size_t dateIndex = pageData.find(dateSearchString);
		if (dateIndex == string::npos) {
			dateIndex = pageData.find(", " + thisDay + " " + thisMonth);
		}
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
						string yearMonthDate = IntToDateYMD(atoi(timeString.c_str()) <= 7 ? d - 1 : d, "2017");
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
#if PLATFORM_OSX
                pitchersDataFileName = GetPlatformCompatibleFileNameFromRelativePath(pitchersDataFileName);
#endif
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
#if PLATFORM_OSX
                battersDataFileName = GetPlatformCompatibleFileNameFromRelativePath(battersDataFileName);
#endif
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
			itoa(page + 1, pageCStr, 10);
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
			itoa(page + 1, pageCStr, 10);
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

				FullSeasonPitcherStats pitcherCurrentYearStats = GetPitcherStats(pitcherGID, CURRENT_YEAR, curl);
				for (int i = 0; i < 19; ++i)
				{
					pitcherIndex = startingPitcherData.find(";", pitcherIndex + 1);
				}
				prevPitcherIndex = startingPitcherData.rfind(";", pitcherIndex - 1);
				string pitcherHandedness = startingPitcherData.substr(prevPitcherIndex + 1, pitcherIndex - prevPitcherIndex - 1);
				FullSeasonStatsAdvanced pitcherCurrentYearAdvancedStats = GetPitcherAdvancedStats(pitcherGID, CURRENT_YEAR, curl);

				allPlayers[i].opposingPitcherEra = pitcherCurrentYearStats.era;
				allPlayers[i].opposingPitcherStrikeOutsPer9 = pitcherCurrentYearStats.strikeOutsPer9;
				allPlayers[i].opposingPitcherWhip = pitcherCurrentYearStats.whip;
				allPlayers[i].opposingPitcherAverageAgainstHandedness = (pitcherCurrentYearAdvancedStats.averageVersusLefty + pitcherCurrentYearAdvancedStats.averageVersusRighty) * 0.5f;
				if (allPlayers[i].batterHandedness == "L")
					allPlayers[i].opposingPitcherAverageAgainstHandedness = pitcherCurrentYearAdvancedStats.averageVersusLefty;
				else if (allPlayers[i].batterHandedness == "R")
					allPlayers[i].opposingPitcherAverageAgainstHandedness = pitcherCurrentYearAdvancedStats.averageVersusRighty;

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
				
				FullSeasonPitcherStats pitcherCurrentYearStats = GetPitcherStats(pitcherGID, CURRENT_YEAR, curl);
				
				if ( pitcherCurrentYearStats.strikeOutsPer9 < 7.69f && pitcherCurrentYearStats.era >= 4.15f && pitcherCurrentYearStats.whip >= 1.308f)
				{
					for (int i = 0; i < 19; ++i)
					{
						pitcherIndex = startingPitcherData.find(";", pitcherIndex + 1);
					}
					prevPitcherIndex = startingPitcherData.rfind(";", pitcherIndex - 1);
					string pitcherHandedness = startingPitcherData.substr(prevPitcherIndex + 1, pitcherIndex - prevPitcherIndex - 1);
					FullSeasonStatsAdvanced pitcher2017AdvancedStats = GetPitcherAdvancedStats(pitcherGID, CURRENT_YEAR, curl);
					
					eligiblePlayers[i].opposingPitcherEra = pitcherCurrentYearStats.era;
					eligiblePlayers[i].opposingPitcherStrikeOutsPer9 = pitcherCurrentYearStats.strikeOutsPer9;
					eligiblePlayers[i].opposingPitcherWhip = pitcherCurrentYearStats.whip;
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
#if PLATFORM_OSX
        resultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(resultsTrackerFileName);
#endif
		resultsTrackerFile.open(resultsTrackerFileName);
		for (unsigned int i = 0; i < eligiblePlayers.size(); ++i)
		{
			resultsTrackerFile << eligiblePlayers[i].playerName << ";" << eligiblePlayers[i].hitsPerGameLast30Days << ";" << eligiblePlayers[i].averageLast7Days << ";" << eligiblePlayers[i].averageVsPitcherFacing << ";";
			resultsTrackerFile << endl;
		}
		resultsTrackerFile.close();

		ofstream allResultsTrackerFile;
		string allResultsTrackerFileName = "2017ResultsTracker\\BeatTheStreak\\AllPlayersDaily\\" + todaysDate + ".txt";
#if PLATFORM_OSX
        allResultsTrackerFileName = GetPlatformCompatibleFileNameFromRelativePath(allResultsTrackerFileName);
#endif
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
		for (int page = 0; page < 8; ++page)
		{
			std::string seasonStats;
			char pageCStr[3];
			itoa(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
            string readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=120&type=1&season=";
            readURL += CURRENT_YEAR;
            readURL += "&month=0&season1=";
            readURL += CURRENT_YEAR;
            readURL += "&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=10,d&page=" + pageStr + "_50";
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
			itoa(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
            string readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=60&type=1&season=";
            readURL += CURRENT_YEAR;
            readURL += "&month=3&season1=";
            readURL += CURRENT_YEAR;
            readURL += "&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=10,d&page=" + pageStr + "_50";
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
			itoa(page + 1, pageCStr, 10);
			string pageStr = pageCStr;
            string readURL = "http://www.fangraphs.com/leaders.aspx?pos=all&stats=bat&lg=all&qual=10&type=1&season=";
            readURL += CURRENT_YEAR;
            readURL += "&month=1&season1=";
            readURL += CURRENT_YEAR;
            readURL += "&ind=0&team=0&rost=0&age=0&filter=&players=0&sort=10,d&page=" + pageStr + "_50";
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
