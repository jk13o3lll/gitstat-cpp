// Compile:
// g++ test2.cpp gitstat.cpp -o test2.exe

#include "gitstat.hpp"

int main(int argc, char *argv[]){
    string response;
    FILE *fp;

    response.reserve(100000);
    http_get("repositories/nordlinglab/nordlinglab-demotransferlearning/commits", response, BITBUCKET_API_V2);
    // http_get("repositories/nordron/nordlinglab-gitstatistics/commits/master", response, BITBUCKET_API_V2);
    // http_get("https://api.bitbucket.org/2.0/repositories/nordron/nordlinglab-gitstatistics/commits/master", response);
    
    fp = fopen("response_test2.json", "w");
    fprintf(fp, "%s", response.c_str());
    fclose(fp);

    return 0;
}