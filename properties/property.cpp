#include "property.h"


// Define the class Square
class Property : Asset
{
    private:
        Square __square;
        float __construction_cost;
        const vector<float> rents;

    public:
        float face_value;
        bool is_mortgaged;
        

        Property(float face_value, Square square, vector<float>&& rents) {
            this->face_value = face_value;
            this->__square = square;
            this->rents = vector<float>(rents);

            this->is_mortgaged = false;
            this->__owner = nullptr;
            this->__current_value = face_value;
        }

        Player* get_owner() const { return __owner; }

        bool buy(Player* buyer, float price) {
            bool success = buyer->account->buy(this, price);
            if(!success) return success;

            __owner = buyer;
            __ltp = price;
            return true;
        }

        bool sell(Player* seller, float price) {
            if(seller != __owner)
                return false;
            
            bool success = seller->account->sell(this, price);
            if(!success) return success;

            __owner = nullptr;
            __ltp = price;
            return true;
        }


        bool trade(Player* buyer, Player* seller, float price) {
            bool success = success && this->sell(seller, price);
            if(!success) return success;

            bool success = this->buy(buyer, price);
            if(!success) return success;
            
            this->calculate_current_value(price);

            return success;
        }

        bool build(Building building)
        {
            if(__square.buildings > building or !(__square.buildings < HOTEL)) return false;
            bool success = __owner->account->pay(__construction_cost, BANK);
            if(!success) return success;
            __square.buildings++;
        }
        bool demolish(int buildings)
        {
            if(__square.buildings < buildings)
                return false;
            bool success = __owner->account->deposit(__construction_cost, BANK);
        }
        float calculate_current_value(float ltp)
        {
            // TODO: Calculate the current value using ltp, building value and round's equivalent;
            return __current_value;
        }

        bool pay_rent(Player* payer)
        {
            if(!__owner || __owner->is_bankrupt()) return false;
            return payer->account->pay(__owner->acco, this->rents[__square->buildings]);
        }

};