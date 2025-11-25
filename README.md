📡 Anexos – Tesis de Redes de Sensores Inalámbricos Tolerantes a Fallos

Este repositorio contiene los anexos desarrollados para la tesis “Modelado y validación de topologías de comunicación para redes de sensores en escenarios de monitoreo remoto”. Incluye código fuente, scripts de análisis y resultados experimentales empleados durante el desarrollo del proyecto.

📁 Contenido del repositorio
1. Broadcast - NodesBidirectionalCommunication/
Implementación de comunicación ESP-NOW con envío y recepción bidireccional entre nodos. Incluye manejo de mensajes, ACKs y pruebas de estabilidad en escenarios de corto alcance.

2. LoRaImplementation/
Implementación principal del sistema de comunicación **LoRa SX1262** en modo **multi-hop**, incluyendo:
- Reenvío de paquetes entre nodos intermedios (multi-salto)
- Manejo de ACKs para confirmación de entrega
- Lógica de enrutamiento basada en vecinos y TTL
- Utilidades generales para configuración y depuración del enlace LoRa

3. Single-hop - LoRaImplementation/
Implementación de **broadcast en modo single-hop** utilizando LoRa SX1262.  
Esta versión se empleó para validar el comportamiento del enlace cuando todos los nodos reciben directamente desde un solo transmisor, sin reenvío intermedio.

4. PruebasLatencia/ 
Incluye los datos obtenidos durante la medición de latencia entre nodos en las pruebas de ESP-NOW, así como los scripts empleados para el procesamiento y visualización de dichos resultados. 

5. PerdidaNodosDirigidosKosaraju.m/ 
Script que permite evaluar el efecto de la pérdida de nodos sobre la conectividad de la red mediante el algortimo de Kosaraju. La eliminación de nodos para las pruebas debe realizarse de forma manual modificando la matriz de incidencia dentro del script. 

6. PerdidaNodosDirigidosKosarajuAutoV2.m/ 
Script que permite evaluar el efecto de la pérdida de nodos sobre la conectividad de la red mediante el algortimo de Kosaraju. La eliminación de nodos se realiza desde la pestaña de comandos, indicando el nodo o los nodos que se desean eliminar. El script actualiza automáticamente la matriz de incidencia y genera las gráficas del grafo antes y después de la eliminación 

7. PerdidaNodosDirigidosTodosLosCasosV2.m/ 
Script que permite evaluar el efecto de la pérdida de nodos sobre la conectividad de la red mediante el algortimo de Kosaraju. La eliminación de nodos se realiza automáticamente para todos los casos posibles de desconexión, calculados según la cantidad total de nodos del grafo. El script actualiza la matriz de incidencia en cada caso y almacena todos los resultados archivos Excel y CSV. Además, muestra un resumen de los resultados en la pestaña de comandos para facilitar su análisis. 

8. PerdidaNodosNoDirigidos.m/ 
El script  toma como entrada una matriz de adyacencia no dirigida y permite especificar los nodos a eliminar. A partir de la matriz resultante, calcula

📈 Resultados experimentales
● resultados_SC_incendencia.xlsx
Resultados del análisis del **grafo dirigido** para todos los casos de pérdida de nodos, indicando si la red resultante es fuertemente conexa o no en cada configuración.

● resultados_lambda2_grafo_nodirigido_preliminar.xlsx
Resultados del análisis para el **grafo no dirigido preliminar**, evaluando todos los casos de pérdida de nodos. Para cada configuración se tiene el **valor de Fiedler (λ₂)** y se indica si el grafo permanece conexo o no.

● resultados_lambda2_grafo_nodirigido_triangulado.xlsx
Resultados del análisis para el **grafo no dirigido triangulado**, considerando igualmente todos los casos posibles. Para cada configuración se tiene el **valor de Fiedler (λ₂)** y se indica si el grafo permanece conexo o no.


📝 README.md

Archivo descriptivo inicial del repositorio.


📌 Autora

Angela Marina Quezada Orozco
Universidad del Valle de Guatemala
Facultad de Ingeniería - Departamento de Ingeniería Electrónica, Mecatrónica, Biomédica
