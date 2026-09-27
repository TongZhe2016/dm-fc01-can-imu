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
# The transport's time type includes the generated Timestamp definition.
set(CAN_DSDL ${CMAKE_CURRENT_BINARY_DIR}/dsdl)
add_custom_command(OUTPUT ${CAN_DSDL}/uavcan/Timestamp.hpp
 COMMAND ${PYTHON_EXECUTABLE} ${CAN_ROOT}/libdronecan/libuavcan/dsdl_compiler/libuavcan_dsdlc
 --outdir ${CAN_DSDL} ${CAN_ROOT}/libdronecan/dsdl/uavcan
 DEPENDS ${CAN_ROOT}/libdronecan/dsdl/uavcan/Timestamp.uavcan)
add_custom_target(can_imu_dsdl DEPENDS ${CAN_DSDL}/uavcan/Timestamp.hpp)
add_dependencies(modules__can_imu can_imu_dsdl)
target_include_directories(modules__can_imu PRIVATE ${CAN_DSDL})
target_compile_options(modules__can_imu PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>)
