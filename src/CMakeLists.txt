# Core library: everything except main(), shared by the application and the tests.
add_library(classboard_lib STATIC
    ai/MathInkRecognizer.cpp
    ai/Recognition.cpp
    ai/ShapeRecognizer.cpp
    app/AppSettings.cpp
    app/ExportController.cpp
    app/LessonController.cpp
    app/MainWindow.cpp
    app/PopoverController.cpp
    canvas/CanvasWidget.cpp
    canvas/MagicHighlightLayer.cpp
    canvas/PageRenderer.cpp
    canvas/ThumbnailCache.cpp
    canvas/ViewTransform.cpp
    core/Crc32.cpp
    core/Geometry.cpp
    document/Command.cpp
    document/Commands.cpp
    document/CoordinateResolver.cpp
    document/Document.cpp
    document/DocumentObject.cpp
    document/ImageObject.cpp
    document/ImageStore.cpp
    document/ObjectFactory.cpp
    document/Page.cpp
    document/PageOperations.cpp
    document/PageSize.cpp
    document/PageTemplate.cpp
    document/ShapeObject.cpp
    document/StrokeObject.cpp
    document/TemplateLibrary.cpp
    document/TextObject.cpp
    export/Exporters.cpp
    export/ZipWriter.cpp
    geometry/GeometryObject.cpp
    geometry/Instrument.cpp
    geometry/InstrumentLayer.cpp
    geometry/Instruments.cpp
    geometry/Label.cpp
    geometry/MeasurementObject.cpp
    geometry/Precision.cpp
    graph/GraphObject.cpp
    graph/TableObject.cpp
    input/InputManager.cpp
    input/PalmGesture.cpp
    math/CoordinateSystem.cpp
    math/MeasureScale.cpp
    math/Calculator.cpp
    math/Expression.cpp
    math/equation/EquationObject.cpp
    math/equation/MathLayout.cpp
    storage/AutosaveManager.cpp
    storage/Container.cpp
    storage/PdfImporter.cpp
    storage/PageImport.cpp
    storage/ProjectSerializer.cpp
    tools/ConstructTool.cpp
    tools/EditOperations.cpp
    tools/EraserTool.cpp
    tools/GraphTool.cpp
    tools/MagicEquation.cpp
    tools/MeasureTool.cpp
    tools/PenTool.cpp
    tools/SelectTool.cpp
    tools/SelectionModel.cpp
    tools/ShapeTool.cpp
    tools/Snap.cpp
    tools/TextTool.cpp
    tools/ToolController.cpp
    tools/ToolSettings.cpp
    ui/IconProvider.cpp
    ui/Popover.cpp
    ui/Ribbon.cpp
    ui/SelectionBar.cpp
    ui/TableCellEditor.cpp
    ui/Theme.cpp
    ui/Toast.cpp
    ui/popovers/BoardPanels.cpp
    ui/popovers/CalculatorPanel.cpp
    ui/popovers/DrawingPanels.cpp
    ui/popovers/LessonPanels.cpp
    ui/popovers/MagicEquationPanel.cpp
    ui/popovers/MathPanels.cpp
    ui/popovers/PageNavigatorPanel.cpp
    ui/popovers/PanelUtil.cpp
    ui/popovers/PropertiesPanel.cpp
    ui/widgets/NumberField.cpp
    ui/widgets/SegmentedControl.cpp
    ui/widgets/SwatchGrid.cpp
    ui/widgets/ToggleRow.cpp
    ui/widgets/TouchButton.cpp
    ui/widgets/TouchSlider.cpp
    ${PROJECT_SOURCE_DIR}/resources/classboard.qrc
)

target_include_directories(classboard_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(classboard_lib PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Svg)
target_compile_definitions(classboard_lib PUBLIC
    CLASSBOARD_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_URL_CAST_FROM_STRING
    QT_DISABLE_DEPRECATED_BEFORE=0x050F00)

if(Qt5Pdf_FOUND)
    target_link_libraries(classboard_lib PUBLIC Qt5::Pdf)
    target_compile_definitions(classboard_lib PUBLIC CLASSBOARD_HAVE_QTPDF)
    message(STATUS "ClassBoard: PDF import via Qt PDF")
else()
    message(STATUS "ClassBoard: Qt PDF not found, PDF import uses Poppler's pdftoppm when installed")
endif()

if(MSVC)
    target_compile_options(classboard_lib PUBLIC /utf-8 /W4 /permissive- /wd4127)
    target_compile_definitions(classboard_lib PUBLIC NOMINMAX _USE_MATH_DEFINES UNICODE _UNICODE)
else()
    target_compile_options(classboard_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Application
set(CLASSBOARD_APP_SOURCES app/main.cpp)
if(WIN32)
    # Windows file properties (version, icon) generated from the project version.
    set(CLASSBOARD_ICON_FILE "${PROJECT_SOURCE_DIR}/resources/app/classboard.ico")
    configure_file(${PROJECT_SOURCE_DIR}/resources/app/classboard.rc.in ${CMAKE_CURRENT_BINARY_DIR}/classboard.rc @ONLY)
    list(APPEND CLASSBOARD_APP_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/classboard.rc)
endif()

add_executable(ClassBoard WIN32 ${CLASSBOARD_APP_SOURCES})
target_link_libraries(ClassBoard PRIVATE classboard_lib)
set_target_properties(ClassBoard PROPERTIES
    OUTPUT_NAME "ClassBoard"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

install(TARGETS ClassBoard RUNTIME DESTINATION bin)

if(WIN32 AND CLASSBOARD_DEPLOY_QT)
    get_target_property(_qmake Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qtbin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qtbin}")
    if(WINDEPLOYQT)
        add_custom_command(TARGET ClassBoard POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations --no-system-d3d-compiler --no-opengl-sw
                    "$<TARGET_FILE:ClassBoard>"
            COMMENT "Deploying Qt runtime next to ClassBoard.exe")
    endif()
endif()
