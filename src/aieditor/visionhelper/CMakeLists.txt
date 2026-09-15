# SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
# SPDX-License-Identifier: BSD-2-Clause

qt_add_executable(kdenlive-ai-vision
    main.cpp
    nanodetpersondetector.cpp
)
kde_enable_exceptions()
set_target_properties(kdenlive-ai-vision PROPERTIES
    OUTPUT_NAME "firawynix-kdenlive-ai-vision"
    CXX_STANDARD 17
)
target_include_directories(kdenlive-ai-vision PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(kdenlive-ai-vision PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Mlt7::mlt++
    ${OpenCV_LIBS}
)
install(TARGETS kdenlive-ai-vision DESTINATION ${KDE_INSTALL_BINDIR})
