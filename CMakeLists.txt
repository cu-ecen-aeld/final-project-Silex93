cmake_minimum_required(VERSION 3.24)
project(Temp_Sensor_Driver C)

set(CMAKE_C_STANDARD 11)

add_executable(Temp_Sensor_Driver mpl3115a2_driver/main.c mpl3115a2_driver/mpl3115a2_driver.h )
add_executable(Web_Server webserver/webserver.c )
