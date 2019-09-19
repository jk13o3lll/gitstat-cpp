#include "gitstat.hpp"

#define REPOSITORY_DIRECTORY "C:\\Users\\Jack Wang\\Repositories\\nordlinglab-profskills\\.git"

int main(int argc, char *argv[]){
    int lines_inserted, lines_deleted, words_inserted, words_deleted;

    update_repository(REPOSITORY_DIRECTORY);
    
    get_lines_statistics(REPOSITORY_DIRECTORY,
                         "jack.wang@nordlinglab.org",
                         "2019-08-01T06:00:00+08:00",
                         "2020-01-31T06:00:00+08:00",
                         lines_inserted,
                         lines_deleted);
    printf("Total lines: %d insertions, %d deletions.\n", lines_inserted, lines_deleted);

    get_words_statistics(REPOSITORY_DIRECTORY,
                         "jack.wang@nordlinglab.org",
                         "2019-08-01T06:00:00+08:00",
                         "2020-01-31T06:00:00+08:00",
                         words_inserted,
                         words_deleted);
    printf("Total words: %d insertions, %d deletions. \n", words_inserted, words_deleted);

    return 0;
}

// use email to find, also make match list?
// sam sheu <samsheu1997@gmail.com>