#include "Calendario.hpp"

#include <cctype>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "Utilidades.hpp"

namespace calendario {

namespace {

// Examenes del mes agrupados por dia.
std::map<int, std::vector<ExamenConAsignatura>> examenesDelMes(const Curso& curso, int anio,
                                                               int mes) {
    std::map<int, std::vector<ExamenConAsignatura>> mapa;
    for (const auto& ea : todosLosExamenes(curso)) {
        const Fecha& f = ea.examen->fecha;
        if (f.anio == anio && f.mes == mes) mapa[f.dia].push_back(ea);
    }
    return mapa;
}

std::string celda(int dia, bool hayExamen, bool esHoy) {
    std::ostringstream os;
    char num[16];
    std::snprintf(num, sizeof(num), "%2d", dia);

    if (esHoy) {
        os << ui::negrita() << ui::cian() << ">" << num << "<" << ui::reset();
    } else if (hayExamen) {
        os << ui::rojo() << "[" << num << "]" << ui::reset();
    } else {
        os << " " << num << " ";
    }
    return os.str();
}

}  // namespace

void dibujarMes(const Curso& curso, int anio, int mes) {
    auto mapa = examenesDelMes(curso, anio, mes);
    Fecha hoy = Fecha::hoy();

    std::ostringstream cabecera;
    cabecera << Fecha::nombreMes(mes) << " de " << anio;
    ui::titulo(cabecera.str());

    std::cout << ui::gris() << " lun  mar  mie  jue  vie  sab  dom" << ui::reset() << "\n";

    Fecha primero;
    primero.anio = anio;
    primero.mes = mes;
    primero.dia = 1;
    int offset = primero.diaSemana();  // 0 = lunes
    for (int i = 0; i < offset; ++i) std::cout << "     ";

    int total = Fecha::diasEnMes(anio, mes);
    for (int dia = 1; dia <= total; ++dia) {
        bool hayExamen = mapa.count(dia) > 0;
        bool esHoy = (hoy.anio == anio && hoy.mes == mes && hoy.dia == dia);
        std::cout << celda(dia, hayExamen, esHoy) << " ";
        if ((offset + dia) % 7 == 0) std::cout << "\n";
    }
    if ((offset + total) % 7 != 0) std::cout << "\n";

    std::cout << ui::gris() << "\n  [dd] examen    >dd< hoy" << ui::reset() << "\n";

    if (mapa.empty()) {
        std::cout << "\n" << ui::gris() << "  Sin examenes este mes." << ui::reset() << "\n";
        return;
    }

    ui::subtitulo("Examenes del mes");
    for (const auto& par : mapa) {
        for (const auto& ea : par.second) {
            const Examen& e = *ea.examen;
            std::cout << "  " << ui::negrita() << ui::encajar(e.fecha.aTextoLargo(), 22)
                      << ui::reset() << " " << ui::cian()
                      << ui::encajar(ea.asignatura->codigo, 7) << ui::reset() << " "
                      << ui::encajar(e.nombre, 24) << " " << ui::gris()
                      << formatearNota(e.ponderacion, 0) << "%";
            std::cout << ui::reset() << "\n";
        }
    }
}

void mostrarProximos(const Curso& curso, int limite) {
    auto lista = proximosExamenes(curso, limite);
    ui::subtitulo("Proximos examenes");
    if (lista.empty()) {
        std::cout << ui::gris() << "  No hay examenes pendientes en el calendario." << ui::reset()
                  << "\n";
        return;
    }

    Fecha hoy = Fecha::hoy();
    for (const auto& ea : lista) {
        long dias = hoy.diasHasta(ea.examen->fecha);
        std::string cuenta;
        const char* color = ui::gris();
        if (dias == 0) {
            cuenta = "HOY";
            color = ui::rojo();
        } else if (dias == 1) {
            cuenta = "manana";
            color = ui::rojo();
        } else if (dias <= 7) {
            cuenta = "en " + std::to_string(dias) + " dias";
            color = ui::amarillo();
        } else {
            cuenta = "en " + std::to_string(dias) + " dias";
        }

        std::cout << "  " << color << ui::encajar(cuenta, 13) << ui::reset() << " "
                  << ui::encajar(ea.examen->fecha.aTextoLargo(), 22) << " " << ui::cian()
                  << ui::encajar(ea.asignatura->codigo, 7) << ui::reset() << " "
                  << ui::encajar(ea.examen->nombre, 24) << " " << ui::gris()
                  << formatearNota(ea.examen->ponderacion, 0) << "%" << ui::reset() << "\n";
    }
}

void vistaInteractiva(const Curso& curso) {
    Fecha hoy = Fecha::hoy();
    int anio = hoy.anio;
    int mes = hoy.mes;

    while (true) {
        ui::limpiarPantalla();
        dibujarMes(curso, anio, mes);
        std::cout << "\n"
                  << ui::gris() << "  [a] mes anterior   [s] mes siguiente   [h] hoy   [0] volver"
                  << ui::reset() << "\n";
        std::string op = ui::leerLinea("> ");
        if (op.empty()) continue;
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(op[0])));
        if (c == '0' || c == 'q') return;
        if (c == 'a') {
            if (--mes < 1) { mes = 12; --anio; }
        } else if (c == 's') {
            if (++mes > 12) { mes = 1; ++anio; }
        } else if (c == 'h') {
            anio = hoy.anio;
            mes = hoy.mes;
        }
    }
}

}  // namespace calendario
