# Crea un acceso directo en el Escritorio que abre el Gestor de Asignaturas.
# Lo invoca crear_acceso_directo.bat, que le pasa la carpeta del proyecto.
param([string]$Carpeta)

try {
    if ([string]::IsNullOrWhiteSpace($Carpeta)) {
        $Carpeta = Split-Path -Parent $MyInvocation.MyCommand.Path
    }
    $Carpeta = (Resolve-Path -LiteralPath $Carpeta).Path.TrimEnd('\')

    # 1) si ya esta compilada la version grafica, el acceso directo apunta al .exe
    #    (asi no aparece ninguna ventana negra al abrirlo)
    $exe = Join-Path $Carpeta 'gestor_gui.exe'
    $bat = Join-Path $Carpeta 'Gestor de Asignaturas.bat'

    if (Test-Path -LiteralPath $exe) {
        $objetivo = $exe
        $icono = $exe
    } elseif (Test-Path -LiteralPath $bat) {
        $objetivo = $bat
        $icono = Join-Path $Carpeta 'gestor.ico'
        if (-not (Test-Path -LiteralPath $icono)) {
            $icono = "$env:SystemRoot\System32\shell32.dll,43"
        }
    } else {
        Write-Host ""
        Write-Host "ERROR: no encuentro ni gestor_gui.exe ni el lanzador en:" -ForegroundColor Red
        Write-Host "  $Carpeta"
        Write-Host "Ejecuta antes compilar.bat"
        exit 1
    }

    $escritorio = [Environment]::GetFolderPath('Desktop')
    $destino = Join-Path $escritorio 'Gestor de Asignaturas.lnk'

    $shell = New-Object -ComObject WScript.Shell
    $acceso = $shell.CreateShortcut($destino)
    $acceso.TargetPath       = $objetivo
    $acceso.WorkingDirectory = $Carpeta
    $acceso.IconLocation     = $icono
    $acceso.Description      = 'Gestor de asignaturas de la universidad'
    $acceso.Save()

    if (Test-Path -LiteralPath $destino) {
        Write-Host ""
        Write-Host "Acceso directo creado correctamente:" -ForegroundColor Green
        Write-Host "  $destino"
        Write-Host "  apunta a: $objetivo"
        Write-Host ""
        Write-Host "Busca el icono 'Gestor de Asignaturas' en tu Escritorio."
        exit 0
    }

    Write-Host "ERROR: el acceso directo no se llego a crear." -ForegroundColor Red
    exit 1
}
catch {
    Write-Host ""
    Write-Host "ERROR al crear el acceso directo:" -ForegroundColor Red
    Write-Host $_.Exception.Message
    exit 1
}
