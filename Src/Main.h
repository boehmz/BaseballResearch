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
void ChooseAPitcher(CURL *curl);
void Analyze2016Stats();

struct BeatTheStreakPlayerProfile
{
	std::string playerName;
	float hitsPerGameLast30Days;
	float averageLast7Days;
	float averageVsPitcherFacing;
	std::string opposingPitcherName;
	float opposingPitcherEra;
	float opposingPitcherStrikeOutsPer9;
	float opposingPitcherWhip;
	float opposingPitcherAverageAgainstHandedness;
	std::string batterHandedness;

	BeatTheStreakPlayerProfile() : playerName(""),
	hitsPerGameLast30Days(-1),
	 averageLast7Days(-1),
	 averageVsPitcherFacing(-1),
	 opposingPitcherName(""),
	 opposingPitcherEra(-1),
	 opposingPitcherStrikeOutsPer9(-1),
	 opposingPitcherWhip(-1),
	 opposingPitcherAverageAgainstHandedness(-1),
	 batterHandedness("")
	{

	}

	BeatTheStreakPlayerProfile(int minMaxInitializer) : playerName(""),		
		opposingPitcherName(""),
		batterHandedness("")
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
};

void RefineAlgorithm();

void ChooseAPitcher();

void GenerateNewLineup();

std::unordered_map<std::string, bool> probableStarters;
void DetermineProbableStarters(CURL* curl);

struct BatterSplitsData
{
	float opsSeason;
	float opsLast30Days;
	float opsLast7Days;

	float opsVersusLeftySeason;
	float opsVersusRightySeason;
	float opsVersusLeftyLast30Days;
	float opsVersusRightyLast30Days;


	float ppgSeason;
	float ppgLast30Days;
	float ppgLast7Days;

	float ppgHandednessSeason;
	float ppgHandednessLast30Days;

	BatterSplitsData() : opsSeason(-1),
		opsLast30Days(-1),
		opsLast7Days(-1),
		opsVersusLeftySeason(-1),
		opsVersusRightySeason(-1),
		opsVersusLeftyLast30Days(-1),
		opsVersusRightyLast30Days(-1),
		ppgSeason(-1),
		ppgLast30Days(-1),
		ppgLast7Days(-1),
		ppgHandednessSeason(-1),
		ppgHandednessLast30Days(-1)
		{
		}
};
std::unordered_map<std::string, BatterSplitsData> allBattersSplits;
void AssembleBatterSplits(CURL *curl);
std::string ConvertFLNameToLFName(std::string firstLast);
std::string ConvertLFNameToFLName(std::string lastFirst);
std::string IntToDateYMD(int date, bool roundup = false);

std::vector<PlayerData> OptimizeLineupToFitBudget();

void PopulateProbableRainoutGames(CURL* curl);

void Analyze2016Stats();
void AnalyzeTeamWinFactors();
void GatherTeamWins();
void GatherPitcherCumulativeData();
std::vector<std::string> GetRankingsRowColumns(std::string teamName, std::string allData, int numColumns);

void UnitTestAllStatCollectionFunctions();

void GetBallparkFactors(std::string ballparkName, std::string statName, float& outFactorLeftyBatter, float& outFactorRightyBatter);

void GetBeatTheStreakCandidates(CURL *curl);

std::vector<std::string> SplitStringIntoMultiple(std::string wholeString, std::string tokens);

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer);
