#ifndef _GITSTAT_HPP_V3_
#define _GITSTAT_HPP_V3_

#include "json.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
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
    std::vector<std::vector<Statistics>> statistics;
    int lines_inserted, lines_deleted, num_commits, words_inserted, words_deleted;
    int net_words_count, weeks_with_commits;
    float w_num_commits, w_lines_inserted, w_lines_deleted, w_words_inserted, w_words_deleted;
    FILE *fp;

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

    statistics.reserve(contributors.size());
    for(int i = 0; i < contributors.size(); ++i)
        statistics[i].reserve(queries.size());
    for(int i = 0; i < contributors.size(); ++i){
        printf("Get data for %s\n", contributors[i].name.c_str());
        for(auto &x: queries){
            for(auto &y: contributors[i].email){
                get_lines_statistics(repository.c_str(), y.c_str(), x.since.c_str(), x.until.c_str(), lines_inserted, lines_deleted, num_commits);
                get_words_statistics(repository.c_str(), y.c_str(), x.since.c_str(), x.until.c_str(), words_inserted, words_deleted);
                if(lines_inserted != 0 || lines_deleted != 0 || num_commits != 0 || words_inserted != 0 || words_deleted != 0)
                    break;
            }
            statistics[i].push_back(Statistics(num_commits, lines_inserted, lines_deleted, words_inserted, words_deleted));
        }
    }
    w_num_commits = js["weights"]["num_commits"].get<float>();
    w_lines_inserted = js["weights"]["lines_inserted"].get<float>();
    w_lines_deleted = js["weights"]["lines_deleted"].get<float>();
    w_words_inserted = js["weights"]["words_inserted"].get<float>();
    w_words_deleted = js["weights"]["words_deleted"].get<float>();
    // printf("weights = %f, %f, %f, %f\n", w_num_commits, w_lines_inserted, w_lines_deleted, w_words_inserted, w_words_deleted);

    fp = fopen(js["html"].get<std::string>().c_str(), "w");
    fprintf(fp, "<html><body>\n");
    // header
    fprintf(fp, "Authors, Semester, ");
    for(auto &x: queries)
        fprintf(fp, "%s, ", x.name.c_str());
    fprintf(fp, "Net words count, Weeks with commits, GIT Score\n");
    // body
    for(int i = 0; i < contributors.size(); ++i){
        printf("Generate statistics for %s\n", contributors[i].name.c_str());
        fprintf(fp, "%s, %s, ", contributors[i].name.c_str(), contributors[i].semester.c_str());
        net_words_count = weeks_with_commits = 0;
        for(int j = 0; j < queries.size(); ++j){
            fprintf(fp, "%d, ", statistics[i][j].words_inserted + statistics[i][j].words_deleted);
            net_words_count += statistics[i][j].words_inserted + statistics[i][j].words_deleted;
            if(statistics[i][j].num_commits > 0)
                ++weeks_with_commits;
        }
        fprintf(fp, "%d, %d, %d\n", net_words_count, weeks_with_commits, 0); // haven't set rule for total commit score
    }
    // end
    fprintf(fp, "</body></html>");
    fclose(fp);

    return true;
}

#endif