#include "account.h"



class Bank : public Account
{
private:
    float __total_float;
    float __total_outstanding;      
    float __interest_rate;


public:
    Bank(vector<Asset*>&& assets)
    {
        this->__total_float = 150000000;
        this->__total_outstanding = 0.0f;
        this->__interest_rate = 0.06;
        for(int i = 0; i < assets.size(); i++)
        {
            this->buy(assets[i], assets[i]->face_value, assets[i]->interest_rate());
        }
    }
}