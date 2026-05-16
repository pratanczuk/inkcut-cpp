# SPDX-License-Identifier: GPL-3.0-or-later

inkcut_add_device_plugin(inkcut_plugin_grbl grbl_plugin.cpp)

target_link_libraries(inkcut_plugin_grbl PRIVATE Qt6::SerialPort)
