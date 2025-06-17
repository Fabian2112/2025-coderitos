
#include "kernel.h"
#include <cliente.h>
#include <server.h>
#include "protocolo.h"

void inicializar_kernel(){
    inicializar_logs();
    inicializar_configs();
    imprimir_configs();
    inicializar_sincronizacion();
    inicializar_semaforos();
}

void inicializar_logs(){

    kernel_logger = log_create("/home/utnso/tp-2025-1c-Coderitos/kernel/kernel.log", "KERNEL_LOG", 1, LOG_LEVEL_INFO);
    if (kernel_logger == NULL)
    {
        perror("Algo salio mal con el memoria_log, no se pudo crear o escuchar el archivo");
        exit(EXIT_FAILURE);
    }
    iniciar_logger(kernel_logger);
}

t_config* inicializar_configs(void){

    kernel_config = config_create("/home/utnso/tp-2025-1c-Coderitos/kernel/kernel.config");

    if (kernel_config == NULL)
    {
        perror("Error al cargar kernel_config");
        exit(EXIT_FAILURE);
    }

    // Cargar configuraciones
    
    IP_MEMORIA = config_get_string_value(kernel_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_int_value(kernel_config, "PUERTO_MEMORIA");
    
    PUERTO_ESCUCHA_DISPATCH = config_get_int_value(kernel_config, "PUERTO_ESCUCHA_DISPATCH");
    PUERTO_ESCUCHA_INTERRUPT = config_get_int_value(kernel_config, "PUERTO_ESCUCHA_INTERRUPT");
    PUERTO_ESCUCHA_IO = config_get_int_value(kernel_config, "PUERTO_ESCUCHA_IO");
    
    ALGORITMO_PLANIFICACION = config_get_string_value(kernel_config, "ALGORITMO_PLANIFICACION");
    ALGORITMO_COLA_NEW = config_get_string_value(kernel_config, "ALGORITMO_COLA_NEW");
    
    ALFA = atof(config_get_string_value(kernel_config, "ALFA"));
    
    TIEMPO_SUSPENSION = config_get_int_value(kernel_config, "TIEMPO_SUSPENSION");
    
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");

    return kernel_config;
}

void imprimir_configs(){
    log_info(kernel_logger, "IP_MEMORIA: %s", IP_MEMORIA);
    log_info(kernel_logger, "PUERTO_MEMORIA: %d", PUERTO_MEMORIA);
    log_info(kernel_logger, "PUERTO_ESCUCHA_DISPATCH: %d", PUERTO_ESCUCHA_DISPATCH);
    log_info(kernel_logger, "PUERTO_ESCUCHA_INTERRUPT: %d", PUERTO_ESCUCHA_INTERRUPT);
    log_info(kernel_logger, "PUERTO_ESCUCHA_IO: %d", PUERTO_ESCUCHA_IO);
    log_info(kernel_logger, "ALGORITMO_PLANIFICACION: %s", ALGORITMO_PLANIFICACION);
    log_info(kernel_logger, "ALGORITMO_COLA_NEW: %s", ALGORITMO_COLA_NEW);
    log_info(kernel_logger, "ALFA: %f", ALFA);
    log_info(kernel_logger, "TIEMPO_SUSPENSION: %d", TIEMPO_SUSPENSION);
    log_info(kernel_logger, "LOG_LEVEL: %s", LOG_LEVEL);
}


int main(int argc, char* argv[]) {

    // Validar argumentos
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <nombre_archivo> <tamaño_archivo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Obtener valores desde argv
    char* nombre_archivo = argv[1];
    int tamanio_archivo = atoi(argv[2]);  // convertir a entero
    
    if (nombre_archivo == NULL || strlen(nombre_archivo) == 0) {
        fprintf(stderr, "Nombre de archivo inválido: %s\n", nombre_archivo);
        exit(EXIT_FAILURE);
    }
    if (tamanio_archivo <= 0) {
        fprintf(stderr, "Tamaño de archivo inválido: %d\n", tamanio_archivo);
        exit(EXIT_FAILURE);
    }

    t_datos_kernel datos_kernel;
    
    datos_kernel.nombre_archivo = nombre_archivo;
    datos_kernel.tamanio_archivo = tamanio_archivo;

    inicializar_kernel();

    log_info(kernel_logger, "Se inicializo kernel.");
    
    pthread_t hilo_cpu_dispatch;
    pthread_t hilo_kernel_memoria;

    // Atender CPU
    log_info(kernel_logger, "Iniciando servidor CPU Dispatch...");

    if (pthread_create(&hilo_cpu_dispatch, NULL, atender_cpu_dispatch, &fd_cpu_dispatch) != 0) {
        log_error(kernel_logger, "Error al crear hilo de CPU Dispatch");
        exit(EXIT_FAILURE);
    } else {
        log_info(kernel_logger, "Creado hilo de CPU Dispatch");
    }

    // Esperar a que el servidor esté listo antes de continuar
    log_info(kernel_logger, "Esperando que el servidor CPU Dispatch inicie...");
    sem_wait(&sem_servidor_cpu_listo);
    log_info(kernel_logger, "Servidor CPU Dispatch iniciado correctamente");

    // Conexión a memoria
    if (pthread_create(&hilo_kernel_memoria, NULL, conectar_a_memoria, (void*)&datos_kernel) != 0) {
        log_error(kernel_logger, "Error al crear hilo de conexión con memoria");
        exit(EXIT_FAILURE);
    }

    log_debug(kernel_logger,"fd_memoria = %d", fd_memoria);

    // Esperar a que memoria responda el handshake
    log_info(kernel_logger, "Esperando handshake de memoria...");
    sem_wait(&sem_kernel_memoria_hs);
    log_info(kernel_logger, "Handshake con memoria completado");

    // Esperar a que CPU responda el handshake
    log_info(kernel_logger, "Esperando handshake de CPU...");
    sem_wait(&sem_kernel_cpu_hs);
    log_info(kernel_logger, "Handshake con CPU completado");

    // Señalizar que todos los módulos están conectados
    sem_post(&sem_modulos_conectados);
    log_info(kernel_logger, "Todos los módulos conectados, procediendo...");


    pthread_join(hilo_kernel_memoria, NULL);
    pthread_join(hilo_cpu_dispatch, NULL);

    
    log_debug(kernel_logger,"Se ha desconectado de kernel");

	printf("\nKERNEL DESCONECTADO!\n\n");

    return EXIT_SUCCESS;
}


void* conectar_a_memoria(void* args){

    t_datos_kernel* datos = (t_datos_kernel*)args;

    log_info(kernel_logger, "Iniciando conexión a memoria...");

    fd_memoria = crear_conexion(kernel_logger, IP_MEMORIA, PUERTO_MEMORIA);

    if (fd_memoria <= 0) {
        log_error(kernel_logger, "Error al conectar con memoria");
        return NULL;
    }

    // Identificarse como KERNEL
    int identificador = KERNEL;
    send(fd_memoria, &identificador, sizeof(int), 0);
    log_info(kernel_logger, "Cliente Memoria: %d conectado", fd_memoria);
    
    // Esperar confirmación de memoria
    bool confirmacion;
    ssize_t bytes_recibidos = recv(fd_memoria, &confirmacion, sizeof(bool), MSG_WAITALL);
    
    if (bytes_recibidos <= 0) {
        log_error(kernel_logger, "Error al recibir confirmación de memoria");
        close(fd_memoria);
        return NULL;
    }

    if (!confirmacion) {
        log_error(kernel_logger, "Memoria rechazó la conexión");
        close(fd_memoria);
        return NULL;
    }
    
    if(confirmacion) {
        log_info(kernel_logger, "Conectado exitosamente a Memoria");
        // Enviar confirmación de conexión
        sem_post(&sem_kernel_memoria_hs);
    } else {
        log_error(kernel_logger, "Error al conectar con Memoria");
        close(fd_memoria);
        return NULL;
    }

    // Esperar a que los 3 módulos estén conectados
    log_info(kernel_logger, "Esperando que todos los módulos se conecten...");
    sem_wait(&sem_modulos_conectados);
    
    // Ahora podemos enviar el archivo
    log_info(kernel_logger, "Todos los módulos conectados, enviando archivo...");
    sleep (1); // Esperar un segundo para asegurar que todo esté listo

    t_paquete* paquete = crear_paquete(ENVIO_ARCHIVO_KERNEL, kernel_logger);
    agregar_a_paquete(paquete, datos->nombre_archivo, strlen(datos->nombre_archivo) + 1);
    //Convertir el int a string antes de enviarlo
    char tamanio_str[32];
    snprintf(tamanio_str, sizeof(tamanio_str), "%d", datos->tamanio_archivo);
    agregar_a_paquete(paquete, tamanio_str, strlen(tamanio_str) + 1);

    log_info(kernel_logger, "Paquete creado:");
    log_info(kernel_logger, "- Código de operación: %d", paquete->codigo_operacion);
    log_info(kernel_logger, "- Tamaño del buffer: %d", paquete->buffer->size);
    log_info(kernel_logger, "- Nombre archivo: '%s' (longitud: %zu)", datos->nombre_archivo, strlen(datos->nombre_archivo));
    log_info(kernel_logger, "- Tamaño archivo string: '%s' (longitud: %zu)", tamanio_str, strlen(tamanio_str));

    enviar_paquete(paquete, fd_memoria);

    log_info(kernel_logger, "Paquete con archivo pseudocodigo enviado con exito a Memoria");

    bool confirmacion_archivo;

    log_info(kernel_logger, "Esperando confirmación de carga de archivo...");
    if (recv(fd_memoria, &confirmacion_archivo, sizeof(bool), MSG_WAITALL) > 0) {
        printf(confirmacion_archivo ? "Éxito\n" : "Error\n");

        if (confirmacion_archivo) {
            // Si se cargó el archivo exitosamente, notificar
            sem_post(&sem_archivo_listo);
            log_info(kernel_logger, "Archivo cargado exitosamente, notificando a los módulos");
        }

    }
    eliminar_paquete(paquete);

    sem_wait(&sem_syscall_procesada);
    log_info(kernel_logger, "Comenzando a recibir instrucciones de Kernel...");

    while (1){
        char* linea_actual = readline("Kernel > En espera o ingrese 'exit' para salir): \n");
        if (linea_actual == NULL) {
            log_error(kernel_logger, "Error al leer la línea de entrada");
            continue;
        }
        if (strcmp(linea_actual, "exit") == 0) {
            log_info(kernel_logger, "Cerrando conexión con Memoria");
            free(linea_actual); // liberar memoria
            break;
        }
        free(linea_actual); // liberar en cada iteración
    }
    
    // Cast seguro a void*
    return (void*)(intptr_t)fd_memoria;
}


void* atender_cpu_dispatch() {
    fd_kernel_dispatch = -1;

    log_info(kernel_logger, "Iniciando conexión a CPU_dispatch...");

    fd_kernel_dispatch = iniciar_servidor(kernel_logger, PUERTO_ESCUCHA_DISPATCH);
    if (fd_kernel_dispatch == -1){
        log_error(kernel_logger, "ERROR: Kernel no levanta servidor");
        return NULL;
    }
    
    log_info(kernel_logger, "Servidor de Kernel Dispatch iniciado. Esperando Clientes... con fd_kernel_dispatch: %d", fd_kernel_dispatch);

    // Señalizar que el servidor está listo
    sem_post(&sem_servidor_cpu_listo);

    int fd_cpu_dispatch = esperar_cliente(kernel_logger, fd_kernel_dispatch);
    if (fd_kernel_dispatch < 0) {
        log_error(kernel_logger, "Error al aceptar cliente de cpu_dispatch");
        return NULL;
    }

    log_info(kernel_logger, "Cliente fd_cpu: %d conectado", fd_cpu_dispatch);

    // Señalizar que se recibió el handshake de CPU
    sem_post(&sem_kernel_cpu_hs);

    log_info(kernel_logger, "Esperando operaciones de CPU...");

    // Esperar a que el archivo esté listo antes de procesar syscalls
    sem_wait(&sem_archivo_listo);

    bool cpu_conectado = true;
    // Bucle principal de atención a syscalls
    while (cpu_conectado) {
        log_info(kernel_logger, "Esperando syscall de CPU...");

        int cod_op = recibir_operacion(kernel_logger, fd_cpu_dispatch);

        switch(cod_op) {
            case -1:
                log_error(kernel_logger, "CPU se desconectó");
                cpu_conectado = false;
                break;

            case SYSCALL: {
                log_info(kernel_logger, "Recibida syscall de CPU");
                
                pthread_mutex_lock(&mutex_syscall_kernel);

                t_list* lista = recibir_paquete_desde_buffer(kernel_logger, fd_cpu_dispatch);

                bool resultado = false;
                
                if (lista != NULL && list_size(lista) > 0) {
                    char* syscall = list_get(lista, 0);
                    
                    if(syscall != NULL && strlen(syscall) > 0) {
                        log_info(kernel_logger, "Procesando syscall: %s", syscall);
                        
                        // Procesar la syscall
                        resultado = procesar_syscall(syscall);
                        
                        log_info(kernel_logger, "Syscall %s procesada: %s", 
                                syscall, resultado ? "EXITOSA" : "ERROR");
                    } else {
                        log_warning(kernel_logger, "Syscall NULL o vacía recibida");
                    }
                    
                    list_destroy_and_destroy_elements(lista, free);
                } else {
                    log_warning(kernel_logger, "Lista de syscall NULL o vacía");
                    if (lista) list_destroy(lista);
                }

                // Enviar respuesta INMEDIATAMENTE
                t_paquete* paquete_respuesta;
                if (resultado) {
                    paquete_respuesta = crear_paquete(SYSCALL_OK, kernel_logger);
                    log_info(kernel_logger, "Enviando SYSCALL_OK al CPU");
                } else {
                    paquete_respuesta = crear_paquete(SYSCALL_ERROR, kernel_logger);
                    log_info(kernel_logger, "Enviando SYSCALL_ERROR al CPU");
                }
                
                enviar_paquete(paquete_respuesta, fd_cpu_dispatch);
                
                eliminar_paquete(paquete_respuesta);
                pthread_mutex_unlock(&mutex_syscall_kernel);
                
                log_info(kernel_logger, "Respuesta enviada al CPU, listo para próxima syscall");
                break;
            }
            default:
                log_warning(kernel_logger, "Operación no reconocida de CPU: %d", cod_op);
                break;
        }
        sem_post(&sem_syscall_procesada);
    }

    // Limpieza final
    close(fd_cpu_dispatch);
    close(fd_kernel_dispatch);
    log_info(kernel_logger, "Servidor Dispatch finalizado");

    return NULL;
}


bool procesar_syscall(char* syscall) {
    if (syscall == NULL) {
        log_error(kernel_logger, "Syscall es NULL");
        return false;
    }
    
    printf("\n=== SYSCALL RECIBIDA ===\n");
    printf("Kernel > Ejecutando syscall: %s\n", syscall);
    printf("=======================\n");
    
    // Registrar en log
    log_info(kernel_logger, "Ejecutando syscall: %s", syscall);

    bool resultado = false;

    // Simular procesamiento
    usleep(10000); // 10ms de procesamiento simulado
    
    // Procesar diferentes tipos de syscalls
  if (strncmp(syscall, "IO", 2) == 0) {
        resultado = procesar_io_syscall(syscall);
    }
    else if (strncmp(syscall, "INIT_PROC", 9) == 0) {
        resultado = procesar_init_proc_syscall(syscall);
    }
    else if (strncmp(syscall, "DUMP_MEMORY", 11) == 0) {
        resultado = procesar_dump_memory_syscall(syscall);
    }
    else if (strncmp(syscall, "EXIT", 4) == 0) {
        resultado = procesar_exit_syscall(syscall);
    }
    else {
        log_warning(kernel_logger, "Syscall no reconocida: %s", syscall);
        printf("Kernel > Syscall no reconocida: %s\n", syscall);
        resultado = false;
    }
    
    printf("Kernel > Syscall %s: %s\n", syscall, resultado ? "COMPLETADA" : "ERROR");
    printf("=======================\n\n");
    
    log_info(kernel_logger, "Syscall %s procesada con resultado: %s", 
             syscall, resultado ? "EXITOSA" : "ERROR");
    
    return resultado;
}



bool procesar_io_syscall(char* syscall) {
    printf("Kernel > Procesando operación de E/S: %s\n", syscall);
    log_info(kernel_logger, "Procesando I/O syscall: %s", syscall);
    
    // Simular tiempo de E/S
    usleep(50000); // 50ms
    
    printf("Kernel > I/O completada: %s\n", syscall);
    
    return true;
}

bool procesar_init_proc_syscall(char* syscall) {
    printf("Kernel > Inicializando proceso: %s\n", syscall);
    log_info(kernel_logger, "Procesando INIT_PROC syscall: %s", syscall);

    // Simular inicialización
    usleep(20000); // 20ms
    
    printf("Kernel > Proceso inicializado: %s\n", syscall);
    
    return true;
}

bool procesar_dump_memory_syscall(char* syscall) {
    printf("Kernel > Volcando memoria: %s\n", syscall);
    log_info(kernel_logger, "Procesando DUMP_MEMORY syscall: %s", syscall);
    
    // Simular volcado
    usleep(30000); // 30ms
    
    printf("Kernel > Memoria volcada: %s\n", syscall);
    return true;

}

bool procesar_exit_syscall(char* syscall) {
    printf("Kernel > Volcando memoria: %s\n", syscall);
    log_info(kernel_logger, "Procesando EXIT syscall: %s", syscall);
    
    // Simular limpieza
    usleep(10000); // 10ms
    
    printf("Kernel > EXIT procesado: %s\n", syscall);
    
    return true;
}