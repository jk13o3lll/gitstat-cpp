#ifndef _GITSTAT_H_
#define _GITSTAT_H_

#include <cstdio>
#include <string>
#include "json.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #define popen _popen
    #define pclose _pclose
#endif

#define CURL_CMD_PREFIX "curl -s "
#define BITBUCKET_API_V2 "https://api.bitbucket.org/2.0/"
#define GITHUB_API_V3 "https://api.github.com/"

using std::string;
using json = nlohmann::json;

bool exec(const char *cmd, string &result);
bool http_get(const char *request, string &response, const char *service = "");

// list function to add and describe steps

// watch out date format, watch out different time zone

// watch out rate limit

// count_all (will generate a json file) (we can latter on generate javascript webpage based on this json file)
// get
//     total line add / delete ("diffstat" of that commit) of
//     certain user ("acount_id" in "commits"), (or all user in list)
//     date ("date" in "commits"),
//     commit type ("parent" in "commits", non merge (parent.count == 1)),
//     words count (use "diff" url in "commits", and counts the word add?)

// add another json file for fake_commit (find out specious commits, and check manually)
// when generate final_score.json, we input statistics.json, fake_commit.json, and extra.json
// we can use this final_score.json to generate javascript website

// if there are too many commits, it would be better to add the file to record the history. so we only need to check commits that are not in history.
// or we can clone the repository to local, and parse the git log message
// but using git rest api usually can obtain more information than using git log in local. but git api usually has rate limit

#endif