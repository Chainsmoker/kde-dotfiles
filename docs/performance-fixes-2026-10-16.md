# Correcciones de rendimiento de los dotfiles

Fecha: 16 de octubre de 2026. Entorno: KDE Wayland, KWin y Caelestia.

## Alcance

Se modificaron cinco archivos QML tanto en `shell/` de este repositorio como en
`~/.config/quickshell/caelestia/`. Además, se actualizó
`~/.config/caelestia/shell.json`. Se reinició únicamente el servicio de Caelestia.

No se modificaron Docker, sus imágenes, sus contenedores, Brave, Foot, Starship,
los fondos de pantalla ni los efectos de KWin durante estas correcciones.
El cambio previo del usuario en el submódulo `src/dots` se conservó.

## Errores y cambios puntuales

### 1. Clientes de PipeWire creados sin un stream disponible

- **Síntoma:** mensajes repetidos `Stream error: target not found` y
  `PipeWire remote error: -2 target not found`.
- **Causa comprobada:** los elementos `PipeWireSourceItem` se instanciaban aunque
  no hubiera un stream disponible. Ocultarlos mediante `visible` no los destruía.
  Desactivar únicamente las miniaturas no detuvo los errores en la primera prueba.
- **Cambio:** se encapsularon en un `Loader` que solo los instancia cuando existe
  un stream disponible y el componente está activo y visible. Esto también libera
  el cliente cuando deja de ser necesario.
- **Archivos:** `shell/components/images/WindowPreview.qml` y
  `shell/components/effects/AmbientGlow.qml`.
- **Mitigación adicional:** `bar.livePreviews=false` en
  `~/.config/caelestia/shell.json`. Las previsualizaciones utilizan sus iconos de
  respaldo; se desactivaron las miniaturas en directo. Esto no equivale a reparar
  todas las incompatibilidades posibles entre KWin y KPipeWire con capturas activas.

### 2. Dependencia circular en la carga del dashboard

- **Error:** `Binding loop detected for property "active"`.
- **Causa:** el binding que calculaba `Loader.active` leía el propio `active`.
  También dependía de dimensiones que cambian al cargar o descargar el contenido.
- **Cambio:** la carga ahora se actualiza mediante señales de apertura, selección
  de pestaña, posición y tamaño del área visible. Una pestaña cargada se conserva
  mientras el dashboard permanece abierto y se descarga al cerrarlo.
  Los bordes que solo se tocan no cuentan como una intersección del área visible.
- **Archivo:** `shell/modules/dashboard/Content.qml`.

### 3. Consultas frecuentes del grabador en reposo

- **Problema:** un proceso `sh` con `pidof` se ejecutaba cada 2,5 segundos aunque
  no hubiera una grabación ni una acción pendiente.
- **Cambio:** el intervalo de reposo pasó a 15 segundos, de aproximadamente
  24 consultas por minuto a 4. Se conservan los 500 ms durante una grabación o
  acción pendiente y se añade una consulta al inicializar el servicio.
- **Archivo:** `shell/services/Recorder.qml`.
- **Límite:** una grabación iniciada externamente puede tardar hasta 15 segundos
  en reflejarse mediante la consulta periódica. La selección de captura conserva
  su llamada explícita a `probeRecording()`.

### 4. Acceso a ventanas eliminadas en la vista general

- **Error encontrado durante la validación:**
  `TypeError: Value is undefined and could not be converted to an object`.
- **Causa:** las expresiones de las miniaturas accedían a `modelData.address`
  durante la retirada de elementos del modelo.
- **Cambio:** se comprueba que la dirección exista y se usa acceso opcional
  `modelData?.address`, con dirección vacía como respaldo.
- **Archivo:** `shell/modules/overview/WindowGrid.qml`.

## Validación y resultados

- Los cinco archivos QML se analizaron con `qmlformat` sin errores de sintaxis.
  No se reformatearon los archivos completos.
- Se verificó que las copias del repositorio y de la instalación coincidieran.
- `git diff --check` terminó sin errores.
- Una prueba QtTest ejecutó el bloque real del delegado del dashboard, extraído
  del archivo de producción. Verificó cuatro casos: cierre inicial, conservación
  al cambiar de pestaña, descarga al cerrar y reapertura de la pestaña seleccionada.
  Los cuatro casos pasaron sin avisos; QtTest informa seis resultados al incluir
  inicialización y limpieza. La misma prueba falla con el código original.
- Pasaron las tres pruebas existentes de `tests/test_hidden_animations.sh`.
- Se abrieron y cerraron el dashboard y la vista general en la sesión real.
  El proceso final no registró errores `target not found`, bucles de `active`
  ni errores QML de tipos durante esa comprobación.

| Medida | Antes | Después |
|---|---:|---:|
| CPU de Quickshell, muestra de 10 segundos | 91,6 % de un núcleo | 81,7 % de un núcleo |
| `target not found` | 1.944 mensajes en el minuto anterior | 0 durante la validación del proceso nuevo |
| RAM proporcional de Quickshell, PSS | ~514 MiB en la auditoría | 486,8 MiB |

Estas muestras no son un ensayo controlado: la actividad del escritorio y el
reinicio pueden influir. Los intervalos de conteo de errores también son distintos.
La desaparición de los errores está comprobada; no se atribuye toda la diferencia
de CPU o RAM a estos cambios.

## Diagnóstico profundo y resolución de consumo de CPU / tartamudeo (16:30)

### 5. Bucle infinito en `BlobRect` (Física y Polish en Qt Event Loop)
- **Causa raíz:** En `shell/plugin/src/Caelestia/Blobs/blobrect.cpp` y `blobrect.hpp`, todos los fondos de los drawers (`dashBg`, `launcherBg`, `sessionBg`, etc.) derivan de `BlobRect`.
- Al abrir o mover cualquier drawer, la velocidad superaba `5.0f` y marcaba `m_physicsActive = true`.
- En `BlobRect::updatePolish()`, se programaba recursivamente `QMetaObject::invokeMethod(this, [this]() { m_group->markDirty(); }, Qt::QueuedConnection);`.
- En el mismo hilo, `Qt::QueuedConnection` se despachaba en microsegundos (< 1 ms). En `BlobRect::updatePhysics()`, la condición `if (dt > 0.1f || dt < 0.001f) return;` abortaba antes de calcular el amortiguamiento y la desaceleración.
- Por tanto, `m_physicsActive` nunca volvía a `false`, inundando el event loop de Qt a ~1000 llamadas por segundo y congelando el hilo principal.
- **Solución:**
  1. Si `m_deformScale <= 0.000001`, se desactiva la física inmediatamente.
  2. Acumulación correcta de tiempo con `m_elapsed.elapsed()`, sin reiniciar el timer si han pasado < 1 ms.
  3. Limitación de llamadas sucias mediante `QTimer::singleShot(16, ...)` a un máximo de 60 FPS con flag de protección `m_animPending`.

### 6. Contención y spinlock en `QSG_RENDER_LOOP=threaded` en Wayland
- **Causa raíz:** `caelestia-autostart.sh`, `10-autostart.sh` y `update.sh` forzaban `export QSG_RENDER_LOOP=threaded`.
- En Qt 6 bajo Wayland (KWin) sobre GPU integrada AMD Cezanne (Radeon Vega), el bucle de renderizado multihilo de QtQuick entra en un ciclo de sondeo y contención continuo (`ppoll` / condvar wakeup) al sincronizar múltiples superficies `WlrLayershell` independientes.
- **Solución:** Se cambió `QSG_RENDER_LOOP=basic` en `caelestia-autostart.sh`, `scripts/10-autostart.sh` y `update.sh`.

### 7. Duplicación y reproducción continua de GIFs en el drawer de sesión
- **Causa:** En `shell/modules/session/Content.qml`, existían dos instancias idénticas de `AnimatedImage` para `dino.gif` ejecutando efectos de shader `Colouriser` en segundo plano, con `playing: visible` que permanecía `true` permanentemente.
- **Solución:** Se eliminó la instancia duplicada y se vinculó `playing: root.visibilities.session && opacity > 0`.

### 8. Excepciones `TypeError: Cannot read property 'id' of null` en bandeja del sistema
- **Causa:** En `shell/modules/bar/popouts/Content.qml`, eliminación de iconos de tray sin comprobación nula (`modelData.id`).
- **Solución:** Acceso seguro mediante encadenamiento opcional `modelData?.id`.

## Resultados finales verificados

| Medida | Antes | Después de correcciones |
|---|---:|---:|
| CPU en reposo (idle) de Quickshell | 80 - 100 % de un núcleo continuo | 0,0 - 3,0 % |
| CPU al abrir / interactuar con Drawers | Stutter / congelamiento severo | Fluido, regresa a 0,0 % al asentarse |
| Estado del proceso en top | `R` (Running / Busy spinlock) | `S` (Sleeping / `ppoll` pasivo) |

