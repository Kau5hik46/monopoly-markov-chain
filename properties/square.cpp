#include "square.h"

class Square
{
    private:
        int __position;
        string __name;
        ColorGroup __color;
        list<Player*> __occupants;
        map<Square*, float> __redirects;
        

    public:
    Buildings buildings;

    // Constructor, getters, setters, and other methods
    Square(int position, string name, ColorGroup color)
    {
        this->__position = position;
        this->__name = name;
        this->__color = color;
        this->__occupants = list<Player*>();
        this->__redirects = map<Square*, float>();
    }
    ~Square()
    {
    }

    void fall(Player* player)
    {
        __occupants.push_back(player);
    }

    void leave(Player* player)
    {
        __occupants.remove(player);
    }
};