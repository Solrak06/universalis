// Json.hpp - Mini librería JSON sin dependencias externas.
// Soporta objetos (orden de inserción preservado), arrays, cadenas,
// números, booleanos y null. Suficiente para persistir el gestor.
#pragma once

#include <string>
#include <vector>

namespace js {

class Valor {
public:
    enum class Tipo { Nulo, Booleano, Numero, Cadena, Array, Objeto };

    Valor() = default;
    Valor(bool b);
    Valor(double n);
    Valor(int n);
    Valor(const char* s);
    Valor(std::string s);

    static Valor array();
    static Valor objeto();

    Tipo tipo() const { return tipo_; }
    bool esNulo() const { return tipo_ == Tipo::Nulo; }
    bool esArray() const { return tipo_ == Tipo::Array; }
    bool esObjeto() const { return tipo_ == Tipo::Objeto; }

    // --- Lectura tolerante: si el tipo no coincide devuelve el valor por defecto.
    bool comoBool(bool porDefecto = false) const;
    double comoNumero(double porDefecto = 0.0) const;
    int comoEntero(int porDefecto = 0) const;
    std::string comoCadena(const std::string& porDefecto = "") const;

    // --- Arrays
    std::size_t tam() const;
    const Valor& operator[](std::size_t i) const;
    void agregar(Valor v);

    // --- Objetos
    bool contiene(const std::string& clave) const;
    const Valor& operator[](const std::string& clave) const;  // nulo si no existe
    Valor& operator[](const std::string& clave);              // crea si no existe

    // --- Serialización / parseo
    std::string volcar(int sangria = 2) const;
    static bool parsear(const std::string& texto, Valor& salida, std::string& error);

private:
    void volcarEn(std::string& out, int sangria, int nivel) const;

    Tipo tipo_ = Tipo::Nulo;
    bool bool_ = false;
    double num_ = 0.0;
    std::string cad_;
    std::vector<Valor> elementos_;        // Array
    std::vector<std::string> claves_;     // Objeto (claves)
    std::vector<Valor> valores_;          // Objeto (valores, mismo índice)
};

}  // namespace js
