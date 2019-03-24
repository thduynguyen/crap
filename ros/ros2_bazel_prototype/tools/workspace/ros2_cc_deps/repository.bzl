load("//tools/skylark/cmake:cmake_cc.bzl", "cmake_cc_repository")

_ros = "/opt/ros/crystal"
_py = "3.6"

def ros2_cc_deps_repository(name):
    cmake_cc_repository(
        name = name,
        packages = [
            "rclcpp",
            "std_msgs",
            "rmw_fastrtps_cpp",  # Specify RMW implementation.
        ],
        cache_entries = dict(
            CMAKE_PREFIX_PATH = ";".join([
                "/home/eacousineau/devel/ros2/rmw_implementation/rmw_implementation/build/install",
                _ros,
            ]),
        ),
        env_vars = dict(
            PYTHONPATH = "{}/lib/python{}/site-packages".format(_ros, _py),
        ),
    )