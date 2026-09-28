#include <console>
#include <ysi_compat>

// Y-Less note #2: YSI y_iterate spelling works on the native set via shims.
main()
{
    new Iterator:nums<8>;      // -> nums[9], count slot included
    Iter_Clear(nums);
    Iter_Add(nums, 3);
    Iter_Add(nums, 7);
    Iter_Add(nums, 5);
    printf("count=%d\n", Iter_Count(nums));       // 3
    printf("has7=%d\n", Iter_Contains(nums, 7));   // 1
    Iter_Remove(nums, 7);
    printf("has7=%d\n", Iter_Contains(nums, 7));   // 0
    new sum = 0;
    foreach (new i : nums) sum += i;               // 3 + 5 = 8
    printf("sum=%d\n", sum);
}
