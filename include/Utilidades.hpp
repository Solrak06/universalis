// Utilidades.hpp - Consola (UTF-8, color), entrada validada y apertura de carpetas.
#pragma once

#include <string>

#include "Fecha.hpp"

namespace ui {

void inicializarConsola();   // UTF-8 + secuencias ANSI en Windows
void limpiarPantalla();
bool hayColor();

// Codigos de color (cadenas vacias si la salida no soporta color).
const char* reset();
const char* negrita();
const char* gris();
const char* rojo();
const char* verde();
const char* amarillo();
const char* azul();
const char* cian();
const char* magenta();

// Color segun la nota: rojo (<5), amarillo (<7), verde (>=7).
const char* colorNota(double nota);

void titulo(const std::string& texto);
void subtitulo(const std::string& texto);
void linea(char c = '-', int ancho = 64);
void aviso(const std::string& texto);
void exito(const std::string& texto);
void error(const std::string& texto);

// --- Entrada. Todas devuelven aunque el usuario escriba basura.
std::string leerLinea(const std::string& prompt);
std::string leerLineaConDefecto(const std::string& prompt, const std::string& porDefecto);
int leerEntero(const std::string& prompt, int minimo, int maximo, int porDefecto);
double leerDecimal(const std::string& prompt, double minimo, double maximo, double porDefecto);
bool leerFecha(const std::string& prompt, Fecha& salida, bool permitirVacio);
bool confirmar(const std::string& prompt, bool porDefecto);
void pausa();

// --- Sistema de ficheros
bool existeRuta(const std::string& ruta);
// Abre una carpeta (o fichero) con la aplicacion predeterminada del sistema.
bool abrirRuta(const std::string& ruta, std::string& mensajeError);

// Recorta espacios al principio y al final.
std::string recortar(const std::string& s);
// Ancho visible aproximado de una cadena UTF-8 (cuenta caracteres, no bytes).
std::size_t anchoVisible(const std::string& s);
// Rellena o recorta a 'ancho' caracteres para alinear columnas.
std::string encajar(const std::string& s, std::size_t ancho);

}  // namespace ui
