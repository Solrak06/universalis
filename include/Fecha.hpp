// Fecha.hpp - Fecha y hora sencillas con aritmetica de dias.
#pragma once

#include <string>

struct Fecha {
    int anio = 0;
    int mes = 0;    // 1-12
    int dia = 0;    // 1-31
    int hora = -1;  // -1 = sin hora definida
    int minuto = 0;

    bool valida() const;
    bool tieneHora() const { return hora >= 0; }

    // "2026-09-14" o "2026-09-14 10:30" (tambien acepta 14/09/2026)
    static bool desdeTexto(const std::string& texto, Fecha& salida);
    std::string aTexto() const;          // formato de guardado: 2026-09-14 10:30
    std::string aTextoLargo() const;     // "lun 14 sep 2026, 10:30"
    std::string aTextoCorto() const;     // "14/09/2026"

    static Fecha hoy();

    long numeroDeDia() const;               // dias desde una epoca fija
    int diaSemana() const;                  // 0 = lunes ... 6 = domingo
    static int diasEnMes(int anio, int mes);
    static bool bisiesto(int anio);
    static const char* nombreMes(int mes);
    static const char* nombreMesCorto(int mes);

    // dias desde 'this' hasta 'otra' (negativo si otra ya paso)
    long diasHasta(const Fecha& otra) const;
};

bool operator<(const Fecha& a, const Fecha& b);
bool operator==(const Fecha& a, const Fecha& b);
