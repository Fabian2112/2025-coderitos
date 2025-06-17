
#include "memoria.h"
#include <server.h>
#include <cliente.h>

int fd_kernel = -1;
char path_instrucciones[256];
FILE* archivo_instrucciones = NULL;


void inicializar_memoria() {
    inicializar_logs();
    inicializar_configs();
    imprimir_configs();
    inicializar_sincronizacion();
    inicializar_semaforos();
}

void inicializar_logs(){

    memoria_logger = log_create("/home/utnso/tp-2025-1c-Coderitos/memoria/memoria.log", "MEMORIA_LOG", 1, LOG_LEVEL_INFO);
    if (memoria_logger == NULL)
    {
        perror("Algo salio mal con el memoria_log, no se pudo crear o escuchar el archivo");
        exit(EXIT_FAILURE);
    }
    iniciar_logger(memoria_logger);
    logger = memoria_logger; // Asignar el logger global para uso en otras funciones
}

void inicializar_configs(){

    memoria_config = config_create("/home/utnso/tp-2025-1c-Coderitos/memoria/memoria.config");

    if (memoria_config == NULL)
    {
        perror("Error al cargar memoria_config");
        exit(EXIT_FAILURE);
    }

    // Cargar configuraciones
    PUERTO_ESCUCHA = config_get_int_value(memoria_config, "PUERTO_ESCUCHA");
    TAM_MEMORIA = config_get_int_value(memoria_config, "TAM_MEMORIA");
    TAM_PAGINA = config_get_int_value(memoria_config, "TAM_PAGINA");
    ENTRADAS_POR_TABLA = config_get_int_value(memoria_config, "ENTRADAS_POR_TABLA");
    CANTIDAD_NIVELES = config_get_int_value(memoria_config, "CANTIDAD_NIVELES");
    RETARDO_MEMORIA = config_get_int_value(memoria_config, "RETARDO_MEMORIA");
    PATH_SWAPFILE = config_get_string_value(memoria_config, "PATH_SWAPFILE");
    RETARDO_SWAP = config_get_int_value(memoria_config, "RETARDO_SWAP");
    LOG_LEVEL = config_get_string_value(memoria_config, "LOG_LEVEL");
    DUMP_PATH = config_get_string_value(memoria_config, "DUMP_PATH");
}

void imprimir_configs(){
    log_info(memoria_logger, "PUERTO_ESCUCHA: %d", PUERTO_ESCUCHA);
    log_info(memoria_logger, "TAM_MEMORIA: %d", TAM_MEMORIA);
    log_info(memoria_logger, "TAM_PAGINA: %d", TAM_PAGINA);
    log_info(memoria_logger, "ENTRADAS_POR_TABLA: %d", ENTRADAS_POR_TABLA);
    log_info(memoria_logger, "CANTIDAD_NIVELES: %d", CANTIDAD_NIVELES);
    log_info(memoria_logger, "RETARDO_MEMORIA: %d", RETARDO_MEMORIA);
    log_info(memoria_logger, "PATH_SWAPFILE: %s", PATH_SWAPFILE);
    log_info(memoria_logger, "RETARDO_SWAP: %d", RETARDO_SWAP);
    log_info(memoria_logger, "LOG_LEVEL: %s", LOG_LEVEL);
    log_info(memoria_logger, "DUMP_PATH: %s", DUMP_PATH);
}


int main(void) {

    inicializar_memoria();

    log_info(memoria_logger, "Se inicializo memoria.");

    fd_memoria = -1;

    fd_memoria = iniciar_servidor(memoria_logger, PUERTO_ESCUCHA);

    if (fd_memoria == -1){
        log_error(memoria_logger, "ERROR: Memoria no levanta servidor");
        return EXIT_SUCCESS;
    }

    log_info(memoria_logger, "Se inicializo servidor Memoria. Creando hilos...");

    pthread_t hilo_memoria_kernel;
    pthread_t hilo_memoria_cpu;
    
    int fd_cliente;
    for (int i = 0; i < 2; i++)
        {
        log_info(memoria_logger, "Servidor de Memoria iniciado. Esperando Clientes... con fd_memoria: %d", fd_memoria);
        
        fd_cliente = esperar_cliente(memoria_logger, fd_memoria);
        if (fd_cliente < 0) {
            log_error(memoria_logger, "Error al aceptar cliente");
            continue;
        }

        int cod_op = recibir_operacion(memoria_logger, fd_cliente);
        switch (cod_op)
            {
            case KERNEL:
            log_info(memoria_logger, "Se conecto el KERNEL");
            fd_kernel = fd_cliente;
            bool confirmacion = true;
            send(fd_kernel, &confirmacion, sizeof(bool), 0);
            pthread_create(&hilo_memoria_kernel, NULL, atender_memoria_kernel, &fd_kernel);

            log_info(memoria_logger, "Creado hilo de kernel");

            break;

            case CPU:
            log_info(memoria_logger, "Se conecto la CPU");
            fd_cpu = fd_cliente;
            pthread_create(&hilo_memoria_cpu, NULL, atender_memoria_cpu, &fd_cpu);

            log_info(memoria_logger, "Creado hilo de CPU");
            // Esperar a que CPU responda el handshake
            sem_wait(&sem_cpu_memoria_hs);
            log_info(memoria_logger, "Handshake con CPU completado");
            break;

            default:
            log_error(memoria_logger, "No reconozco ese codigo");
            break;
            }
        
        }

    pthread_join(hilo_memoria_kernel,NULL);
    pthread_join(hilo_memoria_cpu, NULL);

    log_debug(memoria_logger,"Se ha desconectado de memoria");

	printf("\nMEMORIA DESCONECTADO!\n\n");

    return EXIT_SUCCESS;
}

void* atender_memoria_kernel(void* arg) {
    int* fd_kernel_ptr = (int*)arg;
    int fd_kernel = *fd_kernel_ptr;

    log_info(memoria_logger, "Cliente kernel %d conectado. Esperando operaciones...", fd_kernel);

    

    bool control_key = true;

    while (control_key) {

        pthread_mutex_lock(&mutex_archivo);

        int cod_op = recibir_operacion(memoria_logger, fd_kernel);

        log_info(memoria_logger, "Recibiendo operacion de kernel con cod_op: %d", cod_op);

        if (cod_op == -1) {
            log_error(memoria_logger, "El cliente se desconectó. Terminando servidor");
            break;
        }

        switch (cod_op) {
            case ENVIO_ARCHIVO_KERNEL:

                log_info(memoria_logger, "Recibido cod_op ENVIO_ARCHIVO_KERNEL de kernel");

                t_list* lista = recibir_paquete_desde_buffer(memoria_logger, fd_kernel);
                
                if (lista == NULL) {
                    log_error(memoria_logger, "Error: recibir_paquete devolvió NULL");
                    bool confirmacion = false;
                    send(fd_kernel, &confirmacion, sizeof(bool), 0);
                    break;
                }

                log_info(memoria_logger, "Paquete recibido de kernel con %d elementos", list_size(lista));

                if (list_size(lista) >= 2) {
                    log_info(memoria_logger, "Me llegaron los siguientes valores:");

                    char* nombre_archivo = list_get(lista, 0);
                    char* tamanio_str = list_get(lista, 1);
                    int tamanio_reservar = atoi(tamanio_str);
                
                    if (nombre_archivo == NULL || tamanio_str == NULL) {
                        log_error(memoria_logger, "Error: elementos del paquete son NULL");

                        bool confirmacion = false;
                        send(fd_kernel, &confirmacion, sizeof(bool), 0);
                        pthread_mutex_unlock(&mutex_archivo);
                        list_destroy_and_destroy_elements(lista, free);
                        break;
                    }
                
                    log_info(memoria_logger, "Nombre archivo: %s, Tamaño a reservar: %d", nombre_archivo, tamanio_reservar);

                    char path_completo[256];
                    snprintf(path_completo, sizeof(path_completo), "/home/utnso/tp-2025-1c-Coderitos/utils/pruebas/%s", nombre_archivo);

                    log_info(memoria_logger, "Path completo: %s", path_completo);
                    log_info(memoria_logger, "Guardando path en variable global");

                    strncpy(path_instrucciones, path_completo, sizeof(path_instrucciones));

                    log_info(memoria_logger, "Abriendo archivo de instrucciones");

                    if (access(path_completo, F_OK) == -1) {
                        log_error(memoria_logger, "El archivo no existe en la ruta: %s", path_completo);
                        
                        bool confirmacion = false;
                        send(fd_kernel, &confirmacion, sizeof(bool), 0);
                        pthread_mutex_unlock(&mutex_archivo);
                        list_destroy_and_destroy_elements(lista, free);
                        break;
                    }
                    
                    if (access(path_completo, R_OK) == -1) {
                        log_error(memoria_logger, "No hay permisos para leer el archivo: %s", path_completo);

                        bool confirmacion = false;
                        send(fd_kernel, &confirmacion, sizeof(bool), 0);
                        pthread_mutex_unlock(&mutex_archivo);

                        list_destroy_and_destroy_elements(lista, free);
                        break;
                    }

                    archivo_instrucciones = fopen(path_completo, "r");

                    if (archivo_instrucciones == NULL) {
                        log_error(memoria_logger, "No se pudo abrir el archivo de instrucciones: %s - Error: %s", path_completo, strerror(errno));

                        bool confirmacion = false;
                        send(fd_kernel, &confirmacion, sizeof(bool), 0);
                        pthread_mutex_unlock(&mutex_archivo);
                        list_destroy_and_destroy_elements(lista, free);
                        break;
                    }
                    
                    log_info(memoria_logger, "Archivo abierto exitosamente");
                    mostrar_contenido_archivo(archivo_instrucciones, memoria_logger);
                
                    bool confirmacion = true;
                    log_info(memoria_logger, "Enviando confirmación de carga de archivo a kernel");
                    send(fd_kernel, &confirmacion, sizeof(bool), 0);

                    pthread_mutex_unlock(&mutex_archivo);
                    
                    log_info(memoria_logger, "Ruta completa al archivo: %s", path_completo);

                    sem_post(&sem_archivo_listo);

                } else {
                    log_warning(memoria_logger, "Paquete incompleto recibido de kernel");
                    bool confirmacion = false;
                    send(fd_kernel, &confirmacion, sizeof(bool), 0);
                    pthread_mutex_unlock(&mutex_archivo);
                    list_destroy_and_destroy_elements(lista, free);
                }
                
                // Mostrar contenido del paquete
                log_info(memoria_logger, "Contenido del paquete:");
                for (int i = 0; i < list_size(lista); i++) {
                    char* item = list_get(lista, i);
                    if (item != NULL) {
                        log_info(memoria_logger, "Item %d: %s", i, item);
                    } else {
                        log_warning(memoria_logger, "Item %d es NULL", i);
                    }
                }
                
            list_destroy_and_destroy_elements(lista, free);
            break;
            
            default:
                log_warning(memoria_logger,"Operacion desconocida desde Kernel");
            break;
        }
        sleep(1); // Simular retardo de memoria

    }

    log_info(memoria_logger, "Cerrando conexión con kernel");

    close(fd_kernel);
    return NULL;
}


void* atender_memoria_cpu(void* arg) {
    int* fd_cpu_ptr = (int*)arg;
    int fd_cpu = *fd_cpu_ptr;
    char* linea_actual = NULL;
    const int BUFFER_SIZE = 512;
    bool continuar = true;
    int contador = 1;

    log_info(memoria_logger, "Cliente CPU %d conectado. Esperando operaciones...", fd_cpu);
    
    // Enviar confirmación de conexión
    bool confirmacion = true;
    send(fd_cpu, &confirmacion, sizeof(bool), 0);
    log_info(memoria_logger, "Enviado handshake a CPU");

    sem_post(&sem_cpu_memoria_hs);

    log_info(memoria_logger, "Esperando archivo de Instrucciones de Kernel...");

    sem_wait(&sem_archivo_listo);

    // Asegurarse de que el archivo esté en la posición correcta
    if (archivo_instrucciones != NULL) {
        pthread_mutex_lock(&mutex_archivo_rewind);
        rewind(archivo_instrucciones);  // Reinicia el puntero del archivo
        pthread_mutex_unlock(&mutex_archivo_rewind);
    }
    
    log_info(memoria_logger, "Enviando a CPU el Estado: ARCHIVO_LISTO");

    t_paquete* paquete_listo = crear_paquete(ARCHIVO_LISTO, memoria_logger);
    enviar_paquete(paquete_listo, fd_cpu);
    eliminar_paquete(paquete_listo);
    
    log_info(memoria_logger, "Enviado a CPU el Estado: ARCHIVO_LISTO, Esperando SOLICITUD_INSTRUCCION de CPU...");

    sleep(1); // Simular retardo de memoria
    
    while(continuar){

        // Simular retardo de memoria
        log_info(memoria_logger, "contador de Solicitud de Instrucción: %d", contador);

        // Esperar nueva solicitud de CPU
        int cod_op = recibir_operacion(memoria_logger, fd_cpu);
        log_info(memoria_logger, "Recibiendo operacion de CPU con cod_op: %d", cod_op);

        if (cod_op == -1) {
            log_error(memoria_logger, "CPU se desconectó");
            continuar = false;
            break;
        }

        switch(cod_op) {
            case SOLICITUD_INSTRUCCION:
            log_info(memoria_logger, "Recibida SOLICITUD_INSTRUCCION de CPU");

            pthread_mutex_lock(&mutex_archivo_instrucciones);

            linea_actual = entregar_linea(archivo_instrucciones, memoria_logger, BUFFER_SIZE);

            pthread_mutex_unlock(&mutex_archivo_instrucciones);
      
            if (linea_actual == NULL) {
                log_info(memoria_logger, "Fin del archivo de instrucciones, enviando EXIT a CPU");
                
                pthread_mutex_lock(&mutex_envio_cpu);
                // Enviar señal EXIT
                t_paquete* paquete = crear_paquete(EXIT, memoria_logger);
                enviar_paquete(paquete, fd_cpu);
                eliminar_paquete(paquete);
                pthread_mutex_unlock(&mutex_envio_cpu);

                // Cerrar archivo al finalizar
                if (archivo_instrucciones != NULL) {
                    pthread_mutex_lock(&mutex_archivo_cierre);
                    fclose(archivo_instrucciones);
                    archivo_instrucciones = NULL;
                    pthread_mutex_unlock(&mutex_archivo_cierre);
                }
                continuar = false; // Salir del bucle
                break;
            } else{

            pthread_mutex_lock(&mutex_envio_cpu);

            log_info(memoria_logger, "Enviando LINEA de Instruccion: %s", linea_actual);

            sleep(1); // Simular retardo de memoria

            t_paquete* paquete_instr = crear_paquete(PAQUETE, memoria_logger);
            agregar_a_paquete(paquete_instr, linea_actual,strlen(linea_actual)+1);

            log_info(memoria_logger, "Paquete a enviar - cod_op: %d, size: %d", paquete_instr->codigo_operacion, paquete_instr->buffer->size);

            enviar_paquete(paquete_instr, fd_cpu);
            eliminar_paquete(paquete_instr);

            log_info(memoria_logger, "Linea de Instruccion Enviada, en espera de proxima Solicitud");
            
            pthread_mutex_unlock(&mutex_envio_cpu);

            // Liberar memoria de la línea actual
            free(linea_actual);
            linea_actual = NULL;
            }

            break;
                
        default:
            log_warning(memoria_logger, "Operación no reconocida de CPU: %d", cod_op);
            break;
        }

        sleep(1); // Simular retardo de memoria
        contador++;
    }
    
    if (linea_actual != NULL) {
        free(linea_actual);
        linea_actual = NULL;
    }

    if (archivo_instrucciones != NULL) {
        pthread_mutex_lock(&mutex_archivo_cierre);
        fclose(archivo_instrucciones);
        archivo_instrucciones = NULL;
        pthread_mutex_unlock(&mutex_archivo_cierre);
    }

    log_info(memoria_logger, "Cerrando conexión con CPU");
    close(fd_cpu);
    return NULL;
}


void mostrar_contenido_archivo(FILE* archivo, t_log* logger) {
    char linea[512]; // Buffer para leer líneas

    log_info(logger, "Contenido del archivo:");

    rewind(archivo); // Por si ya se leyó algo

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        // Elimina salto de línea si está presente
        linea[strcspn(linea, "\n")] = 0;
        log_info(logger, "Linea: %s", linea);
    }

    rewind(archivo); // Volver al inicio para uso posterior
}

char* entregar_linea(FILE* archivo, t_log* logger, int buffer_size) {
        
    if (archivo == NULL) {
        log_error(logger, "Error: archivo nulo en entregar_linea");
        return NULL;
    }

    if (buffer_size <= 0) {
        log_error(logger, "Error: buffer_size inválido: %d", buffer_size);
        return NULL;
    }
    
    char* linea = malloc(buffer_size);
    if (linea == NULL) {
        log_error(logger, "Error: no se pudo asignar memoria para la línea");
        return NULL;
    }

    log_info(logger, "Leyendo línea del archivo...");

    // Leer la siguiente línea
    if (fgets(linea, buffer_size, archivo) != NULL) {
        // Eliminar salto de línea si está presente
        linea[strcspn(linea, "\n")] = '\0';

        char* inicio = linea;
        while (*inicio == ' ' || *inicio == '\t') inicio++;
        
        if (*inicio != '\0') {
            // Mover el contenido al inicio del buffer si es necesario
            if (inicio != linea) {
                memmove(linea, inicio, strlen(inicio) + 1);
            }
            
            int len = strlen(linea);
            while (len > 0 && (linea[len-1] == ' ' || linea[len-1] == '\t')) {
                linea[--len] = '\0';
            }
            
            log_info(logger, "Línea leída: '%s' (longitud: %d)", linea, len);
            return linea;
        }
    }
     
    free(linea);
    
    // End of file o error
    if (feof(archivo)) {
        log_info(logger, "Se alcanzó el fin del archivo");
    } else {
        log_error(logger, "Error al leer el archivo: %s", strerror(errno));
    }
    return NULL;
}