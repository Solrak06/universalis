// Calendario.hpp - Vista mensual en consola y navegacion por meses.
#pragma once

#include "Modelo.hpp"

namespace calendario {

// Dibuja el mes indicado marcando los dias con examen y lista los examenes del mes.
void dibujarMes(const Curso& curso, int anio, int mes);

// Bucle interactivo: [a]nterior / [s]iguiente / [h]oy / [0] volver.
void vistaInteractiva(const Curso& curso);

// Panel de "proximos examenes" con cuenta atras.
void mostrarProximos(const Curso& curso, int limite = 8);

}  // namespace calendario
