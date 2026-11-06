// GuiAgenda.hpp - «Tu agenda del dia» de la ventana principal.
//
// Un control dibujado (no un texto plano): la fecha y lo que falta para ella, y debajo
// secciones (Pruebas, Clases, Tareas, Para estudiar hoy) con una fila por elemento: una
// franja del color de la asignatura, el titulo, un detalle y una pastilla a la derecha
// («mañana», «09:00», «40 %»). Las filas de una asignatura se pueden pulsar: avisan a la
// ventana padre con WM_AGENDA_ELEGIDA (wParam = indice de la asignatura).
// Si no cabe todo, se desplaza con la rueda del raton.
#pragma once

#include "Gui.hpp"

#include <string>
#include <vector>

namespace agenda {

const UINT WM_AGENDA_ELEGIDA = WM_APP + 71;

struct Linea {
    std::wstring titulo;
    std::wstring detalle;
    std::wstring pastilla;               // texto corto a la derecha ("" = ninguna)
    COLORREF color = CLR_INVALID;        // franja de la izquierda (color de la asignatura)
    COLORREF colorPastilla = CLR_INVALID;
    int asignatura = -1;                 // al pulsar se elige esta asignatura (-1 = no se puede pulsar)
};

struct Seccion {
    std::wstring titulo;
    std::vector<Linea> lineas;
    std::wstring pie;                    // nota pequena al final ("2 tareas atrasadas…")
};

struct Dia {
    std::wstring fecha;                  // «Miércoles, 14 de octubre»
    std::wstring etiqueta;               // «Hoy», «Mañana», «En 5 días», «Hace 3 días»
    COLORREF colorEtiqueta = CLR_INVALID;
    std::vector<Seccion> secciones;
    std::wstring vacio;                  // texto si no hay ninguna seccion
};

HWND crear(HWND padre, int id);
void poner(HWND agenda, const Dia& dia);

}  // namespace agenda
