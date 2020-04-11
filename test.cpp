// g++ -std=c++11 test.cpp -o test.exe
#include "gitstat.hpp"

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Wrong input arguments.\n");
        return 1;
    }
    switch(atoi(argv[1])){
        case 0: generate_statistics_queries(argv[2]); break;
        case 1: generate_statistics_summary(argv[2]); break;
    }
    return 0;
}