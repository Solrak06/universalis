// ExportadorHtml.hpp - Genera un informe HTML autocontenido del curso.
#pragma once

#include <string>

#include "Modelo.hpp"

namespace html {

// Escribe el informe en 'ruta'. Devuelve false y rellena 'error' si falla.
bool exportar(const Curso& curso, const std::string& ruta, std::string& error);

}  // namespace html
