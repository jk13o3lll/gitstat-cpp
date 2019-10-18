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

struct Query{
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
        name(js["name"].get<std::string>()){
        for(const auto &x: js["email"])
            email.push_back(x.get<std::string>());
        for(const auto &x: js["label"])
            label.push_back(x.get<std::string>());
    }

    std::string name;
    std::vector<std::string> email;
    std::vector<std::string> label;
};

struct Statistics{
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
    FILE *fout;
    // data
    std::string repo_dir;
    std::vector<Query> queries;
    std::vector<Contributor> contributors;
    std::vector<Statistics> statistics[1024];   // statistics[i]: stat for contributor i
    float w_lines_ins, w_lines_del, w_n_commits, w_words_ins, w_words_del; // weights
    // tmp
    int lines_ins, lines_del, n_commits, words_ins, words_del;
    int net_words_count, weeks_with_commits;
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

    // obtain data
    for(int i = 0; i < contributors.size(); ++i)
        statistics[i].reserve(queries.size());
    for(int i = 0; i < contributors.size(); ++i){
        printf("(%d/%d) Get data for %s ... ", i + 1, contributors.size(), contributors[i].name.c_str());
        for(auto &x: queries){
            for(auto &y: contributors[i].email){
                get_lines_stat(repo_dir.c_str(), y.c_str(), x.since.c_str(), x.until.c_str(), lines_ins, lines_del, n_commits);
                get_words_stat(repo_dir.c_str(), y.c_str(), x.since.c_str(), x.until.c_str(), words_ins, words_del);
                if(n_commits != 0)
                    break;
            }
            statistics[i].push_back(Statistics(n_commits, lines_ins, lines_del, words_ins, words_del));
        }
        printf("done.\n");
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
    for(int i = 0; i < contributors.size(); ++i){
        printf("(%d/%d) Generate statistics for %s\n", i + 1, contributors.size(), contributors[i].name.c_str());
        fprintf(fout, "<tr><td>%s</td><td>%s</td>", contributors[i].name.c_str(), contributors[i].label[0].c_str());
        net_words_count = weeks_with_commits = 0;
        for(int j = 0; j < queries.size(); ++j){
            fprintf(fout, "<td>%d</td>", statistics[i][j].words_inserted + statistics[i][j].words_deleted);
            net_words_count += statistics[i][j].words_inserted + statistics[i][j].words_deleted;
            if(statistics[i][j].num_commits > 0)
                ++weeks_with_commits;
        }
        fprintf(fout, "<td>%d</td><td>%d</td><td>%f</td></tr>", net_words_count, weeks_with_commits, 0.0f); // haven't set rule for total commit score
    }
    fprintf(fout,
        "</table>"
        "<p id=\"total_authors\">X Total authors: %d</p>" // 0
        "<p id=\"notes\">Note that merges won't give unfair points.</p>"
        "</main>",
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