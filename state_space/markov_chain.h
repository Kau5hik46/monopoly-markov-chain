#include "state.h"

template <typename T>
class MarkovChain
{
    vector<vector<State<T>*>> __transition_matrix;
    map<State<T>*, vector<float>> __weights;
    public:
    MarkovChain()
    {
        this->__transition_matrix = vector<vector<State<T>*>();
        this->__weights = map<State<T>*, vector<float>>();
    }

    void add_state(State<T>* state, vector<float> weights)
    {

    }
}
