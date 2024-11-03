package sablujo_common

SABLUJO_INTERNAL :: #config(INTERNAL, false)
SABLUJO_SLOW :: #config(SLOW, false)


LANE_WIDTH :: #config(LANE_WIDTH, 8)

Kilobytes :: proc (value: $T) -> T { return (value)*1024 }
Megabytes :: proc (value: $T) -> T { return Kilobytes(value)*1024 }
Gigabytes :: proc (value: $T) -> T { return Megabytes(value)*1024 }
Terabytes :: proc (value: $T) -> T { return Gigabytes(value)*1024 }

FLOAT_MAX :: 3.402823466e+38
FLOAT_MIN :: 1.175494e-38
EPSILON :: 0.0000001