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
void GenerateNewLineup();
void ChooseAPitcher();
void Analyze2016Stats();

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
};

void RefineAlgorithm();

void ChooseAPitcher();

void GenerateNewLineup();

std::vector<PlayerData> OptimizeLineupToFitBudget();

void PopulateProbableRainoutGames();

void Analyze2016Stats();

void UnitTestAllStatCollectionFunctions();

void GetBallparkFactors(std::string ballparkName, std::string statName, float& outFactorLeftyBatter, float& outFactorRightyBatter);
