class Asset
{
private:
    float __current_value;
    float __net_worth;
    float __ltp;
    Account* __owner;
public:
    string name;
    float face_value;
    bool is_mortgaged;

    Asset(float face_value, Account* owner_account) 
    {
        this->face_value = face_value;
        this->__owner = owner_account;
    }
}