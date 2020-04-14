// TODO:
// Fake commit, automatically for too many lines, and load fake commit num from manually record.
// write css to overwrite some datatables settings

// why git log xxx only show merge (pull request)
// It still could be found manually.
// git log -p --word-diff=porcelain 9f5ba70^1..9f5ba70
// for git rev-list, if there are two parents (merged) it will select the first one and ...

// Problems:
// count the files no mater deleted, rename?
// words count to code is unfair

// Notes:
// .m is code, .mat is data
// git shortlog -s -e [-c]  to get all authors
// Include *.txt, *.md: git log ... -- "*.txt" "some_dir/*.md" "some_dir2"
// Exclude *.txt (case insensitve): git log ... -- ":(exclude,icase)*.txt"
// "attedance (query id)" is for manually label the attendance at each query
// invalid commits by all - commits included
// export table content will change based on operation on webpage


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
#define MAX_BUFFER 65536

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
    time_t t0, t1; // since, until
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

    int num_fake_commits;
    std::vector<std::string> filenames;
    std::vector<Statistics> filesstat;
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

bool get_num_commits(const char *repo_dir, const char *author_name, const char *since, const char *until,
                    int &num_commits, const char *pathspec = NULL, const char *revision_range = NULL, const char *git_dir = NULL){
    int len, tmp;
    char cmd[MAX_CMD];
    std::string result;

    num_commits = 0;

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager "
        "shortlog -s --author=\"%s\" --no-merges %s",
        repo_dir, git_dir != NULL? git_dir : ".git", author_name, revision_range != NULL? revision_range : " ");
    if(since != NULL)   len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)   len += sprintf(cmd + len, " --until=\"%s\"", until);
    if(pathspec != NULL)len += sprintf(cmd + len, " -- %s", pathspec);
    if(!exec(cmd, result)) return false;
    strncpy(cmd, result.c_str(), 5);
    if(strstr(cmd, "fatal") != NULL){
        printf("Failed to get_num_commits from the repository.\n%s\n", result.c_str());
        return false;
    }

    //   n1  name1
    if(result.length() > 0)
        num_commits = atoi(result.c_str());
    return true;
}

bool get_lines_stat(const char *repo_dir, const char *author_name,
                    const char *since, const char *until,
                    int &num_commits, int &files_changed, int &lines_inserted, int &lines_deleted,
                    const char *pathspec = NULL, const char *revision_range = NULL, const char *git_dir = NULL){
    int len, tmp;
    char cmd[MAX_CMD];
    std::string result;

    num_commits = files_changed = lines_inserted = lines_deleted = 0;

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log "
        "--date=local --pretty=\"\" --shortstat --author=\"%s\" --no-merges %s",
        repo_dir, git_dir != NULL? git_dir : ".git", author_name, revision_range != NULL? revision_range : " ");
    if(since != NULL)   len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)   len += sprintf(cmd + len, " --until=\"%s\"", until);
    if(pathspec != NULL)len += sprintf(cmd + len, " -- %s", pathspec);
    if(!exec(cmd, result)) return false;
    strncpy(cmd, result.c_str(), 5);
    if(strstr(cmd, "fatal") != NULL){
        printf("Failed to get_lines_stat from the repository.\n%s\n", result.c_str());
        return false;
    }

    // word diff to avoid empty line? (only '\n')
    // xx files changed, xx insertions(+), xx deletions(-)
    tmp = 0;
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
                    const char *pathspec = NULL, const char *revision_range = NULL, const char *git_dir = NULL){
    int len;
    char cmd[MAX_CMD];
    std::string result;
    bool flag;

    words_inserted = words_deleted = 0;

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log "
        "--date=local --pretty=\"\" -p --word-diff=porcelain --author=\"%s\" --no-merges %s",
        repo_dir, git_dir != NULL? git_dir : ".git", author_name, revision_range != NULL? revision_range : " ");
    if(since != NULL)   len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)   len += sprintf(cmd + len, " --until=\"%s\"", until);
    if(pathspec != NULL)len += sprintf(cmd + len, " -- %s", pathspec);
    if(!exec(cmd, result)) return false;
    strncpy(cmd, result.c_str(), 5);
    if(strstr(cmd, "fatal") != NULL){
        printf("Failed to get_words_stat from the repository.\n%s\n", result.c_str());
        return false;
    }

    // \n+... (add), \n-... (delete), \n' ' (no change)
    // start checking first char of every after @\n
    // count words by space or alpha
    flag = false;
    for(const char *p = result.c_str(); *p != '\0'; ++p){
        switch(*p){
            case '+':
                if(flag)
                    for(++p; *p != '\n'; ++p)
                        if(isalnum(p[0]) && !isalnum(p[1]))
                            ++words_inserted;
                break;
            case '-':
                if(flag)
                    for(++p; *p != '\n'; ++p)
                        if(isalnum(p[0]) && !isalnum(p[1]))
                            ++words_deleted;
                break;
            case '@': flag = true; break;
            case 'd': flag = false; break;
        }
        while(*p != '\n') ++p;
    }
    return true;
}

bool get_files_list(const char *repo_dir, const char *author_name, const char *since, const char *until,
                    std::vector<std::string> &files, const char *pathspec = NULL, const char *revision_range = NULL, const char *git_dir = NULL){
    int len, tmp, i;
    char cmd[MAX_CMD];
    std::string result;

    // git log --pretty="" --name-only --author="..." --since=... --until=... -- "*.tex" | sort -u
    // git --no-pager log --pretty="" --name-only --author="..." --no-merges -- "*.png" | sort -u
    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log "
        "--date=local --pretty=\"\" --name-only --author=\"%s\" --no-merges %s",
        repo_dir, git_dir != NULL? git_dir : ".git", author_name, revision_range != NULL? revision_range : " ");
    if(since != NULL)   len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)   len += sprintf(cmd + len, " --until=\"%s\"", until);
    if(pathspec != NULL)len += sprintf(cmd + len, " -- %s", pathspec);
    len += sprintf(cmd + len, " | sort -u"); // sort and unique
    if(!exec(cmd, result)) return false;
    strncpy(cmd, result.c_str(), 5);
    if(strstr(cmd, "fatal") != NULL){
        printf("Failed to get_files_list from the repository.\n%s\n", result.c_str());
        return false;
    }

    // filename \n
    files.clear();
    tmp = len = i = 0;
    for(const char *p = result.c_str(); *p != '\0'; ++p, ++i){
        if(*p == '\n'){
            files.push_back(result.substr(tmp, len));
            tmp = i + 1;
            len = 0;
        }
        else  ++len;
    }
    return true;
}

bool find_date(const char *str, int &date){ // YYYYMMDD
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
    date = year * 10000 + month * 100 + day;
    return true;

    // time(&t); // set to zero, instead of current time
    // timeinfo = localtime(&t);
    // timeinfo->tm_year = year - 1900;
    // timeinfo->tm_mon = month - 1;
    // timeinfo->tm_mday = day;
    // t = mktime(timeinfo); // timeinfo->tm_wday and tm_yday will be set
    // return t != -1;
}

bool generate_statistics_queries(const char *config){
    FILE *fp;
    json js;
    char buffer[MAX_BUFFER];

    std::string repo_dir;
    std::vector<Query> queries;
    std::vector<Contributor> contributors;
    std::vector<std::string> fakecommits;
    float w[5]; // weights: #commits, lines(+), lines(-), words(+), words(-)
    int fig2words;
    std::string pathspec_text, pathspec_figure, pathspec_code, pathspec_include, pathspec_diary;

    std::vector<int> dates;
    int t0, t1;
    time_t raw_time;
    tm *local_time;

    bool has_diary;
    int tmp, num_commits, files_changed, lines_inserted, lines_deleted, words_inserted, words_deleted;
    int net_files_changed, net_lines_inserted, net_lines_deleted, net_words_inserted, net_words_deleted;
    int lines_count_all_queries, words_count_all_queries, commits_all_queries, num_fake_commits;
    float git_score;

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
    printf("%lu queries, %lu contributors\n",
        js["queries"].size(), js["contributors"].size());
    for(const auto &x: js["queries"])
        queries.push_back(Query(x));
    for(const auto &x: js["contributors"])
        contributors.push_back(Contributor(x));
    for(const auto &x: js["fake commit"])
        fakecommits.push_back(x.get<std::string>() + "^1.." + x.get<std::string>()); // only log 1 commit
    w[0] = js["weights"]["num commits"].get<float>();
    w[1] = js["weights"]["lines inserted"].get<float>();
    w[2] = js["weights"]["lines deleted"].get<float>();
    w[3] = js["weights"]["words inserted"].get<float>();
    w[4] = js["weights"]["words deleted"].get<float>();
    fig2words = js["weights"]["figure to words inserted"].get<int>();
    pathspec_text = js["pathspec"]["text"].get<std::string>();
    pathspec_figure = js["pathspec"]["figure"].get<std::string>();
    pathspec_code = js["pathspec"]["code"].get<std::string>();
    pathspec_include = pathspec_text + " " + pathspec_figure + " " + pathspec_code;

    // get statistics
    for(auto &x: contributors){
        printf("Get data for %s ...", x.name.c_str());
        // check diary
        fp = fopen((repo_dir + "/" + x.diary).c_str(), "r");
        if(fp == NULL){
            printf("(no diary founded) ");
            pathspec_diary = "\"*\"";
        }
        else{
            // extract all date in header
            dates.clear();
            while(fgets(buffer, MAX_BUFFER, fp) != NULL){
                if(buffer[0] == '#' && strlen(buffer) > 8)
                    if(find_date(buffer, t0))
                        dates.push_back(t0);
            }
            fclose(fp);
            pathspec_diary = "\"" + x.diary + "\"";
        }
        // querying
        x.stats.clear();
        words_count_all_queries = commits_all_queries = num_fake_commits = 0;
        for(const auto &q: queries){
            net_lines_inserted = net_lines_deleted = net_words_inserted = net_words_deleted = 0;
            // get lines and words statistics
            for(const auto &name: x.email){
                // all
                get_num_commits(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                    num_commits, pathspec_include.c_str());
                if(num_commits != 0){
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
                        tmp, files_changed, lines_inserted, lines_deleted, pathspec_figure.c_str());
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
                    // every fake commit for this query
                    for(const auto &fake: fakecommits){
                        get_num_commits(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                            tmp, pathspec_include.c_str(), fake.c_str());
                        if(tmp > 0){
                            ++num_fake_commits;
                            // text
                            get_lines_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                                tmp, files_changed, lines_inserted, lines_deleted, pathspec_text.c_str(), fake.c_str());
                            get_words_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                                words_inserted, words_deleted, pathspec_text.c_str(), fake.c_str());
                            net_lines_inserted -= lines_inserted;
                            net_lines_deleted -= lines_deleted;
                            net_words_inserted -= words_inserted;
                            net_words_deleted -= words_deleted;
                            // figure
                            get_lines_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                                tmp, files_changed, lines_inserted, lines_deleted, pathspec_figure.c_str()), fake.c_str();
                            net_words_inserted -= files_changed * fig2words;
                            // code
                            get_lines_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                                tmp, files_changed, lines_inserted, lines_deleted, pathspec_code.c_str(), fake.c_str());
                            get_words_stat(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                                words_inserted, words_deleted, pathspec_code.c_str(), fake.c_str());
                            net_lines_inserted -= lines_inserted;
                            net_lines_deleted -= lines_deleted;
                            net_words_inserted -= words_inserted;
                            net_words_deleted -= words_deleted;
                            printf("\n  [fake commit] %s", fake.c_str());
                        }
                    }
                    // break; // until find one valid name
                }
            }
            // check attendane
            if(!find_date(q.since.c_str(), t0) || !find_date(q.until.c_str(), t1)){
                printf("Failed to parse since and until in query: %s\n", q.name.c_str());
                js.clear();
                return false;
            }
            has_diary = false;
            if(q.attendance_criteria == 0){ // only date in diary
                for(const auto &d: dates)
                    if(d >= t0 && d <= t0){ has_diary = true; break; }
            }
            else if(q.attendance_criteria == 1){ // date in diary & commit in duration
                if(num_commits > 0)
                    for(const auto &d: dates)
                        if(d >= t0 && d <= t0){ has_diary = true; break; }
            }
            else if(q.attendance_criteria == 2){
                // by git log --since --until --author -- diary.md
                for(const auto &name: x.email){
                    get_num_commits(repo_dir.c_str(), name.c_str(), q.since.c_str(), q.until.c_str(),
                        tmp, pathspec_diary.c_str());
                    if(tmp > 0){ has_diary = true; break; }
                }
            }
            else if(q.attendance_criteria != -1) has_diary = true;
            
            x.stats.push_back(Statistics(
                num_commits, net_lines_inserted, net_lines_deleted,
                net_words_inserted, net_words_deleted, has_diary
            ));
            x.num_fake_commits = num_fake_commits;
            printf("\n  %s, %s: %d commits, %d fake, L+%d, L-%d, W+%d, W-%d, %s diary",
                x.name.c_str(), q.name.c_str(), num_commits, num_fake_commits,
                net_lines_inserted, net_lines_deleted, net_words_inserted, net_words_deleted,
                has_diary? "has" : "no");
        }
        puts("\ndone.");
    }

    // obtain time
    time(&raw_time);
    local_time = localtime(&raw_time);

    printf("Generate html ...");
    // generate html
    fp = fopen(js["html"].get<std::string>().c_str(), "w");
    if(fp == NULL){
        puts("Failed to open html file.");
        js.clear();
        return false;
    }
    // generate file
    fprintf(fp,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
            "<meta charset=\"utf-8\"/>"
            "<title>Statistics of %s</title>" // 1
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"https://cdn.datatables.net/1.10.20/css/jquery.dataTables.min.css\">" // https://datatables.net/manual/styling/theme-creator
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"https://cdn.datatables.net/buttons/1.6.1/css/buttons.dataTables.min.css\">"
            "<link href=\"style.css\" rel=\"stylesheet\" type=\"text/css\">"
        "</head>"
        "<body>"
            "<header>"
                "<h1>%s</h1>" // 2
                "<p>Accessed at %04d-%02d-%02d %02d:%02d:%02d from git repository.</p>" // 3, 4, 5, 6, 7, 8
            "</header>"
            "<main>"
                "<h2>%s</h2>" // 9
                "<p>Please follow the file format of diary, otherwise it cannot detect attendence correctly.<br/>"
                "Note that merges won\'t give unfair points.<br/>"
                "<font class=\"no-diary\">This color</font> means no diary for that query; "
                "<font class=\"fake\">This color</font> means you have fake commit.</p>"
                "<table id=\"table1\">",
        js["title"].get<std::string>().c_str(),
        js["title"].get<std::string>().c_str(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
        js["subtitle"].get<std::string>().c_str()
    );
    // column heads
    fprintf(fp, "<thead><tr><th>Authors</th><th>Semester</th>");
    for(int i = 0; i < queries.size(); ++i)
        fprintf(fp, "<th>%s</th>", queries[i].name.c_str());
    fprintf(fp, "<th>Net<br/>lines count</th><th>Net<br/>words count</th><th>Weeks<br/>with commits</th><th>Fake commits</th><th>GIT Score</th></tr></thead>");
    // contents
    fprintf(fp, "<tbody>");
    for(const auto &x: contributors){
        fprintf(fp, "<tr><td>%s</td><td>%s</td>", x.name.c_str(), x.label[0].c_str());
        lines_count_all_queries = words_count_all_queries = commits_all_queries = 0;
        git_score = 0;
        for(const auto &xs: x.stats){
            fprintf(fp, "<td %s>%d</td>", xs.attendance? " " : " class=\"no-diary\"", xs.words_inserted + xs.words_deleted);
            if(xs.num_commits > 0){
                lines_count_all_queries += xs.lines_inserted + xs.lines_deleted;
                words_count_all_queries += xs.words_inserted + xs.words_deleted;
                ++commits_all_queries;
                git_score += w[0] * xs.num_commits
                       + w[1] * xs.lines_inserted + w[2] * xs.lines_deleted
                       + w[3] * xs.words_inserted + w[4] * xs.words_deleted;
            }
        }
        fprintf(fp, "<td>%d</td><td>%d</td><td>%d</td><td %s>%d</td><td>%.2f</td></tr>",
            lines_count_all_queries, words_count_all_queries, commits_all_queries, x.num_fake_commits > 0? "class=\"fake\"" : " ", x.num_fake_commits, git_score);
    }
    fprintf(fp, "</tbody>");
    // residual
    fprintf(fp, "</table>"
            "</main>"
            "<footer><p id=\"total_authors\">&#x2716; Total authors: %lu</p></footer>"
            "<script type=\"text/javascript\" src=\"https://code.jquery.com/jquery-3.3.1.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdn.datatables.net/1.10.20/js/jquery.dataTables.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdn.datatables.net/buttons/1.6.1/js/dataTables.buttons.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdnjs.cloudflare.com/ajax/libs/jszip/3.1.3/jszip.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdnjs.cloudflare.com/ajax/libs/pdfmake/0.1.53/pdfmake.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdnjs.cloudflare.com/ajax/libs/pdfmake/0.1.53/vfs_fonts.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdn.datatables.net/buttons/1.6.1/js/buttons.html5.min.js\"></script>"
            "<script>$(document).ready(function(){"
                "$(\'#table1\').DataTable({"
                    "dom: \'Blfrtip\',"// https://datatables.net/reference/option/dom, https://datatables.net/examples/basic_init/dom.html, https://datatables.net/manual/styling/
                    "buttons: ["
                        "\'copyHtml5\',"
                        "{extend:\'excelHtml5\',title:\'%04d%02d%02d_%02d%02d%02d_%s\'},"
                        "{extend:\'csvHtml5\',title:\'%04d%02d%02d_%02d%02d%02d_%s\'}"
                    "]"
                "});"
            "});</script>"
        "</body>"
        "</html>",
        contributors.size(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec, js["export"].get<std::string>().c_str(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec, js["export"].get<std::string>().c_str()
    );
    fclose(fp);
    puts("done.");

    js.clear();
    return true;
}

bool generate_statistics_summary(const char *config){
    FILE *fp, *ftmp;
    json js;
    char buffer[MAX_BUFFER];

    std::string repo_dir;
    std::vector<Contributor> contributors;
    std::vector<std::string> fakecommits;

    float w[5]; // weights: #commits, lines(+), lines(-), words(+), words(-)
    int fig2words;
    std::string pathspec_text, pathspec_figure, pathspec_code, pathspec_textcode, pathspec_include;

    time_t raw_time;
    tm *local_time;

    std::vector<std::string> files;
    int tmp, num_commits, num_fake_commits, files_changed, net_num_commits, net_num_fake_commits;
    int lines_inserted, lines_deleted, words_inserted, words_deleted;
    int net_lines_inserted, net_lines_deleted, net_words_inserted, net_words_deleted;
    float git_score;

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
    printf("%lu contributors\n\n", js["contributors"].size());
    for(const auto &x: js["contributors"])
        contributors.push_back(Contributor(x));
    for(const auto &x: js["fake commit"])
        fakecommits.push_back(x.get<std::string>() + "^1.." + x.get<std::string>()); // only log 1 commit
    w[0] = js["weights"]["num commits"].get<float>();
    w[1] = js["weights"]["lines inserted"].get<float>();
    w[2] = js["weights"]["lines deleted"].get<float>();
    w[3] = js["weights"]["words inserted"].get<float>();
    w[4] = js["weights"]["words deleted"].get<float>();
    fig2words = js["weights"]["figure to words inserted"].get<int>();
    pathspec_text = js["pathspec"]["text"].get<std::string>();
    pathspec_figure = js["pathspec"]["figure"].get<std::string>();
    pathspec_code = js["pathspec"]["code"].get<std::string>();
    pathspec_textcode = pathspec_text + " " + pathspec_code;
    pathspec_include = pathspec_text + " " + pathspec_figure + " " + pathspec_code;

    // get statistics
    for(auto &x: contributors){
        printf("Get data for %s ...", x.name.c_str());
        x.stats.clear();
        net_num_commits = net_num_fake_commits = 0;
        net_lines_inserted = net_lines_deleted = net_words_inserted = net_words_deleted = 0;
        git_score = 0.0f;
        for(auto &name: x.email){
            // get num_commits
            get_num_commits(repo_dir.c_str(), name.c_str(), NULL, NULL, num_commits, pathspec_include.c_str());
            printf("[%d]", num_commits);
            if(num_commits > 0){
                // get text files detail
                get_files_list(repo_dir.c_str(), name.c_str(), NULL, NULL, files, pathspec_text.c_str());
                for(auto &f: files){
                    sprintf(buffer, "\"%s\"", f.c_str());
                    get_lines_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, files_changed, lines_inserted, lines_deleted, buffer);
                    get_words_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, words_inserted, words_deleted, buffer);
                    net_lines_inserted += lines_inserted;
                    net_lines_deleted += lines_deleted;
                    net_words_inserted += words_inserted;
                    net_words_deleted += words_deleted;
                    // store files info
                    x.filenames.push_back(f.c_str());
                    x.filesstat.push_back(Statistics(tmp, lines_inserted, lines_deleted, words_inserted, words_deleted, true));
                }
                // get figure details
                get_files_list(repo_dir.c_str(), name.c_str(), NULL, NULL, files, pathspec_figure.c_str());
                for(auto &f: files){
                    sprintf(buffer, "\"%s\"", f.c_str());
                    // get_lines_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, files_changed, tmp, tmp, buffer);
                    // net_words_inserted += files_changed * fig2words; // one add. delete, or modification cause contribution
                    get_num_commits(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, buffer);
                    // store files info
                    x.filenames.push_back(f.c_str());
                    x.filesstat.push_back(Statistics(tmp, 0, 0, fig2words, 0, true));
                }
                net_words_inserted += files.size() * fig2words; // one figure as on contribution
                // get code details
                get_files_list(repo_dir.c_str(), name.c_str(), NULL, NULL, files, pathspec_code.c_str());
                for(auto &f: files){
                    sprintf(buffer, "\"%s\"", f.c_str());
                    get_lines_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, files_changed, lines_inserted, lines_deleted, buffer);
                    get_words_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, words_inserted, words_deleted, buffer);
                    net_lines_inserted += lines_inserted;
                    net_lines_deleted += lines_deleted;
                    net_words_inserted += words_inserted;
                    net_words_deleted += words_deleted;
                    // store files info
                    x.filenames.push_back(f.c_str());
                    x.filesstat.push_back(Statistics(tmp, lines_inserted, lines_deleted, words_inserted, words_deleted, true));
                }
                // get filenames in fakecommits, then removed from each file
                num_fake_commits = 0;
                for(const auto &fake: fakecommits){
                    // didn't substract stat in each file for convenient, just subtract summary
                    get_num_commits(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, pathspec_include.c_str(), fake.c_str());
                    if(tmp > 0){
                        ++num_fake_commits;
                        // text
                        get_lines_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, files_changed, lines_inserted, lines_deleted, pathspec_text.c_str(), fake.c_str());
                        get_words_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, words_inserted, words_deleted, pathspec_text.c_str(), fake.c_str());
                        net_lines_inserted -= lines_inserted;
                        net_lines_deleted -= lines_deleted;
                        net_words_inserted -= words_inserted;
                        net_words_deleted -= words_deleted;
                        // figure
                        get_files_list(repo_dir.c_str(), name.c_str(), NULL, NULL, files, pathspec_figure.c_str(), fake.c_str());
                        net_words_inserted -= files.size() * fig2words; // one figure as on contribution
                        // code
                        get_lines_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, tmp, files_changed, lines_inserted, lines_deleted, pathspec_code.c_str(), fake.c_str());
                        get_words_stat(repo_dir.c_str(), name.c_str(), NULL, NULL, words_inserted, words_deleted, pathspec_code.c_str(), fake.c_str());
                        net_lines_inserted -= lines_inserted;
                        net_lines_deleted -= lines_deleted;
                        net_words_inserted -= words_inserted;
                        net_words_deleted -= words_deleted;
                        printf("\n  [fake commit] %s", fake.c_str());
                    }
                }
                net_num_commits += num_commits - num_fake_commits;
                net_num_commits += num_fake_commits;
            }
        }
        x.stats.push_back(Statistics(net_num_commits, net_lines_inserted, net_lines_deleted, net_words_inserted, net_words_deleted, true));
        x.num_fake_commits = net_num_fake_commits;
        printf("\n  C%d, FC%d, L+%d, L-%d, W+%d, W-%d\n", net_num_commits, net_num_fake_commits, net_lines_inserted, net_lines_deleted, net_words_inserted, net_words_deleted);
        puts("done.");
    }

    // obtain time
    time(&raw_time);
    local_time = localtime(&raw_time);
    
    // generate html
    printf("Generate html ...");
    // generate html
    fp = fopen(js["html"].get<std::string>().c_str(), "w");
    if(fp == NULL){
        puts("Failed to open html file.");
        js.clear();
        return false;
    }
    // generate file
    fprintf(fp,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
            "<meta charset=\"utf-8\"/>"
            "<title>Statistics of %s</title>" // 1
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"https://cdn.datatables.net/1.10.20/css/jquery.dataTables.min.css\">" // https://datatables.net/manual/styling/theme-creator
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"https://cdn.datatables.net/buttons/1.6.1/css/buttons.dataTables.min.css\">"
            "<link href=\"style.css\" rel=\"stylesheet\" type=\"text/css\">"
        "</head>"
        "<body>"
            "<header>"
                "<h1>%s</h1>" // 2
                "<p>Accessed at %04d-%02d-%02d %02d:%02d:%02d from git repository.</p>" // 3, 4, 5, 6, 7, 8
            "</header>"
            "<main>"
                "<h2>%s</h2>" // 9
                "<p>Please follow the file format of diary, otherwise it cannot detect attendence correctly.<br/>"
                "Note that merges won\'t give unfair points.<br/>"
                "<font class=\"fake\">This color</font> means you have fake commit.</p>"
                "<table id=\"table1\" class=\"display\">",
        js["title"].get<std::string>().c_str(),
        js["title"].get<std::string>().c_str(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
        js["subtitle"].get<std::string>().c_str()
    );
    // table
    fprintf(fp, "<thead><tr><th></th>"
                    "<th>Authors</th><th>Semester</th><th>Fake Commits</th><th>Invalid Commits</th><th>Valid Commits</th>" // valid commit for file ext not included?
                    "<th>Line Inserted</th><th>Line Deleted</th><th>Word Inserted</th><th>Word Deleted</th><th>Total GIT Score</th>"
                "</tr></thead>");
    fprintf(fp, "<tbody>");
    for(const auto &x: contributors){
        fprintf(fp, "<tr data-child-value=\"<table>");
        fprintf(fp, "<thead><tr><th>Filename</th><th>Fake commits</th><th>Invalid commits</th><th>Valid commits</th>"
                    "<th>Line inserted</th><th>Line deleted</th><th>Word inserted</th><th>Word deleted</th></thead>");
        fprintf(fp, "<tbody>");
        for(int i = 0; i < x.filenames.size(); ++i){
            const Statistics &fstat = x.filesstat[i];
            fprintf(fp, "<tr><td>%s</td><td>%d</td><td>%d</td><td>%d</td>"
                        "<td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                        x.filenames[i].c_str(), 0, 0, fstat.num_commits,
                        fstat.lines_inserted, fstat.lines_deleted, fstat.words_inserted, fstat.words_deleted);
        }
        fprintf(fp, "</tbody>");
        fprintf(fp, "</table>\"><td class=\"details-control\"></td>");

        fprintf(fp, "<td>%s</td><td>%s</td><td %s>%d</td><td>%d</td><td>%d</td>"
                    "<td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td>",
                    x.name.c_str(), x.label[0].c_str(), x.num_fake_commits > 0? "class=\"fake\"" : " ", x.num_fake_commits, 0, x.stats[0].num_commits,
                    x.stats[0].lines_inserted, x.stats[0].lines_deleted, x.stats[0].words_inserted, x.stats[0].words_deleted,
                    w[0] * x.stats[0].num_commits + w[1] * x.stats[0].lines_inserted + w[2] * x.stats[0].lines_deleted + w[3] * x.stats[0].words_inserted + w[4] * x.stats[0].words_deleted);
        fprintf(fp, "</tr>");
    }
    fprintf(fp, "</tbody>");
    // residual
    fprintf(fp, "</table>"
            "</main>"
            "<footer><p id=\"total_authors\">&#x2716; Total authors: %lu</p></footer>"
            "<script type=\"text/javascript\" src=\"https://code.jquery.com/jquery-3.3.1.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdn.datatables.net/1.10.20/js/jquery.dataTables.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdn.datatables.net/buttons/1.6.1/js/dataTables.buttons.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdnjs.cloudflare.com/ajax/libs/jszip/3.1.3/jszip.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdnjs.cloudflare.com/ajax/libs/pdfmake/0.1.53/pdfmake.min.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdnjs.cloudflare.com/ajax/libs/pdfmake/0.1.53/vfs_fonts.js\"></script>"
            "<script type=\"text/javascript\" charset=\"utf8\" src=\"https://cdn.datatables.net/buttons/1.6.1/js/buttons.html5.min.js\"></script>"
            "<script>$(document).ready(function(){"
                "var table = $(\'#table1\').DataTable({"
                    "dom: \'Blfrtip\',"// https://datatables.net/reference/option/dom, https://datatables.net/examples/basic_init/dom.html, https://datatables.net/manual/styling/
                    "buttons: ["
                        "\'copyHtml5\',"
                        "{extend:\'excelHtml5\',title:\'%04d%02d%02d_%02d%02d%02d_%s\'},"
                        "{extend:\'csvHtml5\',title:\'%04d%02d%02d_%02d%02d%02d_%s\'}"
                    "]"
                "});"
                "$(\'#table1\').on(\'click\', \'td.details-control\', function(){"
                    "var tr = $(this).closest(\'tr\');"
                    "var row = table.row(tr);"
                    "if(row.child.isShown()){ row.child.hide(); tr.removeClass(\'shown\'); }"
                    "else{ row.child(tr.data(\'child-value\')).show(); tr.addClass(\'shown\'); }"
                "});"
            "});</script>"
        "</body>"
        "</html>",
        contributors.size(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec, js["export"].get<std::string>().c_str(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec, js["export"].get<std::string>().c_str()
    );
    fclose(fp);
    puts("done.");

    js.clear();
    return true;
}

// bool remove_fake_commit(){} // load fake commit id from file (manually record in json), and then count lines and words in that commit and subtract from total
// bool detect_potential_fake_commit(){} // too many lines in one commit, ...

#endif // __GITSTAT_HPP__



