/*
// baseball ref search is deprecated (unreliable)  use fangraphs from now on
string GetPlayerBbrefPageData(string playerId, CURL *curl, bool bCachedOk, int advancedStatsFlags);

FullSeasonPitcherStats GetPitcher2017Stats(string playerId, CURL *curl)
{
	if (curl == NULL)
		curl = curl_easy_init();

	FullSeasonPitcherStats pitcher2017Stats;

	string bbrefPlayerData = GetPlayerBbrefPageData(playerId, curl, false, AdvancedStatsPitching);
	size_t bbrefCurrentIndex = bbrefPlayerData.find("pitching_standard.2017", 0);
	if (bbrefCurrentIndex != string::npos)
	{
		// Name GID fip era k/9 oppR/G oppK/9 FDPcorrected
		bbrefCurrentIndex = bbrefPlayerData.find("data-stat=\"earned_run_avg\"", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find(">", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find("/", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.rfind(">", bbrefCurrentIndex);
		size_t bbrefNextIndex = bbrefPlayerData.find("<", bbrefCurrentIndex);
		pitcher2017Stats.era = stof(bbrefPlayerData.substr(bbrefCurrentIndex + 1, bbrefNextIndex - bbrefCurrentIndex - 1).c_str());

		bbrefCurrentIndex = bbrefPlayerData.find("data-stat=\"fip\"", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find(">", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find("/", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.rfind(">", bbrefCurrentIndex);
		bbrefNextIndex = bbrefPlayerData.find("<", bbrefCurrentIndex);
		pitcher2017Stats.fip = stof(bbrefPlayerData.substr(bbrefCurrentIndex + 1, bbrefNextIndex - bbrefCurrentIndex - 1).c_str());

		bbrefCurrentIndex = bbrefPlayerData.find("data-stat=\"strikeouts_per_nine\"", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find(">", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find("/", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.rfind(">", bbrefCurrentIndex);
		bbrefNextIndex = bbrefPlayerData.find("<", bbrefCurrentIndex);
		pitcher2017Stats.strikeOutsPer9 = stof(bbrefPlayerData.substr(bbrefCurrentIndex + 1, bbrefNextIndex - bbrefCurrentIndex - 1).c_str());
	}
	return pitcher2017Stats;
}

FullSeasonPitcherStats GetPitcher2016Stats(string playerId, CURL *curl)
{
	if (curl == NULL)
		curl = curl_easy_init();

	FullSeasonPitcherStats pitcher2016Stats;

	string bbrefPlayerData = GetPlayerBbrefPageData(playerId, curl, true, AdvancedStatsPitching);
	size_t bbrefCurrentIndex = bbrefPlayerData.find("pitching_standard.2016", 0);
	if (bbrefCurrentIndex != string::npos)
	{
		// Name GID fip era k/9 oppR/G oppK/9 FDPcorrected
		bbrefCurrentIndex = bbrefPlayerData.find("data-stat=\"earned_run_avg\"", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find(">", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find("/", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.rfind(">", bbrefCurrentIndex);
		size_t bbrefNextIndex = bbrefPlayerData.find("<", bbrefCurrentIndex);
		pitcher2016Stats.era = stof(bbrefPlayerData.substr(bbrefCurrentIndex + 1, bbrefNextIndex - bbrefCurrentIndex - 1).c_str());

		bbrefCurrentIndex = bbrefPlayerData.find("data-stat=\"fip\"", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find(">", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find("/", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.rfind(">", bbrefCurrentIndex);
		bbrefNextIndex = bbrefPlayerData.find("<", bbrefCurrentIndex);
		pitcher2016Stats.fip = stof(bbrefPlayerData.substr(bbrefCurrentIndex + 1, bbrefNextIndex - bbrefCurrentIndex - 1).c_str());

		bbrefCurrentIndex = bbrefPlayerData.find("data-stat=\"strikeouts_per_nine\"", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find(">", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.find("/", bbrefCurrentIndex);
		bbrefCurrentIndex = bbrefPlayerData.rfind(">", bbrefCurrentIndex);
		bbrefNextIndex = bbrefPlayerData.find("<", bbrefCurrentIndex);
		pitcher2016Stats.strikeOutsPer9 = stof(bbrefPlayerData.substr(bbrefCurrentIndex + 1, bbrefNextIndex - bbrefCurrentIndex - 1).c_str());
	}
	return pitcher2016Stats;
}

string GetPlayerBbrefPageData(string playerId, CURL *curl, bool bCachedOk, int advancedStatsFlags)
{
	if (curl == NULL)
		curl = curl_easy_init();

	string bbrefPlayerData = "";
	bbrefPlayerData = GetEntireFileContents("BaseballReferenceCachedPages\\PlayerId" + playerId + ".txt");

	if (!bCachedOk)
	{
		size_t dateMetaDataIndex = bbrefPlayerData.find("/ZachDateMetaData", 0);
		if (dateMetaDataIndex == string::npos || bbrefPlayerData.substr(0, dateMetaDataIndex) < todaysDate)
			bbrefPlayerData = "";
	}

	if (bbrefPlayerData == "")
	{
		string rotoguruPlayerIdURL = "http://rotoguru1.com/cgi-bin/player16.cgi?" + playerId + "x";
		string playerRotoGuruData;
		curl_easy_setopt(curl, CURLOPT_URL, rotoguruPlayerIdURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &playerRotoGuruData);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		size_t bbrefURLIndexStart = playerRotoGuruData.find("www.baseball-reference.com", 0);
		bbrefURLIndexStart = playerRotoGuruData.rfind("\"", bbrefURLIndexStart);
		size_t bbrefURLIndexEnd = playerRotoGuruData.find("\" ", bbrefURLIndexStart + 1);
		string bbrefURL = playerRotoGuruData.substr(bbrefURLIndexStart + 1, bbrefURLIndexEnd - bbrefURLIndexStart - 1);
		bbrefURL = ReplaceURLWhiteSpaces(bbrefURL);

		curl_easy_setopt(curl, CURLOPT_URL, bbrefURL.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		if (advancedStatsFlags == 0)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bbrefPlayerData);
		}
		else
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
		}
		curl_easy_perform(curl);
		char* finalBbUrlCStr;
		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &finalBbUrlCStr);
		string finalBbURL = finalBbUrlCStr;
		curl_easy_reset(curl);

		if (advancedStatsFlags != 0)
		{
			if (advancedStatsFlags == AdvancedStatsPitching)
			{
				size_t shtmlIndex = finalBbURL.find(".shtml", 0);
				finalBbURL = finalBbURL.substr(0, shtmlIndex);
				finalBbURL += "-pitch.shtml";
				curl_easy_setopt(curl, CURLOPT_URL, finalBbURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bbrefPlayerData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
			else if (advancedStatsFlags == AdvancedStatsBatting)
			{
				size_t shtmlIndex = finalBbURL.find(".shtml", 0);
				finalBbURL = finalBbURL.substr(0, shtmlIndex);
				finalBbURL += "-bat.shtml";
				curl_easy_setopt(curl, CURLOPT_URL, finalBbURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bbrefPlayerData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
		}

		size_t writeToFileIndex = bbrefPlayerData.find(">MLB Players<", 0);
		if (writeToFileIndex == string::npos)
			writeToFileIndex = 0;
		size_t writeToFileIndexEnd = bbrefPlayerData.find(">MLB Players<", writeToFileIndex + 1);
		ofstream writeToFile;
		writeToFile.open("BaseballReferenceCachedPages\\PlayerId" + playerId + ".txt");
		writeToFile << todaysDate << "/ZachDateMetaData" << endl;
		writeToFile << bbrefPlayerData;// .substr(writeToFileIndex, writeToFileIndexEnd == string::npos ? writeToFileIndexEnd - writeToFileIndex : writeToFileIndexEnd);
		writeToFile.close();
	}

	return bbrefPlayerData;
}
*/


/*
size_t fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"dashboard\"", 0);
if (fangraphsCurrentIndex != string::npos)
{
size_t fangraphsStandardIndex = fangraphsPlayerData.find("name=\"standard\"", fangraphsCurrentIndex);
// Name GID fip era k/9 oppR/G oppK/9 FDPcorrected
fangraphsCurrentIndex = fangraphsPlayerData.find(">2017<", fangraphsCurrentIndex + 1);
size_t fangraphsPostseasonIndex = fangraphsPlayerData.find("postseason", fangraphsCurrentIndex + 1);
size_t fangraphsProjectionsIndex = fangraphsPlayerData.find("projections", fangraphsCurrentIndex + 1);

bool bIsProjectionsSection = false;
size_t fangraphsRowBeginIndex = fangraphsPlayerData.rfind("<tr", fangraphsCurrentIndex);
size_t fangraphsPreviousProjectionsIndex = fangraphsPlayerData.rfind("projections", fangraphsCurrentIndex);
if (fangraphsPreviousProjectionsIndex > fangraphsRowBeginIndex)
bIsProjectionsSection = true;
size_t fangraphsNextIndex = fangraphsPlayerData.find(">2017<", fangraphsCurrentIndex + 1);
// skip minor league stats and playoff stats
while (fangraphsNextIndex < fangraphsStandardIndex
&& fangraphsNextIndex < fangraphsPostseasonIndex
&& (bIsProjectionsSection || fangraphsNextIndex < fangraphsProjectionsIndex))
{
fangraphsCurrentIndex = fangraphsNextIndex;
fangraphsRowBeginIndex = fangraphsPlayerData.rfind("<tr", fangraphsCurrentIndex);
fangraphsPreviousProjectionsIndex = fangraphsPlayerData.rfind("projections", fangraphsCurrentIndex);
bIsProjectionsSection = fangraphsPreviousProjectionsIndex > fangraphsRowBeginIndex;
fangraphsNextIndex = fangraphsPlayerData.find(">2017<", fangraphsCurrentIndex + 1);
fangraphsProjectionsIndex = fangraphsPlayerData.find("projections", fangraphsCurrentIndex + 1);
}

bool bIsMinorLeagueSection = false;
fangraphsRowBeginIndex = fangraphsPlayerData.rfind("<tr", fangraphsCurrentIndex);
size_t fangraphsPreviousMinorsIndex = fangraphsPlayerData.rfind("minors", fangraphsCurrentIndex);
if (fangraphsPreviousMinorsIndex > fangraphsRowBeginIndex)
bIsMinorLeagueSection = true;

if (!bIsMinorLeagueSection && fangraphsCurrentIndex != string::npos)
*/

/*
http://rotoguru1.com/cgi-bin/stats.cgi?pos=6&sort=6&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=0&hithand=1&numlist=c
0    1    2               3     4         5             6      7    8     9        10             11       12           13      14      15      16        17     18    19       20    21     22          23
GID; Pos; Name;           Team; Salary; Salary Change; Points; GS;  GP; Active; Pts / Game; Pts / G / $; Pts / G(alt); Last; Days ago; MLBID;  ESPNID; YahooID; Bats; Throws; H / A; Oppt; Oppt hand; Game title
5125; 3; Cabrera, Miguel; det;  4000;      0;             0;     0; 0;    1;     0;           0;             0;         0;      0;     408234; 5544;   7163;     R;      R;     A;    chw;    L;       Jose Quintana(L) chw vs.det - 4:10 PM EDT - U.S.Cellular Field



http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=0
http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=1&nohead=1
pitcher vs batter matchups

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


http://rotoguru1.com/cgi-bin/mlb-dbd-2016.pl
0     1        2                 3                 4     5      6         7      8      9      10                              11                 12            13           14           15            16     17          18        19          20      21          22   23   24    25      26                27              28                  29                30   31         32    33      34   35          36          37          38          39          40          41          42          43       44       45       46
GID:  MLB_ID:  Name_Last_First:  Name_First_Last:  P/H:  Hand:  Date:     Team:  Oppt:  H/A:  Game#(1 unless double header):  Game_ID:            Gametime_ET:  Team_score:  Oppt_score:  Home_Ump:     Temp:  Condition:  W_speed:  W_dir:      ADI:    prior_ADI:  GS:  GP:  Pos:  Order:  Oppt_pitch_hand:  Oppt_pich_GID:  Oppt_pitch_MLB_ID:  Oppt_pitch_Name:  PA:  wOBA_num:  IP:   W/L/S:  QS:  FD_points:  DK_points:  DD_points:  YH_points:  FD_salary:  DK_salary:  DD_salary:  YH_salary:  FD_pos:  DK_pos:  DD_pos:  YH_pos
2407: 547989:  Abreu, Jose:      Jose Abreu:       H:    R:     20161002: chw:   min:   h:    1:                              20161002-min-chw-1: 15.10:        3:           6:           Nic Lentz:    65:    cloudy:     6:        Out to LF:  65.90:  65.56:      1:   1:   1B:   4:      R:                136p:           621244:             Berrios, Jose:    4:   2.3:       :     :       :    12.5:       9.00:       19:         8:          3200:       3900:       8350:       19:         3:       3:       3:       3:
136r: 534947:  Adleman, Timothy: Tim Adleman:      P:    R:     20161001: cin:   chc:   h:    1:                              20161001-chc-cin-1: 16.10:        7:           4:           Tom Hallion:  65:    overcast:   8:        R to L:     65.05:  66.30:      1:   1:   P:    9:      L:                1506:           452657:             Lester, Jon:      2:   0.9:       5.0:  W:      1:   30:         12.45:      20:         13:         6100:       5500:       9000:       28:         1:       1:       1:       1:

*/