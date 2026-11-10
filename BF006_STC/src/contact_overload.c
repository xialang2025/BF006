#include "config.h"
#include "contact_overload.h"

/******************************************************************************
 * 函数名称       : Contact_Overload_Trigger
 * 功能描述       : 根据滤波后的输入状态建立触点过载锁存并返回首次触发事件。
 * 输入参数       : input_active, lock_flag
 * 返回值         : 1 表示首次触发，0 表示未触发
 * 日期            : 2026.07.29
 ******************************************************************************/
u8 Contact_Overload_Trigger(u8 input_active, u8 *lock_flag) //20260729-5 add
{
    if ((input_active != 0) && (*lock_flag == 0)) //20260729-5 change 同一锁存周期只产生一次触发事件
    {
        *lock_flag = 1;
        return 1;
    }

    return 0;
}

/******************************************************************************
 * 函数名称       : Contact_Overload_Clear
 * 功能描述       : 根据当前输入状态处理触点过载锁存的显式清除。
 * 输入参数       : input_active, lock_flag
 * 返回值         : 1 表示清除成功，0 表示输入仍有效
 * 日期            : 2026.07.29
 ******************************************************************************/
u8 Contact_Overload_Clear(u8 input_active, u8 *lock_flag) //20260729-5 add
{
    if (input_active == 0)
    {
        *lock_flag = 0;
        return 1;
    }

    return 0;
}
