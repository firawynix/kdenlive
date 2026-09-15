# SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
# SPDX-License-Identifier: BSD-2-Clause

set(kdenlive_SRCS
    ${kdenlive_SRCS}
    aieditor/aiproviderclient.cpp
    aieditor/aisessionstore.cpp
    aieditor/assistantdock.cpp
    aieditor/editplan.cpp
    aieditor/editplanexecutor.cpp
    aieditor/localtimelinetranscriber.cpp
    aieditor/localvisionanalyzer.cpp
    aieditor/localaimanager.cpp
    aieditor/muterangeexecutor.cpp
    aieditor/personretimeplanner.cpp
    aieditor/resourcebudget.cpp
    aieditor/retimerangeexecutor.cpp
    aieditor/securecredentialstore.cpp
    PARENT_SCOPE
)

if(OpenCV_FOUND)
    add_subdirectory(visionhelper)
endif()
