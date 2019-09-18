#ifndef _GITSTAT_H_V2_
#define _GITSTAT_H_V2_

#include "json.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
    #define popen _popen
    #define pclose _pclose
#endif

using json = nlohmann::json;

const float weights[] = {0.3, 0.2, 0.15, 0.2, 0.15};

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
    printf("Updating git repository ...");
    sprintf(command, "git --no-pager --git-dir=\"%s\" pull origin", repository_directory);
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

// haven't consider time
// https://stackoverflow.com/questions/7651644/git-timezone-and-timestamp-format
bool get_lines_statistics(const char *repository_directory, const char *auther_name, const char *since, const char *before, int &lines_inserted, int &lines_deleted){
    char command[1024];
    std::string result;
    sprintf(command, "git --no-pager --git-dir=\"%s\" log --pretty=\"\" --shortstat --author=\"%s\" --no-merges", repository_directory, auther_name);
    exec(command, result);
    printf("%s\n", result.c_str());
    lines_inserted = lines_deleted = 0;
    for(const char *p = strstr(result.c_str(), "insertion"); p != NULL; p = strstr(p + 9, "insertion"))
        lines_inserted += get_number_reverse(p - 2);
    for(const char *p = strstr(result.c_str(), "deletion"); p != NULL; p = strstr(p + 8, "deletion"))
        lines_deleted += get_number_reverse(p - 2);
    return true;
}

bool get_words_statistics(const char *repository_directory, const char *auther_name, const char *since, const char *before, int &words_inserted, int &words_deleted){
    char command[1024];
    std::string result;
    sprintf(command, "git --no-pager --git-dir=\"%s\" log --pretty=\"\" -p --word-diff=porcelain --author=\"%s\" --no-merges", repository_directory, auther_name);
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
// fwrite(result.c_str(), 1, result.length() + 1, stdout); // I don't know why fwrite cannot work for long string
// puts(result.c_str()); // I don't know why puts() cannot work for long string (need formating?)


// have config_profskill_2019.json (directory for repository, for specific folder, for specific time duration)
// make contributers_profskill_2019.json (name, email)
// load information from that json and get statistics.
// then generate a javascript webpage based on those statistics

#endif