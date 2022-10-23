#include <iostream>
#include <unordered_map>
#include <vector>

// userspace-RCU include
#define _LGPL_SOURCE 1
#include <urcu/urcu-memb.h>
// This is the preferred version of the library, in terms of grace-period detection speed, read-side speed and flexibility

std::vector<float> data = {};

int main()
{
    const int amnt = 1000;
    data.resize(amnt);
    for (int i = 0; i < amnt; i++) {
        data.push_back(static_cast<float>(i));
    }

    return 0;
}
