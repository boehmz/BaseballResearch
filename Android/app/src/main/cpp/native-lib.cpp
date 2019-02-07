#include <jni.h>
#include <string>
#include <curl/curl.h>
#include <android/log.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
#include <thread>
#include "SharedGlobals.h"
#include "GameTeamWinContainer.h"
#include "StatsCollectionFunctions.h"


int latestGameTime =   2050;
int earliestGameTime = 2005;
std::string todaysDate = "20180924";
bool skipStatsCollection = false;
int reviewDateStart = 515;
int reviewDateEnd = 609;

#define TRUE 1
#define FALSE 0


#ifdef ANDROID
	#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "MLBPredictor", __VA_ARGS__))
#else
	#define LOGI(...) printf(__VA_ARGS__)
#endif



std::string GetSiteHtml() {

    CURL *curl = curl_easy_init();
    if (curl)
    {

        std::string httpBuffer;
        std::string thisPositionURL = "http://rotoguru1.com/cgi-bin/stats.cgi?pos=1&sort=4";
        thisPositionURL = "https://www.fangraphs.com/dailyprojections.aspx?pos=all&stats=bat&type=sabersim&team=0&lg=all&players=0";
        curl_easy_setopt(curl, CURLOPT_URL, thisPositionURL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpBuffer);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, TRUE);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, FALSE);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK){
            LOGI("CURL failed with error code %d", res);
        }
        curl_easy_reset(curl);
        int testInt = 0;
        LOGI("Html result from site %s was: %s", thisPositionURL.c_str(), httpBuffer.c_str());
        return httpBuffer;

    }
    return "could not make curl";
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_predictor_mlb_mlbpredictor_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string readBuffer = GetSiteHtml();
    return env->NewStringUTF(readBuffer.substr(0,100).c_str());
}
