
#include "cpu.h"
#include <cliente.h>
#include <server.h>

char instruccion_syscall[256];
t_instruccion_pendiente instruccion_pendiente;

void inicializar_cpu(){
    inicializar_logs();
    inicializar_configs();
    imprimir_configs();
    inicializar_sincronizacion();
    inicializar_semaforos();
}

void inicializar_logs(){

    cpu_logger = log_create("/home/utnso/tp-2025-1c-Coderitos/cpu/cpu.log", "CPU_LOG", 1, LOG_LEVEL_INFO);
    if(cpu_logger == NULL) {    
        perror("Algo salio mal con el cpu_log, no se pudo crear o escuchar el archivo");
        exit(EXIT_FAILURE);
    }

    iniciar_logger(cpu_logger);

}

t_config* inicializar_configs(void){

    cpu_config = config_create("/home/utnso/tp-2025-1c-Coderitos/cpu/cpu.config");
   
   if(cpu_config == NULL) {    
       perror("Error al cargar cpu_config");
       exit(EXIT_FAILURE);
   }

    IP_MEMORIA = config_get_string_value(cpu_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_int_value(cpu_config, "PUERTO_MEMORIA");
    IP_KERNEL = config_get_string_value(cpu_config, "IP_KERNEL");
    PUERTO_KERNEL_DISPATCH = config_get_int_value(cpu_config, "PUERTO_KERNEL_DISPATCH");
    PUERTO_KERNEL_INTERRUPT = config_get_int_value(cpu_config, "PUERTO_KERNEL_INTERRUPT");
    ENTRADAS_TLB = config_get_int_value(cpu_config, "ENTRADAS_TLB");
    REEMPLAZO_TLB = config_get_string_value(cpu_config, "REEMPLAZO_TLB");
    ENTRADAS_CACHE = config_get_int_value(cpu_config, "ENTRADAS_CACHE");
    REEMPLAZO_CACHE = config_get_string_value(cpu_config, "REEMPLAZO_CACHE");
    RETARDO_CACHE = config_get_int_value(cpu_config, "RETARDO_CACHE");
    LOG_LEVEL = config_get_string_value(cpu_config, "LOG_LEVEL");

    return cpu_config;
}

void imprimir_configs(){
    log_info(cpu_logger, "IP_MEMORIA: %s", IP_MEMORIA);
    log_info(cpu_logger, "PUERTO_MEMORIA: %d", PUERTO_MEMORIA);
    log_info(cpu_logger, "IP_KERNEL: %s", IP_KERNEL);
    log_info(cpu_logger, "PUERTO_KERNEL_DISPATCH: %d", PUERTO_KERNEL_DISPATCH);
    log_info(cpu_logger, "PUERTO_KERNEL_INTERRUPT: %d", PUERTO_KERNEL_INTERRUPT);
    log_info(cpu_logger, "ENTRADAS_TLB: %d", ENTRADAS_TLB);
    log_info(cpu_logger, "REEMPLAZO_TLB: %s", REEMPLAZO_TLB);
    log_info(cpu_logger, "ENTRADAS_CACHE: %d", ENTRADAS_CACHE);
    log_info(cpu_logger, "REEMPLAZO_CACHE: %s", REEMPLAZO_CACHE);
    log_info(cpu_logger, "RETARDO_CACHE: %d", RETARDO_CACHE);
    log_info(cpu_logger, "LOG_LEVEL: %s", LOG_LEVEL);

}


int main(void) {
    
    inicializar_cpu();

    log_info(cpu_logger, "CPU inicializado y creando hilos...");

    pthread_t hilo_kernel_dispatch;
    pthread_t hilo_cpu_memoria;

    // Conexion kernel dispatch
    log_info(cpu_logger, "Iniciando conexión con Kernel Dispatch...");
    if (pthread_create(&hilo_kernel_dispatch, NULL, conectar_kernel_dispatch, &fd_kernel_dispatch) != 0) {
        log_error(cpu_logger, "Error al crear hilo de conexión con kernel_dispatch");
        exit(EXIT_FAILURE);
    }

    // Esperar a que se establezca la conexión con kernel
    log_info(cpu_logger, "Esperando confirmación de conexión con kernel...");
    sem_wait(&sem_cpu_kernel_hs);
    log_info(cpu_logger, "Handshake con kernel completado");

    // Atender memoria
    if (pthread_create(&hilo_cpu_memoria, NULL, conectar_cpu_memoria, &fd_memoria) != 0) {
        log_error(cpu_logger, "Error al crear hilo con Memoria");
        exit(EXIT_FAILURE);
    } else {
        log_info(cpu_logger, "Creado hilo con Memoria");
    }

    // Esperar a que se establezca la conexión con memoria
    log_info(cpu_logger, "Esperando handshake de memoria...");
    sem_wait(&sem_memoria_cpu_hs);
    log_info(cpu_logger, "Handshake con memoria completado");

    pthread_join(hilo_kernel_dispatch,NULL);
    pthread_join(hilo_cpu_memoria, NULL);

    log_debug(cpu_logger,"Se ha desconectado de cpu");

	printf("\nCPU DESCONECTADO!\n\n");

    return EXIT_SUCCESS;
}

void* conectar_kernel_dispatch(void* arg) {
    int* fd_kernel_ptr = (int*)arg;
    fd_kernel_dispatch = *fd_kernel_ptr;

    log_info(cpu_logger, "Iniciando conexión con Kernel Dispatch en %s:%d", IP_KERNEL, PUERTO_KERNEL_DISPATCH);

    while((fd_kernel_dispatch)<=0) {
        fd_kernel_dispatch = crear_conexion(cpu_logger, IP_KERNEL, PUERTO_KERNEL_DISPATCH);

        log_info(cpu_logger, "fd_kernel_dispatch: %d", fd_kernel_dispatch);

        if((fd_kernel_dispatch)<=0) 
            log_error(cpu_logger, "Intentando conectar a kernel_Dispatch");
    }
    
    log_info(cpu_logger, "Conectado exitosamente al servidor de kernel_Dispatch en %s:%d", IP_KERNEL, PUERTO_KERNEL_DISPATCH);

    // Señalizar que el handshake con kernel está completo
    sem_post(&sem_cpu_kernel_hs);

    log_info(cpu_logger, "Conexión con Kernel Dispatch establecida, listo para syscalls");

   while (1) {
        // Esperar hasta que haya una instrucción syscall pendiente
        log_info(cpu_logger, "Esperando syscall...");
        sem_wait(&sem_instruccion_lista);
        
        // Hay una syscall para procesar
        pthread_mutex_lock(&mutex_instruccion_pendiente);

        char syscall_a_procesar[256];
        strncpy(syscall_a_procesar, instruccion_pendiente.instruccion, sizeof(syscall_a_procesar));
        
        pthread_mutex_unlock(&mutex_instruccion_pendiente);
        
        log_info(cpu_logger, "Procesando syscall: %s", syscall_a_procesar);
        
        // Verificar conexión
        if(fd_kernel_dispatch <= 0) {
            log_error(cpu_logger, "No hay conexión con Kernel Dispatch");
            
            // Marcar como procesada (con error)
            pthread_mutex_lock(&mutex_instruccion_pendiente);
            instruccion_pendiente.procesada = true;
            instruccion_pendiente.es_syscall = false;
            pthread_mutex_unlock(&mutex_instruccion_pendiente);
            
            sem_post(&sem_instruccion_procesada);
            continue;
        }
        
        // Crear y enviar paquete con la syscall
        t_paquete* paquete = crear_paquete(SYSCALL, cpu_logger);
        agregar_a_paquete(paquete, syscall_a_procesar, strlen(syscall_a_procesar) + 1);
        
        log_info(cpu_logger, "Enviando syscall al Kernel: %s", syscall_a_procesar);
        enviar_paquete(paquete, fd_kernel_dispatch);
        eliminar_paquete(paquete);
        
        // Esperar respuesta del kernel
        log_info(cpu_logger, "Esperando confirmación del Kernel para syscall: %s", syscall_a_procesar);
        int cod_op = recibir_operacion(cpu_logger, fd_kernel_dispatch);
        
        // Procesar respuesta
        bool syscall_exitosa = false;
        switch(cod_op) {
            case SYSCALL_OK:
                log_info(cpu_logger, "Syscall procesada exitosamente por el Kernel");
                printf("CPU > Syscall %s completada exitosamente\n", syscall_a_procesar);
                syscall_exitosa = true;
                break;
                
            case SYSCALL_ERROR:
                log_warning(cpu_logger, "Error al procesar syscall en el Kernel");
                printf("CPU > Error al procesar syscall %s\n", syscall_a_procesar);
                break;
                
            case -1:
                log_error(cpu_logger, "Conexión con Kernel perdida durante syscall");
                printf("CPU > Conexión con Kernel perdida\n");
                // Intentar reconectar o manejar error
                break;
                
            default:
                log_warning(cpu_logger, "Respuesta inesperada del Kernel: %d", cod_op);
                printf("CPU > Respuesta inesperada del Kernel: %d\n", cod_op);
                break;
        }
        
        // Marcar syscall como procesada y señalizar
        pthread_mutex_lock(&mutex_instruccion_pendiente);
        instruccion_pendiente.procesada = true;
        instruccion_pendiente.es_syscall = false; // Reset para la próxima
        pthread_mutex_unlock(&mutex_instruccion_pendiente);
        
        sem_post(&sem_instruccion_procesada);
        
        log_info(cpu_logger, "Syscall %s completamente procesada, resultado: %s", 
        syscall_a_procesar, syscall_exitosa ? "EXITOSA" : "ERROR");
    }
    

    close(fd_kernel_dispatch);
    log_info(cpu_logger, "Desconectado de Kernel Dispatch");
    return NULL;
}


void* conectar_cpu_memoria(void* arg) {
    int* fd_memoria_ptr = (int*)arg;
    fd_memoria = *fd_memoria_ptr;

    fd_memoria = crear_conexion(cpu_logger, IP_MEMORIA, PUERTO_MEMORIA);  // Asumimos estas constantes cargadas
    if (fd_memoria == -1) {
        log_error(cpu_logger, "No se pudo conectar a Memoria");
        exit(EXIT_FAILURE);
    }

    log_info(cpu_logger, "Iniciando conexión a Memoria...");

    // Enviar handshake - código de identificación
    int identificador = CPU;
    send(fd_memoria, &identificador, sizeof(int), 0);
    log_info(cpu_logger, "Handshake enviado a Memoria");

    // Esperar confirmación de memoria
    bool confirmacion;
    if(recv(fd_memoria, &confirmacion, sizeof(bool), MSG_WAITALL) <= 0) {
        log_error(cpu_logger, "Error en handshake con Memoria");
        close(fd_memoria);
        return NULL;
    }
    
    if(!confirmacion) {
        log_error(cpu_logger, "Memoria rechazó la conexión");
        close(fd_memoria);
        return NULL;
    }

    log_info(cpu_logger, "Cliente Memoria: %d conectado", fd_memoria);

    // Señalizar que el handshake con memoria está completo
    sem_post(&sem_memoria_cpu_hs);
    
    log_info(cpu_logger, "Iniciando bucle de solicitud de estado y procesamiento...");

    //Esperar un poco antes de recibir el estado
    usleep(100000); // 100ms

    // Recibir respuesta
    int cod_op = recibir_operacion(cpu_logger, fd_memoria);
    if(cod_op == -1) {
        log_error(cpu_logger, "Conexión con Memoria perdida");
        close(fd_memoria);
        return NULL;
    }

    if(cod_op == ARCHIVO_LISTO) {

        log_info(cpu_logger, "Archivo listo en Memoria, comenzando SOLICITUD_INSTRUCCION");

        if (!limpiar_buffer(fd_memoria)) {
            log_warning(cpu_logger, "No se pudo limpiar completamente el buffer, puede haber problemas");
        }

        // Llamar a la función recibir_instruccion para manejar la recepción de instrucciones
        recibir_instruccion(fd_memoria);
    } else {
        log_error(cpu_logger, "Se esperaba ARCHIVO_LISTO pero se recibió: %d", cod_op);
    }
            
    close(fd_memoria);
    log_info(cpu_logger, "Desconectado de Memoria");
    return NULL;
}


//Función para recibir instrucciones
void recibir_instruccion(int fd_memoria) {
    log_info(cpu_logger, "Iniciando recepción de instrucciones desde Memoria");
    bool continuar = true;

    while(continuar) {
        
        // Verificar si hay syscall en proceso
        while(syscall_en_proceso) {
            log_info(cpu_logger, "Syscall en proceso, esperando...");
            usleep(100000); // Esperar 100ms
            continue;
        }

        // Solicitar instrucción
        pthread_mutex_lock(&mutex_comunicacion_memoria);
        
        log_info(cpu_logger, "Enviando SOLICITUD_INSTRUCCION (cod_op: %d)", SOLICITUD_INSTRUCCION);

        sleep(1); // Simular retardo de memoria

        t_paquete* paquete = crear_paquete(SOLICITUD_INSTRUCCION, cpu_logger);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        log_info(cpu_logger, "SOLICITUD_INSTRUCCION enviada");
        
        // Esperar respuesta de Memoria
        int cod_op = recibir_operacion(cpu_logger, fd_memoria);
        log_info(cpu_logger, "Recibido cod_op: %d", cod_op);

        pthread_mutex_unlock(&mutex_comunicacion_memoria);

        if(cod_op == -1) {
            log_error(cpu_logger, "Error en la conexión con Memoria");
            continuar = false;
            return;
        }

        // Verificación adicional del código de operación
        if(cod_op == 0) {   
            log_warning(cpu_logger, "Se recibió cod_op 0, verificando estado de la conexión");
            // Posiblemente leer un byte adicional para ver si hay datos residuales
            char byte_extra;
            if(recv(fd_memoria, &byte_extra, 1, MSG_PEEK) > 0) {
                log_warning(cpu_logger, "Hay datos residuales en el buffer: %02x", byte_extra);
            }
            pthread_mutex_unlock(&mutex_comunicacion_memoria);
            usleep(10000); // Pausa de 10ms antes de reintentar
            continue;
        }

        switch(cod_op) {
            case PAQUETE: {
            log_info(cpu_logger, "Cod_op fue igual a PAQUETE y se prepara a recibir paquete");

            t_list* lista = recibir_paquete_desde_buffer(cpu_logger, fd_memoria);

            int list_size_val = list_size(lista);

            if(lista == NULL || list_size(lista) == 0) {
                log_error(cpu_logger, "Error: lista NULL o vacía recibida de memoria");
                if(lista) list_destroy(lista);
                continue;
            }

            log_info(cpu_logger, "Lista recibida con %d elementos", list_size_val);


            log_info(cpu_logger, "Recibido paquete de instrucciones desde Memoria");

            char* instruccion = list_get(lista, 0);
            
            if(instruccion == NULL || strlen(instruccion) == 0) {
                log_error(cpu_logger, "Instrucción NULL o vacía");
                list_destroy_and_destroy_elements(lista, free);
                pthread_mutex_unlock(&mutex_comunicacion_memoria);
                continue;
            }

            log_info(cpu_logger, "Instrucción recibida: %s", instruccion);

            // Procesar la instrucción
            procesar_instruccion(instruccion);

            list_destroy_and_destroy_elements(lista, free);

            log_info(cpu_logger, "Instrucción procesada, preparándose para solicitar la siguiente...");

            break;
            }
            
            case EXIT:
                log_info(cpu_logger, "Instrucción recibida: EXIT");
                printf("CPU > Fin de ejecución\n");
                pthread_mutex_unlock(&mutex_comunicacion_memoria);
                continuar = false;
                break;

            default:
                log_warning(cpu_logger, "Código de operación no reconocido: %d", cod_op);
                pthread_mutex_unlock(&mutex_comunicacion_memoria);
            break;
        }

        // Pequeña pausa para no saturar el sistema
        usleep(50000); // 50ms
    }
    log_info(cpu_logger, "Finalizada recepción de instrucciones desde Memoria");
}


void procesar_instruccion(char* instruccion) {
    log_info(cpu_logger, "Procesando instrucción: %s", instruccion);

    // Verificar si es una syscall
    if(es_syscall(instruccion)) {
        log_info(cpu_logger, "Detectada syscall, enviando al hilo de kernel dispatch");
        
        // Enviar la syscall al hilo de kernel dispatch
        pthread_mutex_lock(&mutex_instruccion_pendiente);
        
        // Preparar la instrucción para el hilo de kernel
        strncpy(instruccion_pendiente.instruccion, instruccion, sizeof(instruccion_pendiente.instruccion) - 1);
        instruccion_pendiente.instruccion[sizeof(instruccion_pendiente.instruccion) - 1] = '\0';
        instruccion_pendiente.es_syscall = true;
        instruccion_pendiente.procesada = false;
        
        // Señalar que hay una instrucción lista
        sem_post(&sem_instruccion_lista);
        
        pthread_mutex_unlock(&mutex_instruccion_pendiente);
        
        // Esperar a que sea procesada
        sem_wait(&sem_instruccion_procesada);
        
        log_info(cpu_logger, "Syscall %s procesada completamente", instruccion);
    }
    else {
        // Procesar instrucciones normales
        if(strcmp(instruccion, "NOOP") == 0) {
            printf("CPU > Ejecutando NOOP\n");
            sleep(1);
        }
        else if(strncmp(instruccion, "WRITE", 5) == 0) {
            printf("CPU > Ejecutando WRITE\n");
            sleep(1);
        }
        else if(strncmp(instruccion, "READ", 4) == 0) {
            printf("CPU > Ejecutando READ\n");
            sleep(1);
        }
        else if(strncmp(instruccion, "GOTO", 4) == 0) {
            printf("CPU > Ejecutando GOTO\n");
            sleep(1);
        }
        else {
            printf("CPU > Instrucción no reconocida: %s\n", instruccion);
            log_warning(cpu_logger, "Instrucción no reconocida: %s", instruccion);
        }
    }
    
    log_info(cpu_logger, "Instrucción %s procesada completamente", instruccion);
}

bool es_syscall(char* instruccion) {
    // Verificar si la instrucción es una syscall
    return (strncmp(instruccion, "IO", 2) == 0 ||
            strncmp(instruccion, "INIT_PROC", 9) == 0 ||
            strncmp(instruccion, "DUMP_MEMORY", 11) == 0 ||
            strncmp(instruccion, "EXIT", 4) == 0
    );
}
