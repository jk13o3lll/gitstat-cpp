// Compile:
// g++ test.cpp -o test.exe

// Test inputs:
// "curl -s https://api.bitbucket.org/2.0/repositories/nordron/nordlinglab-gitstatistics/commits/master"

#include <cstdio>
#include <string>
#include <iostream>
#include "json.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #define popen _popen
    #define pclose _pclose
#endif

using namespace std;
using json = nlohmann::json;

bool exec(const char *cmd, string &result){
    char buffer[1024];
    FILE *pipe = popen(cmd, "r");
    result.clear();
    if(!pipe)
        return false;
    while(fgets(buffer, sizeof(buffer), pipe) != NULL)
        result.append(buffer);
    pclose(pipe);
    return true;
}

int main(int argc, char *argv[]){
    string response;
    json j;
    FILE *fp;
    const char prefix[] = "curl -s ";
    char request[256] = "https://api.bitbucket.org/2.0/repositories/nordlinglab/nordlinglab-demotransferlearning/commits";
    // char request[256] = "https://api.bitbucket.org/2.0/repositories/nordron/nordlinglab-gitstatistics/commits/master";
    char command[256];

    response.reserve(65536);

    while(1){
        sprintf(command, "%s%s", prefix, request);
        exec(command, response);
        j = json::parse(response.c_str());
        printf("There are %d commits in this response.\n", j["values"].size());
        if(j["next"] == nullptr){
            puts("There is no next page.");
            break;
        }
        else{
            memcpy(request, j["next"].get<string>().c_str(), j["next"].get<string>().size() + 1);
            printf("Next: %s\n", request);
        }
        system("pause");
    }

    // if(argc != 2){
    //     fprintf(stderr, "Wrong input arguments.\n");
    //     return 1;
    // }
    // exec(argv[1], result);
    // puts(result.c_str());

    return 0;
}
