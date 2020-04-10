// regex_search example
#include <iostream>
#include <string>
#include <regex>
#include <cstdlib>

#include <stdio.h>

bool find_time(const char *str, time_t &t){
    std::smatch m; // smatch also separate result in each ()
    std::string s(str);
    std::regex e1("(\\d{4})(\\d{2})(\\d{2})");
    std::regex e2("(\\d{4})(-|/)(\\d{1,2})(-|/)(\\d{1,2})");
    int year, month, day;
    struct tm *timeinfo;

    if(std::regex_search (s,m,e1)){ // find first one
        year = atoi(m.str(1).c_str());
        month = atoi(m.str(2).c_str());
        day = atoi(m.str(3).c_str());
    }
    else if(std::regex_search (s,m,e2)){ // find first one
        year = atoi(m.str(1).c_str());
        month = atoi(m.str(3).c_str());
        day = atoi(m.str(5).c_str());
    }
    else return false;
    
    // check wether year, month, and day are valid
    if(month > 12 || month < 1 || day > 31 || day < 1) return false;
    else if(month == 2){
        if(day > ((year % 4 == 0 && year % 100 !=0) || year % 400 == 0? 29 : 28))
            return false;
    }
    else if(month == 4 || month == 6 || month == 9 || month == 11){
        if(day > 30)
            return false;
    }

    time(&t);
    timeinfo = localtime(&t);
    timeinfo->tm_year = year - 1900;
    timeinfo->tm_mon = month - 1;
    timeinfo->tm_mday = day;
    t = mktime(timeinfo); // timeinfo->tm_wday and tm_yday will be set

    return t != -1;
}

int main(){
    std::string s;
    time_t t;
    struct tm *timeinfo;

    printf("%c%c%c", 'a', '\0', 'b');
    
    // while (std::getline(std::cin, s)) {
    //     if(find_time(s.c_str(), t)){
    //         timeinfo = localtime(&t);
    //         std::cout << timeinfo->tm_year + 1900 << " "
    //                   << timeinfo->tm_mon + 1 << " "
    //                   << timeinfo->tm_mday << std::endl;
    //     }
    //     else
    //         std::cout << "No valid date." << std::endl;
    // }

    return 0;
}