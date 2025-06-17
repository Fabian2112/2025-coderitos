#ifndef CPU_MAIN_H
#define CPU_MAIN_H

#include <cliente.h>

// Logger y Config
t_log* cpu_logger;
t_config* cpu_config;


char* IP_MEMORIA;
int PUERTO_MEMORIA;

char* IP_KERNEL;
int PUERTO_KERNEL_DISPATCH;
int PUERTO_KERNEL_INTERRUPT;

int ENTRADAS_TLB;
char* REEMPLAZO_TLB;

int ENTRADAS_CACHE;
char* REEMPLAZO_CACHE;

int RETARDO_CACHE;

char* LOG_LEVEL;

int fd_memoria;
int fd_kernel_dispatch;
int fd_kernel_interrupt;
int fd_cpu_interrupt;


void inicializar_kernel();
void inicializar_logs();
t_config* inicializar_configs(void);
void imprimir_configs();

void* conectar_kernel_dispatch(void* arg);

void* conectar_cpu_memoria(void* arg);
void recibir_instruccion(int fd_memoria);
void procesar_instruccion(char* instruccion);

bool es_syscall(char* instruccion);
void ejecutar_syscall(char* instruccion);


#endif // CPU_MAIN_H