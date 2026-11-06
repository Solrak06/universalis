#pragma once
#include <vector>

#include "Gui.hpp"
#include "Modelo.hpp"
namespace profesor {
// Abre el profesor de `asignatura`. `todas`: asignaturas entre las que se puede cambiar sin cerrarlo.
void abrir(HWND padre, const Asignatura& asignatura, const std::vector<Asignatura>& todas = std::vector<Asignatura>());
}
