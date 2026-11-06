cmake_minimum_required(VERSION 3.15)
project(gestor_asignaturas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Codigo compartido por las dos interfaces
set(FUENTES_COMUNES
    src/Json.cpp
    src/Fecha.cpp
    src/Modelo.cpp
    src/Rutas.cpp
    src/Almacen.cpp
    src/Utilidades.cpp
    src/Calendario.cpp
    src/ExportadorHtml.cpp
)

# ---------------------------------------------------------------- consola
add_executable(gestor_asignaturas ${FUENTES_COMUNES} src/main.cpp)
target_include_directories(gestor_asignaturas PRIVATE include)

# ------------------------------------------------------- interfaz grafica
if(WIN32)
    enable_language(RC)
    add_executable(gestor_gui WIN32
        ${FUENTES_COMUNES}
        src/GuiMain.cpp
        src/GuiProfesor.cpp
        src/GuiProfesorEstudio.cpp
        src/GuiProfesorHerramientas.cpp
        src/GuiAjustes.cpp
        src/GuiAgenda.cpp
        src/GuiProfesorEnfoque.cpp
        src/GuiProfesorComandos.cpp
        src/GuiProfesorTemario.cpp
        src/GuiProfesorAmbiente.cpp
        src/Musica.cpp
        src/ProfesorMotor.cpp
        src/GuiUtil.cpp
        src/GuiCalendario.cpp
        gestor.rc
    )
    target_include_directories(gestor_gui PRIVATE include)
    target_link_libraries(gestor_gui PRIVATE gdiplus comctl32 comdlg32 shell32 ole32 gdi32 user32 msimg32 advapi32 winmm)
endif()

foreach(destino gestor_asignaturas gestor_gui)
    if(TARGET ${destino})
        if(MSVC)
            target_compile_options(${destino} PRIVATE /W4 /utf-8)
        else()
            target_compile_options(${destino} PRIVATE -Wall)
        endif()
        if(WIN32)
            target_link_libraries(${destino} PRIVATE shell32)
            if(NOT MSVC)
                target_link_options(${destino} PRIVATE -static -static-libgcc -static-libstdc++)
            endif()
        endif()
        # Los .exe quedan en la raiz del proyecto, junto a datos/
        set_target_properties(${destino} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
        )
    endif()
endforeach()
