#include <limits>
#include <safe_integer.hpp>
using namespace boost::numeric;

int f(int i){
    return i;
}

using safe_t = safe<long>;

int main(){
    const long x = 97;
    f(x);   // OK - implicit conversion to int
    const safe_t y = 97;
    f(y);   // Also OK - checked implicit conversion to int
    return 0;
}
