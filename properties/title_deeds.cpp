#include <iostream>
#include <fstream>
#include <map>

#include "title_deeds.h"

std::map<Square*, Property*> create_title_deeds(std::string config_file)
{
    std::map<Square*, Property*> title_deeds;
    std::string line_buffer;

    std::ifstream config(config_file);
    if(!config){
        throw std::runtime_error("Couldn't open "+ config_file);
    }

    while(getline(config, line_buffer))
    {
        std::cout << line_buffer << std::endl; 
    }
    return title_deeds;
}
