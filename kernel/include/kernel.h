#ifndef KERNEL_MAIN_H
#define KERNEL_MAIN_H

#include <cliente.h>

// Logger y Config
t_log* kernel_logger;
t_config* kernel_config;

// Variables de configuración
char* IP_MEMORIA;
int PUERTO_MEMORIA;

int PUERTO_ESCUCHA_DISPATCH;
int PUERTO_ESCUCHA_INTERRUPT;
int PUERTO_ESCUCHA_IO;

char* ALGORITMO_PLANIFICACION;
char* ALGORITMO_COLA_NEW;
float ALFA;
int TIEMPO_SUSPENSION;
char* LOG_LEVEL;

int fd_memoria;
int fd_kernel_dispatch;
int fd_cpu_dispatch;
int fd_cpu_interrupt;
int conexiones = 0;

// Funciones de inicialización
void inicializar_kernel();
void inicializar_logs();
t_config* inicializar_configs();
void imprimir_configs();

void* conectar_a_memoria();

void* atender_cpu_dispatch();

void* iniciar_consola_funciones(void* arg);

bool procesar_syscall(char* syscall);
bool procesar_io_syscall(char* syscall);
bool procesar_init_proc_syscall(char* syscall);
bool procesar_dump_memory_syscall(char* syscall);
bool procesar_exit_syscall(char* syscall);


#endif // KERNEL_MAIN_H
