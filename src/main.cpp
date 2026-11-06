// Gestor de Asignaturas - aplicacion de consola para llevar el control de
// examenes, notas ponderadas y material de cada asignatura.
//
//   Uso: gestor_asignaturas [ruta_del_fichero.json]
//
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "Almacen.hpp"
#include "Calendario.hpp"
#include "ExportadorHtml.hpp"
#include "Modelo.hpp"
#include "Rutas.hpp"
#include "Utilidades.hpp"

namespace {

Curso g_curso;
Almacen* g_almacen = nullptr;

void guardar() {
    if (!g_almacen) return;
    if (!g_almacen->guardar(g_curso)) {
        ui::error("No se pudieron guardar los cambios: " + g_almacen->ultimoError());
    }
}

// ------------------------------------------------------------------ pintado

void cabeceraApp() {
    ui::limpiarPantalla();
    std::cout << ui::negrita() << ui::cian() << "  GESTOR DE ASIGNATURAS" << ui::reset() << "  "
              << ui::gris() << g_curso.titulo << ui::reset() << "\n";
    ui::linea('=', 78);
}

std::string proximoExamenTexto(const Asignatura& a) {
    Fecha hoy = Fecha::hoy();
    const Examen* mejor = nullptr;
    for (const auto& e : a.examenes) {
        if (hoy.diasHasta(e.fecha) >= 0 && (!mejor || e.fecha < mejor->fecha)) mejor = &e;
    }
    if (!mejor) return "-";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d/%02d ", mejor->fecha.dia, mejor->fecha.mes);
    return std::string(buf) + mejor->nombre;
}

void tablaAsignaturas() {
    std::cout << ui::gris() << "  #  " << ui::encajar("CODIGO", 9) << ui::encajar("ASIGNATURA", 28)
              << ui::encajar("CR", 4) << ui::encajar("MEDIA", 7) << ui::encajar("EVALUADO", 10)
              << "PROXIMO EXAMEN" << ui::reset() << "\n";
    ui::linea('-', 78);

    if (g_curso.asignaturas.empty()) {
        std::cout << ui::gris()
                  << "  Todavia no hay asignaturas. Usa la opcion [n] para crear la primera.\n"
                  << ui::reset();
        return;
    }

    for (std::size_t i = 0; i < g_curso.asignaturas.size(); ++i) {
        const Asignatura& a = g_curso.asignaturas[i];
        double media = a.notaMediaActual();
        std::string mediaTxt = a.tieneNotas() ? formatearNota(media) : "--";

        std::cout << "  " << ui::encajar(std::to_string(i + 1), 3) << ui::cian()
                  << ui::encajar(a.codigo, 9) << ui::reset() << ui::encajar(a.nombre, 28)
                  << ui::encajar(std::to_string(a.creditos), 4);
        if (a.tieneNotas()) std::cout << ui::colorNota(media);
        std::cout << ui::encajar(mediaTxt, 7) << ui::reset()
                  << ui::encajar(formatearNota(a.ponderacionEvaluada(), 0) + "%", 10) << ui::gris()
                  << proximoExamenTexto(a) << ui::reset() << "\n";
    }

    ui::linea('-', 78);
    double media = g_curso.mediaExpediente();
    std::cout << "  " << ui::negrita() << "Media del expediente: " << ui::colorNota(media)
              << formatearNota(media) << ui::reset() << ui::gris() << "  ("
              << g_curso.creditosConNota() << " creditos con nota)" << ui::reset() << "\n";
}

void tablaExamenes(const Asignatura& a) {
    std::cout << ui::gris() << "  #  " << ui::encajar("PRUEBA", 26) << ui::encajar("FECHA", 22)
              << ui::encajar("PESO", 7) << ui::encajar("NOTA", 8) << "APORTA" << ui::reset()
              << "\n";
    ui::linea('-', 78);

    if (a.examenes.empty()) {
        std::cout << ui::gris() << "  Sin pruebas registradas.\n" << ui::reset();
        return;
    }

    for (std::size_t i = 0; i < a.examenes.size(); ++i) {
        const Examen& e = a.examenes[i];
        std::cout << "  " << ui::encajar(std::to_string(i + 1), 3) << ui::encajar(e.nombre, 26)
                  << ui::encajar(e.fecha.aTextoLargo(), 22)
                  << ui::encajar(formatearNota(e.ponderacion, 0) + "%", 7);
        if (e.calificado) {
            std::cout << ui::colorNota(e.nota) << ui::encajar(formatearNota(e.nota), 8)
                      << ui::reset() << formatearNota(e.aportacion()) << " pts";
        } else {
            std::cout << ui::gris() << ui::encajar("pendiente", 8) << "-" << ui::reset();
        }
        std::cout << "\n";
    }
}

void resumenAsignatura(const Asignatura& a) {
    double media = a.notaMediaActual();
    double evaluado = a.ponderacionEvaluada();
    double total = a.ponderacionTotal();

    ui::linea('-', 78);
    std::cout << "  Nota media actual: ";
    if (a.tieneNotas()) {
        std::cout << ui::negrita() << ui::colorNota(media) << formatearNota(media) << " / 10  "
                  << calificacionDeNota(media) << ui::reset();
    } else {
        std::cout << ui::gris() << "sin notas todavia" << ui::reset();
    }
    std::cout << "\n";

    std::cout << "  Nota acumulada:    " << ui::negrita() << formatearNota(a.notaAcumulada())
              << ui::reset() << ui::gris() << " / 10 puntos del total del curso" << ui::reset()
              << "\n";

    std::cout << "  Progreso:          " << barraProgreso(evaluado, 24) << " "
              << formatearNota(evaluado, 0) << "% de " << formatearNota(total, 0) << "%\n";

    (void)total;
}

void mostrarCarpetas(const Asignatura& a) {
    auto estado = [](const std::string& ruta) {
        return ui::existeRuta(ruta) ? std::string("") : std::string("  (se creara al abrirla)");
    };
    std::string temario = carpetaDeAsignatura(a, true);
    std::string practicas = carpetaDeAsignatura(a, false);
    std::cout << ui::gris() << "  Temario:   " << temario << estado(temario) << "\n"
              << "  Practicas: " << practicas << estado(practicas) << ui::reset() << "\n";
}

// ------------------------------------------------------------------ edicion

Examen pedirDatosExamen(const Examen& base, bool esNuevo) {
    Examen e = base;
    e.nombre = ui::leerLineaConDefecto("  Nombre de la prueba", esNuevo ? "" : base.nombre);
    if (e.nombre.empty()) e.nombre = "Prueba sin nombre";

    Fecha f;
    if (ui::leerFecha("  Fecha", f, !esNuevo)) {
        e.fecha = f;
    } else if (esNuevo) {
        e.fecha = Fecha::hoy();
    }

    e.fecha.hora = -1;  // los examenes se guardan sin hora
    e.ponderacion = ui::leerDecimal("  Ponderacion sobre la nota final (%)", 0, 100,
                                    esNuevo ? 25.0 : base.ponderacion);
    e.apuntes = ui::leerLineaConDefecto("  Que entra / notas", base.apuntes);

    if (ui::confirmar("  Ya tienes la nota?", base.calificado)) {
        e.calificado = true;
        e.nota = ui::leerDecimal("  Nota obtenida (0-10)", 0, 10, base.nota);
    } else {
        e.calificado = false;
    }
    return e;
}

void anadirExamen(Asignatura& a) {
    ui::subtitulo("Nueva prueba en " + a.codigo);
    Examen e = pedirDatosExamen(Examen{}, true);
    a.examenes.push_back(e);
    a.ordenarExamenes();
    guardar();
    ui::exito("Prueba anadida.");
}

int pedirIndiceExamen(const Asignatura& a, const std::string& accion) {
    if (a.examenes.empty()) {
        ui::aviso("No hay pruebas registradas.");
        return -1;
    }
    int n = ui::leerEntero("  Numero de la prueba a " + accion + " (0 = cancelar)", 0,
                           static_cast<int>(a.examenes.size()), 0);
    return n == 0 ? -1 : n - 1;
}

void ponerNota(Asignatura& a) {
    int i = pedirIndiceExamen(a, "calificar");
    if (i < 0) return;
    Examen& e = a.examenes[i];
    std::cout << ui::gris() << "  " << e.nombre << " (" << formatearNota(e.ponderacion, 0)
              << "% de la asignatura)" << ui::reset() << "\n";
    e.nota = ui::leerDecimal("  Nota (0-10)", 0, 10, e.calificado ? e.nota : 5.0);
    e.calificado = true;
    guardar();
    ui::exito("Nota guardada. Nueva media: " + formatearNota(a.notaMediaActual()));
}

void editarExamen(Asignatura& a) {
    int i = pedirIndiceExamen(a, "editar");
    if (i < 0) return;
    ui::subtitulo("Editando: " + a.examenes[i].nombre);
    std::cout << ui::gris() << "  (Enter para mantener el valor actual)\n" << ui::reset();
    a.examenes[i] = pedirDatosExamen(a.examenes[i], false);
    a.ordenarExamenes();
    guardar();
    ui::exito("Prueba actualizada.");
}

void borrarExamen(Asignatura& a) {
    int i = pedirIndiceExamen(a, "borrar");
    if (i < 0) return;
    if (!ui::confirmar("  Seguro que quieres borrar \"" + a.examenes[i].nombre + "\"?", false)) {
        return;
    }
    a.examenes.erase(a.examenes.begin() + i);
    guardar();
    ui::exito("Prueba borrada.");
}

void abrirCarpeta(const std::string& ruta, const std::string& etiqueta) {
    std::string err;
    if (ui::abrirRuta(ruta, err)) {
        ui::exito("Abriendo " + etiqueta + "...");
    } else {
        ui::error(err);
        ui::aviso("Configura la ruta con la opcion [7] Editar datos de la asignatura.");
    }
}

void editarDatosAsignatura(Asignatura& a) {
    ui::subtitulo("Datos de " + a.codigo);
    std::cout << ui::gris() << "  (Enter para mantener el valor actual)\n" << ui::reset();
    a.codigo = ui::leerLineaConDefecto("  Codigo", a.codigo);
    a.nombre = ui::leerLineaConDefecto("  Nombre", a.nombre);
    a.profesor = ui::leerLineaConDefecto("  Profesor", a.profesor);
    a.creditos = ui::leerEntero("  Creditos ECTS", 1, 60, a.creditos);
    a.cuatrimestre = ui::leerLineaConDefecto("  Curso", a.cuatrimestre);
    a.cuatri = ui::leerEntero("  Cuatrimestre (1 o 2)", 1, 2, a.cuatri);
    a.diasClase = normalizarDiasClase(
        ui::leerLineaConDefecto("  Dias de clase (L M X J V)", a.diasClase));
    a.color = ui::leerLineaConDefecto("  Color (#rrggbb)", a.color);
    guardar();
    ui::exito("Datos actualizados.");
}

// ------------------------------------------------------------------- menus

void menuAsignatura(std::size_t indice) {
    while (true) {
        if (indice >= g_curso.asignaturas.size()) return;
        Asignatura& a = g_curso.asignaturas[indice];

        cabeceraApp();
        std::cout << "\n  " << ui::negrita() << ui::cian() << a.codigo << ui::reset() << "  "
                  << ui::negrita() << a.nombre << ui::reset() << "\n";
        std::cout << ui::gris() << "  " << a.cuatrimestre << " | "
                  << (a.cuatri == 2 ? "2o cuatrimestre" : "1er cuatrimestre") << " | "
                  << a.creditos << " ECTS";
        if (!a.diasClase.empty()) std::cout << " | clase: " << diasClaseLegibles(a);
        if (!a.profesor.empty()) std::cout << " | " << a.profesor;
        std::cout << ui::reset() << "\n\n";

        tablaExamenes(a);
        resumenAsignatura(a);
        ui::linea('-', 78);
        mostrarCarpetas(a);

        std::cout << "\n  [1] Anadir prueba      [2] Poner nota       [3] Editar prueba\n"
                  << "  [4] Borrar prueba      [5] Abrir temario    [6] Abrir practicas\n"
                  << "  [7] Editar asignatura  [0] Volver\n";

        std::string op = ui::leerLinea("  > ");
        if (op == "0" || op.empty()) return;
        if (op == "1") anadirExamen(a);
        else if (op == "2") ponerNota(a);
        else if (op == "3") editarExamen(a);
        else if (op == "4") borrarExamen(a);
        else if (op == "5") abrirCarpeta(prepararCarpetaDeAsignatura(a, true), "el temario");
        else if (op == "6") abrirCarpeta(prepararCarpetaDeAsignatura(a, false), "las practicas");
        else if (op == "7") editarDatosAsignatura(a);
        else continue;
        ui::pausa();
    }
}

void crearAsignatura() {
    Asignatura a;
    ui::subtitulo("Nueva asignatura");
    a.codigo = ui::leerLineaConDefecto("  Codigo (ej. PROG2)", "");
    if (a.codigo.empty()) {
        ui::aviso("Cancelado: el codigo es obligatorio.");
        return;
    }
    a.nombre = ui::leerLineaConDefecto("  Nombre completo", a.codigo);
    a.profesor = ui::leerLineaConDefecto("  Profesor", "");
    a.creditos = ui::leerEntero("  Creditos ECTS", 1, 60, 6);
    a.cuatrimestre = ui::leerLineaConDefecto("  Curso", "1 curso");
    a.cuatri = ui::leerEntero("  Cuatrimestre (1 o 2)", 1, 2, 1);
    a.diasClase = normalizarDiasClase(ui::leerLineaConDefecto("  Dias de clase (L M X J V)", ""));
    a.color = colorDePaleta(g_curso.asignaturas.size());
    g_curso.asignaturas.push_back(a);
    guardar();
    ui::exito("Asignatura creada. Anade sus pruebas desde su ficha.");
}

void borrarAsignatura() {
    if (g_curso.asignaturas.empty()) return;
    int n = ui::leerEntero("  Numero de la asignatura a borrar (0 = cancelar)", 0,
                           static_cast<int>(g_curso.asignaturas.size()), 0);
    if (n == 0) return;
    const Asignatura& a = g_curso.asignaturas[n - 1];
    if (!ui::confirmar("  Borrar \"" + a.nombre + "\" y todas sus pruebas?", false)) return;
    g_curso.asignaturas.erase(g_curso.asignaturas.begin() + (n - 1));
    guardar();
    ui::exito("Asignatura borrada.");
}

void menuAsignaturas() {
    while (true) {
        cabeceraApp();
        ui::subtitulo("  Mis asignaturas");
        tablaAsignaturas();
        std::cout << "\n  Escribe el numero de una asignatura para ver su ficha.\n"
                  << "  [n] Nueva asignatura   [b] Borrar asignatura   [0] Volver\n";

        std::string op = ui::leerLinea("  > ");
        if (op.empty() || op == "0") return;
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(op[0])));
        if (c == 'n') {
            crearAsignatura();
            ui::pausa();
            continue;
        }
        if (c == 'b') {
            borrarAsignatura();
            ui::pausa();
            continue;
        }
        try {
            int n = std::stoi(op);
            if (n >= 1 && n <= static_cast<int>(g_curso.asignaturas.size())) {
                menuAsignatura(static_cast<std::size_t>(n - 1));
            }
        } catch (...) {
        }
    }
}

void vistaResumen() {
    cabeceraApp();
    ui::subtitulo("  Resumen del curso");
    tablaAsignaturas();
    calendario::mostrarProximos(g_curso, 8);

    ui::subtitulo("Carga pendiente por asignatura");
    for (const auto& a : g_curso.asignaturas) {
        double pendiente = a.ponderacionTotal() - a.ponderacionEvaluada();
        std::cout << "  " << ui::cian() << ui::encajar(a.codigo, 9) << ui::reset()
                  << barraProgreso(a.ponderacionEvaluada(), 20) << " "
                  << ui::encajar(formatearNota(pendiente, 0) + "% por evaluar", 18);
        if (a.tieneNotas()) {
            std::cout << ui::gris() << "media actual " << formatearNota(a.notaMediaActual())
                      << ui::reset();
        }
        std::cout << "\n";
    }
    ui::pausa();
}

void exportarInforme() {
    const std::string destino =
        rutas::absoluta(rutas::unir(rutas::padre(g_almacen->ruta()), "informe.html"));
    std::string err;
    if (!html::exportar(g_curso, destino, err)) {
        ui::error(err);
        ui::pausa();
        return;
    }
    ui::exito("Informe generado: " + destino);
    if (ui::confirmar("  Abrirlo en el navegador?", true)) {
        std::string err2;
        if (!ui::abrirRuta(destino, err2)) ui::error(err2);
    }
    ui::pausa();
}

void menuPrincipal() {
    while (true) {
        cabeceraApp();
        calendario::mostrarProximos(g_curso, 5);

        double media = g_curso.mediaExpediente();
        std::cout << "\n  " << ui::negrita() << "Media del expediente: " << ui::colorNota(media)
                  << formatearNota(media) << ui::reset() << ui::gris() << "   |   "
                  << g_curso.asignaturas.size() << " asignaturas" << ui::reset() << "\n";

        std::cout << "\n  [1] Calendario de examenes\n"
                  << "  [2] Mis asignaturas (notas, pruebas y carpetas)\n"
                  << "  [3] Resumen del curso\n"
                  << "  [4] Exportar informe HTML\n"
                  << "  [0] Guardar y salir\n";

        std::string op = ui::leerLinea("\n  > ");
        if (op == "0" || op == "q" || op.empty()) {
            guardar();
            std::cout << ui::verde() << "\n  Datos guardados en " << g_almacen->ruta()
                      << ". Hasta luego!\n"
                      << ui::reset();
            return;
        }
        if (op == "1") calendario::vistaInteractiva(g_curso);
        else if (op == "2") menuAsignaturas();
        else if (op == "3") vistaResumen();
        else if (op == "4") exportarInforme();
    }
}

}  // namespace

int main(int argc, char** argv) {
    ui::inicializarConsola();

    // Por defecto, los datos viven junto al ejecutable: asi la version de
    // consola y la grafica comparten siempre el mismo fichero.
    std::string ruta;
    if (argc > 1) {
        ruta = argv[1];
    } else {
        ruta = rutas::unir(rutas::directorioDelEjecutable(), "datos/asignaturas.json");
    }
    Almacen almacen(ruta);
    g_almacen = &almacen;

    if (!almacen.cargar(g_curso)) {
        ui::error(almacen.ultimoError());
        ui::aviso("Se arranca con datos de ejemplo; al guardar se sobrescribira el fichero.");
        g_curso = Almacen::cursoDeEjemplo();
        ui::pausa();
    }

    // Cada asignatura tiene su carpeta con Temario y Practicas dentro.
    prepararCarpetasDelCurso(g_curso);

    menuPrincipal();
    return 0;
}
