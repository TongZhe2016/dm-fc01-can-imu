set(CAN_ROOT ${PX4_SOURCE_DIR}/src/drivers/uavcan)
set(CAN_HW ${CAN_ROOT}/uavcan_drivers/stm32h7/driver)
px4_add_module(
 MODULE modules__can_imu
 MAIN can_imu
 STACK_MAIN 8000
 COMPILE_FLAGS
  -DUAVCAN_CPP_VERSION=2003 -DUAVCAN_NO_ASSERTIONS -DUAVCAN_STM32H7_NUTTX=1
  -DUAVCAN_STM32H7_NUM_IFACES=1 -DUAVCAN_STM32H7_TIMER_NUMBER=5
  -DUAVCAN_TOSTRING=0 -Wno-cast-align
  -DUAVCAN_NO_GLOBAL_DATA_TYPE_REGISTRY=1
 INCLUDES ${CAN_ROOT}/libdronecan/libuavcan/include ${CAN_HW}/include
 SRCS CanImu.cpp
  ${CAN_HW}/src/uc_stm32h7_can.cpp
  ${CAN_HW}/src/uc_stm32h7_clock.cpp
  ${CAN_HW}/src/uc_stm32h7_thread.cpp
  ${CAN_ROOT}/libdronecan/libuavcan/src/driver/uc_can.cpp
  ${CAN_ROOT}/libdronecan/libuavcan/src/uc_error.cpp
 DEPENDS px4_work_queue
)
target_compile_options(modules__can_imu PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>)
