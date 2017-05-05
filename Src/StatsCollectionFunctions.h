#ifndef STATSCOLLECTION_H_INCLUDED
#define STATSCOLLECTION_H_INCLUDED

#include <string>
#include <vector>

struct PlayerData
{
	std::string playerId;
	std::string playerName;
	int playerSalary;
	bool batsLeftHanded;
	bool isFacingLefthander;
	float playerPointsPerGame;
};

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);
std::string GetEntireFileContents(std::string fileName);
std::string ReplaceURLWhiteSpaces(std::string originalURL);

float CalculateRSquared(std::vector<float> finalInputs, std::vector<float> outputValues);

std::string GetBatter2016StatsRawString(std::string playerId, CURL *curl);
const int AdvancedStatsBattingSplitsVersusRightHand = 1;
const int AdvancedStatsBattingSplitsVersusLeftHand = 2;
const int AdvancedStatsPitchingSplitsVersusRightHand = 4;
const int AdvancedStatsPitchingSplitsVersusLeftHand = 8;
const int AdvancedStatsPitchingStarterStatsOnly = 16;
std::string GetPlayerFangraphsPageData(std::string playerId, CURL *curl, bool bCachedOk, int advancedStatsFlags);

struct FullSeasonStats
{
	float averagePpg;
	float averagePpgVsLefty;
	float averagePpgVsRighty;
	int totalGamesStarted;
};
FullSeasonStats GetBatter2016Stats(std::string playerId, CURL *curl);

struct FullSeasonStatsAdvanced
{
	float averageVersusLefty;
	float averageVersusRighty;
	float sluggingVersusLefty;
	float sluggingVersusRighty;
	float opsVersusLefty;
	float opsVersusRighty;
	float wobaVersusLefty;
	float wobaVersusRighty;
	float isoVersusLefty;
	float isoVersusRighty;

	FullSeasonStatsAdvanced() : sluggingVersusLefty(-1),
		sluggingVersusRighty(-1),
		opsVersusLefty(-1),
		opsVersusRighty(-1),
		wobaVersusLefty(-1),
		wobaVersusRighty(-1),
		isoVersusLefty(-1),
		isoVersusRighty(-1),
		averageVersusRighty(-1),
		averageVersusLefty(-1)
	{
	}
};
FullSeasonStatsAdvanced GetBatterAdvancedStats(std::string playerId, std::string yearString, CURL *curl);
FullSeasonStatsAdvanced GetPitcherAdvancedStats(std::string playerId, std::string yearString, CURL *curl);

struct FullSeasonPitcherStats
{
	float era;
	float fip;
	float strikeOutsPer9;
	float numInnings;
	float whip;

	FullSeasonPitcherStats()
	{
		era = -999;
		fip = -999;
		strikeOutsPer9 = -999;
		numInnings = -1;
	}
};
FullSeasonPitcherStats GetPitcherCareerStats(std::string playerId, CURL *curl);
FullSeasonPitcherStats GetPitcher2016Stats(std::string playerId, CURL *curl);
FullSeasonPitcherStats GetPitcher2017Stats(std::string playerId, CURL *curl);

#endif

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
