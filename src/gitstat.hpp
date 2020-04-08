#ifndef _GITSTAT_HPP_
#define _GITSTAT_HPP_

#include "json.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;

#if defined(_WIN32) || defined(_WIN64)
    #define popen _popen
    #define pclose _pclose
#endif

struct Statistics{
    Statistics(int num_commits_, int lines_inserted_, int lines_deleted_, int words_inserted_, int words_deleted_, bool has_diary_):
        num_commits(num_commits_),
        lines_inserted(lines_inserted_),
        lines_deleted(lines_deleted_),
        words_inserted(words_inserted_),
        words_deleted(words_deleted_),
        has_diary(has_diary_){}

    int num_commits;
    int lines_inserted;
    int lines_deleted;
    int words_inserted;
    int words_deleted;
    bool has_diary;
};

struct Query{
    Query(const json &js):
        name(js["name"].get<std::string>()),
        since(js["duration"][0].get<std::string>()),
        until(js["duration"][1].get<std::string>()),
        attendance_criteria(js["attendance_criteria"].get<int>()){}

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
};


bool exec(const char *cmd, std::string &result){
    FILE *pipe = popen(cmd, "r");
    if(!pipe)
        return false;
    for(result.clear(); !feof(pipe); result.push_back(fgetc(pipe)));
    pclose(pipe);
    return true;
}

bool update_repo(const char *repo_dir, const char *git_dir = NULL){
    char cmd[1024];
    std::string result;

    printf("Updateing git repository ...");
    sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager pull 2>&1",
        repo_dir, git_dir == NULL? ".git" : git_dir);
    exec(cmd, result);
    if(strstr(result.c_str(), "fatal") != NULL){
        printf("failed.\n%s\n", result.c_str());
        return false;
    }else{
        printf("done.\n%s\n", result.c_str());
        return true;
    }
}

int get_int_in_front(const char *p){
    int num = 0;
    for(int base = 1; *p >= '0' && *p <='9'; --p, base *= 10)
        num += (*p - '0') * base;
    return num;
}
int get_date_in_front(const char *p){ // ex. 20120203, -1 is None
    const char *q = p - 10;
    int date = 0;
    char token;
    if(q[0] < '0' || q[0] > '9')    return -1;  date = date * 10 + q[0] - '0';
    if(q[1] < '0' || q[1] > '9')    return -1;  date = date * 10 + q[1] - '0';
    if(q[2] < '0' || q[2] > '9')    return -1;  date = date * 10 + q[2] - '0';
    if(q[3] < '0' || q[3] > '9')    return -1;  date = date * 10 + q[3] - '0';
    if(q[4] != '-' && q[4] != ' ' && q[4] != '_')    return -1;  token = q[4];
    if(q[5] < '0' || q[5] > '9')    return -1;  date = date * 10 + q[5] - '0';
    if(q[6] < '0' || q[6] > '9')    return -1;  date = date * 10 + q[6] - '0';
    if(q[7] != token)   return -1;
    if(q[8] < '0' || q[8] > '9')    return -1;  date = date * 10 + q[8] - '0';
    if(q[9] < '0' || q[9] > '9')    return -1;  date = date * 10 + q[9] - '0';
    return date;
}

bool get_lines_stat(const char *repo_dir, const char *auther_name,
                    const char *since, const char *until,
                    int &lines_inserted, int &lines_deleted, int &num_commits,
                    const char *git_dir = NULL){
    int len;
    char cmd[1024];
    std::string result;

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log --date=local --pretty=\"\" --shortstat --author=\"%s\" --no-merges",
        repo_dir, git_dir == NULL? ".git" : git_dir, auther_name);
    if(since != NULL)
        len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)
        len += sprintf(cmd + len, " --until=\"%s\"", until);
    exec(cmd, result);
    // printf("%s\n", result.c_str());
    
    lines_inserted = lines_deleted = num_commits = 0;
    for(const char *p = result.c_str(); p[1] != '\0'; ++p){
        if(p[1] == 'i' && p[0] == ' ')
            lines_inserted += get_int_in_front(p - 1);
        else if(p[1] == 'd' && p[0] == ' ')
            lines_deleted += get_int_in_front(p - 1);
        else if(p[0] == '\n')
            ++num_commits;
    }
    return true;
}

bool get_words_stat(const char *repo_dir, const char *auther_name,
                    const char *since, const char *until,
                    int &words_inserted, int &words_deleted,
                    const char *git_dir = NULL){
    int len;
    char cmd[1024];
    std::string result;

    len = sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager log --date=local --pretty=\"\" -p --word-diff=porcelain --author=\"%s\" --no-merges",
        repo_dir, git_dir == NULL? ".git" : git_dir, auther_name);
    if(since != NULL)
        len += sprintf(cmd + len, " --since=\"%s\"", since);
    if(until != NULL)
        len += sprintf(cmd + len, " --until=\"%s\"", until);
    exec(cmd, result);
    // printf("%s\n", result.c_str());

    words_inserted = words_deleted = 0;
    for(const char *p = result.c_str(); *p != '\0'; ++p){
        // seperator: ' ', '\n', '\t', '\0'
        // criteria: read one non-seperator with one seperator followed behind, we count one word
        // (Punctuations like '.', ',', ';', ... should be followed by a seperator)
        const char *q;
        if(p[0] == '+' && p[1] != '+'){ // count every modified
            for(q = p + 1; *q != '\n' && *q != '\0'; ++q)
                if(q[0] != ' ' && q[0] != '\t' && (q[1] == ' ' || q[1] == '\t' || q[1] == '\n' || q[1] == '\0'))
                    ++words_inserted;
        }else if(p[0] == '-' && p[1] != '-'){
            for(q = p + 1; *q != '\n' && *q != '\0'; ++q)
                if(q[0] != ' ' && q[0] != '\t' && (q[1] == ' ' || q[1] == '\t' || q[1] == '\n' || q[1] == '\0'))
                    ++words_deleted;
        }else
            for(q = p; q[0] != '\n' && q[0] !='\0'; ++q);  // read until next line or EOF
        if(*q == '\0')
            break;
        p = q;
    }
    return true;
}

bool generate_stat(const char *config_dir){
    json js;
    std::ifstream fin(config_dir);
    FILE *fout, *fp;
    char buffer[1001];
    std::string repo_dir;
    std::vector<Query> queries;
    std::vector<Contributor> contributors;
    float w_lines_ins, w_lines_del, w_n_commits, w_words_ins, w_words_del; // weights
    int lines_ins, lines_del, n_commits, words_ins, words_del;
    int net_words_count, weeks_with_commits;
    std::vector<int> dates;
    int since, until;
    bool has_diary;
    time_t raw_time;
    tm *local_time;

    // load config file
    if(!fin.is_open()){
        fprintf(stderr, "Failed to open config file.\n");
        return false;
    }
    fin >> js;
    fin.close();

    // extract repo dir
    repo_dir = js["repository"].get<std::string>();
    update_repo(repo_dir.c_str());
    
    // extract more info
    printf("%d queries, %d contributors\n\n", js["queries"].size(), js["contributors"].size());
    for(const auto &x: js["queries"])
        queries.push_back(Query(x));
    for(const auto &x: js["contributors"])
        contributors.push_back(Contributor(x));
    w_n_commits = js["weights"]["num_commits"].get<float>();
    w_lines_ins = js["weights"]["lines_inserted"].get<float>();
    w_lines_del = js["weights"]["lines_deleted"].get<float>();
    w_words_ins = js["weights"]["words_inserted"].get<float>();
    w_words_del = js["weights"]["words_deleted"].get<float>();

    // obtain data (check diary attendance by finding date between "# " ...  "\n" substring)
    for(auto &x: contributors){
        printf("Get data for %s ...", x.name.c_str());
        // check diary
#if defined(_WIN32) || defined(_WIN64)
        fp = fopen((repo_dir + "\\" + x.diary).c_str(), "r");
#else
        fp = fopen((repo_dir + "/" + x.diary).c_str(), "r");
#endif
        dates.clear();
        if(fp == NULL)
            printf("(no diary founded) ");
        else{
            // extract all date in header
            while(fgets(buffer, 1000, fp) != NULL){
                if(buffer[0] == '#' && strlen(buffer) > 10){
                    int tmp;
                    for(const char *p = buffer + 10; *p != '\0'; ++p){
                        tmp = get_date_in_front(p);
                        if(tmp != -1)
                            dates.push_back(tmp);
                    }
                }
            }
            fclose(fp);
        }
        // query
        for(const auto &y: queries){
            for(const auto &z: x.email){
                get_lines_stat(repo_dir.c_str(), z.c_str(), y.since.c_str(), y.until.c_str(), lines_ins, lines_del, n_commits);
                get_words_stat(repo_dir.c_str(), z.c_str(), y.since.c_str(), y.until.c_str(), words_ins, words_del);
                if(n_commits != 0)
                    break;
            }
            // check attendance
            if(y.attendance_criteria == 0){ // has diary within duration
                has_diary = false;
                since = get_date_in_front(y.since.c_str() + 10);
                until = get_date_in_front(y.until.c_str() + 10);
                for(const auto &d: dates)
                    if(d >= since && d <= until){
                        has_diary = true;
                        break;
                    }
            }
            else if(y.attendance_criteria == 1){ // has diary within duration & commits within duration
                has_diary = false;
                if(n_commits > 0){
                    since = get_date_in_front(y.since.c_str() + 10);
                    until = get_date_in_front(y.until.c_str() + 10);
                    for(const auto &d: dates)
                        if(d >= since && d <= until){
                            has_diary = true;
                            break;
                        }
                }
            }
            else if(y.attendance_criteria == -1)
                has_diary = false;
            else
                has_diary = true;
            // save statistics
            x.stats.push_back(Statistics(n_commits, lines_ins, lines_del, words_ins, words_del, has_diary));
        }
        puts("done.");
    }

    // obtain time
    time(&raw_time);
    local_time = localtime(&raw_time);
    
    // open file to output
    fout = fopen(js["html"].get<std::string>().c_str(), "w");
    if(fout == NULL){
        fprintf(stderr, "Failed to open html file.\n");
        return false;
    }

    // generate header
    fprintf(fout,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>Statistics</title>"
        "<meta charset=\"UTF-8\">"
        "<link href=\"https://fonts.googleapis.com/icon?family=Material+Icons\" rel=\"stylesheet\">"
        "<link href=\"https://fonts.googleapis.com/css?family=Roboto|Noto+Serif+TC\" rel=\"stylesheet\">"
        "<link href=\"css/style.css\" rel=\"stylesheet\" type=\"text/css\">"
        "</head>"
        "<body>"
        "<header>"
        "<h1>%s</h1>"
        "<p>Accessed at %04d-%02d-%02d %02d:%02d:%02d from git repository.</p>"
        "</header>",
        js["title"].get<std::string>().c_str(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec
    );

    // generate main (begin, table header)
    fprintf(fout,
        "<main>"
        "<h1>Statistics of the Commits on Bitbucket</h1>"
        "<p id=\"notes\">Note that merges won't give unfair points. Also, please follow the file format of diary, otherwise it cannot detect attendence corretly.</p>"
        "<p>Cells in gray represent missing diary. In diary, please use the date of that lecture.</p>"
        "<table id=\"statistics\">"
        "<tr>"
        "<th onclick=\"sortTable(0)\" class=\"normal\">Authors <i class=\"material-icons\">unfold_more</i></th>"
        "<th onclick=\"sortTable(1)\" class=\"normal\">Semester <i class=\"material-icons\">unfold_more</i></th>"
    );
    for(int i = 0; i < queries.size(); ++i)
        fprintf(fout, "<th onclick=\"sortTable(%d)\" class=\"normal\">%s <i class=\"material-icons\">unfold_more</i></th>", i + 2, queries[i].name.c_str());
    fprintf(fout, "<th onclick=\"sortTable(%d)\" class=\"normal\">Net words count <i class=\"material-icons\">unfold_more</i></th>", 2 + queries.size());
    fprintf(fout, "<th onclick=\"sortTable(%d)\" class=\"normal\">Weeks with commits <i class=\"material-icons\">unfold_more</i></th>", 3 + queries.size());
    fprintf(fout, "<th onclick=\"sortTable(%d)\" class=\"normal\">GIT Score <i class=\"material-icons\">unfold_more</i></th></tr>", 4 + queries.size());

    // generate main (table data, end)
    for(const auto &x: contributors){
        printf("Generate statistics for %s\n", x.name.c_str());
        fprintf(fout, "<tr><td>%s</td><td>%s</td>", x.name.c_str(), x.label[0].c_str());
        net_words_count = weeks_with_commits = 0;
        for(const auto &y: x.stats){
            if(y.has_diary)
                fprintf(fout, "<td>%d</td>", y.words_inserted + y.words_deleted);
            else
                fprintf(fout, "<td class=\"nodiary\">%d</td>", y.words_inserted + y.words_deleted);
            net_words_count += y.words_inserted + y.words_deleted;
            if(y.num_commits > 0)
                ++weeks_with_commits;
        }
        fprintf(fout, "<td>%d</td><td>%d</td><td>%f</td></tr>", net_words_count, weeks_with_commits, 0.0f); // haven't set rule for total commit score
    }
    fprintf(fout,
        "</table>"
        "<p id=\"total_authors\">X Total authors: %d</p>", // 0
        js["contributors"].size()
    );

    // generate residual parts (footer, js script (sort table))
    fprintf(fout,
        "<footer></footer>"
        "<script>"
        "function sortTable(n) {"
            "var table, rows, switching, i, j, x, y, shouldSwitch, headers, descend;"
            "table = document.getElementById(\"statistics\");"
            "rows = table.rows;"
            "headers = rows[0].cells;"
            "if(headers[n].className == \"descend\") {"
                "headers[n].className = \"ascend\";"
                "headers[n].getElementsByTagName(\"I\")[0].innerHTML = \"expand_less\";"
                "descend = 0;"
            "}"
            "else {"
                "headers[n].className = \"descend\";"
                "headers[n].getElementsByTagName(\"I\")[0].innerHTML = \"expand_more\";"
                "descend = 1;"
            "}"
            "for (i = 0; i < headers.length; i++) {"
                "if (i != n && headers[i].className != \"normal\") {"
                    "headers[i].className = \"normal\";"
                    "headers[i].getElementsByTagName(\"I\")[0].innerHTML = \"unfold_more\";"
                "}"
            "}"
            "switching = true;"
            "while (switching) {"
                "switching = false;"
                "rows = table.rows;"
                "for (i = 1; i < (rows.length - 1); i++) {"
                    "shouldSwitch = false;"
                    "x = rows[i].getElementsByTagName(\"TD\")[n];"
                    "y = rows[i + 1].getElementsByTagName(\"TD\")[n];"
                    "if (n != 0) {"
                        "if (descend == 0 && Number(x.innerHTML) > Number(y.innerHTML)) {"
                            "shouldSwitch = true;"
                            "break;"
                        "}"
                        "else if (descend == 1 && Number(x.innerHTML) < Number(y.innerHTML)) {"
                            "shouldSwitch = true;"
                            "break;"
                        "}"
                    "}"
                    "else {"
                        "if (descend == 0 && x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {"
                            "shouldSwitch = true;"
                            "break;"
                        "}"
                        "else if (descend == 1 && x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {"
                            "shouldSwitch = true;"
                            "break;"
                        "}"
                    "}"
                "}"
                "if (shouldSwitch) {"
                    "rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);"
                    "switching = true;"
                "}"
            "}"
        "}"
        "</script>"
        "</body>"
        "</html>"
    );
    fclose(fout);

    return true;
}

#endif