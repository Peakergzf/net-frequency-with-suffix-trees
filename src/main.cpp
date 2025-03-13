#include "suffix_tree.hpp"
#include <assert.h>


int main() {
    const std::string txt = "#abcdabybcdbxbcyabcd$";
    
    SuffixTree st{txt};
    
    assert(st.single_nf("abcd") == 2);
    
    st.all_nf();
    
    return 0;
}
