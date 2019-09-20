#include "gitstat.hpp"

int main(int argc, char *argv[]){
    if(argc != 2)
        fprintf(stderr, "Wrong input arguments.\n");
    generate_statistics(argv[1]);
    
    return 0;
}