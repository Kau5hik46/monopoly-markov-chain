#pragma once
#include <list>
#include "asset.h"

using namespace std;
class Account {
    private:
        float cash;
        set<sset*> assets;
        static list<string> ledger;
    public:
    string name;

        bool deposit(float cash)
        {
            this->cash += cash;
            ledger.push_back("Deposited: " + to_string(cash) + "to " + this->name);
            return true;
        }

        bool withdraw(float cash)
        {
            if(this->cash < cash)
                return false; 
            this->cash -= cash;
            ledger.push_back("Withdrew: " + to_string(cash) + "from " + this->name);
            return true;
        }

        static bool transfer(Account* from_account, Account* to_account, float cash)
        {
            ledger.push_back(
                "Depositing: " + to_string(cash) + 
                "from " + from_account->name + 
                " to " + to_account->name
            );
            return from_account->withdraw(cash) && to_account->deposit(cash);
        }

        static bool transfer(Account* seller, Account* buyer, Asset* asset, float price = 0)
        {
            ledger.push_back(
                "Transfering: " + asset->name + 
                "from " + seller->name + 
                " to " + buyer->name
            );
            return seller->sell(asset, price) && buyer->buy(asset, price);
        }

        bool withdraw(float cash)
        {
            if(this->cash < cash)
                return false;
            this->cash -= cash;

            return true;
        }

        bool buy(Asset* asset, float price)
        {
            bool success = this->withdraw(price);
            if(!success)
                return success;
            this->assets.insert(*asset);
            ledger.push_back("Bought: " + asset->getName() + " for: " + to_string(price));
            return true;
        }

        bool sell(Asset* asset, float price)
        {
            if(this->assets.find(asset) == this->assets.end())
                return false;
            
            this->deposit(price);
            this->assets.erase(asset);
            ledger.push_back("Sold: " + asset->getName() + " for: " + to_string(price));
            return true;
        }

        bool pay(Account* account, float price)
        {
            bool success = this->withdraw(price);
            success = success && account->deposit(price);
            ledger.push_back("Paid: " + to_string(price) + " to: " + account->name);
            return success;
        }


};