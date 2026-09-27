#include <cassert>
#include <uavcan/time.hpp>

int main()
{
    using namespace uavcan;
    const auto start = MonotonicTime::fromUSec(0x100000000ULL + 123);
    const auto deadline = start + MonotonicDuration::fromUSec(3000);
    assert((deadline - start).toUSec() == 3000);
    assert((start - deadline).toUSec() == -3000);
    assert(deadline > start);
    assert(MonotonicTime().toUSec() == 0);
    assert(UtcTime().toUSec() == 0);
    const auto utc = UtcTime::fromUSec(1700000000000000ULL);
    assert((utc + UtcDuration::fromUSec(5000)).toUSec() == 1700000000005000ULL);
    assert((utc - UtcDuration::fromUSec(5000)).toUSec() == 1699999999995000ULL);
}
