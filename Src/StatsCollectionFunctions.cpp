#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <sys/stat.h>
#include <sys/types.h>
#if !PLATFORM_OSX
#include <direct.h>
#endif
#include "StatsCollectionFunctions.h"
#include "SharedGlobals.h"
#include "StringUtils.h"
using namespace std;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	if (userp != NULL)
		((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

string GetEntireFileContents(string fileName)
{
#if PLATFORM_OSX
    fileName = GetPlatformCompatibleFileNameFromRelativePath(fileName);
#endif

	ifstream ifs(fileName.c_str(), ios::in | ios::binary | ios::ate);
	if (!ifs.good()) {
		ifs.close();
		return "";
	}
	unsigned int fileSize = static_cast<unsigned int>( ifs.tellg());
	ifs.seekg(0, ios::beg);

	vector<char> bytes(fileSize);
	ifs.read(bytes.data(), fileSize);
	ifs.close();
	return string(bytes.data(), fileSize);
}

string ReplaceURLWhiteSpaces(string originalURL)
{
	size_t urlWhiteSpaceIndex = originalURL.find(" ", 0);
	while (urlWhiteSpaceIndex != string::npos)
	{
		originalURL.erase(urlWhiteSpaceIndex, 1);
		originalURL.insert(urlWhiteSpaceIndex, "%20");
		urlWhiteSpaceIndex = originalURL.find(" ", urlWhiteSpaceIndex);
	}
	return originalURL;
}

float CalculateRSquared(vector<float> finalInputs, vector<float> outputValues)
{
	if (finalInputs.size() != outputValues.size())
		return -2;

	float inputAverage = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		inputAverage += finalInputs[i];
	}
	inputAverage /= (float)finalInputs.size();

	float outputAverage = 0;
	for (unsigned int i = 0; i < outputValues.size(); ++i)
	{
		outputAverage += outputValues[i];
	}
	outputAverage /= (float)outputValues.size();
	float inputOutputAverage = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		inputOutputAverage += finalInputs[i] * outputValues[i];
	}
	inputOutputAverage /= (float)finalInputs.size();
	float inputSquaredAverage = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		inputSquaredAverage += finalInputs[i] * finalInputs[i];
	}
	inputSquaredAverage /= (float)finalInputs.size();

	float m = (inputAverage * outputAverage - inputOutputAverage) / (inputAverage * inputAverage - inputSquaredAverage);
	float b = outputAverage - m * inputAverage;
	float ssRes = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		float estimatedOutput = m * finalInputs[i] + b;
		ssRes += (outputValues[i] - estimatedOutput) * (outputValues[i] - estimatedOutput);
	}
	float ssTot = 0;
	for (unsigned int i = 0; i < outputValues.size(); ++i)
	{
		ssTot += (outputValues[i] - outputAverage) * (outputValues[i] - outputAverage);
	}
	float rSquared = 1.0f - ssRes / ssTot;
	return rSquared;
}

bool doesPlayerThrowLeftHanded(std::string playerId, CURL *curl) {
    string rotoguruData = GetPlayerStatsRawString(playerId, "any", curl);
    size_t throwsIndex = rotoguruData.find("Throws:");
    if (throwsIndex != string::npos) {
        size_t throwBeginIndex = rotoguruData.find(">", throwsIndex + 1);
        size_t throwEndIndex = rotoguruData.find("<", throwBeginIndex);
        if (throwBeginIndex != string::npos && throwEndIndex != string::npos) {
            string throwsString = rotoguruData.substr(throwBeginIndex + 1, throwEndIndex - throwBeginIndex - 1);
            if (throwsString == "") {
                rotoguruData = GetPlayerStatsRawString(playerId, "2018", curl);
                throwsIndex = rotoguruData.find("Throws:");
                if (throwsIndex != string::npos) {
                    throwBeginIndex = rotoguruData.find(">", throwsIndex + 1);
                    throwEndIndex = rotoguruData.find("<", throwBeginIndex);
                    if (throwBeginIndex != string::npos && throwEndIndex != string::npos) {
                        throwsString = rotoguruData.substr(throwBeginIndex + 1, throwEndIndex - throwBeginIndex - 1);
                    }
                }
            }
            if (throwsString == "Right") {
                return false;
            } else if (throwsString == "Left") {
                return true;
            }
            // hard code because some players have their throws field missing
            if (playerId == "147u") {
                return false;
            }
            if (throwsString.at(0) == 'R' || throwsString.at(0) == 'r') {
                return false;
            }
            if (throwsString.at(0) == 'L' || throwsString.at(0) == 'l') {
                return true;
            }
        }
    }
    cout << "Could not find throwing hand information about " << playerId << endl;
    return false;
}

char getPlayerBattingHandedness(std::string playerId, CURL *curl) {
    string rotoguruData = GetPlayerStatsRawString(playerId, "any", curl);
    size_t throwsIndex = rotoguruData.find("Bats:");
    if (throwsIndex != string::npos) {
        size_t throwBeginIndex = rotoguruData.find(">", throwsIndex + 1);
        size_t throwEndIndex = rotoguruData.find("<", throwBeginIndex);
        if (throwBeginIndex != string::npos && throwEndIndex != string::npos) {
            string throwsString = rotoguruData.substr(throwBeginIndex + 1, throwEndIndex - throwBeginIndex - 1);
            if (throwsString == "") {
                rotoguruData = GetPlayerStatsRawString(playerId, "2018", curl);
                throwsIndex = rotoguruData.find("Bats:");
                if (throwsIndex != string::npos) {
                    throwBeginIndex = rotoguruData.find(">", throwsIndex + 1);
                    throwEndIndex = rotoguruData.find("<", throwBeginIndex);
                    if (throwBeginIndex != string::npos && throwEndIndex != string::npos) {
                        throwsString = rotoguruData.substr(throwBeginIndex + 1, throwEndIndex - throwBeginIndex - 1);
                    }
                }
            }
            if (throwsString == "Right") {
                return 'R';
            } else if (throwsString == "Left") {
                return 'L';
            } else if (throwsString == "Switch") {
                return 'S';
            }
            if (throwsString.at(0) == 'R' || throwsString.at(0) == 'r') {
                return 'R';
            }
            if (throwsString.at(0) == 'L' || throwsString.at(0) == 'l') {
                return 'L';
            }
            if (throwsString.at(0) == 'S' || throwsString.at(0) == 's') {
                return 'S';
            }
        }
    }
    cout << "Could not find batting hand information about " << playerId << endl;
    return false;
}

string GetPlayerStatsRawString(string playerId, string yearString, CURL *curl)
{
	string playerStatsLookupBuffer = "";
    string playerStatsFileName = "Player";
    if (yearString == "any")
        playerStatsFileName += CURRENT_YEAR;
    else
        playerStatsFileName += yearString;
    playerStatsFileName += "DataCached\\PlayerId" + playerId + ".txt";
    playerStatsLookupBuffer = GetEntireFileContents(playerStatsFileName);
#if PLATFORM_OSX
    playerStatsFileName = GetPlatformCompatibleFileNameFromRelativePath(playerStatsFileName);
#endif
	if (yearString == CURRENT_YEAR)
	{
		size_t dateMetaDataIndex = playerStatsLookupBuffer.find("/ZachDateMetaData", 0);
		if (dateMetaDataIndex == string::npos || playerStatsLookupBuffer.substr(0, dateMetaDataIndex) < todaysDate)
			playerStatsLookupBuffer = "";
	}
    // if this year cached file not found, try last year because looking for any
    if (playerStatsLookupBuffer == "" && yearString == "any") {
        string tempPlayerStatsFileName = "Player";
        tempPlayerStatsFileName += LAST_YEAR;
        tempPlayerStatsFileName += "DataCached\\PlayerId" + playerId + ".txt";
        playerStatsLookupBuffer = GetEntireFileContents(tempPlayerStatsFileName);
    }

	if (playerStatsLookupBuffer == "")
	{
		if (curl == NULL)
			curl = curl_easy_init();
        string playerStatsLookupURL = "http://rotoguru1.com/cgi-bin/player";
        if (yearString != "any" && yearString != CURRENT_YEAR)
            playerStatsLookupURL += yearString.substr(2);
        playerStatsLookupURL += ".cgi?" + playerId + "x";

		curl_easy_setopt(curl, CURLOPT_URL, playerStatsLookupURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &playerStatsLookupBuffer);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		size_t writeToFileIndex = playerStatsLookupBuffer.find("Daily results", 0);
		if (writeToFileIndex == string::npos)
			writeToFileIndex = playerStatsLookupBuffer.find("No data found", 0);
		ofstream writeToFile;
        writeToFile.open(playerStatsFileName);
        if (yearString == CURRENT_YEAR)
            writeToFile << todaysDate << "/ZachDateMetaData" << endl;
		writeToFile << playerStatsLookupBuffer.substr(0, writeToFileIndex);
		writeToFile.close();
		
	}
	return playerStatsLookupBuffer;
}

FullSeasonStats GetBatterStats(string playerId, string yearString, CURL *curl)
{
	string playerStatsLookupBuffer = GetPlayerStatsRawString(playerId, yearString, curl);
	FullSeasonStats playerStats;
	playerStats.averagePpg = playerStats.averagePpgVsLefty = playerStats.averagePpgVsRighty = 0;
	playerStats.totalGamesStarted = 0;

	size_t ppgIndex = playerStatsLookupBuffer.find("full season averages", 0);
	if (ppgIndex != string::npos)
	{
		ppgIndex = playerStatsLookupBuffer.find("(R)", ppgIndex);
		size_t ppgStartIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex);
		playerStats.averagePpgVsRighty = stof(playerStatsLookupBuffer.substr(ppgStartIndex + 1, ppgIndex - ppgStartIndex - 1).c_str());

		ppgIndex = playerStatsLookupBuffer.find("(L)", ppgIndex);
		ppgStartIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex);
		playerStats.averagePpgVsLefty = stof(playerStatsLookupBuffer.substr(ppgStartIndex + 1, ppgIndex - ppgStartIndex - 1).c_str());

		ppgIndex = playerStatsLookupBuffer.find("(starting)", 0);
		ppgIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex);
		ppgStartIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex - 1);
		playerStats.averagePpg = stof(playerStatsLookupBuffer.substr(ppgStartIndex + 1, ppgIndex - ppgStartIndex - 1).c_str());

		size_t gamesStartedIndex = playerStatsLookupBuffer.find("started:", 0);
		if (gamesStartedIndex != string::npos)
		{
			size_t gamesStartedLeftIndex = playerStatsLookupBuffer.rfind(">", gamesStartedIndex);
			string gamesStartedString = playerStatsLookupBuffer.substr(gamesStartedLeftIndex + 1, gamesStartedIndex - gamesStartedLeftIndex - 2);
			playerStats.totalGamesStarted = atoi(gamesStartedString.c_str());
		}
	}

	return playerStats;
}

std::vector<string> GetFangraphsRowColumns(std::string yearRow, std::string allData, int numColumns, std::string section, std::string nextSection, bool watchOutForProjections)
{
	vector<string> allColumns;
	size_t fangraphsCurrentIndex = allData.find(section, 0);
	size_t fangraphsThisCategoryIndex = fangraphsCurrentIndex;
	size_t fangraphsNextCategoryIndex = allData.find(nextSection, 0);
	if (nextSection == "")
		fangraphsNextCategoryIndex = string::npos;
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = allData.find(yearRow, fangraphsCurrentIndex + 1);
		if (watchOutForProjections)
		{
			while (true && fangraphsCurrentIndex != string::npos)
			{
				size_t prevTr = allData.rfind("<tr", fangraphsCurrentIndex);
				size_t prevProjections = allData.rfind("projections", fangraphsCurrentIndex);
				size_t prevMinors = allData.rfind("minors", fangraphsCurrentIndex);
				if ((prevProjections != string::npos && prevProjections > prevTr) ||
					(prevMinors != string::npos && prevMinors > prevTr))
				{
					fangraphsCurrentIndex = allData.find(yearRow, fangraphsCurrentIndex + 1);
				}
				else
				{
					break;
				}
				// none exist, it was all playoffs/minors
				if (fangraphsCurrentIndex < fangraphsThisCategoryIndex)
					return allColumns;
			}
		}
		if (fangraphsCurrentIndex != string::npos && fangraphsCurrentIndex < fangraphsNextCategoryIndex)
		{
			fangraphsCurrentIndex = allData.find("</td>", fangraphsCurrentIndex + 1);
			while (numColumns > 0)
			{
				fangraphsCurrentIndex = allData.find("</td>", fangraphsCurrentIndex + 1);
				size_t fangraphsPrevIndex = allData.rfind(">", fangraphsCurrentIndex);
				allColumns.push_back(allData.substr(fangraphsPrevIndex + 1, fangraphsCurrentIndex - fangraphsPrevIndex - 1));
				numColumns--;
			}
		}
	}
	return allColumns;
}

FullSeasonPitcherStats GetPitcherStats(string playerId, string yearString, CURL *curl)
{
	FullSeasonPitcherStats pitcherStats;
    pitcherStats.isLeftHanded = doesPlayerThrowLeftHanded(playerId, curl);

	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, AdvancedStatsPitchingStarterStatsOnly);
	if (fangraphsPlayerData == "")
	{
		fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, 0);
	}
	bool bStarterOnlyNotAvailable = fangraphsPlayerData.find("As Starter") == string::npos;

	vector<string> fangraphsStandardRows = GetFangraphsRowColumns(yearString, fangraphsPlayerData, 18, "name=\"standard\"", "name=\"advanced\"", bStarterOnlyNotAvailable);
	if (fangraphsStandardRows.size() == 0)
		return pitcherStats;
	if (bStarterOnlyNotAvailable)
	{
		pitcherStats.era = stof(fangraphsStandardRows[3].c_str());
		pitcherStats.numInnings = stof(fangraphsStandardRows[11].c_str());
		pitcherStats.numInnings = floor(pitcherStats.numInnings) + ((pitcherStats.numInnings - floor(pitcherStats.numInnings)) * 3.4f);
	}
	else
	{
		pitcherStats.numInnings = stof(fangraphsStandardRows[1].c_str());
		pitcherStats.numInnings = floor(pitcherStats.numInnings) + ((pitcherStats.numInnings - floor(pitcherStats.numInnings)) * 3.4f);
		pitcherStats.era = stof(fangraphsStandardRows[2].c_str());
		pitcherStats.opsAllowed = stof(fangraphsStandardRows[15].c_str());
		pitcherStats.opsAllowed += stof(fangraphsStandardRows[16].c_str());
		pitcherStats.wobaAllowed = stof(fangraphsStandardRows[17].c_str());
	}

	vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(yearString, fangraphsPlayerData, 14, "name=\"advanced\"", "name=\"battedball\"", bStarterOnlyNotAvailable);
	pitcherStats.whip = stof(fangraphsAdvancedRows[9].c_str());
	if (bStarterOnlyNotAvailable)
	{
		vector<string> fangraphsDashboardRows = GetFangraphsRowColumns(yearString, fangraphsPlayerData, 17, "name=\"dashboard\"", "name=\"standard\"", bStarterOnlyNotAvailable);
		pitcherStats.strikeOutsPer9 = stof(fangraphsDashboardRows[7].c_str());
		pitcherStats.fip = stof(fangraphsDashboardRows[15].c_str());
		pitcherStats.xfip = stof(fangraphsDashboardRows[16].c_str());

	}
	else
	{
		pitcherStats.strikeOutsPer9 = stof(fangraphsAdvancedRows[1].c_str());
		pitcherStats.whip = stof(fangraphsAdvancedRows[9].c_str());
		pitcherStats.fip = stof(fangraphsAdvancedRows[12].c_str());
		pitcherStats.xfip = stof(fangraphsAdvancedRows[13].c_str());
	}
	return pitcherStats;
}

FullSeasonStatsAdvancedNoHandedness GetBatterStatsSeason(std::string playerId, CURL *curl, std::string yearString) {
	FullSeasonStatsAdvancedNoHandedness batterStats;
	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, 0);
	vector<string> fangraphsYearRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 20, "name=\"dashboard\"", "", true);
	if (fangraphsYearRows.size() > 19 && fangraphsYearRows[12] != "" && fangraphsYearRows[13] != "") {
		batterStats.average = stof(fangraphsYearRows[11]);
		batterStats.onBaseAverage = stof(fangraphsYearRows[12]);
		batterStats.slugging = stof(fangraphsYearRows[13]);
		batterStats.ops = batterStats.onBaseAverage + batterStats.slugging;
		batterStats.woba = stof(fangraphsYearRows[14]);
		batterStats.wrcPlus = stof(fangraphsYearRows[15]);
		batterStats.iso = stof(fangraphsYearRows[9]);
		batterStats.walkPercent = stof(fangraphsYearRows[7].substr(0, fangraphsYearRows[7].length() - 1));
		batterStats.strikeoutPercent = stof(fangraphsYearRows[8].substr(0, fangraphsYearRows[8].length() - 1));
		batterStats.numPlateAppearances = atoi(fangraphsYearRows[2].c_str());
        batterStats.runsPerPA = stof(fangraphsYearRows[4]) / batterStats.numPlateAppearances;
        batterStats.rbisPerPA = stof(fangraphsYearRows[5]) / batterStats.numPlateAppearances;
	}

	return batterStats;
}

FullSeasonStatsAdvanced GetBatterCumulativeAdvancedStatsUpTo(std::string playerId, std::string dateUpTo, bool entireCareer) {
	FullSeasonStatsAdvanced batterAdvancedStats;

	string cachedDate = GetDateBeforeOrAfterNumDays(dateUpTo, 1);
	string cachedAtDateVersusLeftFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId + "VsLeft.txt";
	string cachedAtDateVersusLeftFileContents = GetEntireFileContents(cachedAtDateVersusLeftFileName);
	if (cachedAtDateVersusLeftFileContents != "") {
		string rowTitle = ">Total<";
		if (!entireCareer) {
			rowTitle = ">" + cachedDate.substr(0, 4) + "<";
		}

		vector<string> fangraphsStandardRows = GetFangraphsRowColumns(rowTitle, cachedAtDateVersusLeftFileContents, 21, "name=\"standard\"", "name=\"advanced\"", false);
		if (fangraphsStandardRows.size() == 21) {
			batterAdvancedStats.numPlateAppearancesVersusLefty = atoi(fangraphsStandardRows[3].c_str());
			batterAdvancedStats.averageVersusLefty = stof(fangraphsStandardRows[20]);
		}
		vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(rowTitle, cachedAtDateVersusLeftFileContents, 14, "name=\"advanced\"", "name=\"battedball\"", false);
		if (fangraphsAdvancedRows.size() == 14) {
			batterAdvancedStats.sluggingVersusLefty = stof(fangraphsAdvancedRows[6]);
			batterAdvancedStats.isoVersusLefty = stof(fangraphsAdvancedRows[8]);
			batterAdvancedStats.opsVersusLefty = stof(fangraphsAdvancedRows[7]);
			batterAdvancedStats.wobaVersusLefty = stof(fangraphsAdvancedRows[12]);
		}
	}

	string cachedAtDateVersusRightFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId + "VsRight.txt";
	string cachedAtDateVersusRightFileContents = GetEntireFileContents(cachedAtDateVersusRightFileName);
	if (cachedAtDateVersusRightFileContents != "") {
		string rowTitle = ">Total<";
		if (!entireCareer) {
			rowTitle = ">" + cachedDate.substr(0, 4) + "<";
		}
		vector<string> fangraphsStandardRows = GetFangraphsRowColumns(rowTitle, cachedAtDateVersusRightFileContents, 21, "name=\"standard\"", "name=\"advanced\"", false);
		if (fangraphsStandardRows.size() == 21) {
			batterAdvancedStats.numPlateAppearancesVersusRighty = atoi(fangraphsStandardRows[3].c_str());
			batterAdvancedStats.averageVersusRighty = stof(fangraphsStandardRows[20]);
		}
		vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(rowTitle, cachedAtDateVersusRightFileContents, 14, "name=\"advanced\"", "name=\"battedball\"", false);
		if (fangraphsAdvancedRows.size() == 14) {
			batterAdvancedStats.sluggingVersusRighty = stof(fangraphsAdvancedRows[6]);
			batterAdvancedStats.isoVersusRighty = stof(fangraphsAdvancedRows[8]);
			batterAdvancedStats.opsVersusRighty = stof(fangraphsAdvancedRows[7]);
			batterAdvancedStats.wobaVersusRighty = stof(fangraphsAdvancedRows[12]);
		}
	}
	return batterAdvancedStats;
}

FullSeasonStatsAdvancedNoHandedness GetBatterCumulativeStatsUpTo(std::string playerId, CURL *curl, std::string dateUpTo, bool entireCareer) {
	FullSeasonStatsAdvancedNoHandedness batterStats;
    
    string cachedDate = GetDateBeforeOrAfterNumDays(dateUpTo, 1);
    string cachedAtDateFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId + ".txt";
    string cachedAtDateFileContents = GetEntireFileContents(cachedAtDateFileName);
    if (cachedAtDateFileContents != "") {
        string rowTitle = ">Total<";
        if (!entireCareer) {
            rowTitle = ">" + cachedDate.substr(0,4) + "<";
        }
        vector<string> fangraphsStandardRows = GetFangraphsRowColumns(rowTitle, cachedAtDateFileContents, 20, "name=\"dashboard", "name=\"standard", false);
        if (fangraphsStandardRows.size() == 0)
            return batterStats;
        batterStats.numPlateAppearances = atoi(fangraphsStandardRows[2].c_str());
        batterStats.average = stof(fangraphsStandardRows[11]);
        batterStats.onBaseAverage = stof(fangraphsStandardRows[12]);
        batterStats.slugging = stof(fangraphsStandardRows[13]);
        batterStats.ops = batterStats.onBaseAverage + batterStats.slugging;
        batterStats.iso = stof(fangraphsStandardRows[9]);
        batterStats.woba = stof(fangraphsStandardRows[14]);
        if (batterStats.average > 0) {
            batterStats.wrcPlus = stof(fangraphsStandardRows[15]);
        }
        batterStats.strikeoutPercent = stof( fangraphsStandardRows[8].substr(0, fangraphsStandardRows[8].length() - 1));
        batterStats.walkPercent = stof(fangraphsStandardRows[7].substr(0, fangraphsStandardRows[7].length() - 1));
        batterStats.rbisPerPA = stof(fangraphsStandardRows[5]) / batterStats.numPlateAppearances;
        batterStats.runsPerPA = stof(fangraphsStandardRows[4]) / batterStats.numPlateAppearances;
        return batterStats;
    }

	string fangraphsPlayerData = GetPlayerFangraphsPageDataCumulativeUpTo(playerId, curl, dateUpTo, false, entireCareer);

	vector<string> fangraphsStandardRows = GetFangraphsRowColumns(">Total<", fangraphsPlayerData, 22, "id=\"DailyStats", "", false);
	if (fangraphsStandardRows.size() == 0)
		return batterStats;

	batterStats.average = stof(fangraphsStandardRows[17]);
	batterStats.onBaseAverage = stof(fangraphsStandardRows[18]);
	batterStats.slugging = stof(fangraphsStandardRows[19]);
	batterStats.ops = batterStats.onBaseAverage + batterStats.slugging;
	batterStats.iso = stof(fangraphsStandardRows[15]);
	batterStats.woba = stof(fangraphsStandardRows[20]);
	if (batterStats.average > 0) {
		batterStats.wrcPlus = stof(fangraphsStandardRows[21]);
	}
	batterStats.strikeoutPercent = stof( fangraphsStandardRows[14].substr(0, fangraphsStandardRows[14].length() - 1));
	batterStats.walkPercent = stof(fangraphsStandardRows[13].substr(0, fangraphsStandardRows[13].length() - 1));
	batterStats.numPlateAppearances = atoi(fangraphsStandardRows[4].c_str());
    batterStats.rbisPerPA = stof(fangraphsStandardRows[10]) / batterStats.numPlateAppearances;
    batterStats.runsPerPA = stof(fangraphsStandardRows[9]) / batterStats.numPlateAppearances;
	return batterStats;
}
void FullSeasonStatsAdvancedNoHandedness::operator+=(const FullSeasonStatsAdvancedNoHandedness& other) {
	if (other.average >= 0 && average >= 0) {
		average += other.average;
		iso += other.iso;
		onBaseAverage += other.onBaseAverage;
		ops += other.ops;
		slugging += other.slugging;
		if (other.strikeoutPercent >= 0 && strikeoutPercent >= 0)
			strikeoutPercent += other.strikeoutPercent;
		if (other.walkPercent >= 0 && walkPercent >= 0)
			walkPercent += other.walkPercent;
		woba += other.woba;
		wrcPlus += other.wrcPlus;
        rbisPerPA += other.rbisPerPA;
        runsPerPA += other.runsPerPA;
	}
}
FullSeasonStatsAdvancedNoHandedness operator+(const FullSeasonStatsAdvancedNoHandedness& lhs, const FullSeasonStatsAdvancedNoHandedness& rhs) {
	FullSeasonStatsAdvancedNoHandedness newStats(rhs);
	newStats += lhs;
	return newStats;
}
void FullSeasonStatsAdvancedNoHandedness::operator*=(float rhs) {
	if (average >= 0) {
		average *= rhs;
		onBaseAverage *= rhs;
		slugging *= rhs;
		ops *= rhs;
		iso *= rhs;
		woba *= rhs;
		if (average > 0) {
			wrcPlus *= rhs;
		}
		if (strikeoutPercent >= 0)
			strikeoutPercent *= rhs;
		if (walkPercent >= 0)
			walkPercent *= rhs;
        rbisPerPA *= rhs;
        runsPerPA *= rhs;
	}
}
FullSeasonStatsAdvancedNoHandedness operator*(float floatFactor, const FullSeasonStatsAdvancedNoHandedness& stats) {
	FullSeasonStatsAdvancedNoHandedness newStats(stats);
	newStats *= floatFactor;
	return newStats;
}
FullSeasonStatsAdvancedNoHandedness operator*(const FullSeasonStatsAdvancedNoHandedness& stats, float floatFactor) {
	return floatFactor * stats;
}
bool FullSeasonStatsAdvancedNoHandedness::operator==(const FullSeasonStatsAdvancedNoHandedness& rhs) {
	if (abs(rhs.average - average) >= 0.0015f)
		return false;
	if (abs(rhs.onBaseAverage - onBaseAverage) >= 0.0015f)
		return false;
	if (abs(rhs.slugging - slugging) >= 0.0015f)
		return false;
	if (abs(rhs.ops - ops) >= 0.0015f)
		return false;
	if (abs(rhs.iso - iso) >= 0.0015f)
		return false;
	if (abs(rhs.woba - woba) >= 0.0015f)
		return false;
	if (abs(rhs.strikeoutPercent - strikeoutPercent) >= 0.15f)
		return false;
	if (abs(rhs.walkPercent - walkPercent) >= 0.15f)
		return false;
	if (abs(rhs.wrcPlus - wrcPlus) >= 0.5f)
		return false;
    if (abs(rhs.rbisPerPA - rbisPerPA) >= 0.0005f)
        return false;
    if (abs(rhs.runsPerPA - runsPerPA) >= 0.0005f)
        return false;
	return true;
}


//rotogrinder playerId
// dateUpTo = "2017-08-08"
FullSeasonPitcherStats GetPitcherCumulativeStatsUpTo(string playerId, CURL *curl, string dateUpTo, bool entireCareer)
{
	FullSeasonPitcherStats pitcherStats;
    pitcherStats.isLeftHanded = doesPlayerThrowLeftHanded(playerId, curl);

    string cachedDate = GetDateBeforeOrAfterNumDays(dateUpTo, 1);
    string cachedAtDateFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId + ".txt";
    string cachedAtDateFileContents = GetEntireFileContents(cachedAtDateFileName);
    if (cachedAtDateFileContents != "") {
        string rowTitle = ">Total<";
        if (!entireCareer) {
            rowTitle = ">" + cachedDate.substr(0,4) + "<";
        }
        vector<string> fangraphsStandardRows = GetFangraphsRowColumns(rowTitle, cachedAtDateFileContents, 18, "name=\"standard", "name=\"advanced", false);
        if (fangraphsStandardRows.size() == 0)
            return pitcherStats;
        
        pitcherStats.numInnings = stof(fangraphsStandardRows[1].c_str());
        pitcherStats.numInnings = floor(pitcherStats.numInnings) + ((pitcherStats.numInnings - floor(pitcherStats.numInnings)) * 3.4f);
        if (pitcherStats.numInnings > 0) {
            pitcherStats.era = stof(fangraphsStandardRows[2].c_str());
            float obpAllowed = stof(fangraphsStandardRows[15].c_str());
            float slgAllowed = stof(fangraphsStandardRows[16].c_str());
            pitcherStats.opsAllowed = obpAllowed + slgAllowed;
            pitcherStats.wobaAllowed = stof(fangraphsStandardRows[17].c_str());
            
            vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(rowTitle, cachedAtDateFileContents, 14, "name=\"advanced", "name=\"battedball\"", false);
            if (fangraphsAdvancedRows.size() == 0)
                return pitcherStats;
            pitcherStats.whip = stof(fangraphsAdvancedRows[9].c_str());
            pitcherStats.fip = stof(fangraphsAdvancedRows[12].c_str());
            pitcherStats.xfip = stof(fangraphsAdvancedRows[13].c_str());
            pitcherStats.strikeOutsPer9 = stof(fangraphsAdvancedRows[1].c_str());
        }
        return pitcherStats;
    }
    
	string fangraphsPlayerData = GetPlayerFangraphsPageDataCumulativeUpTo(playerId, curl, dateUpTo, false, entireCareer);
	string fangraphsPlayerDataAdvanced = GetPlayerFangraphsPageDataCumulativeUpTo(playerId, curl, dateUpTo, true, entireCareer);

	vector<string> fangraphsStandardRows = GetFangraphsRowColumns(">Total<", fangraphsPlayerData, 26, "id=\"DailyStats", "", false);
	if (fangraphsStandardRows.size() == 0)
		return pitcherStats;
	
	pitcherStats.numInnings = stof(fangraphsStandardRows[7].c_str());
	pitcherStats.numInnings = floor(pitcherStats.numInnings) + ((pitcherStats.numInnings - floor(pitcherStats.numInnings)) * 3.4f);
	if (pitcherStats.numInnings > 0) {
		pitcherStats.era = stof(fangraphsStandardRows[22].c_str());
		pitcherStats.fip = stof(fangraphsStandardRows[23].c_str());
		pitcherStats.xfip = stof(fangraphsStandardRows[24].c_str());
		pitcherStats.strikeOutsPer9 = stof(fangraphsStandardRows[15].c_str());

		vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(">Total<", fangraphsPlayerDataAdvanced, 17, "id=\"DailyStats", "", false);
		if (fangraphsAdvancedRows.size() == 0)
			return pitcherStats;
		pitcherStats.whip = stof(fangraphsAdvancedRows[11].c_str());
	}
	return pitcherStats;
}

FullSeasonStatsAdvanced GetPitcherAdvancedStats(string playerId, string yearString, CURL *curl)
{
	FullSeasonStatsAdvanced pitcherAdvancedStats;

	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, AdvancedStatsPitchingSplitsVersusLeftHand);
    vector<string> fangraphsStandardRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 18, "name=\"standard", "name=\"advanced", true);
    if (fangraphsStandardRows.size() == 0)
        return pitcherAdvancedStats;
    pitcherAdvancedStats.averageVersusLefty = stof(fangraphsStandardRows[14]);
    pitcherAdvancedStats.sluggingVersusLefty = stof(fangraphsStandardRows[16]);
    pitcherAdvancedStats.wobaVersusLefty = stof(fangraphsStandardRows[17]);
    pitcherAdvancedStats.isoVersusLefty = pitcherAdvancedStats.sluggingVersusLefty - pitcherAdvancedStats.averageVersusLefty;
    pitcherAdvancedStats.opsVersusLefty = stof(fangraphsStandardRows[15]) + pitcherAdvancedStats.sluggingVersusLefty;
    pitcherAdvancedStats.numPlateAppearancesVersusLefty = atoi(fangraphsStandardRows[3].c_str());

	fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, AdvancedStatsPitchingSplitsVersusRightHand);
    fangraphsStandardRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 18, "name=\"standard", "name=\"advanced", true);
    if (fangraphsStandardRows.size() == 0)
        return pitcherAdvancedStats;
    pitcherAdvancedStats.averageVersusRighty = stof(fangraphsStandardRows[14]);
    pitcherAdvancedStats.sluggingVersusRighty = stof(fangraphsStandardRows[16]);
    pitcherAdvancedStats.wobaVersusRighty = stof(fangraphsStandardRows[17]);
    pitcherAdvancedStats.isoVersusRighty = pitcherAdvancedStats.sluggingVersusRighty - pitcherAdvancedStats.averageVersusRighty;
    pitcherAdvancedStats.opsVersusRighty = stof(fangraphsStandardRows[15]) + pitcherAdvancedStats.sluggingVersusRighty;
    pitcherAdvancedStats.numPlateAppearancesVersusRighty = atoi(fangraphsStandardRows[3].c_str());

	return pitcherAdvancedStats;
}

FullSeasonStatsAdvanced GetPitcherCumulativeAdvancedStatsUpTo(std::string playerId, std::string dateUpTo, bool entireCareer) {
    FullSeasonStatsAdvanced pitcherAdvancedStats;
    
    string cachedDate = GetDateBeforeOrAfterNumDays(dateUpTo, 1);
    string cachedAtDateVersusLeftFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId + "VsLeft.txt";
    string cachedAtDateVersusLeftFileContents = GetEntireFileContents(cachedAtDateVersusLeftFileName);
    if (cachedAtDateVersusLeftFileContents != "") {
        string rowTitle = ">Total<";
        if (!entireCareer) {
            rowTitle = ">" + cachedDate.substr(0, 4) + "<";
        }
        
        vector<string> fangraphsStandardRows = GetFangraphsRowColumns(rowTitle, cachedAtDateVersusLeftFileContents, 18, "name=\"standard", "name=\"advanced", true);
        if (fangraphsStandardRows.size() == 0)
            return pitcherAdvancedStats;
        pitcherAdvancedStats.averageVersusLefty = stof(fangraphsStandardRows[14]);
        pitcherAdvancedStats.sluggingVersusLefty = stof(fangraphsStandardRows[16]);
        pitcherAdvancedStats.wobaVersusLefty = stof(fangraphsStandardRows[17]);
        pitcherAdvancedStats.isoVersusLefty = pitcherAdvancedStats.sluggingVersusLefty - pitcherAdvancedStats.averageVersusLefty;
        pitcherAdvancedStats.opsVersusLefty = stof(fangraphsStandardRows[15]) + pitcherAdvancedStats.sluggingVersusLefty;
        pitcherAdvancedStats.numPlateAppearancesVersusLefty = atoi(fangraphsStandardRows[3].c_str());
    }
    
    string cachedAtDateVersusRightFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId + "VsRight.txt";
    string cachedAtDateVersusRightFileContents = GetEntireFileContents(cachedAtDateVersusRightFileName);
    if (cachedAtDateVersusRightFileContents != "") {
        string rowTitle = ">Total<";
        if (!entireCareer) {
            rowTitle = ">" + cachedDate.substr(0, 4) + "<";
        }
        vector<string> fangraphsStandardRows = GetFangraphsRowColumns(rowTitle, cachedAtDateVersusRightFileContents, 18, "name=\"standard", "name=\"advanced", true);
        if (fangraphsStandardRows.size() == 0)
            return pitcherAdvancedStats;
        pitcherAdvancedStats.averageVersusRighty = stof(fangraphsStandardRows[14]);
        pitcherAdvancedStats.sluggingVersusRighty = stof(fangraphsStandardRows[16]);
        pitcherAdvancedStats.wobaVersusRighty = stof(fangraphsStandardRows[17]);
        pitcherAdvancedStats.isoVersusRighty = pitcherAdvancedStats.sluggingVersusRighty - pitcherAdvancedStats.averageVersusRighty;
        pitcherAdvancedStats.opsVersusRighty = stof(fangraphsStandardRows[15]) + pitcherAdvancedStats.sluggingVersusRighty;
        pitcherAdvancedStats.numPlateAppearancesVersusRighty = atoi(fangraphsStandardRows[3].c_str());
    }
    return pitcherAdvancedStats;
}

FullSeasonStatsAdvanced GetBatterAdvancedStats(string playerId, string yearString, CURL *curl)
{
	FullSeasonStatsAdvanced batterAdvancedStats;

	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, AdvancedStatsBattingSplitsVersusLeftHand);
	vector<string> fangraphsStandardRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 21, "name=\"standard\"", "name=\"advanced\"", false);
	if (fangraphsStandardRows.size() == 21) {
		batterAdvancedStats.numPlateAppearancesVersusLefty = atoi(fangraphsStandardRows[3].c_str());
		batterAdvancedStats.averageVersusLefty = stof(fangraphsStandardRows[20]);
	}
	vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 14, "name=\"advanced\"", "name=\"battedball\"", false);
	if (fangraphsAdvancedRows.size() == 14) {
		batterAdvancedStats.sluggingVersusLefty = stof(fangraphsAdvancedRows[6]);
			batterAdvancedStats.isoVersusLefty = stof(fangraphsAdvancedRows[8]);
			batterAdvancedStats.opsVersusLefty = stof(fangraphsAdvancedRows[7]);
			batterAdvancedStats.wobaVersusLefty = stof(fangraphsAdvancedRows[12]);
	}

	fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != CURRENT_YEAR, AdvancedStatsBattingSplitsVersusRightHand);
	fangraphsStandardRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 21, "name=\"standard\"", "name=\"advanced\"", false);
	if (fangraphsStandardRows.size() == 21) {
		batterAdvancedStats.numPlateAppearancesVersusRighty = atoi(fangraphsStandardRows[3].c_str());
		batterAdvancedStats.averageVersusRighty = stof(fangraphsStandardRows[20]);
	}
	fangraphsAdvancedRows = GetFangraphsRowColumns(">" + yearString + "<", fangraphsPlayerData, 14, "name=\"advanced\"", "name=\"battedball\"", false);
	if (fangraphsAdvancedRows.size() == 14) {
		batterAdvancedStats.sluggingVersusRighty = stof(fangraphsAdvancedRows[6]);
		batterAdvancedStats.isoVersusRighty = stof(fangraphsAdvancedRows[8]);
		batterAdvancedStats.opsVersusRighty = stof(fangraphsAdvancedRows[7]);
		batterAdvancedStats.wobaVersusRighty = stof(fangraphsAdvancedRows[12]);
	}

	return batterAdvancedStats;
}

void RemovePreCurrentYearTablesFromString(string& fangraphsData) {
	int removingYear = atoi(CURRENT_YEAR);
    removingYear--;
    int consecutiveEmptyYears = 0;
	while (true) {
		char removeYearStringC[5];
		itoa(removingYear, removeYearStringC, 10);
		string removeYearString = ">";
		removeYearString += removeYearStringC;
		removeYearString += "<";
		size_t yearIndex = fangraphsData.find(removeYearString);
        if (yearIndex == string::npos) {
            consecutiveEmptyYears++;
            if (consecutiveEmptyYears >= 3)
                break;
        } else {
            consecutiveEmptyYears = 0;
            while (yearIndex != string::npos) {
                size_t rowStartIndex = fangraphsData.rfind("<tr ", yearIndex);
                size_t rowEndIndex = fangraphsData.find("</tr>", yearIndex);
                if (rowStartIndex == string::npos || rowEndIndex == string::npos) {
                    yearIndex = fangraphsData.find(removeYearString, yearIndex + 1);
                }
                else {
                    fangraphsData.erase(rowStartIndex, rowEndIndex + 5 - rowStartIndex);
                    yearIndex = fangraphsData.find(removeYearString);
                }
            }
        }
		removingYear--;
	}
}

string GetPlayerFangraphsPageData(string playerId, CURL *curl, bool bCachedOk, int advancedStatsFlags)
{
	if (skipStatsCollection)
		return "";
	string fangraphsData = "";
	string cachedFileName = "FangraphsCachedPages\\PlayerId" + playerId + ".txt";
	if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand) ||
		(advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand))
		cachedFileName = "FangraphsCachedPages\\PlayerId" + playerId + "VsLeft.txt";
	else if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusRightHand) ||
		(advancedStatsFlags & AdvancedStatsPitchingSplitsVersusRightHand))
		cachedFileName = "FangraphsCachedPages\\PlayerId" + playerId + "VsRight.txt";

	fangraphsData = GetEntireFileContents(cachedFileName);
#if PLATFORM_OSX
    cachedFileName = GetPlatformCompatibleFileNameFromRelativePath(cachedFileName);
#endif
    
	if (!bCachedOk)
	{
		size_t dateMetaDataIndex = fangraphsData.find("/ZachDateMetaData", 0);
		if (dateMetaDataIndex == string::npos || fangraphsData.substr(0, dateMetaDataIndex) < todaysDate)
			fangraphsData = "";
	}

	if (fangraphsData == "")
	{
		if (curl == NULL)
			curl = curl_easy_init();

		string playerRotoGuruData = GetPlayerStatsRawString(playerId, "any", curl);

		size_t fangraphsURLIndexStart = playerRotoGuruData.find("www.fangraphs.com", 0);
		fangraphsURLIndexStart = playerRotoGuruData.rfind("\"", fangraphsURLIndexStart);
		size_t fangraphsURLIndexEnd = playerRotoGuruData.find("\" ", fangraphsURLIndexStart + 1);
		string fangraphsURL = playerRotoGuruData.substr(fangraphsURLIndexStart + 1, fangraphsURLIndexEnd - fangraphsURLIndexStart - 1);
		fangraphsURL = ReplaceURLWhiteSpaces(fangraphsURL);
		if (fangraphsURL.find("?letter=") != string::npos) {
			string writeBuffer;
			CurlGetSiteContents(curl, fangraphsURL, writeBuffer, true);

			string playerName = GetSubStringBetweenStrings(playerRotoGuruData, "<TITLE>", "</TITLE>");
			size_t nameIndex = writeBuffer.find(ConvertLFNameToFLName(playerName));
			if (nameIndex != string::npos) {
				size_t firstRedirectIndex = writeBuffer.rfind("statss.aspx?playerid=", nameIndex);
				if (firstRedirectIndex != string::npos) {
					size_t nextQuoteIndex = writeBuffer.find("\"", firstRedirectIndex + 1);
					fangraphsURL = "https://www.fangraphs.com/" + writeBuffer.substr(firstRedirectIndex, nextQuoteIndex - firstRedirectIndex);
				}
			}

		}

		curl_easy_setopt(curl, CURLOPT_URL, fangraphsURL.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		if (advancedStatsFlags == 0)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
		}
		else
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
		}
		curl_easy_perform(curl);
		char* finalFangraphsUrlCStr;
		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &finalFangraphsUrlCStr);
		string finalFangraphsURL = finalFangraphsUrlCStr;
		curl_easy_reset(curl);

		if (advancedStatsFlags != 0)
		{
			if (advancedStatsFlags & AdvancedStatsPitchingStarterStatsOnly)
			{
				size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
				finalFangraphsURL.erase(statsIndex + 5, 1);
				finalFangraphsURL.insert(statsIndex + 5, "plits");
				finalFangraphsURL += "&season=0";
				finalFangraphsURL += "&split=8.1";
				curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
			else if ((advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand)
				|| (advancedStatsFlags & AdvancedStatsPitchingSplitsVersusRightHand))
			{
				size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
				finalFangraphsURL.erase(statsIndex + 5, 1);
				finalFangraphsURL.insert(statsIndex + 5, "plits");
				finalFangraphsURL += "&season=0";
				if (advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand)
					finalFangraphsURL += "&split=0.1";
				else
					finalFangraphsURL += "&split=0.2";
				curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
			else if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand)
				|| (advancedStatsFlags & AdvancedStatsBattingSplitsVersusRightHand))
			{
				size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
				finalFangraphsURL.erase(statsIndex + 5, 1);
				finalFangraphsURL.insert(statsIndex + 5, "plits");
				finalFangraphsURL += "&season=0";
				if (advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand)
					finalFangraphsURL += "&split=0.5";
				else
					finalFangraphsURL += "&split=0.6";
				curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
		}

		if (fangraphsData.find("You have encountered an unexpected error.  If the problem persists, please fill out a") == string::npos)
		{
			size_t writeToFileIndexBegin = fangraphsData.find("text/javascript", 0);
			if (writeToFileIndexBegin == string::npos)
				writeToFileIndexBegin = 0;
			size_t writeToFileLength = fangraphsData.find("BEGIN FIRSTIMPRESSION TAG", writeToFileIndexBegin);
			if (writeToFileLength != string::npos)
				writeToFileLength -= writeToFileIndexBegin;
			ofstream writeToFile;
			writeToFile.open(cachedFileName);
			writeToFile << todaysDate << "/ZachDateMetaData" << endl;
			string finalWriteString = fangraphsData.substr(writeToFileIndexBegin, writeToFileLength);
			size_t startIndex = finalWriteString.find("class=\"player-info-box\"");
			if (startIndex == string::npos) {
				startIndex = finalWriteString.find("Birthdate:");
				if (startIndex != string::npos && startIndex > 350)
					startIndex -= 350;
			}
			if (startIndex == string::npos)
				startIndex = 0;
			size_t endIndex = finalWriteString.find("\"footer", startIndex);
			finalWriteString = finalWriteString.substr(startIndex, endIndex - startIndex);
			RemoveJavaScriptBlocksFromFileText(finalWriteString);
			size_t firstDashboardSection = finalWriteString.find("name=\"dashboard");
			if (firstDashboardSection != string::npos) {
				finalWriteString = finalWriteString.substr(firstDashboardSection);
			} else {
				size_t firstStandardSection = finalWriteString.find("name=\"standard");
				if (firstStandardSection != string::npos) {
					finalWriteString = finalWriteString.substr(firstStandardSection);
				}
			}
			size_t firstFieldingSection = finalWriteString.find("name=\"fielding");
			if (firstFieldingSection != string::npos)
				finalWriteString = finalWriteString.substr(0, firstFieldingSection);
			RemoveAllSectionsWithKeyword(finalWriteString, "<col  />", "<colgroup>", "</colgroup>");
			RemoveAllSectionsWithKeyword(finalWriteString, "grid_average", "<tr ", "</tr>");
            RemoveAllSectionsWithKeyword(finalWriteString, "grid_minors_show", "<tr ", "</tr>");
			RemoveAllSectionsWithKeyword(finalWriteString, "grid_multi", "<tr ", "</tr>");
			RemoveAllSectionsWithKeyword(finalWriteString, "grid_projections", "<tr ", "</tr>");
			RemoveAllSectionsWithKeyword(finalWriteString, "grid_postseason", "<tr ", "</tr>");
			RemoveAllSectionsWithKeyword(finalWriteString, "href=\"http", "<", ">");
			RemoveAllSectionsWithKeyword(finalWriteString, "javascript:doNothing", "<", ">");
			RemoveAllSectionsWithKeyword(finalWriteString, "href =\"javascript", "<a ", "</a>");
			RemoveAllSectionsWithKeyword(finalWriteString, "class=\"grid_line_regular\"", "", "");
			RemoveAllSectionsWithKeyword(finalWriteString, "align=\"right\"", "", "");
			RemoveAllSectionsWithKeyword(finalWriteString, "<input id=\"", "", "</div>");
            RemoveAllSectionsWithKeyword(finalWriteString, "scope=\"col\" class=\"rgHeader\"", "", "");
            RemoveAllSectionsWithKeyword(finalWriteString, "style=\"", "", ";\"");
            RemoveAllSectionsWithKeyword(finalWriteString, "href=\"javascript:", "", ";\"");
            RemoveAllSectionsWithKeyword(finalWriteString, "scope=\"col\" class=\"grid_line_breakh", "", "");
            RemoveAllSectionsWithKeyword(finalWriteString, "class=\"", "", "\"");
            RemoveAllSectionsWithKeyword(finalWriteString, "id=\"SeasonStats", "", "\"");
            RemoveAllSectionsWithKeyword(finalWriteString, "href=\"#", "", "\"");
            RemoveAllSectionsWithKeyword(finalWriteString, "<input type=\"hidden\"/>", "", "");
            
			writeToFile << finalWriteString;
			writeToFile.close();
            
            fangraphsData = finalWriteString;
            

			size_t fileNamePlayerIdIndex = cachedFileName.find("\\PlayerId");
#if PLATFORM_OSX
            if (fileNamePlayerIdIndex == string::npos) {
                fileNamePlayerIdIndex = cachedFileName.find("/PlayerId");
            }
#endif
			if (fileNamePlayerIdIndex != string::npos) {
#if PLATFORM_OSX
                cachedFileName.insert(fileNamePlayerIdIndex, "/CachedAtDate/" + todaysDate);
                string directory = cachedFileName.substr(0, cachedFileName.find_last_of("/"));
                mkdir(directory.c_str(), ACCESSPERMS);
#else
                cachedFileName.insert(fileNamePlayerIdIndex, "\\CachedAtDate\\" + todaysDate);
				string directory = cachedFileName.substr(0, cachedFileName.find_last_of("\\"));
				_mkdir(directory.c_str());
#endif
                RemovePreCurrentYearTablesFromString(finalWriteString);
				ofstream cachedAtDateFile;
				cachedAtDateFile.open(cachedFileName);
				cachedAtDateFile << finalWriteString;
				cachedAtDateFile.close();
			}
		}
		else
		{
			fangraphsData = "";
		}
	} 
#if 0
	else {
		size_t startIndex = fangraphsData.find("class=\"player-info-box\"");
		if (startIndex == string::npos) {
			startIndex = fangraphsData.find("Birthdate:");
			if (startIndex != string::npos && startIndex > 350)
				startIndex -= 350;
		}
		if (startIndex == string::npos)
			startIndex = 0;
		size_t endIndex = fangraphsData.find("\"footer", startIndex);
		size_t dateMetaDataIndex = fangraphsData.find("/ZachDateMetaData", 0);
		if (dateMetaDataIndex != string::npos)
			dateMetaDataIndex = fangraphsData.find("\n", dateMetaDataIndex);
		string finalWriteString = "";
		if (dateMetaDataIndex != string::npos)
			finalWriteString += fangraphsData.substr(0, dateMetaDataIndex);
		finalWriteString += "\n";
		finalWriteString += fangraphsData.substr(startIndex, endIndex - startIndex);
		RemoveJavaScriptBlocksFromFileText(finalWriteString);
		ofstream writeToFile;
		writeToFile.open(cachedFileName);
		writeToFile << finalWriteString;
		writeToFile.close();
	}
#endif

	return fangraphsData;
}

// playerId = rotogrinders player id
// dateUpTo = "2017-08-08"
string GetPlayerFangraphsPageDataCumulativeUpTo(string playerId, CURL *curl, string dateUpTo, bool advancedPage, bool entireCareer)
{
	dateUpTo = DateToDateWithDashes(dateUpTo);

	string fangraphsData = "";
	string cachedFileName = "FangraphsCachedPages\\CumulativeUpTo\\PlayerId" + playerId + (entireCareer ? "Career" : "") + "UpTo" + dateUpTo;
	if (advancedPage)
		cachedFileName += "Advanced";
	cachedFileName += ".txt";
	fangraphsData = GetEntireFileContents(cachedFileName);
#if PLATFORM_OSX
    cachedFileName = GetPlatformCompatibleFileNameFromRelativePath(cachedFileName);
#endif

	if (fangraphsData == "")
	{
		if (curl == NULL)
			curl = curl_easy_init();

		string playerRotoGuruData = GetPlayerStatsRawString(playerId, "any", curl);

		size_t fangraphsURLIndexStart = playerRotoGuruData.find("www.fangraphs.com", 0);
		fangraphsURLIndexStart = playerRotoGuruData.rfind("\"", fangraphsURLIndexStart);
		size_t fangraphsURLIndexEnd = playerRotoGuruData.find("\" ", fangraphsURLIndexStart + 1);
		string fangraphsURL = playerRotoGuruData.substr(fangraphsURLIndexStart + 1, fangraphsURLIndexEnd - fangraphsURLIndexStart - 1);
		fangraphsURL = ReplaceURLWhiteSpaces(fangraphsURL);
		curl_easy_setopt(curl, CURLOPT_URL, fangraphsURL.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
		
		curl_easy_perform(curl);
		char* finalFangraphsUrlCStr;
		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &finalFangraphsUrlCStr);
		string finalFangraphsURL = finalFangraphsUrlCStr;
		curl_easy_reset(curl);
        

		size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
		finalFangraphsURL.erase(statsIndex + 5, 1);
		finalFangraphsURL.insert(statsIndex + 5, "d");
		finalFangraphsURL += "&type=";
		if (advancedPage)
			finalFangraphsURL += "2";
		else
			finalFangraphsURL += "0";
		string beginningDateWithDashes = dateUpTo.substr(0, 4) + "-04-00";
		if (entireCareer) {
			beginningDateWithDashes = "2006-04-01";
		}
		finalFangraphsURL += "&gds=" + beginningDateWithDashes + "&gde=" + dateUpTo;
		curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		size_t writeToFileIndexBegin = fangraphsData.find("id=\"DailyStats", 0);
		if (writeToFileIndexBegin == string::npos)
			writeToFileIndexBegin = 0;
		size_t writeToFileIndexEnd = string::npos;
		size_t fileTotalsIndex = fangraphsData.find(">Total<", writeToFileIndexBegin);
		if (fileTotalsIndex != string::npos) {
            
			writeToFileIndexEnd = fangraphsData.find("id=\"DailyStats", fileTotalsIndex);
            size_t theadIndex = fangraphsData.rfind("<thead", fileTotalsIndex);
            if (theadIndex != string::npos) {
                writeToFileIndexBegin = theadIndex;
            }
		}
		else {
			writeToFileIndexEnd = fangraphsData.find("id=\"footer\"", writeToFileIndexBegin);
		}
		size_t writeToFileLength = string::npos;
		if (writeToFileIndexEnd != string::npos) {
			writeToFileLength = writeToFileIndexEnd - writeToFileIndexBegin;
		}
		ofstream writeToFile;
		writeToFile.open(cachedFileName);
        if (writeToFileIndexBegin < fangraphsData.size())
            writeToFile << fangraphsData.substr(writeToFileIndexBegin, writeToFileLength);
        else
            writeToFile << "No Data";
		writeToFile.close();
	}

	return fangraphsData;
}

BattedBallProfile GetBattedBallProfileFromBattedBallProfileRows(string fangraphsData, string rowTitle) {
	BattedBallProfile profile;
	vector<string> fangraphsBattedBallRows = GetFangraphsRowColumns(rowTitle, fangraphsData, 15, "name=\"battedball\"", "", true);
	if (fangraphsBattedBallRows.size() == 0)
		return profile;
	for (unsigned int i = 0; i < fangraphsBattedBallRows.size(); ++i) {
		size_t percentIndex = fangraphsBattedBallRows[i].find(" %");
		if (percentIndex != string::npos) {
			fangraphsBattedBallRows[i] = fangraphsBattedBallRows[i].substr(0, percentIndex);
		}
	}
	profile.homerunPerFlyBallPercent = stof(fangraphsBattedBallRows[6]);
	profile.softPercent = stof(fangraphsBattedBallRows[12]);
	profile.mediumPercent = stof(fangraphsBattedBallRows[13]);
	profile.hardPercent = stof(fangraphsBattedBallRows[14]);
	return profile;
}

BattedBallProfile GetPlayerBattedBallProfile(std::string playerId, std::string year, CURL *curl, int advancedStatsFlags) {
	string fangraphsData = GetPlayerFangraphsPageData(playerId, curl, year != CURRENT_YEAR, advancedStatsFlags);
	year = ">" + year + "<";
	return GetBattedBallProfileFromBattedBallProfileRows(fangraphsData, year);
}

BattedBallProfile GetPlayerCumulativeBattedBallProfileUpTo(std::string playerId, std::string dateUpTo, bool entireCareer, int advancedStatsFlags) {
	FullSeasonStatsAdvanced batterAdvancedStats;

	string cachedDate = GetDateBeforeOrAfterNumDays(dateUpTo, 1);
	string cachedAtDateFileName = "FangraphsCachedPages\\CachedAtDate\\" + cachedDate + "\\PlayerId" + playerId;
	if (advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand) {
		cachedAtDateFileName += "VsLeft";
	}
	if (advancedStatsFlags & AdvancedStatsBattingSplitsVersusRightHand) {
		cachedAtDateFileName += "VsRight";
	}
	cachedAtDateFileName += ".txt";
	string cachedAtDateFileContents = GetEntireFileContents(cachedAtDateFileName);
	if (cachedAtDateFileContents != "") {
		string rowTitle = ">Total<";
		if (!entireCareer) {
			rowTitle = ">" + cachedDate.substr(0, 4) + "<";
		}
		return GetBattedBallProfileFromBattedBallProfileRows(cachedAtDateFileContents, rowTitle);
	}
	BattedBallProfile bpp;
	return bpp;
}

FullSeasonPitcherStats::FullSeasonPitcherStats(std::string inString)
{
	vector<string> statLines = SplitStringIntoMultiple(inString, ";");
	if (statLines.size() == 8 || (statLines.size() == 9 && statLines[8] == ""))
	{
		era = stof(statLines[0]);
		fip = stof(statLines[1]);
		xfip = stof(statLines[2]);
		strikeOutsPer9 = stof(statLines[3]);
		numInnings = stof(statLines[4]);
		whip = stof(statLines[5]);
		wobaAllowed = stof(statLines[6]);
		opsAllowed = stof(statLines[7]);
	}
}

string FullSeasonPitcherStats::ToString()
{
	//era; fip; xfip; strikeOutsPer9; numInnings; whip; wobaAllowed; opsAllowed;
	return to_string(era) + ";" + to_string(fip) + ";" + to_string(xfip) + ";" + to_string(strikeOutsPer9) + ";" + to_string(numInnings) + ";" + to_string(whip) + ";" + to_string(wobaAllowed) + ";" + to_string(opsAllowed) + ";";
}

bool FullSeasonPitcherStats::operator==(const FullSeasonPitcherStats& rhs)
{
	if (abs(rhs.era - era) > 0.0015f)
		return false;
	if (abs(rhs.fip - fip) > 0.0015f)
		return false;
	if (abs(rhs.xfip - xfip) > 0.0015f)
		return false;
	if (abs(rhs.numInnings - numInnings) > 0.015f)
		return false;
	if (abs(rhs.strikeOutsPer9 - strikeOutsPer9) > 0.015f)
		return false;
	if (abs(rhs.whip - whip) > 0.015f)
		return false;
	if (abs(rhs.opsAllowed - opsAllowed) > 0.0015f)
		return false;
	if (abs(rhs.wobaAllowed - wobaAllowed) > 0.0015f)
		return false;
	return true;
}
void FullSeasonPitcherStats::operator*=(float rhs)
{
	if (strikeOutsPer9 >= 0) {
		era *= rhs;
		fip *= rhs;
		strikeOutsPer9 *= rhs;
		numInnings *= rhs;
		whip *= rhs;
		xfip *= rhs;
		wobaAllowed *= rhs;
		opsAllowed *= rhs;
	}
}
FullSeasonPitcherStats operator*(float floatFactor, const FullSeasonPitcherStats& stats) {
	FullSeasonPitcherStats newStats(stats);
	newStats *= floatFactor;
	return newStats;
}
FullSeasonPitcherStats operator*(const FullSeasonPitcherStats& stats, float floatFactor) {
	return floatFactor * stats;
}
void FullSeasonPitcherStats::operator+=(const FullSeasonPitcherStats& other) {
	if (strikeOutsPer9 >= 0 && other.strikeOutsPer9 >= 0) {
		era += other.era;
		fip += other.fip;
		strikeOutsPer9 += other.strikeOutsPer9;
		numInnings += other.numInnings;
		whip += other.whip;
		xfip += other.xfip;
		wobaAllowed += other.wobaAllowed;
		opsAllowed += other.opsAllowed;
	}
}
FullSeasonPitcherStats operator+(const FullSeasonPitcherStats& lhs, const FullSeasonPitcherStats& rhs) {
	FullSeasonPitcherStats newStats(lhs);
	newStats += rhs;
	return newStats;
}

bool FullSeasonStatsAdvanced::operator==(const FullSeasonStatsAdvanced& rhs)
{
	if (abs(rhs.averageVersusLefty - averageVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.averageVersusRighty - averageVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.isoVersusLefty - isoVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.isoVersusRighty - isoVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.opsVersusLefty - opsVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.opsVersusRighty - opsVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.sluggingVersusLefty - sluggingVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.sluggingVersusRighty - sluggingVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.wobaVersusLefty - wobaVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.wobaVersusRighty - wobaVersusRighty) > 0.0015f)
		return false;
	if (rhs.numPlateAppearancesVersusLefty != numPlateAppearancesVersusLefty)
		return false;
	if (rhs.numPlateAppearancesVersusRighty != numPlateAppearancesVersusRighty)
		return false;
	return true;
}

void FullSeasonStatsAdvanced::operator*=(float rhs)
{
	if (opsVersusLefty >= 0) {
		opsVersusLefty *= rhs;
		isoVersusLefty *= rhs;
		averageVersusLefty *= rhs;
		sluggingVersusLefty *= rhs;
		wobaVersusLefty *= rhs;
	}
	if (opsVersusRighty >= 0) {
		opsVersusRighty *= rhs;
		isoVersusRighty *= rhs;
		averageVersusRighty *= rhs;
		sluggingVersusRighty *= rhs;
		wobaVersusRighty *= rhs;
	}
}
FullSeasonStatsAdvanced operator*(float floatFactor, const FullSeasonStatsAdvanced& stats) {
	FullSeasonStatsAdvanced newStats(stats);
	newStats *= floatFactor;
	return newStats;
}
FullSeasonStatsAdvanced operator*(const FullSeasonStatsAdvanced& stats, float floatFactor) {
	return floatFactor * stats;
}
void FullSeasonStatsAdvanced::operator+=(const FullSeasonStatsAdvanced& rhs)
{
	if (opsVersusLefty >= 0 && rhs.opsVersusLefty >= 0) {
		opsVersusLefty += rhs.opsVersusLefty;
		isoVersusLefty += rhs.isoVersusLefty;
		averageVersusLefty += rhs.averageVersusLefty;
		sluggingVersusLefty += rhs.sluggingVersusLefty;
		wobaVersusLefty += rhs.wobaVersusLefty;
	}
	if (opsVersusRighty >= 0 && rhs.opsVersusRighty >= 0) {
		opsVersusRighty += rhs.opsVersusRighty;
		isoVersusRighty += rhs.isoVersusRighty;
		averageVersusRighty += rhs.averageVersusRighty;
		sluggingVersusRighty += rhs.sluggingVersusRighty;
		wobaVersusRighty += rhs.wobaVersusRighty;
	}
}
FullSeasonStatsAdvanced operator+(const FullSeasonStatsAdvanced& lhs, const FullSeasonStatsAdvanced& rhs) {
	FullSeasonStatsAdvanced newStats(lhs);
	newStats += rhs;
	return newStats;
}

bool FullSeasonStats::operator==(const FullSeasonStats& rhs)
{
	if (abs(rhs.averagePpg - averagePpg) > 0.15f)
		return false;
	if (abs(rhs.averagePpgVsLefty - averagePpgVsLefty) > 0.15f)
		return false;
	if (abs(rhs.averagePpgVsRighty - averagePpgVsRighty) > 0.0015f)
		return false;
	if (abs(rhs.totalGamesStarted - totalGamesStarted) > 0.5f)
		return false;
	return true;
}

bool BattedBallProfile::operator==(const BattedBallProfile& rhs) {
	if (abs(homerunPerFlyBallPercent - rhs.homerunPerFlyBallPercent) > 0.1)
		return false;
	if (abs(softPercent - rhs.softPercent) > 0.1f)
		return false;
	if (abs(mediumPercent- rhs.mediumPercent) > 0.1f)
		return false;
	if (abs(hardPercent - rhs.hardPercent) > 0.1f)
		return false;
	return true;
}
void BattedBallProfile::operator+=(const BattedBallProfile& other) {
	if (softPercent >= 0 && other.softPercent >= 0) {
		homerunPerFlyBallPercent += other.homerunPerFlyBallPercent;
		softPercent += other.softPercent;
		mediumPercent += other.mediumPercent;
		hardPercent += other.hardPercent;
	}
}
void BattedBallProfile::operator*=(float rhs) {
	if (softPercent >= 0) {
		homerunPerFlyBallPercent *= rhs;
		softPercent *= rhs;
		mediumPercent *= rhs;
		hardPercent *= rhs;
	}
}

BattedBallProfile operator*(float floatFactor, const BattedBallProfile& stats) {
	BattedBallProfile bbp(stats);
	bbp *= floatFactor;
	return bbp;
}
BattedBallProfile operator*(const BattedBallProfile& stats, float floatFactor) {
	BattedBallProfile bbp(stats);
	bbp *= floatFactor;
	return bbp;
}
BattedBallProfile operator+(const BattedBallProfile& lhs, const BattedBallProfile& rhs) {
	BattedBallProfile bbp(lhs);
	bbp += rhs;
	return bbp;
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
