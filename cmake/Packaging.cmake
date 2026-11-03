# Installation and packaging (CPack). On Windows this produces a ZIP and, when NSIS is
# installed, an installer that also registers the .classboard file type.
install(FILES
    "${PROJECT_SOURCE_DIR}/README.md"
    "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${PROJECT_SOURCE_DIR}/docs/ClassBoard-User-Guide.pdf"
    DESTINATION doc)
# Full texts of the licences named in THIRD_PARTY_NOTICES.md (Qt: LGPL 3, Poppler: GPL).
install(DIRECTORY "${PROJECT_SOURCE_DIR}/licenses/" DESTINATION doc/licenses)

# Optional: ship Poppler's command line tools (pdftoppm + DLLs) for offline PDF import.
set(CLASSBOARD_POPPLER_DIR "" CACHE PATH "Folder containing pdftoppm and its libraries, installed as bin/poppler/bin")
if(CLASSBOARD_POPPLER_DIR)
    install(DIRECTORY "${CLASSBOARD_POPPLER_DIR}/" DESTINATION bin/poppler/bin)
endif()

set(CPACK_PACKAGE_NAME "ClassBoard")
set(CPACK_PACKAGE_VENDOR "ClassBoard contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ClassBoard")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "ClassBoard" "ClassBoard")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "ClassBoard ${PROJECT_VERSION}")
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/resources/app/classboard.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\ClassBoard.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.classboard' '' 'ClassBoard.Lesson'
        WriteRegStr HKCR 'ClassBoard.Lesson' '' 'ClassBoard lesson'
        WriteRegStr HKCR 'ClassBoard.Lesson\\\\DefaultIcon' '' '$INSTDIR\\\\bin\\\\ClassBoard.exe,0'
        WriteRegStr HKCR 'ClassBoard.Lesson\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\ClassBoard.exe\\\" \\\"%1\\\"'
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.classboard'
        DeleteRegKey HKCR 'ClassBoard.Lesson'
    ")
    # Qt runtime: deploy with windeployqt into the install tree.
    install(CODE "
        get_filename_component(_qtbin \"${Qt5_DIR}/../../../bin\" ABSOLUTE)
        find_program(_windeployqt windeployqt HINTS \"\${_qtbin}\")
        if(_windeployqt)
            execute_process(COMMAND \"\${_windeployqt}\" --no-translations --no-system-d3d-compiler --no-opengl-sw
                                    \"\${CMAKE_INSTALL_PREFIX}/bin/ClassBoard.exe\")
        endif()
    ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
