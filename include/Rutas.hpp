// Rutas.hpp - Operaciones minimas de ficheros sin <filesystem>.
//
// Se evita <filesystem> a proposito: el MinGW que acompana a Code::Blocks
// (GCC 8.x) necesita enlazar -lstdc++fs y en muchas instalaciones falla.
// Aqui solo se usan la libreria estandar de C y las cabeceras del sistema,
// asi que el proyecto compila con cualquier compilador desde C++11.
#pragma once

#include <string>

namespace rutas {

bool existe(const std::string& ruta);
bool esDirectorio(const std::string& ruta);

// Separadores '\' -> '/' y sin barra final sobrante.
std::string normalizar(const std::string& ruta);
// "datos/curso.json" -> "datos"   |   "curso.json" -> ""
std::string padre(const std::string& ruta);
// Une dos trozos evitando barras duplicadas.
std::string unir(const std::string& a, const std::string& b);
bool esAbsoluta(const std::string& ruta);
// Carpeta donde esta el .exe (para que los datos no dependan del directorio actual).
std::string directorioDelEjecutable();
// Convierte a ruta absoluta usando el directorio de trabajo actual.
std::string absoluta(const std::string& ruta);

// Crea la carpeta y todas las intermedias que falten.
bool crearDirectorios(const std::string& ruta);
bool borrar(const std::string& ruta);
// Renombra sobrescribiendo el destino si ya existe.
bool renombrarSobrescribiendo(const std::string& origen, const std::string& destino);

}  // namespace rutas
