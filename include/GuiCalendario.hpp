// GuiCalendario.hpp - Calendario propio dibujado a mano.
//
// Sustituye al control SysMonthCal32 de Windows, que no permite colorear
// dias sueltos ni respeta el modo oscuro.
//
// Avisa al padre con WM_COMMAND: LOWORD(wParam) = id del control,
// HIWORD(wParam) = CAL_DIA_ELEGIDO, lParam = HWND del calendario.
#pragma once

#include "Gui.hpp"
#include "Modelo.hpp"

namespace cal {

const WORD CAL_DIA_ELEGIDO = 1;

void registrarClase();
HWND crear(HWND padre, int id);

void ponerCurso(HWND calendario, const Curso* curso);
// Opciones de los ajustes: primer dia de la semana y si se ve el fin de semana.
void ponerOpciones(HWND calendario, bool lunesPrimero, bool verFinDeSemana, int diasDeAviso);
void irAlMes(HWND calendario, int anio, int mes);
// Marca el dia y viaja a su mes. No genera notificacion.
void seleccionar(HWND calendario, const Fecha& fecha);
Fecha seleccion(HWND calendario);
void refrescar(HWND calendario);

}  // namespace cal
