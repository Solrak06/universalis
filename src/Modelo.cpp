#include "Modelo.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "Rutas.hpp"

// ------------------------------------------------------------------- Examen

double Examen::aportacion() const {
    if (!calificado) return 0.0;
    return nota * (ponderacion / 100.0);
}

bool Examen::esFuturo() const {
    Fecha ahora = Fecha::hoy();
    return !(fecha < ahora);
}

// --------------------------------------------------------------- Asignatura

double Asignatura::ponderacionTotal() const {
    double total = 0.0;
    for (const auto& e : examenes) total += e.ponderacion;
    return total;
}

double Asignatura::ponderacionEvaluada() const {
    double total = 0.0;
    for (const auto& e : examenes) {
        if (e.calificado) total += e.ponderacion;
    }
    return total;
}

double Asignatura::notaMediaActual() const {
    double peso = ponderacionEvaluada();
    if (peso <= 0.0) return 0.0;
    double suma = 0.0;
    for (const auto& e : examenes) {
        if (e.calificado) suma += e.nota * e.ponderacion;
    }
    return suma / peso;
}

double Asignatura::notaAcumulada() const {
    double suma = 0.0;
    for (const auto& e : examenes) suma += e.aportacion();
    return suma;
}

double Asignatura::notaNecesaria(double objetivo) const {
    double restante = ponderacionTotal() - ponderacionEvaluada();
    if (restante <= 0.001) return -1.0;  // ya no queda nada por evaluar
    double faltan = objetivo - notaAcumulada();
    if (faltan <= 0.0) return 0.0;       // el objetivo ya esta asegurado
    return faltan / (restante / 100.0);  // media necesaria en lo que queda
}

bool Asignatura::tieneNotas() const {
    for (const auto& e : examenes) {
        if (e.calificado) return true;
    }
    return false;
}

void Asignatura::ordenarExamenes() {
    std::stable_sort(examenes.begin(), examenes.end(),
                     [](const Examen& a, const Examen& b) { return a.fecha < b.fecha; });
}

// -------------------------------------------------------------------- Curso

Asignatura* Curso::buscarPorCodigo(const std::string& codigo) {
    for (auto& a : asignaturas) {
        if (a.codigo == codigo) return &a;
    }
    return nullptr;
}

int Curso::creditosConNota() const {
    int total = 0;
    for (const auto& a : asignaturas) {
        if (a.tieneNotas()) total += a.creditos;
    }
    return total;
}

double Curso::mediaExpediente() const {
    double suma = 0.0;
    int creditos = 0;
    for (const auto& a : asignaturas) {
        if (!a.tieneNotas()) continue;
        suma += a.notaMediaActual() * a.creditos;
        creditos += a.creditos;
    }
    return creditos > 0 ? suma / creditos : 0.0;
}

// ------------------------------------------------------------ Vistas globales

std::vector<ExamenConAsignatura> todosLosExamenes(const Curso& curso) {
    std::vector<ExamenConAsignatura> lista;
    for (const auto& a : curso.asignaturas) {
        for (const auto& e : a.examenes) {
            ExamenConAsignatura ea;
            ea.asignatura = &a;
            ea.examen = &e;
            lista.push_back(ea);
        }
    }
    std::sort(lista.begin(), lista.end(),
              [](const ExamenConAsignatura& x, const ExamenConAsignatura& y) {
                  return x.examen->fecha < y.examen->fecha;
              });
    return lista;
}

std::vector<ExamenConAsignatura> proximosExamenes(const Curso& curso, int limite) {
    Fecha hoy = Fecha::hoy();
    hoy.hora = -1;
    std::vector<ExamenConAsignatura> lista;
    for (const auto& ea : todosLosExamenes(curso)) {
        if (hoy.diasHasta(ea.examen->fecha) >= 0) lista.push_back(ea);
        if (limite > 0 && static_cast<int>(lista.size()) >= limite) break;
    }
    return lista;
}

// ------------------------------------------------------- Vistas del curso

int primerDiaDeClase(const Asignatura& a) {
    for (int d = 0; d < 7; ++d) {
        if (hayClase(a, d)) return d;
    }
    return 7;  // sin dias puestos: al final
}

std::vector<std::size_t> asignaturasDe(const Curso& curso, int cuatri, bool ordenarPorDia) {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < curso.asignaturas.size(); ++i) {
        if (curso.asignaturas[i].cuatri == cuatri) indices.push_back(i);
    }
    if (ordenarPorDia) {
        // orden estable: primero por dia de clase, luego por nombre
        for (std::size_t i = 1; i < indices.size(); ++i) {
            std::size_t actual = indices[i];
            int diaActual = primerDiaDeClase(curso.asignaturas[actual]);
            std::size_t j = i;
            while (j > 0) {
                std::size_t previo = indices[j - 1];
                int diaPrevio = primerDiaDeClase(curso.asignaturas[previo]);
                bool despues = (diaPrevio > diaActual) ||
                               (diaPrevio == diaActual &&
                                curso.asignaturas[previo].nombre >
                                    curso.asignaturas[actual].nombre);
                if (!despues) break;
                indices[j] = previo;
                --j;
            }
            indices[j] = actual;
        }
    }
    return indices;
}

long diasHastaProximaPrueba(const Asignatura& a) {
    Fecha hoy = Fecha::hoy();
    hoy.hora = -1;
    long mejor = -1;
    for (std::size_t i = 0; i < a.examenes.size(); ++i) {
        long dias = hoy.diasHasta(a.examenes[i].fecha);
        if (dias < 0) continue;
        if (mejor < 0 || dias < mejor) mejor = dias;
    }
    return mejor;
}

int pruebasPendientes(const Curso& curso) {
    int total = 0;
    for (std::size_t i = 0; i < curso.asignaturas.size(); ++i) {
        const Asignatura& a = curso.asignaturas[i];
        for (std::size_t j = 0; j < a.examenes.size(); ++j) {
            if (!a.examenes[j].calificado) ++total;
        }
    }
    return total;
}

double mediaDeCuatrimestre(const Curso& curso, int cuatri) {
    double suma = 0.0;
    int creditos = 0;
    for (std::size_t i = 0; i < curso.asignaturas.size(); ++i) {
        const Asignatura& a = curso.asignaturas[i];
        if (a.cuatri != cuatri || !a.tieneNotas()) continue;
        suma += a.notaMediaActual() * a.creditos;
        creditos += a.creditos;
    }
    return creditos > 0 ? suma / creditos : 0.0;
}

Asignatura duplicarAsignatura(const Asignatura& original) {
    Asignatura copia = original;
    copia.codigo = original.codigo + "2";
    copia.nombre = original.nombre + " (copia)";
    copia.carpetaTemario.clear();    // que use su carpeta nueva
    copia.carpetaPracticas.clear();
    for (std::size_t i = 0; i < copia.examenes.size(); ++i) {
        copia.examenes[i].calificado = false;
        copia.examenes[i].nota = 0.0;
    }
    return copia;
}

// ----------------------------------------------------------------- Carpetas

std::string nombreSeguroParaCarpeta(const std::string& texto) {
    std::string salida;
    for (std::size_t i = 0; i < texto.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(texto[i]);
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            continue;
        }
        if (c == ' ' && (salida.empty() || salida[salida.size() - 1] == ' ')) continue;
        salida += static_cast<char>(c);
    }
    while (!salida.empty() && (salida[salida.size() - 1] == ' ' || salida[salida.size() - 1] == '.')) {
        salida.erase(salida.size() - 1);
    }
    if (salida.empty()) salida = "Sin nombre";
    return salida;
}

std::string carpetaRaizDeAsignatura(const Asignatura& a) {
    std::string clave = a.codigo.empty() ? a.nombre : a.codigo;
    return rutas::unir(rutas::directorioDelEjecutable(),
                       std::string("Asignaturas/") + nombreSeguroParaCarpeta(clave));
}

std::string carpetaRelativaPorDefecto(const Asignatura& a, bool temario) {
    std::string clave = a.codigo.empty() ? a.nombre : a.codigo;
    return std::string("Asignaturas/") + nombreSeguroParaCarpeta(clave) +
           (temario ? "/Temario" : "/Practicas");
}

std::string carpetaDeAsignatura(const Asignatura& a, bool temario) {
    const std::string base = rutas::directorioDelEjecutable();
    const std::string personalizada = temario ? a.carpetaTemario : a.carpetaPracticas;

    if (!personalizada.empty()) {
        if (!rutas::esAbsoluta(personalizada)) {
            return rutas::unir(base, personalizada);  // relativa al programa
        }
        // Ruta absoluta antigua: solo se respeta si sigue existiendo.
        if (rutas::existe(personalizada)) return rutas::normalizar(personalizada);
    }
    return rutas::unir(base, carpetaRelativaPorDefecto(a, temario));
}

std::string prepararCarpetaDeAsignatura(const Asignatura& a, bool temario) {
    std::string ruta = carpetaDeAsignatura(a, temario);
    if (ruta.empty()) return "";
    if (!rutas::existe(ruta) && !rutas::crearDirectorios(ruta)) return "";
    return ruta;
}

void prepararCarpetasDelCurso(const Curso& curso) {
    for (std::size_t i = 0; i < curso.asignaturas.size(); ++i) {
        prepararCarpetaDeAsignatura(curso.asignaturas[i], true);
        prepararCarpetaDeAsignatura(curso.asignaturas[i], false);
    }
}

// ------------------------------------------------------------------ Colores

namespace {
const char* kPaleta[] = {"#5b9dff", "#3ecf8e", "#f2c14e", "#ff6b6b",
                         "#b57bff", "#4dd0e1", "#ff9f68", "#9ccc65"};
}

std::size_t coloresDisponibles() {
    return sizeof(kPaleta) / sizeof(kPaleta[0]);
}

std::string colorDePaleta(std::size_t indice) {
    return kPaleta[indice % coloresDisponibles()];
}

bool colorDesdeTexto(const std::string& texto, unsigned long& rgb) {
    std::string s = texto;
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    if (s.size() != 6) return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    rgb = std::strtoul(s.c_str(), 0, 16);
    return true;
}

std::string colorATexto(unsigned long rgb) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%06lx", rgb & 0xFFFFFFul);
    return buf;
}

// -------------------------------------------------------- Dias de la semana

namespace {
const char* kLetrasDias[] = {"L", "M", "X", "J", "V", "S", "D"};
const char* kNombresDias[] = {"lunes", "martes", "miércoles", "jueves",
                              "viernes", "sábado", "domingo"};
const char* kNombresCortos[] = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
}  // namespace

const char* letraDiaSemana(int diaSemana) {
    return (diaSemana >= 0 && diaSemana < 7) ? kLetrasDias[diaSemana] : "?";
}

std::string nombreDiaSemana(int diaSemana) {
    return (diaSemana >= 0 && diaSemana < 7) ? kNombresDias[diaSemana] : "?";
}

std::string normalizarDiasClase(const std::string& texto) {
    std::string salida;
    for (int d = 0; d < 7; ++d) {
        char letra = kLetrasDias[d][0];
        for (std::size_t i = 0; i < texto.size(); ++i) {
            char c = static_cast<char>(std::toupper(static_cast<unsigned char>(texto[i])));
            if (c == letra) {
                salida += letra;
                break;
            }
        }
    }
    return salida;
}

bool hayClase(const Asignatura& a, int diaSemana) {
    if (diaSemana < 0 || diaSemana > 6) return false;
    char letra = kLetrasDias[diaSemana][0];
    for (std::size_t i = 0; i < a.diasClase.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a.diasClase[i])) == letra) return true;
    }
    return false;
}

std::string diasClaseLegibles(const Asignatura& a) {
    std::string salida;
    for (int d = 0; d < 7; ++d) {
        if (!hayClase(a, d)) continue;
        if (!salida.empty()) salida += ", ";
        salida += kNombresCortos[d];
    }
    return salida;
}

// ----------------------------------------------------------- Calificaciones

int nivelDeNota(double nota) {
    if (nota < 5.0) return NIVEL_SUSPENSO;
    if (nota < 6.0) return NIVEL_APROBADO;
    if (nota < 7.0) return NIVEL_BIEN;
    if (nota < 9.0) return NIVEL_NOTABLE;
    return NIVEL_SOBRESALIENTE;
}

std::string calificacionDeNivel(int nivel) {
    switch (nivel) {
        case NIVEL_SUSPENSO: return "Suspenso";
        case NIVEL_APROBADO: return "Aprobado";
        case NIVEL_BIEN: return "Bien";
        case NIVEL_NOTABLE: return "Notable";
        default: return "Sobresaliente";
    }
}

std::string calificacionDeNota(double nota) {
    return calificacionDeNivel(nivelDeNota(nota));
}

// ------------------------------------------------------------------ Formato

std::string formatearNota(double nota, int decimales) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", decimales, nota);
    return buf;
}

std::string barraProgreso(double porcentaje, int ancho) {
    if (porcentaje < 0) porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;
    int llenos = static_cast<int>((porcentaje / 100.0) * ancho + 0.5);
    std::string s;
    s.reserve(static_cast<std::size_t>(ancho) + 2);
    s += '[';
    for (int i = 0; i < ancho; ++i) s += (i < llenos ? '#' : '.');
    s += ']';
    return s;
}
