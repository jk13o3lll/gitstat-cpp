#ifndef _GITSTAT_HPP_V3_
#define _GITSTAT_HPP_V3_

#include "json.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <fstream>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
    #define popen _popen
    #define pclose _pclose
#endif

using json = nlohmann::json;

struct Query {
    Query(const json &js):
        name(js["name"].get<std::string>()),
        since(js["duration"][0].get<std::string>()),
        until(js["duration"][1].get<std::string>()){}

    std::string name;
    std::string since;
    std::string until;
};

struct Contributor{
    Contributor(const json &js):
        name(js["name"].get<std::string>()), semester(js["semester"].get<std::string>()){
        for(auto &x: js["email"])
            email.push_back(x.get<std::string>());
    }

    std::string name;
    std::vector<std::string> email;
    std::string semester;
};

struct Statistics {
    Statistics(int num_commits_, int lines_inserted_, int lines_deleted_, int words_inserted_, int words_deleted_):
        num_commits(num_commits_),
        lines_inserted(lines_inserted_), lines_deleted(lines_deleted_),
        words_inserted(words_inserted_), words_deleted(words_deleted_){}
    int num_commits;
    int lines_inserted;
    int lines_deleted;
    int words_inserted;
    int words_deleted;
};

bool exec(const char *cmd, std::string &result){
    char buffer[256];
    FILE *pipe = popen(cmd, "r");
    if(!pipe)
        return false;
    while(fgets(buffer, sizeof(buffer), pipe) != NULL)
        result.append(buffer);
    pclose(pipe);
    return true;
}

int get_number_reverse(const char *str){
    int number = 0;
    for(int base = 1; *str >= '0' && *str <= '9'; base *= 10){
        number += (*str - '0') * base;
        --str;
    }
    return number;
}

bool update_repository(const char *repository_directory){
    char command[1024];
    std::string result;

    // bug of git pull: https://stackoverflow.com/questions/5083224/git-pull-while-not-in-a-git-directory

    printf("Updating git repository ...");
    sprintf(command, "git -C \"%s\" --git-dir=\".git\" --no-pager pull", repository_directory);
    exec(command, result);
    if(strstr(result.c_str(), "fatal") != NULL){
        printf("failed.\n%s\n", result.c_str());
        return false;
    }
    else{
        printf("done.\n%s\n", result.c_str());
        return true;
    }
}

// git -C "C:\\Users\\Jack Wang\\Repositories\\nordlinglab-profskills" --git-dir=".git" --no-pager log --date=local --pretty="" --shortstat --author="jack.wang@nordlinglab.org" --no-merges
bool get_lines_statistics(const char *repository_directory, const char *auther_name, const char *since, const char *until, int &lines_inserted, int &lines_deleted, int &num_commits){
    char command[1024];
    std::string result;
    sprintf(command, "git -C \"%s\" --git-dir=\".git\" --no-pager log --date=local --pretty=\"\" --shortstat --author=\"%s\" --no-merges", repository_directory, auther_name);
    if(since != NULL)
        sprintf(command, "%s --since=\"%s\"", command, since);
    if(until != NULL)
        sprintf(command, "%s --until=\"%s\"", command, until);
    exec(command, result);
    // printf("%s\n", result.c_str());
    lines_inserted = lines_deleted = num_commits = 0;
    for(const char *p = strstr(result.c_str(), "insertion"); p != NULL; p = strstr(p + 9, "insertion"))
        lines_inserted += get_number_reverse(p - 2);
    for(const char *p = strstr(result.c_str(), "deletion"); p != NULL; p = strstr(p + 8, "deletion"))
        lines_deleted += get_number_reverse(p - 2);
    for(const char *p = strchr(result.c_str(), '\n'); p != NULL; p = strchr(p + 1, '\n'))
        ++num_commits;
    return true;
}

bool get_words_statistics(const char *repository_directory, const char *auther_name, const char *since, const char *until, int &words_inserted, int &words_deleted){
    char command[1024];
    std::string result;
    
    sprintf(command, "git -C \"%s\" --git-dir=\".git\" --no-pager log --date=local --pretty=\"\" -p --word-diff=porcelain --author=\"%s\" --no-merges", repository_directory, auther_name);
    if(since != NULL)
        sprintf(command, "%s --since=\"%s\"", command, since);
    if(until != NULL)
        sprintf(command, "%s --until=\"%s\"", command, until);
    exec(command, result);
    // printf("%s\n", result.c_str()); // or use while loop with putchar is ok

    words_inserted = words_deleted = 0;
    for(const char *p = result.c_str(); *p != '\0'; ++p){
        // seperator: ' ', '\n', '\t', '\0'
        // principle: read one non-seperator with one seperator followed behind, we count one word
        // (if include '.' ',' may count wrong for url or directory. Should I include these symbols?)
        const char *q;
        if(p[0] == '+' && p[1] != '+'){
            for(q = p + 1; *q != '\n' && *q != '\0'; ++q)
                if(q[0] != ' ' && q[0] != '\t' && (q[1] == ' ' || q[1] == '\t' || q[1] == '\n' || q[1] == '\0'))
                    ++words_inserted;
        }
        else if(p[0] == '-' && p[1] != '-'){
            for(q = p + 1; *q != '\n' && *q != '\0'; ++q)
                if(q[0] != ' ' && q[0] != '\t' && (q[1] == ' ' || q[1] == '\t' || q[1] == '\n' || q[1] == '\0'))
                    ++words_deleted;
        }
        else
            for(q = p; q[0] != '\n' && q[0] !='\0'; ++q);  // read until next line or EOF
        if(*q == '\0')
            break;
        p = q;
    }
    return true;
}

bool generate_statistics(const char *config){
    std::ifstream fs(config);
    json js;
    std::string repository;
    std::vector<Query> queries;
    std::vector<Contributor> contributors;
    std::vector<Statistics> statistics[10001];
    int lines_inserted, lines_deleted, num_commits, words_inserted, words_deleted;
    int net_words_count, weeks_with_commits;
    float w_num_commits, w_lines_inserted, w_lines_deleted, w_words_inserted, w_words_deleted;
    FILE *fp;
    time_t raw_time;
    tm *local_time;

    fs >> js;
    fs.close();
    repository = js["repository"].get<std::string>();

    update_repository(repository.c_str());

    printf("%d queries, %d contributors\n\n", js["queries"].size(), js["contributors"].size());

    for(auto &x: js["queries"])
        queries.push_back(Query(x));
    for(auto &x: js["contributors"])
        contributors.push_back(Contributor(x));

    // for(auto &x: queries)
    //     printf("%s: [%s] - [%s]\n", x.name.c_str(), x.since.c_str(), x.until.c_str());
    // for(auto &x: contributors){
    //     printf("%s:", x.name.c_str());
    //     for(auto &y: x.email)
    //         printf(" <%s>", y.c_str());
    //     putchar('\n');
    // }

    for(int i = 0; i < contributors.size(); ++i)
        statistics[i].reserve(queries.size());
    
    for(int i = 0; i < contributors.size(); ++i){
        printf("(%d/%d) Get data for %s ... ", i + 1, contributors.size(), contributors[i].name.c_str());
        for(auto &x: queries){
            for(auto &y: contributors[i].email){
                get_lines_statistics(repository.c_str(), y.c_str(), x.since.c_str(), x.until.c_str(), lines_inserted, lines_deleted, num_commits);
                get_words_statistics(repository.c_str(), y.c_str(), x.since.c_str(), x.until.c_str(), words_inserted, words_deleted);
                if(num_commits != 0)
                    break;
            }
            statistics[i].push_back(Statistics(num_commits, lines_inserted, lines_deleted, words_inserted, words_deleted));
        }
        printf("done.\n");
    }
    w_num_commits = js["weights"]["num_commits"].get<float>();
    w_lines_inserted = js["weights"]["lines_inserted"].get<float>();
    w_lines_deleted = js["weights"]["lines_deleted"].get<float>();
    w_words_inserted = js["weights"]["words_inserted"].get<float>();
    w_words_deleted = js["weights"]["words_deleted"].get<float>();
    // printf("weights = %f, %f, %f, %f\n", w_num_commits, w_lines_inserted, w_lines_deleted, w_words_inserted, w_words_deleted);

    // generate webpage
    time(&raw_time);
    local_time = localtime(&raw_time);
    fp = fopen(js["html"].get<std::string>().c_str(), "w");
    if(fp == NULL){
        fprintf(stderr, "Failed to open html file.\n");
        return false;
    }
    fprintf(fp,
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
        "</header>"
        "<main>"
        "<h1>Statistics of the Commits on Bitbucket</h1>"
        "<table id=\"statistics\">"
        "<tr>"
        "<th onclick=\"sortTable(0)\" class=\"normal\">Authors <i class=\"material-icons\">unfold_more</i></th>"
        "<th onclick=\"sortTable(1)\" class=\"normal\">Semester <i class=\"material-icons\">unfold_more</i></th>",
        js["title"].get<std::string>().c_str(),
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec
    );
    for(int i = 0; i < queries.size(); ++i)
        fprintf(fp, "<th onclick=\"sortTable(%d)\" class=\"normal\">%s <i class=\"material-icons\">unfold_more</i></th>", i + 2, queries[i].name.c_str());
    fprintf(fp, "<th onclick=\"sortTable(%d)\" class=\"normal\">Net words count <i class=\"material-icons\">unfold_more</i></th>", 2 + queries.size());
    fprintf(fp, "<th onclick=\"sortTable(%d)\" class=\"normal\">Weeks with commits <i class=\"material-icons\">unfold_more</i></th>", 3 + queries.size());
    fprintf(fp, "<th onclick=\"sortTable(%d)\" class=\"normal\">GIT Score <i class=\"material-icons\">unfold_more</i></th></tr>", 4 + queries.size());
    for(int i = 0; i < contributors.size(); ++i){
        printf("(%d/%d) Generate statistics for %s\n", i + 1, contributors.size(), contributors[i].name.c_str());
        fprintf(fp, "<tr><td>%s</td><td>%s</td>", contributors[i].name.c_str(), contributors[i].semester.c_str());
        net_words_count = weeks_with_commits = 0;
        for(int j = 0; j < queries.size(); ++j){
            fprintf(fp, "<td>%d</td>", statistics[i][j].words_inserted + statistics[i][j].words_deleted);
            net_words_count += statistics[i][j].words_inserted + statistics[i][j].words_deleted;
            if(statistics[i][j].num_commits > 0)
                ++weeks_with_commits;
        }
        fprintf(fp, "<td>%d</td><td>%d</td><td>%f</td></tr>", net_words_count, weeks_with_commits, 0.0f); // haven't set rule for total commit score
    }
    fprintf(fp,
        "</table>"
        "<p id=\"total_authors\">X Total authors: %d</p>" // 0
        "<p id=\"notes\">Note that merges won't give unfair points.</p>"
        "</main>"
        "<footer>"
        "</footer>"
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
        "</html>",
        js["contributors"].size()
    );
    fclose(fp);

    return true;
}

#endif