cmake_minimum_required(VERSION 3.5)

project(%{ProjectName} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

@if %{HasTranslation}
find_package(Qt5 COMPONENTS Core Quick LinguistTools REQUIRED)

set(TS_FILES %{TsFileName})
@else
find_package(Qt5 COMPONENTS Core Quick REQUIRED)
@endif

if(ANDROID)
    add_library(%{ProjectName} SHARED
      %{MainCppFileName}
      qml.qrc
@if %{HasTranslation}
      ${TS_FILES}
@endif
    )
else()
    add_executable(%{ProjectName}
      %{MainCppFileName}
      qml.qrc
@if %{HasTranslation}
      ${TS_FILES}
@endif
    )
endif()

target_compile_definitions(%{ProjectName}
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(%{ProjectName}
  PRIVATE Qt5::Core Qt5::Quick)
@if %{HasTranslation}

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
@endif

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# These variables must use CACHE, otherwise QtCreator won't see them.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android" CACHE INTERNAL "")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so CACHE INTERNAL "")
#    endif()
#endif()
