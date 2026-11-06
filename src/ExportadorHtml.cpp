#include "ExportadorHtml.hpp"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "Rutas.hpp"

namespace html {
namespace {

std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string colorDe(const Asignatura& a, std::size_t indice) {
    unsigned long rgb = 0;
    if (colorDesdeTexto(a.color, rgb)) return a.color;
    return colorDePaleta(indice);
}

// Clase CSS segun la escala: suspenso / aprobado / bien / notable / sobresaliente
const char* claseNota(double nota) {
    switch (nivelDeNota(nota)) {
        case NIVEL_SUSPENSO: return "n-suspenso";
        case NIVEL_APROBADO: return "n-aprobado";
        case NIVEL_BIEN: return "n-bien";
        case NIVEL_NOTABLE: return "n-notable";
        default: return "n-sobre";
    }
}

// Ruta local -> URL file:// para poder abrir la carpeta desde el navegador.
std::string aUrlArchivo(const std::string& ruta) {
    std::string s = ruta;
    for (auto& c : s) {
        if (c == '\\') c = '/';
    }
    std::string out = "file:///";
    for (unsigned char c : s) {
        if (c == ' ') out += "%20";
        else if (c == '#') out += "%23";
        else if (c == '?') out += "%3F";
        else out += static_cast<char>(c);
    }
    return out;
}

void escribirEstilos(std::ostream& o) {
    o << R"(<style>
:root{--bg:#0f1218;--card:#171c26;--card2:#1e2532;--txt:#e6e9ef;--dim:#8b93a7;
--acc:#5b9dff;--bien:#3ecf8e;--medio:#f2c14e;--mal:#ff6b6b;--bord:#262d3b;}
*{box-sizing:border-box;}
body{margin:0;padding:32px 20px 64px;background:var(--bg);color:var(--txt);
font:15px/1.55 "Segoe UI",system-ui,-apple-system,sans-serif;}
.wrap{max-width:1080px;margin:0 auto;}
h1{font-size:28px;margin:0 0 4px;} h2{font-size:19px;margin:36px 0 14px;
border-bottom:1px solid var(--bord);padding-bottom:8px;}
.sub{color:var(--dim);font-size:13px;margin-bottom:28px;}
.kpis{display:flex;gap:14px;flex-wrap:wrap;margin-bottom:8px;}
.kpi{background:var(--card);border:1px solid var(--bord);border-radius:12px;
padding:14px 18px;min-width:150px;flex:1;}
.kpi .v{font-size:26px;font-weight:600;} .kpi .l{color:var(--dim);font-size:12px;
text-transform:uppercase;letter-spacing:.06em;}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:16px;}
.card{background:var(--card);border:1px solid var(--bord);border-radius:12px;padding:16px 18px;}
.cab{display:flex;justify-content:space-between;align-items:flex-start;gap:10px;margin-bottom:6px;}
.cod{font-size:12px;color:var(--acc);font-weight:600;letter-spacing:.05em;}
.nom{font-size:17px;font-weight:600;}
.meta{color:var(--dim);font-size:12px;margin-bottom:12px;}
.media{font-size:30px;font-weight:700;line-height:1;}
.n-suspenso{color:#ff5c5c;} .n-aprobado{color:#ff9f43;} .n-bien{color:#f7d154;}
.n-notable{color:#3ecf8e;} .n-sobre{color:#78aaff;}
.nivel{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.08em;
margin-top:2px;} .nivel.pend{color:var(--dim);}
.barra{height:7px;background:var(--card2);border-radius:6px;overflow:hidden;margin:10px 0 4px;}
.barra span{display:block;height:100%;background:var(--acc);}
.pie{color:var(--dim);font-size:12px;margin-bottom:10px;}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;color:var(--dim);font-weight:500;font-size:11px;
text-transform:uppercase;letter-spacing:.05em;padding:6px 6px 6px 0;border-bottom:1px solid var(--bord);}
td{padding:7px 6px 7px 0;border-bottom:1px solid var(--bord);}
tr:last-child td{border-bottom:none;}
.pend{color:var(--dim);font-style:italic;}
.links a{display:inline-block;margin:10px 8px 0 0;padding:6px 12px;background:var(--card2);
border:1px solid var(--bord);border-radius:8px;color:var(--acc);text-decoration:none;font-size:12px;}
.links a:hover{border-color:var(--acc);}
.cal{background:var(--card);border:1px solid var(--bord);border-radius:12px;padding:14px 16px;}
.cal h3{margin:0 0 10px;font-size:15px;text-transform:capitalize;}
.cal table{table-layout:fixed;} .cal td,.cal th{text-align:center;padding:5px 0;border:none;}
.cal td.ex{background:rgba(255,107,107,.16);color:var(--mal);border-radius:6px;font-weight:600;}
.cal td.hoy{outline:2px solid var(--acc);border-radius:6px;font-weight:600;}
.cal td.vacio{color:transparent;}
.prox li{margin-bottom:6px;} .prox .d{color:var(--acc);font-weight:600;}
ul{padding-left:18px;}
</style>)";
}

struct DiaMarcado {
    std::vector<std::string> textos;
    std::string color;
};

void escribirCalendarioMes(std::ostream& o, int anio, int mes,
                           const std::map<int, DiaMarcado>& diasConExamen, const Fecha& hoy) {
    Fecha primero;
    primero.anio = anio;
    primero.mes = mes;
    primero.dia = 1;
    int offset = primero.diaSemana();
    int total = Fecha::diasEnMes(anio, mes);

    o << "<div class=\"cal\"><h3>" << Fecha::nombreMes(mes) << " " << anio << "</h3><table><tr>";
    const char* dias[] = {"L", "M", "X", "J", "V", "S", "D"};
    for (const char* d : dias) o << "<th>" << d << "</th>";
    o << "</tr><tr>";

    for (int i = 0; i < offset; ++i) o << "<td class=\"vacio\">.</td>";
    for (int dia = 1; dia <= total; ++dia) {
        std::string clase;
        auto it = diasConExamen.find(dia);
        if (it != diasConExamen.end()) clase = "ex";
        if (hoy.anio == anio && hoy.mes == mes && hoy.dia == dia) {
            clase += clase.empty() ? "hoy" : " hoy";
        }
        o << "<td";
        if (!clase.empty()) o << " class=\"" << clase << "\"";
        if (it != diasConExamen.end()) {
            std::string tip;
            for (const auto& t : it->second.textos) {
                if (!tip.empty()) tip += " | ";
                tip += t;
            }
            o << " title=\"" << esc(tip) << "\"";
            o << " style=\"background:" << esc(it->second.color)
              << "33;color:" << esc(it->second.color) << ";border-radius:6px;font-weight:600\"";
        }
        o << ">" << dia << "</td>";
        if ((offset + dia) % 7 == 0 && dia != total) o << "</tr><tr>";
    }
    int resto = (offset + total) % 7;
    if (resto != 0) {
        for (int i = resto; i < 7; ++i) o << "<td class=\"vacio\">.</td>";
    }
    o << "</tr></table></div>";
}

}  // namespace

bool exportar(const Curso& curso, const std::string& ruta, std::string& error) {
    error.clear();
    const std::string carpeta = rutas::padre(ruta);
    if (!carpeta.empty()) rutas::crearDirectorios(carpeta);

    std::ofstream o(ruta.c_str(), std::ios::binary | std::ios::trunc);
    if (!o) {
        error = "No se pudo crear " + ruta;
        return false;
    }

    const Fecha hoy = Fecha::hoy();
    const auto examenes = todosLosExamenes(curso);

    o << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"utf-8\">\n"
      << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
      << "<title>" << esc(curso.titulo) << " - Informe</title>\n";
    escribirEstilos(o);
    o << "\n</head>\n<body>\n<div class=\"wrap\">\n";

    o << "<h1>" << esc(curso.titulo) << "</h1>\n"
      << "<div class=\"sub\">Informe generado el " << hoy.aTextoLargo() << "</div>\n";

    // --- KPIs
    int pendientes = 0;
    for (const auto& ea : examenes) {
        if (hoy.diasHasta(ea.examen->fecha) >= 0) ++pendientes;
    }
    o << "<div class=\"kpis\">"
      << "<div class=\"kpi\"><div class=\"l\">Media del expediente</div><div class=\"v "
      << claseNota(curso.mediaExpediente()) << "\">" << formatearNota(curso.mediaExpediente())
      << "</div></div>"
      << "<div class=\"kpi\"><div class=\"l\">Asignaturas</div><div class=\"v\">"
      << curso.asignaturas.size() << "</div></div>"
      << "<div class=\"kpi\"><div class=\"l\">Pruebas registradas</div><div class=\"v\">"
      << examenes.size() << "</div></div>"
      << "<div class=\"kpi\"><div class=\"l\">Pendientes</div><div class=\"v\">" << pendientes
      << "</div></div></div>\n";

    // --- Proximos examenes
    o << "<h2>Próximas pruebas</h2>\n";
    auto proximos = proximosExamenes(curso, 10);
    if (proximos.empty()) {
        o << "<p class=\"pend\">No hay pruebas pendientes.</p>\n";
    } else {
        o << "<ul class=\"prox\">\n";
        for (const auto& ea : proximos) {
            long d = hoy.diasHasta(ea.examen->fecha);
            std::string cuenta = (d == 0) ? "hoy" : (d == 1 ? "mañana" : "en " + std::to_string(d) + " días");
            o << "<li><span class=\"d\">" << cuenta << "</span> &middot; "
              << esc(ea.examen->fecha.aTextoLargo()) << " &middot; <b>"
              << esc(ea.asignatura->codigo) << "</b> " << esc(ea.examen->nombre) << " ("
              << formatearNota(ea.examen->ponderacion, 0) << " %)";
            o << "</li>\n";
        }
        o << "</ul>\n";
    }

    // --- Calendarios de los meses con examenes
    std::set<std::pair<int, int>> meses;
    std::map<std::pair<int, int>, std::map<int, DiaMarcado>> porMes;
    for (const auto& ea : examenes) {
        const Fecha& f = ea.examen->fecha;
        auto clave = std::make_pair(f.anio, f.mes);
        meses.insert(clave);

        std::size_t indice = 0;
        for (std::size_t i = 0; i < curso.asignaturas.size(); ++i) {
            if (&curso.asignaturas[i] == ea.asignatura) indice = i;
        }
        DiaMarcado& dia = porMes[clave][f.dia];
        dia.textos.push_back(ea.asignatura->codigo + ": " + ea.examen->nombre);
        if (dia.color.empty()) dia.color = colorDe(*ea.asignatura, indice);
    }
    meses.insert(std::make_pair(hoy.anio, hoy.mes));

    o << "<h2>Calendario</h2>\n<div class=\"grid\">\n";
    for (const auto& m : meses) {
        escribirCalendarioMes(o, m.first, m.second, porMes[m], hoy);
    }
    o << "</div>\n";

    // --- Asignaturas
    o << "<h2>Asignaturas</h2>\n<div class=\"grid\">\n";
    for (std::size_t indice = 0; indice < curso.asignaturas.size(); ++indice) {
        const Asignatura& a = curso.asignaturas[indice];
        double media = a.notaMediaActual();
        double evaluado = a.ponderacionEvaluada();
        double totalPond = a.ponderacionTotal();
        std::string color = colorDe(a, indice);

        o << "<div class=\"card\" style=\"border-left:4px solid " << esc(color) << "\">\n"
          << "<div class=\"cab\"><div><div class=\"cod\" style=\"color:" << esc(color) << "\">"
          << esc(a.codigo) << "</div>"
          << "<div class=\"nom\">" << esc(a.nombre) << "</div></div>"
          << "<div style=\"text-align:right\"><div class=\"media " << claseNota(media) << "\">"
          << (a.tieneNotas() ? formatearNota(media) : std::string("--")) << "</div>"
          << (a.tieneNotas() ? "<div class=\"nivel " + std::string(claseNota(media)) + "\">" +
                                   esc(calificacionDeNota(media)) + "</div>"
                             : std::string("<div class=\"nivel pend\">sin notas</div>"))
          << "</div></div>\n"
          << "<div class=\"meta\">" << esc(a.cuatrimestre) << " &middot; "
          << (a.cuatri == 2 ? "2.º cuatrimestre" : "1.er cuatrimestre") << " &middot; " << a.creditos
          << " créditos";
        if (!a.diasClase.empty()) o << " &middot; clase: " << esc(diasClaseLegibles(a));
        if (!a.profesor.empty()) o << " &middot; " << esc(a.profesor);
        o << "</div>\n";

        o << "<div class=\"barra\"><span style=\"width:" << formatearNota(evaluado, 1)
          << "%\"></span></div>\n"
          << "<div class=\"pie\">Evaluado el " << formatearNota(evaluado, 0) << " % de "
          << formatearNota(totalPond, 0) << " % &middot; nota acumulada "
          << formatearNota(a.notaAcumulada()) << "/10";
        o << "</div>\n";

        o << "<table><tr><th>Prueba</th><th>Fecha</th><th>Peso</th><th>Nota</th>"
             "<th>Calificación</th></tr>\n";
        for (const auto& e : a.examenes) {
            o << "<tr><td>" << esc(e.nombre) << "</td><td>" << esc(e.fecha.aTextoCorto())
              << "</td><td>" << formatearNota(e.ponderacion, 0) << " %</td><td>";
            if (e.calificado) {
                o << "<b class=\"" << claseNota(e.nota) << "\">" << formatearNota(e.nota, 2)
                  << "</b></td><td class=\"" << claseNota(e.nota) << "\">"
                  << esc(calificacionDeNota(e.nota)) << "</td>";
            } else {
                o << "-</td><td><span class=\"pend\">pendiente</span></td>";
            }
            o << "</tr>\n";
        }
        o << "</table>\n";

        o << "<div class=\"links\">"
          << "<a href=\"" << esc(aUrlArchivo(carpetaDeAsignatura(a, true))) << "\">Temario</a>"
          << "<a href=\"" << esc(aUrlArchivo(carpetaDeAsignatura(a, false))) << "\">Prácticas</a>"
          << "</div>\n";
        o << "</div>\n";
    }
    o << "</div>\n";

    o << "</div>\n</body>\n</html>\n";
    if (!o.good()) {
        error = "Error escribiendo el informe";
        return false;
    }
    return true;
}

}  // namespace html
