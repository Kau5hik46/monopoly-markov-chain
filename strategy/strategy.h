#include "../accounts/account.h"

class Strategy
{
    public:
    virtual void execute(Account*) = 0;
}

