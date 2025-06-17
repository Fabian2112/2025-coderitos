#ifndef CONEXION_H_
#define CONEXION_H_

#include <stdio.h>          // printf, fprintf, perror
#include <stdlib.h>         // malloc, free
#include <fcntl.h>        // fcntl, O_NONBLOCK

#include <commons/log.h>  // Necesario para t_log*
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/temporal.h>
#include <commons/collections/queue.h>
#include <semaphore.h>

#include <readline/readline.h>
#include <signal.h>
#include <unistd.h>         // close, sleep
#include <sys/types.h>	 	// ssize_t
#include <sys/socket.h>     // socket, bind, listen, connect, setsockopt
#include <netdb.h>          // struct addrinfo, getaddrinfo
#include <string.h>         // memset, strerror
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>        // pthread_create, pthread_join
#include <libgen.h>  // Para dirname()

#include <errno.h>          // errno

// Constantes que podrían usar otros módulos
#define OK 0
#define ERROR -1

// Variables compartidas
extern char* instruccion_global;
extern bool instruccion_disponible;
extern bool conexion_activa;
extern bool syscall_en_proceso;
extern t_log* logger;

// Mecanismos de sincronización
extern pthread_mutex_t mutex_instruccion;
extern pthread_cond_t cond_instruccion;
extern pthread_mutex_t mutex_archivo;
extern pthread_mutex_t mutex_path;
extern pthread_cond_t cond_archivo;
extern pthread_mutex_t mutex_envio_cpu;
extern pthread_mutex_t mutex_comunicacion_memoria;
extern pthread_mutex_t mutex_syscall;
extern pthread_mutex_t mutex_syscall_kernel;
extern pthread_mutex_t mutex_instruccion_pendiente;
extern pthread_mutex_t mutex_archivo_rewind;
extern pthread_mutex_t mutex_archivo_instrucciones;
extern pthread_mutex_t mutex_archivo_cierre;

// Semáforos para sincronización entre módulos
extern sem_t sem_kernel_memoria_hs;       // Kernel espera handshake de memoria
extern sem_t sem_kernel_cpu_hs;           // Kernel espera handshake de CPU
extern sem_t sem_memoria_kernel_ready;    // Memoria lista para recibir archivo
extern sem_t sem_memoria_cpu_hs;          // Memoria espera handshake de CPU
extern sem_t sem_cpu_memoria_hs;          // CPU espera handshake de memoria
extern sem_t sem_cpu_kernel_hs;           // CPU espera handshake de kernel
extern sem_t sem_archivo_listo;           // Archivo listo para ser procesado
extern sem_t sem_instruccion_lista;       // Instrucción lista para ser enviada
extern sem_t sem_servidor_cpu_listo;     // Servidor CPU listo para recibir conexiones
extern sem_t sem_instruccion_disponible; // Instrucción disponible para ser procesada
extern sem_t sem_instruccion_cpu_kernel;
extern sem_t sem_modulos_conectados;      // Módulos conectados y listos para operar
extern sem_t sem_instruccion_procesada; // Sincronización para indicar que una instrucción ha sido procesada
extern sem_t sem_syscall_procesada; // Sincronización para indicar que una syscall ha sido procesada

// Códigos de operación para identificación de módulos
typedef enum {
    KERNEL,
    CPU,
    MEMORIA
} modulo_id;

typedef struct {
    char* nombre_archivo;
    int tamanio_archivo;
} t_datos_kernel;

typedef struct {
    char instruccion[256];
    bool es_syscall;
    bool procesada;
} t_instruccion_pendiente;

// Códigos de operación para mensajes
typedef enum
{
    MENSAJE,
    PAQUETE,
    ENVIO_ARCHIVO_KERNEL,
    ARCHIVO_LISTO,
    SOLICITUD_INSTRUCCION,
    ESPERANDO_ARCHIVO,
    FIN_ARCHIVO,
    INSTRUCCION_PROCESADA,
    NOOP,
    WRITE,
    READ,
    GOTO,
    IO,
    INIT_PROC,
    DUMP_MEMORY,    
    EXIT,
    SYSCALL,
    SYSCALL_OK,
    SYSCALL_ERROR
}op_code;



typedef struct
{
	int size;
	void* stream;
} t_buffer;


typedef struct
{
	int pid;
	int pc;
	t_list ME;
	t_list MT;
} t_pcb;


// Estructura para intercambiar instrucciones
typedef struct {
    char instruccion[256];
    int longitud;
} t_instruccion;


typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;


void saludar(char* quien);
void eliminar_paquete(t_paquete* paquete);
void iniciar_logger(t_log* logger);
void print_lista(void* valor);
void inicializar_sincronizacion();
void destruir_sincronizacion();
void inicializar_semaforos();
void destruir_semaforos();
void limpiar_buffer_comunicacion(int fd);


#endif // CONEXION_H_