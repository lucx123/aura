# Recuperación manual de AURA: EC2 y notebook de respaldo

Actualizado: 2026-10-03. Estado: diseño pendiente de implementación y pruebas.

Este documento reemplaza la propuesta anterior de failover automático.
El usuario confirmó que el notebook no estará encendido 24/7.

## Funcionamiento normal

EC2 es el único nodo activo. Ejecuta AURA Gateway, el motor personal y los
workers laborales autorizados mediante Docker Compose. El notebook puede
conservar el código y las herramientas de restauración, pero permanece apagado;
no monitorea EC2 ni sincroniza estado continuamente.

Los respaldos se generan desde EC2 y se guardan fuera de esa instancia. Su
frecuencia, retención y destino se definirán al implementar. No requieren que
el notebook esté encendido.

## Qué respaldar

- Git privado: código, Compose, instrucciones, agentes, skills y memoria curada
  permitida, con accesos separados para personal y trabajo.
- Backup cifrado y consistente: bases de datos, sesiones necesarias, tareas,
  artefactos y estado de coordinación si se incorpora Paperclip.
- Secretos: provisionarlos mediante un canal separado; no almacenarlos en Git.

Git por sí solo no conserva todo el estado operativo. No copiar bases de datos
activas sin su procedimiento de backup. El respaldo debe poder recuperarse
sin depender de que EC2 esté disponible.

## Cuando EC2 cae

1. AURA queda desconectada hasta que el usuario active el respaldo.
2. El usuario enciende el notebook y confirma la caída o detiene el despliegue
   original para evitar dos instancias ejecutando las mismas tareas/canales.
3. Descarga el último backup válido y obtiene la versión compatible del código
   y de las imágenes de contenedor.
4. Restaura volúmenes/datos y configura credenciales y conectividad.
5. Arranca Docker Compose y comprueba estado, permisos y acceso a los proveedores.
6. Cambia la conexión de la app/reloj al notebook mediante configuración de
   servidor o un nombre estable cuya ruta se pueda modificar. El mecanismo
   concreto y el acceso remoto al notebook quedan pendientes de implementar.
7. Revisa tareas interrumpidas y reanuda solo las que puedan ejecutarse sin
   duplicar efectos. La app conserva pendientes válidos y descarta los vencidos.

El despliegue en EC2 debe permanecer detenido o deshabilitado durante esta
recuperación. Si EC2 vuelve por sí sola, no se realiza retorno automático.

## Volver a EC2

1. Pausar la entrada de nuevas tareas y reconciliar las que estén en curso.
2. Crear un backup consistente del estado actualizado en el notebook.
3. Detener el despliegue del notebook y restaurar ese estado en EC2.
4. Arrancar y comprobar EC2; devolver a ella la conexión de los dispositivos.
5. Apagar el notebook después de verificar que el servicio se recuperó.

Hay un solo despliegue activo en cada momento. Se conserva el estado actualizado
por transferencia manual, sin sincronización continua entre hosts.

## Límites y validación

La pérdida potencial de datos corresponde al intervalo desde el último backup
completado. El tiempo de recuperación incluye encender el notebook, restaurar,
configurar la conexión y verificar el servicio. No se promete disponibilidad
ininterrumpida ni un tiempo de recuperación hasta medir el procedimiento.

Prueba mínima: restaurar un respaldo en el notebook, consultar una sesión,
comprobar la separación personal/laboral, recuperar una tarea pendiente,
verificar que no se duplica una acción y ensayar el retorno a EC2.

Todavía no se configuraron backups, Docker, rutas de conexión ni automatizaciones.
El costo del almacenamiento y las transferencias se incluirá en el presupuesto;
no se presupone costo adicional cero.
