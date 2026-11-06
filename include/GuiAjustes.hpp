// GuiAjustes.hpp - Ventana de Ajustes y personalizacion de toda la interfaz.
#pragma once

#include "Gui.hpp"

#include <string>
#include <vector>

#include "Modelo.hpp"

namespace ajustes {

// Botones de la seccion «Datos y ayuda» (abrir carpetas, copia de seguridad...).
struct AccionExtra {
    std::wstring nombre;
    void (*hacer)(HWND);
};

// Oscuro o claro: el elegido o, con «Como Windows», el del sistema.
bool temaOscuroEfectivo(const Curso& curso);
// Pone el estilo global (gui::ponerEstilo) y el tema (gui::ponerTema) segun los ajustes del curso.
void aplicarApariencia(const Curso& curso);

// Abre la ventana. Los cambios se ven al momento en la propia ventana; «Guardar» los deja en
// `curso` y devuelve true; «Cancelar» (o cerrar) vuelve a la apariencia de antes y devuelve false.
bool abrir(HWND padre, Curso& curso, const std::vector<AccionExtra>& acciones);

}  // namespace ajustes
