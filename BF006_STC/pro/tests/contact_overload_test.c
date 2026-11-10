#include "config.h"
#include "contact_overload.h"

volatile u8 Contact_Overload_Test_Result;

/******************************************************************************
 * 函数名称       : Contact_Overload_RunTests
 * 功能描述       : 验证触点过载状态模块的外部行为。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void Contact_Overload_RunTests(void) //20260729-5 add
{
    u8 lock_flag;
    u8 lock_flag_b; //20260729-7 change B 泵独立触点锁存
    u8 event_count_a; //20260729-8 add A 泵锁存周期首次触发计数
    u8 event_count_b; //20260729-8 add B 泵锁存周期首次触发计数

    lock_flag = 0;
    lock_flag_b = 0;
    event_count_a = 0;
    event_count_b = 0;
    Contact_Overload_Test_Result = 1;

    if (Contact_Overload_Trigger(1, &lock_flag) != 1)
    {
        return;
    }
    if (lock_flag != 1)
    {
        return;
    }
    Contact_Overload_Test_Result = 2;
    if (Contact_Overload_Trigger(1, &lock_flag) != 0)
    {
        return;
    }
    if (lock_flag != 1)
    {
        return;
    }
    Contact_Overload_Test_Result = 3;
    if (Contact_Overload_Trigger(0, &lock_flag) != 0) //20260729-6 add 输入断开不能代替显式清除
    {
        return;
    }
    if ((lock_flag != 1) || (lock_flag_b != 0))
    {
        return;
    }
    Contact_Overload_Test_Result = 4;
    if (Contact_Overload_Clear(1, &lock_flag) != 0)
    {
        return;
    }
    if (lock_flag != 1)
    {
        return;
    }
    Contact_Overload_Test_Result = 5;
    if (Contact_Overload_Clear(0, &lock_flag) != 1)
    {
        return;
    }
    if (lock_flag != 0)
    {
        return;
    }
    Contact_Overload_Test_Result = 6;
    if (Contact_Overload_Trigger(0, &lock_flag) != 0)
    {
        return;
    }
    if (Contact_Overload_Trigger(1, &lock_flag) != 1)
    {
        return;
    }
    if (lock_flag != 1)
    {
        return;
    }

    Contact_Overload_Test_Result = 7;
    if (Contact_Overload_Clear(0, &lock_flag) != 1)
    {
        return;
    }
    if (Contact_Overload_Trigger(1, &lock_flag_b) != 1) //20260729-7 add B 泵使用独立锁存并产生首次触发事件
    {
        return;
    }
    if ((lock_flag != 0) || (lock_flag_b != 1))
    {
        return;
    }
    Contact_Overload_Test_Result = 8;
    if (Contact_Overload_Trigger(1, &lock_flag_b) != 0)
    {
        return;
    }
    if ((lock_flag != 0) || (lock_flag_b != 1))
    {
        return;
    }

    lock_flag = 0;
    lock_flag_b = 0;
    Contact_Overload_Test_Result = 9;
    event_count_a += Contact_Overload_Trigger(1, &lock_flag); //20260729-8 add 同一周期分别建立两路锁存
    event_count_b += Contact_Overload_Trigger(1, &lock_flag_b);
    if ((lock_flag != 1) || (lock_flag_b != 1) || (event_count_a != 1) || (event_count_b != 1))
    {
        return;
    }
    Contact_Overload_Test_Result = 10;
    event_count_a += Contact_Overload_Trigger(1, &lock_flag);
    event_count_b += Contact_Overload_Trigger(1, &lock_flag_b);
    if ((event_count_a != 1) || (event_count_b != 1)) // 持续接通不能重复产生历史记录事件
    {
        return;
    }
    Contact_Overload_Test_Result = 11;
    if (Contact_Overload_Clear(0, &lock_flag) != 1)
    {
        return;
    }
    if (Contact_Overload_Clear(1, &lock_flag_b) != 0)
    {
        return;
    }
    if ((lock_flag != 0) || (lock_flag_b != 1)) // A 泵清除不能覆盖仍接通的 B 泵锁存
    {
        return;
    }
    Contact_Overload_Test_Result = 12;
    event_count_a += Contact_Overload_Trigger(1, &lock_flag);
    event_count_b += Contact_Overload_Trigger(1, &lock_flag_b);
    if ((lock_flag != 1) || (lock_flag_b != 1) || (event_count_a != 2) || (event_count_b != 1))
    {
        return;
    }
    Contact_Overload_Test_Result = 13;
    if (Contact_Overload_Clear(1, &lock_flag) != 0)
    {
        return;
    }
    if (Contact_Overload_Clear(0, &lock_flag_b) != 1)
    {
        return;
    }
    if ((lock_flag != 1) || (lock_flag_b != 0)) // B 泵清除不能覆盖仍接通的 A 泵锁存
    {
        return;
    }

    Contact_Overload_Test_Result = 0;
}

/******************************************************************************
 * 函数名称       : Contact_Overload_Test_Done
 * 功能描述       : 为模拟器提供测试完成断点。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void Contact_Overload_Test_Done(void) //20260729-5 add
{
}

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 执行触点过载测试并停在测试完成位置。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void main(void) //20260729-5 add
{
    Contact_Overload_RunTests();
    Contact_Overload_Test_Done();

    while (1)
    {
    }
}
