// g++ -std=c++11 test.cpp -o test.exe
#include "gitstat.hpp"

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Wrong input arguments.\n");
        return 1;
    }
    generate_statistics(argv[1]);
    return 0;
}