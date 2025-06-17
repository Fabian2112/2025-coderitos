#ifndef MEMORIA_MAIN_H
#define MEMORIA_MAIN_H

#include "conexion.h"

t_config* memoria_config;
t_log* memoria_logger;

// Variables de configuración
int PUERTO_ESCUCHA;
int TAM_MEMORIA;
int TAM_PAGINA;
int ENTRADAS_POR_TABLA;
int CANTIDAD_NIVELES;
int RETARDO_MEMORIA;
char* PATH_SWAPFILE;
int RETARDO_SWAP;
char* LOG_LEVEL;
char* DUMP_PATH;

int fd_memoria;
int fd_cpu;
int fd_kernel;

// Funciones de inicialización
void inicializar_memoria();
void inicializar_logs();
void inicializar_configs();
void imprimir_configs();

void* atender_memoria_kernel(void* arg);
void* atender_memoria_cpu(void* arg);

void mostrar_contenido_archivo(FILE* archivo, t_log* logger);
char* entregar_linea(FILE* archivo, t_log* logger, int buffer_size);

void simular_retardo_swap();
void procesar_envio_archivo_kernel(int fd_kernel);
void procesar_crear_proceso(int fd_kernel);
t_proceso* crear_proceso(int pid);


#endif // MEMORIA_MAIN_H
