// Musica.hpp - Sonido de fondo para estudiar.
//
// Ambientes generados por el propio programa (sin ficheros ni conexion): lluvia, olas,
// ruido marron, chimenea, piano tranquilo, ondas alfa, tormenta, bosque, cafeteria y
// arroyo. Cada uno es un bucle de casi un
// minuto que empalma sin cortes. Ademas, «Mi musica» reproduce en orden aleatorio los
// ficheros (mp3, wav, wma, m4a) de la carpeta <programa>/Musica.
#pragma once

#include "Gui.hpp"

#include <string>
#include <vector>

namespace musica {

enum Sonido { NADA = 0, LLUVIA, OLAS, MARRON, CHIMENEA, PIANO, ALFA, TORMENTA, BOSQUE, CAFE, ARROYO, MIA, N_SONIDOS };

const wchar_t* nombre(int sonido);
const wchar_t* descripcion(int sonido);
COLORREF color(int sonido);

void tocar(int sonido);   // empieza (con un fundido suave); NADA para
void parar();
int sonando();            // NADA si no suena nada
void ponerVolumen(int v); // 0 - 100
int volumen();
int ultimo();             // el ultimo elegido (para volver a ponerlo con un clic)

// «Mi musica»
std::string carpeta();    // <programa>/Musica (se crea si no existe)
int canciones();          // cuantos ficheros hay
std::wstring cancion();   // titulo de la que suena
void siguiente();

// Muestras PCM de un ambiente (16 bits, estereo, 22050 Hz); para las pruebas.
bool generar(int sonido, std::vector<short>& muestras);

// Campanilla del temporizador: 0 = fin de un bloque de estudio (baja), 1 = fin del descanso (sube).
// Suena por encima de la musica de fondo, sin cortarla.
void campana(int tipo);
// Muestras de la campanilla (16 bits, mono, 22050 Hz); para las pruebas.
void generarCampana(int tipo, std::vector<short>& muestras);

}  // namespace musica
