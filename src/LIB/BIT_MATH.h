/***************************************************************************************/
/****************************  IMT School Training Center ******************************/
/***************************************************************************************/
/** This file is developed by IMT School training center, All copyrights are reserved **/
/***************************************************************************************/
#ifndef BIT_MATH_H
#define BIT_MATH_H

#define SET_BIT(REG,BitNo)              REG |= (1<<BitNo)
#define CLR_BIT(REG,BitNo)              REG &= ~(1<<BitNo)
#define TOG_BIT(REG,BitNo)              REG ^= (1<<BitNo)
#define GET_BIT(REG,BitNo)              ((REG>>BitNo) & 0x01)

#endif