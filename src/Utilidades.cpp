#include "Utilidades.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "Rutas.hpp"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <io.h>
#  include <shellapi.h>
#else
#  include <unistd.h>
#endif

namespace ui {

namespace {
bool g_color = false;

bool salidaEsTerminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}
}  // namespace

void inicializarConsola() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &modo)) {
        SetConsoleMode(h, modo | 0x0004 /*ENABLE_VIRTUAL_TERMINAL_PROCESSING*/);
    }
#endif
    g_color = salidaEsTerminal();
    std::ios::sync_with_stdio(true);
}

bool hayColor() { return g_color; }

const char* reset()    { return g_color ? "\033[0m"  : ""; }
const char* negrita()  { return g_color ? "\033[1m"  : ""; }
const char* gris()     { return g_color ? "\033[90m" : ""; }
const char* rojo()     { return g_color ? "\033[31m" : ""; }
const char* verde()    { return g_color ? "\033[32m" : ""; }
const char* amarillo() { return g_color ? "\033[33m" : ""; }
const char* azul()     { return g_color ? "\033[34m" : ""; }
const char* cian()     { return g_color ? "\033[36m" : ""; }
const char* magenta()  { return g_color ? "\033[35m" : ""; }

const char* colorNota(double nota) {
    if (nota < 5.0) return rojo();
    if (nota < 7.0) return amarillo();
    return verde();
}

void limpiarPantalla() {
    if (g_color) {
        std::cout << "\033[2J\033[H";
    } else {
        std::cout << "\n\n";
    }
}

void linea(char c, int ancho) {
    std::cout << gris() << std::string(static_cast<std::size_t>(ancho), c) << reset() << "\n";
}

void titulo(const std::string& texto) {
    std::cout << "\n" << negrita() << cian() << texto << reset() << "\n";
    linea('=', 64);
}

void subtitulo(const std::string& texto) {
    std::cout << "\n" << negrita() << texto << reset() << "\n";
}

void aviso(const std::string& texto)  { std::cout << amarillo() << "! " << texto << reset() << "\n"; }
void exito(const std::string& texto)  { std::cout << verde()    << "+ " << texto << reset() << "\n"; }
void error(const std::string& texto)  { std::cout << rojo()     << "x " << texto << reset() << "\n"; }

// ------------------------------------------------------------------- entrada

std::string recortar(const std::string& s) {
    std::size_t ini = s.find_first_not_of(" \t\r\n");
    if (ini == std::string::npos) return "";
    std::size_t fin = s.find_last_not_of(" \t\r\n");
    return s.substr(ini, fin - ini + 1);
}

std::string leerLinea(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();
    std::string s;
    if (!std::getline(std::cin, s)) {
        std::cin.clear();
        return "";
    }
    return recortar(s);
}

std::string leerLineaConDefecto(const std::string& prompt, const std::string& porDefecto) {
    std::string etiqueta = prompt;
    if (!porDefecto.empty()) {
        etiqueta += std::string(" [") + porDefecto + "]";
    }
    etiqueta += ": ";
    std::string s = leerLinea(etiqueta);
    return s.empty() ? porDefecto : s;
}

int leerEntero(const std::string& prompt, int minimo, int maximo, int porDefecto) {
    while (true) {
        std::ostringstream etiqueta;
        etiqueta << prompt << " [" << porDefecto << "]: ";
        std::string s = leerLinea(etiqueta.str());
        if (s.empty()) return porDefecto;
        try {
            std::size_t pos = 0;
            int v = std::stoi(s, &pos);
            if (pos == s.size() && v >= minimo && v <= maximo) return v;
        } catch (...) {
        }
        error("Introduce un numero entero entre " + std::to_string(minimo) + " y " +
              std::to_string(maximo) + ".");
    }
}

double leerDecimal(const std::string& prompt, double minimo, double maximo, double porDefecto) {
    while (true) {
        std::ostringstream etiqueta;
        etiqueta << prompt << " [" << porDefecto << "]: ";
        std::string s = leerLinea(etiqueta.str());
        if (s.empty()) return porDefecto;
        for (auto& c : s) {
            if (c == ',') c = '.';  // aceptar coma decimal
        }
        try {
            std::size_t pos = 0;
            double v = std::stod(s, &pos);
            if (pos == s.size() && v >= minimo && v <= maximo) return v;
        } catch (...) {
        }
        char rango[64];
        std::snprintf(rango, sizeof(rango), "%g y %g", minimo, maximo);
        error(std::string("Introduce un numero entre ") + rango + ".");
    }
}

bool leerFecha(const std::string& prompt, Fecha& salida, bool permitirVacio) {
    while (true) {
        std::string s = leerLinea(prompt + " (AAAA-MM-DD [HH:MM]): ");
        if (s.empty()) {
            if (permitirVacio) return false;
            error("La fecha es obligatoria.");
            continue;
        }
        Fecha f;
        if (Fecha::desdeTexto(s, f)) {
            salida = f;
            return true;
        }
        error("Formato no valido. Ejemplos: 2026-12-03  |  2026-12-03 09:30  |  03/12/2026");
    }
}

bool confirmar(const std::string& prompt, bool porDefecto) {
    std::string sufijo = porDefecto ? " [S/n]: " : " [s/N]: ";
    while (true) {
        std::string s = leerLinea(prompt + sufijo);
        if (s.empty()) return porDefecto;
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
        if (c == 's' || c == 'y') return true;
        if (c == 'n') return false;
    }
}

void pausa() {
    std::cout << gris() << "\n(pulsa Enter para continuar)" << reset();
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
}

// ------------------------------------------------------------------ ficheros

bool existeRuta(const std::string& ruta) {
    return rutas::existe(ruta);
}

bool abrirRuta(const std::string& ruta, std::string& mensajeError) {
    mensajeError.clear();
    if (ruta.empty()) {
        mensajeError = "Esta asignatura no tiene ninguna carpeta configurada.";
        return false;
    }
    if (!existeRuta(ruta)) {
        mensajeError = "La ruta no existe: " + ruta;
        return false;
    }

#if defined(_WIN32)
    HINSTANCE r = ShellExecuteA(nullptr, "open", ruta.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32) {
        mensajeError = "El sistema no pudo abrir la ruta.";
        return false;
    }
    return true;
#elif defined(__APPLE__)
    std::string cmd = "open \"" + ruta + "\"";
    if (std::system(cmd.c_str()) != 0) {
        mensajeError = "El sistema no pudo abrir la ruta.";
        return false;
    }
    return true;
#else
    std::string cmd = "xdg-open \"" + ruta + "\" >/dev/null 2>&1 &";
    if (std::system(cmd.c_str()) != 0) {
        mensajeError = "El sistema no pudo abrir la ruta.";
        return false;
    }
    return true;
#endif
}

// -------------------------------------------------------------------- texto

std::size_t anchoVisible(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;  // no contar bytes de continuacion UTF-8
    }
    return n;
}

std::string encajar(const std::string& s, std::size_t ancho) {
    std::size_t visible = anchoVisible(s);
    if (visible == ancho) return s;
    if (visible < ancho) return s + std::string(ancho - visible, ' ');

    // Recortar respetando limites de caracter UTF-8, dejando hueco para "..".
    std::size_t objetivo = ancho > 2 ? ancho - 2 : ancho;
    std::size_t contados = 0, i = 0;
    while (i < s.size() && contados < objetivo) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t paso = 1;
        if ((c & 0xF8) == 0xF0) paso = 4;
        else if ((c & 0xF0) == 0xE0) paso = 3;
        else if ((c & 0xE0) == 0xC0) paso = 2;
        i += paso;
        ++contados;
    }
    std::string cortado = s.substr(0, i);
    if (ancho > 2) cortado += "..";
    std::size_t v = anchoVisible(cortado);
    if (v < ancho) cortado += std::string(ancho - v, ' ');
    return cortado;
}

}  // namespace ui
