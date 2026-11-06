#include "Rutas.hpp"

#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <direct.h>
#  define MI_STAT_S struct _stat
#  define MI_STAT   _stat
#  define MI_MKDIR(p) _mkdir(p)
#  define MI_GETCWD(b, n) _getcwd(b, n)
#else
#  include <unistd.h>
#  define MI_STAT_S struct stat
#  define MI_STAT   stat
#  define MI_MKDIR(p) mkdir(p, 0755)
#  define MI_GETCWD(b, n) getcwd(b, n)
#endif

namespace rutas {

namespace {
bool datos(const std::string& ruta, MI_STAT_S& st) {
    if (ruta.empty()) return false;
    std::string limpia = normalizar(ruta);
    // En Windows, _stat falla con "C:/" si lleva barra final; ya la quita normalizar,
    // pero la raiz de unidad necesita conservarla.
    if (limpia.size() == 2 && limpia[1] == ':') limpia += '/';
    return MI_STAT(limpia.c_str(), &st) == 0;
}
}  // namespace

std::string normalizar(const std::string& ruta) {
    std::string s = ruta;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\') s[i] = '/';
    }
    while (s.size() > 1 && s[s.size() - 1] == '/') s.erase(s.size() - 1);
    return s;
}

bool existe(const std::string& ruta) {
    MI_STAT_S st;
    return datos(ruta, st);
}

bool esDirectorio(const std::string& ruta) {
    MI_STAT_S st;
    if (!datos(ruta, st)) return false;
    return (st.st_mode & S_IFDIR) != 0;
}

std::string padre(const std::string& ruta) {
    std::string s = normalizar(ruta);
    std::size_t corte = s.find_last_of('/');
    if (corte == std::string::npos) return "";
    if (corte == 0) return "/";
    return s.substr(0, corte);
}

std::string unir(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    std::string izq = normalizar(a);
    std::string der = b;
    while (!der.empty() && (der[0] == '/' || der[0] == '\\')) der.erase(0, 1);
    if (!izq.empty() && izq[izq.size() - 1] == '/') return izq + der;
    return izq + "/" + der;
}

bool esAbsoluta(const std::string& ruta) {
    if (ruta.empty()) return false;
    if (ruta[0] == '/' || ruta[0] == '\\') return true;
    return ruta.size() > 1 && ruta[1] == ':';  // C:\...
}

std::string absoluta(const std::string& ruta) {
    if (esAbsoluta(ruta)) return normalizar(ruta);
    char buf[4096];
    if (!MI_GETCWD(buf, static_cast<int>(sizeof(buf)))) return normalizar(ruta);
    return unir(normalizar(buf), normalizar(ruta));
}

std::string directorioDelEjecutable() {
#if defined(_WIN32)
    wchar_t bufW[4096];
    DWORD n = GetModuleFileNameW(NULL, bufW, 4095);
    if (n == 0) return "";
    bufW[n] = L'\0';
    int bytes = WideCharToMultiByte(CP_UTF8, 0, bufW, (int)n, NULL, 0, NULL, NULL);
    if (bytes <= 0) return "";
    std::string ruta((size_t)bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, bufW, (int)n, &ruta[0], bytes, NULL, NULL);
    return padre(ruta);
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return padre(std::string(buf));
#endif
}

bool crearDirectorios(const std::string& ruta) {
    std::string s = normalizar(ruta);
    if (s.empty()) return true;

    std::size_t i = 0;
    if (s.size() > 1 && s[1] == ':') i = 2;   // saltar "C:"
    else if (!s.empty() && s[0] == '/') i = 1;  // saltar la raiz

    while (i <= s.size()) {
        if (i == s.size() || s[i] == '/') {
            std::string trozo = s.substr(0, i);
            if (!trozo.empty() && trozo != "." && !(trozo.size() == 2 && trozo[1] == ':')) {
                if (!esDirectorio(trozo)) {
                    if (MI_MKDIR(trozo.c_str()) != 0 && !esDirectorio(trozo)) return false;
                }
            }
        }
        ++i;
    }
    return esDirectorio(s);
}

bool borrar(const std::string& ruta) {
    if (!existe(ruta)) return true;
    return std::remove(normalizar(ruta).c_str()) == 0;
}

bool renombrarSobrescribiendo(const std::string& origen, const std::string& destino) {
    std::string o = normalizar(origen);
    std::string d = normalizar(destino);
    if (existe(d)) std::remove(d.c_str());
    return std::rename(o.c_str(), d.c_str()) == 0;
}

}  // namespace rutas
