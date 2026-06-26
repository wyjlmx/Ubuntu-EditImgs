cmake_minimum_required(VERSION 3.8)
project(edit_imgs_wrapper)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 1. 寻找依赖（这是头部）
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)

# 2. 寻找你的 SDK（在这里加入 find_library）
# 如果你的 .so 已经在 /usr/local/lib，CMake 通常能自动找到
find_library(MIMGS_LIBRARY NAMES mimgs PATHS /usr/local/lib)
find_library(MLOG_LIBRARY NAMES mlog PATHS /usr/local/lib)

# 3. 定义你的节点可执行文件
add_executable(image_wrapper_node src/image_wrapper_node.cpp)

# 4. 指定头文件路径（在这里加入 include_directories）
# 使用 target_include_directories 比 include_directories 更规范
target_include_directories(image_wrapper_node PRIVATE 
    /usr/local/include
)

# 5. 链接你的 SDK 和 ROS 库
# ament_target_dependencies 会自动处理 ROS 库的头文件和依赖
ament_target_dependencies(image_wrapper_node
    rclcpp
    std_msgs
    sensor_msgs
    cv_bridge
)

# 链接你自己的 SDK
target_link_libraries(image_wrapper_node
    ${MIMGS_LIBRARY} 
    ${MLOG_LIBRARY}
)

# 6. 安装规则（这是必须的，否则 colcon 不知道怎么发布你的节点）
install(TARGETS image_wrapper_node
    DESTINATION lib/${PROJECT_NAME}
)

# 7. 测试部分
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()