// TODO: use jquery plugin DataTables to present table
// TODO: Attendance based on diary, (1) check date (#YYYYMMDD or #YYYY-MM-DD) in diary (2) changes for diary in duration
// TODO: Statistic of every files per person can be achieved by querying one by one
// TODO: Fake commit, automatically for too many lines, and load fake commit num from manually record.

// Suggested compiler: GCC (Linux), Mingw-w64 (Windows)

#ifndef __GITSTAT_HPP__
#define __GITSTAT_HPP__

#include "json.hpp"
using json = nlohmann::json;

#include <cstdio>   // printf, fopen, popen, ...
#include <cctype>   // isdigits, ...
#include <cstring>  // strstr, ...
#include <ctime>

#include <iostream>
#include <string> // haven't support UTF8, git log return utf8 as default
#include <vector>
#include <regex>

#define MAX_CMD 1024
#define MAX_BUFFER 1024

// Include *.txt, *.md: git log ... -- "*.txt" "some_dir/*.md" "some_dir2"
// Exclude *.txt (case insensitve): git log ... -- ":(exclude,icase)*.txt"


struct Statistics{
    Statistics(int num_commits_, int lines_inserted_, int lines_deleted_,
        int words_inserted_, int words_deleted_, bool attendance_):
        num_commits(num_commits_),
        lines_inserted(lines_inserted_),
        lines_deleted(lines_deleted_),
        words_inserted(words_inserted_),
        words_deleted(words_deleted_),
        attendance(attendance_){}

    int num_commits;
    int lines_inserted;
    int lines_deleted;
    int words_inserted;
    int words_deleted;
    bool attendance;
};

struct Query{
    Query(const json &js):
        name(js["name"].get<std::string>()),
        since(js["duration"][0].get<std::string>()),
        until(js["duration"][1].get<std::string>()),
        attendance_criteria(js["attendance criteria"].get<int>()){}

    std::string name;
    std::string since;
    std::string until;
    int attendance_criteria;
};

struct Contributor{
    Contributor(const json &js):
        name(js["name"].get<std::string>()),
        diary(js["diary"].get<std::string>()){
        for(const auto &x: js["email"])
            email.push_back(x.get<std::string>());
        for(const auto &x: js["label"])
            label.push_back(x.get<std::string>());
        stats.reserve(64);
    }

    std::string name;
    std::vector<std::string> email;
    std::vector<std::string> label;
    std::string diary;
    std::vector<Statistics> stats;
    // "attedance (query id)" is for manually label the attendanc at each query
};

bool exec(const char *cmd, std::string &result){
    char c;
    FILE *pipe = popen(cmd, "r");
    if(!pipe){ printf("Failed to open a pipe."); return false; }
    for(result.clear(); (c = fgetc(pipe)) != EOF; result.push_back(c));
    // push_back appends a '\0' every time
    pclose(pipe);
    return true;
}

// repo_dir: directory of the repository; git_name: .git directory
bool pull_from_repository(const char *repo_dir, const char *git_dir = NULL){
    char cmd[MAX_CMD];
    std::string result;

    puts("Pull from the repository ...");
    sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager pull",
        repo_dir, git_dir != NULL? git_dir : ".git");
    if(!exec(cmd, result)) return false;
    if(strstr(result.c_str(), "fatal") != NULL){
        printf("Failed to pull from the repository.\n%s\n", result.c_str());
        return false;
    }
    printf("%sdone.\n\n", result.c_str());
    return true;
}

bool get_lines_stat(const char *repo_dir, const char *author_name,
                    const char *since, const char *until,
                    int &num_commits, int &files_changed, int &lines_inserted, int &lines_deleted,
                    const char *pathspec = NULL, const char *git_dir = NULL){
    int len, tmp;
    char cmd[MAX_CMD];
    std::string result;

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log "
        "--date=local --pretty=\"\" --shortstat --author=\"%s\" --no-merges",
        repo_dir, git_dir != NULL? git_dir : ".git", author_name);
    if(since != NULL)   len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)   len += sprintf(cmd + len, " --until=\"%s\"", until);
    if(pathspec != NULL)len += sprintf(cmd + len, " -- %s", pathspec);
    if(!exec(cmd, result)) return false;

    // xx files changed, xx insertions(+), xx deletions(-)
    tmp = num_commits = files_changed = lines_inserted = lines_deleted = 0;
    for(const char *p= result.c_str(); *p != '\0'; ++p)
        switch(*p){
            case ' ':   if(isdigit(p[1])) tmp = atoi(++p);  break;
            case 'f':   files_changed += tmp;               break;
            case '+':   lines_inserted += tmp;              break;
            case '-':   lines_deleted += tmp;               break;
            case '\n':  ++num_commits;                      break;
        }
    return true;
}

bool get_words_stat(const char *repo_dir, const char *author_name,
                    const char *since, const char *until,
                    int &words_inserted, int &words_deleted,
                    const char *pathspec = NULL, const char *git_dir = NULL){
    int len;
    char cmd[MAX_CMD];
    std::string result;
    bool newline; // previous char is '\n'

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log "
        "--date=local --pretty=\"\" -p --word-diff=porcelain --author=\"%s\" --no-merges",
        repo_dir, git_dir != NULL? git_dir : ".git", author_name);
    if(since != NULL)   len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)   len += sprintf(cmd + len, " --until=\"%s\"", until);
    if(pathspec != NULL)len += sprintf(cmd + len, " -- %s", pathspec);
    if(!exec(cmd, result)) return false;

    // \n+... (add), \n-... (delete), \n' ' (no change)
    newline = true;
    words_inserted = words_deleted = 0;
    for(const char *p = result.c_str(); *p != '\0'; ++p){
        if(newline){
            if(*p == '+'){
                for(++p; *p != '\n' && *p !='\0'; ++p)
                    if(isalnum(p[0]) && !isalnum(p[1]))
                        ++words_inserted;
            }
            else if(*p == '-'){
                for(++p; *p != '\n' && *p !='\0'; ++p)
                    if(isalnum(p[0]) && !isalnum(p[1]))
                        ++words_deleted;
            }
            else if(*p != '\n') newline = false;
        }
        else if(*p == '\n') newline = true;
    }
    return true;
}

bool find_date(const char *str, time_t &t){
    std::smatch m; // smatch also separate result in each ()
    std::string s(str);
    std::regex e1("(\\d{4})(\\d{2})(\\d{2})");
    std::regex e2("(\\d{4})(-|/)(\\d{1,2})(-|/)(\\d{1,2})");
    int year, month, day;
    struct tm *timeinfo;

    if(std::regex_search (s,m,e1)){ // find first one
        year = atoi(m.str(1).c_str());
        month = atoi(m.str(2).c_str());
        day = atoi(m.str(3).c_str());
    }
    else if(std::regex_search (s,m,e2)){ // find first one
        year = atoi(m.str(1).c_str());
        month = atoi(m.str(3).c_str());
        day = atoi(m.str(5).c_str());
    }
    else return false;
    
    // check wether year, month, and day are valid
    if(month > 12 || month < 1 || day > 31 || day < 1) return false;
    else if(month == 2){
        if(day > ((year % 4 == 0 && year % 100 !=0) || year % 400 == 0? 29 : 28))
            return false;
    }
    else if(month == 4 || month == 6 || month == 9 || month == 11){
        if(day > 30)
            return false;
    }

    // printf("%04d-%02d-%02d\n", year, month, day);
    time(&t);
    timeinfo = localtime(&t);
    timeinfo->tm_year = year - 1900;
    timeinfo->tm_mon = month - 1;
    timeinfo->tm_mday = day;
    t = mktime(timeinfo); // timeinfo->tm_wday and tm_yday will be set

    return t != -1;
}

bool generate_statistics(const char *config){
    FILE *fp, *ftmp;
    json js;
    char buffer[MAX_BUFFER];

    std::string repo_dir;
    std::vector<Query> queries;
    std::vector<Contributor> contributors;
    float w[5]; // weights: #commits, lines(+), lines(-), words(+), words(-)
    int fig2words;
    std::string pathspec_text, pathspec_figure, pathspec_code;

    std::vector<time_t> dates; // hr, min, sec info are rand
    time_t t;
    tm *local_time;

    bool has_diary;
    int tmp, num_commtis, files_changed, lines_inserted, lines_deleted, words_inserted, words_deleted;
    int net_files_changed, net_lines_inserted, net_lines_deleted, net_words_inserted, net_words_deleted;
    int words_count_all_queries, commits_all_queries;

    // load configuration
    fp = fopen(config, "r");
    if(fp == NULL){
        printf("Failed to load config file.\n");
        return false;
    }
    js = json::parse(fp);
    fclose(fp);

    // extract repo dir and update
    repo_dir = js["repository"].get<std::string>();
    pull_from_repository(repo_dir.c_str());

    // extract more info
    printf("%lu queries, %lu contributors\n\n",
        js["queries"].size(), js["contributors"].size());
    for(const auto &x: js["queries"])
        queries.push_back(Query(x));
    for(const auto &x: js["contributors"])
        contributors.push_back(Contributor(x));
    w[0] = js["weights"]["num commits"].get<float>();
    w[1] = js["weights"]["lines inserted"].get<float>();
    w[2] = js["weights"]["lines deleted"].get<float>();
    w[3] = js["weights"]["words inserted"].get<float>();
    w[4] = js["weights"]["words deleted"].get<float>();
    fig2words = js["weights"]["figure to words inserted"].get<int>();
    pathspec_text = js["pathspec"]["text"].get<std::string>();
    pathspec_figure = js["pathspec"]["figure"].get<std::string>();
    pathspec_code = js["pathspec"]["code"].get<std::string>();

    // get statistics
    for(auto &x: contributors){
        printf("Get data for %s ...", x.name.c_str());
        // check diary
        fp = fopen((repo_dir + "/" + x.diary).c_str(), "r");
        if(fp == NULL) printf("(no diary founded) ");
        else{
            // extract all date in header
            dates.clear();
            while(fgets(buffer, MAX_BUFFER, fp) != NULL){
                if(buffer[0] == '#' && strlen(buffer) > 8)
                    if(find_date(buffer, t))
                        dates.push_back(t);
            }
            fclose(fp);
        }
        // querying
        words_count_all_queries = commits_all_queries = 0;
        for(const auto &q: queries){
            net_lines_inserted = net_lines_deleted = net_words_inserted = net_words_deleted = 0;
            // get lines and words statistics
            for(const auto &name: x.email){
                // all
                get_lines_stat(repo_dir.c_str(), name.c_str(),
                    q.since.c_str(), q.until.c_str(),
                    num_commtis, files_changed, lines_inserted, lines_deleted);
                if(num_commtis != 0){
                    // text
                    get_lines_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                        tmp, files_changed, lines_inserted, lines_deleted, pathspec_text.c_str());
                    get_words_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                        words_inserted, words_deleted, pathspec_text.c_str());
                    net_lines_inserted += lines_inserted;
                    net_lines_deleted += lines_deleted;
                    net_words_inserted += words_inserted;
                    net_words_deleted += words_deleted;
                    // figure
                    get_lines_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                        tmp, files_changed, lines_inserted, lines_deleted, pathspec_text.c_str());
                    net_words_inserted += files_changed * fig2words;
                    // code
                    get_lines_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                        tmp, files_changed, lines_inserted, lines_deleted, pathspec_code.c_str());
                    get_words_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                        words_inserted, words_deleted, pathspec_code.c_str());
                    net_lines_inserted += lines_inserted;
                    net_lines_deleted += lines_deleted;
                    net_words_inserted += words_inserted;
                    net_words_deleted += words_deleted;
                    break; // until find one valid name
                }
            }
            // check attendane
            if(q.attendance_criteria == 0){ // only date in diary

            }
            else if(q.attendance_criteria == 1){ // date in diary & commit in duration

            }
            else if(q.attendance_criteria == -1)    has_diary = false;
            else                                    has_diary = true;
            
            x.stats.push_back(Statistics(
                num_commtis, net_lines_inserted, net_lines_deleted,
                net_words_inserted, net_words_deleted, has_diary
            ));
            printf("\n%s, %s: %d commits, L+%d, L-%d, W+%d, W-%d",
                x.name.c_str(), q.name.c_str(), num_commtis,
                net_lines_inserted, net_lines_deleted,
                net_words_inserted, net_words_deleted);
        }
        puts("\ndone.");
    }

    // obtain time
    time(&t);
    local_time = localtime(&t);

    // generate html
    fp = fopen(js["html"].get<std::string>().c_str(), "w");
    if(fp == NULL){
        puts("Failed to open html file.");
        js.clear();
        return false;
    }
    // 1. generate header

    // 2. generate main (things before table)

    // 3. generate table

    // 4. generate residual parts (footer)


    fclose(fp);
    js.clear();
    return true;
}

// bool generate_deatils_of_contribution(){} // user's contribution on every files

// bool remove_fake_commit(){} // load fake commit id from file (manually record in json), and then count lines and words in that commit and subtract from total

// bool detect_potential_fake_commit(){} // too many lines in one commit, ...

#endif // __GITSTAT_HPP__