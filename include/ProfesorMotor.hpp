// ProfesorMotor.hpp - Proceso Python del profesor (profesor/servidor.py).
//
// Se arranca una sola vez por ventana y se habla con el por tuberias: una
// peticion JSON por linea en stdin y una respuesta o evento por linea en
// stdout. Un hilo lector entrega cada linea a la ventana con PostMessage
// (lParam = std::string* que la ventana debe borrar).
#pragma once

#include "Gui.hpp"
#include "Json.hpp"

#include <string>

namespace profesor {

class Motor {
public:
    Motor() = default;
    ~Motor();
    Motor(const Motor&) = delete;
    Motor& operator=(const Motor&) = delete;

    // Lanza el proceso. `mensaje` se envia a `ventana` por cada linea recibida.
    bool arrancar(HWND ventana, UINT mensaje, std::wstring& error);
    void detener();
    bool vivo() const;

    // Asigna un id a la peticion, la envia y devuelve ese id (0 si fallo).
    int enviar(js::Valor peticion);

    // Python que se usara (vacio si no hay ninguno).
    static std::wstring buscarPython();

private:
    static DWORD WINAPI hiloLector(LPVOID datos);

    HANDLE proceso_ = NULL;
    HANDLE escritura_ = NULL;  // stdin del proceso
    HANDLE lectura_ = NULL;    // stdout del proceso
    HANDLE hilo_ = NULL;
    HWND ventana_ = NULL;
    UINT mensaje_ = 0;
    int siguienteId_ = 1;
};

}  // namespace profesor
