#include "Almacen.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "Json.hpp"
#include "Rutas.hpp"

Almacen::Almacen(std::string ruta) : ruta_(std::move(ruta)) {}

// --------------------------------------------------------------- conversion

static js::Valor aJson(const Examen& e) {
    js::Valor v = js::Valor::objeto();
    v["nombre"] = js::Valor(e.nombre);
    v["fecha"] = js::Valor(e.fecha.aTexto());
    v["ponderacion"] = js::Valor(e.ponderacion);
    v["calificado"] = js::Valor(e.calificado);
    v["nota"] = js::Valor(e.nota);
    v["apuntes"] = js::Valor(e.apuntes);
    return v;
}

static Examen examenDesdeJson(const js::Valor& v) {
    Examen e;
    e.nombre = v["nombre"].comoCadena("Sin nombre");
    Fecha::desdeTexto(v["fecha"].comoCadena(), e.fecha);
    e.ponderacion = v["ponderacion"].comoNumero(0.0);
    e.calificado = v["calificado"].comoBool(false);
    e.nota = v["nota"].comoNumero(0.0);
    e.apuntes = v["apuntes"].comoCadena();
    e.fecha.hora = -1;  // ya no se guarda la hora de los examenes
    return e;
}

static js::Valor aJson(const Asignatura& a) {
    js::Valor v = js::Valor::objeto();
    v["codigo"] = js::Valor(a.codigo);
    v["nombre"] = js::Valor(a.nombre);
    v["profesor"] = js::Valor(a.profesor);
    v["creditos"] = js::Valor(a.creditos);
    v["cuatrimestre"] = js::Valor(a.cuatrimestre);
    v["cuatri"] = js::Valor(a.cuatri);
    v["diasClase"] = js::Valor(a.diasClase);
    v["color"] = js::Valor(a.color);
    v["carpetaTemario"] = js::Valor(a.carpetaTemario);
    v["carpetaPracticas"] = js::Valor(a.carpetaPracticas);
    js::Valor lista = js::Valor::array();
    for (const auto& e : a.examenes) lista.agregar(aJson(e));
    v["examenes"] = lista;
    return v;
}

static Asignatura asignaturaDesdeJson(const js::Valor& v) {
    Asignatura a;
    a.codigo = v["codigo"].comoCadena();
    a.nombre = v["nombre"].comoCadena("Sin nombre");
    a.profesor = v["profesor"].comoCadena();
    a.creditos = v["creditos"].comoEntero(6);
    a.cuatrimestre = v["cuatrimestre"].comoCadena();
    a.cuatri = v["cuatri"].comoEntero(1);
    if (a.cuatri != 1 && a.cuatri != 2) a.cuatri = 1;
    a.diasClase = normalizarDiasClase(v["diasClase"].comoCadena());
    a.color = v["color"].comoCadena();
    a.carpetaTemario = v["carpetaTemario"].comoCadena();
    a.carpetaPracticas = v["carpetaPracticas"].comoCadena();
    const js::Valor& lista = v["examenes"];
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        a.examenes.push_back(examenDesdeJson(lista[i]));
    }
    a.ordenarExamenes();
    return a;
}

// ------------------------------------------------------------------ ficheros

bool Almacen::cargar(Curso& curso) {
    error_.clear();
    if (!rutas::existe(ruta_)) {
        curso = cursoDeEjemplo();
        return true;  // primer arranque: datos de ejemplo
    }

    std::ifstream f(ruta_, std::ios::binary);
    if (!f) {
        error_ = "No se pudo abrir " + ruta_;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();

    js::Valor raiz;
    std::string errorJson;
    if (!js::Valor::parsear(ss.str(), raiz, errorJson)) {
        error_ = "JSON invalido: " + errorJson;
        return false;
    }

    Curso nuevo;
    nuevo.titulo = raiz["titulo"].comoCadena("Mi curso");
    nuevo.temaOscuro = raiz["temaOscuro"].comoBool(true);
    const js::Valor& aj = raiz["ajustes"];
    Ajustes& ajustes = nuevo.ajustes;
    ajustes.colorAcento = aj["colorAcento"].comoCadena("#5b9dff");
    ajustes.escalaInterfaz = aj["escalaInterfaz"].comoEntero(100);
    if (ajustes.escalaInterfaz < 100) ajustes.escalaInterfaz = 100;
    if (ajustes.escalaInterfaz > 150) ajustes.escalaInterfaz = 150;
    ajustes.primerDiaLunes = aj["primerDiaLunes"].comoBool(true);
    ajustes.mostrarFinDeSemana = aj["mostrarFinDeSemana"].comoBool(true);
    ajustes.diasDeAviso = aj["diasDeAviso"].comoEntero(7);
    if (ajustes.diasDeAviso < 0) ajustes.diasDeAviso = 0;
    if (ajustes.diasDeAviso > 60) ajustes.diasDeAviso = 60;
    ajustes.notaObjetivo = aj["notaObjetivo"].comoNumero(5.0);
    if (ajustes.notaObjetivo < 0) ajustes.notaObjetivo = 0;
    if (ajustes.notaObjetivo > 10) ajustes.notaObjetivo = 10;
    ajustes.ordenarPorDia = aj["ordenarPorDia"].comoBool(false);
    ajustes.confirmarBorrado = aj["confirmarBorrado"].comoBool(true);
    auto entre = [](int v, int minimo, int maximo) { return v < minimo ? minimo : v > maximo ? maximo : v; };
    ajustes.temaSistema = aj["temaSistema"].comoBool(false);
    ajustes.paleta = entre(aj["paleta"].comoEntero(0), 0, 20);
    ajustes.fuente = aj["fuente"].comoCadena("");
    ajustes.tamTexto = entre(aj["tamTexto"].comoEntero(100), 85, 135);
    ajustes.esquinas = entre(aj["esquinas"].comoEntero(100), 0, 200);
    ajustes.fondo = entre(aj["fondo"].comoEntero(2), 0, 3);
    ajustes.cristal = aj["cristal"].comoBool(true);
    ajustes.animaciones = aj["animaciones"].comoBool(true);
    ajustes.barra = entre(aj["barra"].comoEntero(0), 0, 3);
    ajustes.contraste = aj["contraste"].comoBool(false);
    ajustes.anchura = entre(aj["anchura"].comoEntero(0), 0, 2);
    ajustes.pomodoro = entre(aj["pomodoro"].comoEntero(25), 5, 90);
    ajustes.descanso = entre(aj["descanso"].comoEntero(5), 1, 30);
    ajustes.musicaAlConcentrarse = aj["musicaAlConcentrarse"].comoBool(false);
    ajustes.campana = aj["campana"].comoBool(true);
    ajustes.respiracion = aj["respiracion"].comoBool(true);
    ajustes.objetivoDiario = entre(aj["objetivoDiario"].comoEntero(60), 0, 600);
    ajustes.efectosAmbiente = aj["efectosAmbiente"].comoBool(true);
    const js::Valor& lista = raiz["asignaturas"];
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        nuevo.asignaturas.push_back(asignaturaDesdeJson(lista[i]));
    }
    curso = std::move(nuevo);
    return true;
}

bool Almacen::guardar(const Curso& curso) {
    error_.clear();
    js::Valor raiz = js::Valor::objeto();
    raiz["titulo"] = js::Valor(curso.titulo);
    raiz["temaOscuro"] = js::Valor(curso.temaOscuro);
    raiz["guardadoEl"] = js::Valor(Fecha::hoy().aTexto());

    js::Valor ajustes = js::Valor::objeto();
    ajustes["colorAcento"] = js::Valor(curso.ajustes.colorAcento);
    ajustes["escalaInterfaz"] = js::Valor(curso.ajustes.escalaInterfaz);
    ajustes["primerDiaLunes"] = js::Valor(curso.ajustes.primerDiaLunes);
    ajustes["mostrarFinDeSemana"] = js::Valor(curso.ajustes.mostrarFinDeSemana);
    ajustes["diasDeAviso"] = js::Valor(curso.ajustes.diasDeAviso);
    ajustes["notaObjetivo"] = js::Valor(curso.ajustes.notaObjetivo);
    ajustes["ordenarPorDia"] = js::Valor(curso.ajustes.ordenarPorDia);
    ajustes["confirmarBorrado"] = js::Valor(curso.ajustes.confirmarBorrado);
    const Ajustes& a = curso.ajustes;
    ajustes["temaSistema"] = js::Valor(a.temaSistema);
    ajustes["paleta"] = js::Valor(a.paleta);
    ajustes["fuente"] = js::Valor(a.fuente);
    ajustes["tamTexto"] = js::Valor(a.tamTexto);
    ajustes["esquinas"] = js::Valor(a.esquinas);
    ajustes["fondo"] = js::Valor(a.fondo);
    ajustes["cristal"] = js::Valor(a.cristal);
    ajustes["animaciones"] = js::Valor(a.animaciones);
    ajustes["barra"] = js::Valor(a.barra);
    ajustes["contraste"] = js::Valor(a.contraste);
    ajustes["anchura"] = js::Valor(a.anchura);
    ajustes["pomodoro"] = js::Valor(a.pomodoro);
    ajustes["descanso"] = js::Valor(a.descanso);
    ajustes["musicaAlConcentrarse"] = js::Valor(a.musicaAlConcentrarse);
    ajustes["campana"] = js::Valor(a.campana);
    ajustes["respiracion"] = js::Valor(a.respiracion);
    ajustes["objetivoDiario"] = js::Valor(a.objetivoDiario);
    ajustes["efectosAmbiente"] = js::Valor(a.efectosAmbiente);
    raiz["ajustes"] = ajustes;
    js::Valor lista = js::Valor::array();
    for (const auto& a : curso.asignaturas) lista.agregar(aJson(a));
    raiz["asignaturas"] = lista;

    const std::string carpeta = rutas::padre(ruta_);
    if (!carpeta.empty() && !rutas::crearDirectorios(carpeta)) {
        error_ = "No se pudo crear la carpeta " + carpeta;
        return false;
    }

    const std::string temporal = ruta_ + ".tmp";
    {
        std::ofstream f(temporal.c_str(), std::ios::binary | std::ios::trunc);
        if (!f) {
            error_ = "No se pudo escribir en " + temporal;
            return false;
        }
        f << raiz.volcar(2) << "\n";
        if (!f.good()) {
            error_ = "Error escribiendo los datos";
            return false;
        }
    }

    if (!rutas::renombrarSobrescribiendo(temporal, ruta_)) {
        error_ = "No se pudo reemplazar el fichero " + ruta_;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Curso inicial: Grado en Ingenieria Informatica (G781), 3 curso, grupo 3A.
// Los dias salen del horario oficial 2026/2027 (solo interesa el dia, no la hora).
// ---------------------------------------------------------------------------
static Asignatura mkAsignatura(const char* codigo, const char* nombre, int cuatri,
                               const char* dias, const char* color) {
    Asignatura a;
    a.codigo = codigo;
    a.nombre = nombre;
    a.profesor = "";
    a.creditos = 6;
    a.cuatrimestre = "3.º curso · grupo 3A";
    a.cuatri = cuatri;
    a.diasClase = normalizarDiasClase(dias);
    a.color = color;
    return a;
}

Curso Almacen::cursoDeEjemplo() {
    Curso c;
    c.titulo = "Grado en Ingeniería Informática (G781) · 3.º curso, grupo 3A";

    c.asignaturas.push_back(
        mkAsignatura("SOA", "Sistemas Operativos Avanzados", 1, "L", "#5b9dff"));
    c.asignaturas.push_back(
        mkAsignatura("ISA", "Ingeniería del Software Avanzada", 1, "M", "#3ecf8e"));
    c.asignaturas.push_back(
        mkAsignatura("PL", "Procesadores del Lenguaje", 1, "X", "#f2c14e"));
    c.asignaturas.push_back(
        mkAsignatura("IA", "Inteligencia Artificial", 1, "J", "#ff6b6b"));
    c.asignaturas.push_back(
        mkAsignatura("CU", "Computación Ubicua", 1, "V", "#b57bff"));

    c.asignaturas.push_back(
        mkAsignatura("GP", "Gestión de Proyectos", 2, "L", "#4dd0e1"));
    c.asignaturas.push_back(
        mkAsignatura("SE", "Sistemas Empresariales", 2, "M", "#ff9f68"));
    c.asignaturas.push_back(
        mkAsignatura("CRA", "Conocimiento y Razonamiento Automatizado", 2, "X", "#9ccc65"));
    c.asignaturas.push_back(
        mkAsignatura("PLA", "Planificación Automática", 2, "J", "#f06292"));
    c.asignaturas.push_back(
        mkAsignatura("PAV", "Programación Avanzada", 2, "J", "#80cbc4"));
    c.asignaturas.push_back(
        mkAsignatura("PAP", "Paradigmas Avanzados de Programación", 2, "V", "#7c8cff"));

    for (std::size_t i = 0; i < c.asignaturas.size(); ++i) c.asignaturas[i].ordenarExamenes();
    return c;
}
