#include "gitstat.hpp"

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

bool http_get(const char *request, string &response, const char *service){
    char command[1024];
    sprintf(command, CURL_CMD_PREFIX"%s%s", service, request);
    return exec(command, response);
}