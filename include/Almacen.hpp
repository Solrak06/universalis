// Almacen.hpp - Carga y guardado del curso en un fichero JSON.
#pragma once

#include <string>

#include "Modelo.hpp"

class Almacen {
public:
    explicit Almacen(std::string ruta);

    const std::string& ruta() const { return ruta_; }
    const std::string& ultimoError() const { return error_; }

    // Si el fichero no existe, deja 'curso' con datos de ejemplo y devuelve true.
    bool cargar(Curso& curso);
    // Escribe primero en un .tmp y luego renombra, para no corromper el fichero.
    bool guardar(const Curso& curso);

    static Curso cursoDeEjemplo();

private:
    std::string ruta_;
    std::string error_;
};
