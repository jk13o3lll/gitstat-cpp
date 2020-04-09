#ifndef __GITSTAT_HPP__
#define __GITSTAT_HPP__

#include "json.hpp"
using json = nlohmann::json;

#include <cstdio>   // pipe also included here
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <ctime>

#include <string> // haven't support UTF8, git log return utf8 as default
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#define popen _popen
#define pclose _pclose
#endif

#define MAX_CMD 1024

// git log --name-only --oneline
// include *.txt, *.md
// git log ... -- "*.txt" "some_dir/*.md" "some_dir2"
// exclude *.txt (case sensitive, insensitve)
// git log ... -- ":!*.txt"
// git log ... -- ":(exclude,icase)*.txt"
const char pathspec_figure[] = "\":(icase)*.pdf\" \":(icase)*.eps\" \":(icase)*.png\" \":(icase)*.jpg\"";
const char pathspec_code[] = "\":(icase)*.mat\" \":(icase)*.py\" \":(icase)*.cpp\" \":(icase)*.c\" \":(icase)*.h\"";
const char pathspec_text[] = "\":(icase)*.tex\" \":(icase)*.md\" \":(icase)*.txt\"";


bool exec(const char *cmd, std::string &result){
    FILE *pipe = popen(cmd, "r");
    if(!pipe){ printf("Failed to open a pipe."); return false; }
    for(result.clear(); !feof(pipe); result.push_back(fgetc(pipe)));
    pclose(pipe);
    return true;
}

/*
    repo_dir: directory of the repository
    git_name: .git directory
*/
bool pull_from_repository(const char *repo_dir, const char *git_dir = NULL){
    char cmd[MAX_CMD];
    std::string result;

    printf("Pull from the repository ... ");
    sprintf(cmd, "git -C \"%s\" --git-dir=\"%s\" --no-pager pull 2>&1",
        repo_dir, git_dir != NULL? git_dir : ".git");
    if(!exec(cmd, result)) return false;
    if(strstr(result.c_str(), "fatal") != NULL){
        printf("Failed to pull from the repository.\n%s\n", result.c_str());
        return false;
    }
    printf("done.\n%s\n", result.c_str());
    return true;
}

bool get_lines_stat(const char *repo_dir, const char *author_name,
                    const char *since, const char *until,
                    int &num_commits, int &lines_inserted, int &lines_deleted,
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
    tmp = num_commits = lines_inserted = lines_deleted = 0;
    for(const char *p= result.c_str(); *p != '\0'; ++p)
        switch(*p){
            case ',':   tmp = atoi(++p);        break;
            case '+':   lines_inserted += tmp;  break;
            case '-':   lines_deleted += tmp;   break;
            case '\n':  ++num_commits;          break;
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

// bool generate_statistics(){}






#endif // __GITSTAT_HPP__