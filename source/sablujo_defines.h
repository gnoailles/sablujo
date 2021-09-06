#if !defined(SABLUJO_DEFINES_H)
#define internal static
#define local_persist static
#define global_variable static 

#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024)
#define Terabytes(Value) (Gigabytes(Value)*1024)


#define FLOAT_MAX 3.402823466e+38F
#define FLOAT_MIN 1.175494e-38F

/*
#ifdef UINT16_MAX
#undef UINT16_MAX
#endif
#define UINT16_MAX 65535
*/

#if SABLUJO_SLOW
// TODO: Complete assertion macro - don't worry everyone!
#define Assert(Expression) if(!(Expression)) {*(int32_t *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define ArrayCount(Value) (sizeof(Value) / sizeof((Value)[0]))


#define SABLUJO_DEFINES_H
#endif