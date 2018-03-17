#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include "StatsCollectionFunctions.h"

std::vector<PlayerData> OptimizeLineupToFitBudget();

bool comparePlayerByPointsPerGame(PlayerData i, PlayerData j);

bool comparePlayersBySalary(PlayerData i, PlayerData j);

void RefineAlgorithm();
void RefineAlgorithmForBeatTheStreak();
void GenerateNewLineup(CURL *curl);
void GenerateNewLineupFromSabrPredictor(CURL *curl);
void ChooseAPitcher(CURL *curl);
void Analyze2016Stats();

struct TeamStackTracker {
	std::string teamCode;
	float teamTotalExpectedPoints;
	int numPlayersAdded;
};

struct BeatTheStreakPlayerProfile
{
	std::string playerName =  "";
	float hitsPerGameLast30Days = -1;
	float averageLast7Days = -1;
	float averageVsPitcherFacing = -1;
	std::string opposingPitcherName = "";
	float opposingPitcherEra = -1;
	float opposingPitcherStrikeOutsPer9 = -1;
	float opposingPitcherWhip = -1;
	float opposingPitcherAverageAgainstHandedness = -1;
	std::string batterHandedness = "";

	BeatTheStreakPlayerProfile() {}

	BeatTheStreakPlayerProfile(int minMaxInitializer)
	{
		if (minMaxInitializer == 0)
		{
			hitsPerGameLast30Days = 0;
			averageLast7Days = 0;
			averageVsPitcherFacing = 0;
			opposingPitcherEra = 0;
			opposingPitcherStrikeOutsPer9 = 0;
			opposingPitcherWhip = 0;
			opposingPitcherAverageAgainstHandedness = 0;
		}
		else if (minMaxInitializer < 0)
		{
			hitsPerGameLast30Days = -1;
			averageLast7Days = -1;
			averageVsPitcherFacing = -1;
			opposingPitcherEra = -1;
			opposingPitcherStrikeOutsPer9 = 9999;
			opposingPitcherWhip = -1;
			opposingPitcherAverageAgainstHandedness = -1;
		}
		else if (minMaxInitializer > 0)
		{
			hitsPerGameLast30Days = 9999;
			averageLast7Days = 9999;
			averageVsPitcherFacing = 9999;
			opposingPitcherEra = 9999;
			opposingPitcherStrikeOutsPer9 = -1;
			opposingPitcherWhip = 9999;
			opposingPitcherAverageAgainstHandedness = 9999;
		}
	}

	std::string ToString();
};

void PopulateProbableRainoutGames();

struct OpponentInformation
{
	std::string ballParkPlayedIn;
	// weather report team name
	std::string weatherSiteTeamName;
	// teamrankings.com team name
	std::string rankingsSiteTeamName;
	// 3 letter code
	std::string teamCodeRotoGuru;
	// 3 letter code used for team statistics stie
	std::string teamCodeRankingsSite;

	int gameTime;

	FullSeasonStatsAdvanced pitcherAdvancedStats;
	float pitcherEstimatedPpg;
	float teamWinEstimatedScore;
};

void RefineAlgorithm();

std::unordered_map<std::string, bool> probableStarters;
void DetermineProbableStarters(CURL* curl);

struct BatterSplitsData
{
	float opsSeason = -1;
	float opsLast30Days = -1;
	float opsLast7Days = -1;

	float opsVersusLeftySeason = -1;
	float opsVersusRightySeason = -1;
	float opsVersusLeftyLast30Days = -1;
	float opsVersusRightyLast30Days = -1;


	float ppgSeason = -1;
	float ppgLast30Days = -1;
	float ppgLast7Days = -1;

	float ppgHandednessSeason = -1;
	float ppgHandednessLast30Days = -1;

	BatterSplitsData() {}
};
std::unordered_map<std::string, BatterSplitsData> allBattersSplits;
void AssembleBatterSplits(CURL *curl);


std::vector<PlayerData> OptimizeLineupToFitBudget();

void PopulateProbableRainoutGames(CURL* curl);

void Analyze2016Stats();
void AnalyzeTeamWinFactors();
void Gather2016TeamWins();
void GatherPitcher2016CumulativeData();
void Analyze2016TeamWins();
void Analyze2016TeamWinFactors();
void Refine2016TeamWinFactors();
std::vector<std::string> GetRankingsRowColumns(std::string teamName, std::string allData, int numColumns);

float GetExpectedFanduelPointsFromPitcherStats(FullSeasonPitcherStats pitcherStats, float opponentRunsPerGame = 4.4f, float opponentStrikeoutsPerGame = 8.1f);
void UnitTestAllStatCollectionFunctions();

void GetBallparkFactors(std::string ballparkName, std::string statName, float& outFactorLeftyBatter, float& outFactorRightyBatter);

void GetBeatTheStreakCandidates(CURL *curl);
std::string ConvertOddsPortalNameToTeamRankingsName(std::string oddsportalTeamName);
std::string ConvertTeamCodeToTeamRankingsName(std::string teamCode);
std::string ConvertOddsPortalNameToTeamCodeName(std::string oddsportalTeamName, bool standardTeamCode);
std::string ConvertTeamCodeToOddsPortalName(std::string teamCode, bool standardTeamCode);
std::string ConvertStandardTeamCodeToRotoGuruTeamCode(std::string standardCode);
std::string ConvertRotoGuruTeamCodeToStandardTeamCode(std::string rotoGuruCode);

