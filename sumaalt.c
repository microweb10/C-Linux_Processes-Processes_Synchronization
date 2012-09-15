#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

int shmid;
int *variable=NULL;

#define OP_P ((short) -1)
#define OP_V ((short) 1)

int semid;
int max;


int creaSEM(int cuantosSEM, char *queClave){
int idSEM;
key_t llave;
llave=ftok(queClave,'k');
if((idSEM=semget(llave,cuantosSEM,IPC_CREAT|0600))==-1){
	perror("semget");
	exit(-1);
}
return idSEM;
}

void iniSEM(int semID,ushort queSemaforo, int queValor){
semctl(semID,queSemaforo,SETVAL,queValor);
}

void P(int semID, ushort queSemaforo){
struct sembuf operacion;
operacion.sem_num=queSemaforo; // iD Semaforo
operacion.sem_op=OP_P;  // operacion P
operacion.sem_flg=0;   
semop(semID,&operacion,1); // Ejecucion operacion
}

void V(int semID, ushort queSemaforo){
struct sembuf operacion;
operacion.sem_num=queSemaforo; // iD Semaforo
operacion.sem_op=OP_V;  // operacion V
operacion.sem_flg=0;   
semop(semID,&operacion,1); // Ejecucion operacion
}


int main(int argc,char *argv[]){
	pid_t pid;
	int semaforo,estado;
	char *padre="padre";
	char *hijo="hijo";
	shmid=shmget(IPC_PRIVATE,sizeof(int),IPC_CREAT|0666);  // Creamos el segmento de memoria compartida
	semaforo=creaSEM(2,"clave");      // 0-Padre    1-Hijo
	if(strcmp(argv[5],padre)==0){
		iniSEM(semaforo,0,1);
		iniSEM(semaforo,1,0);
	}
	else{
		iniSEM(semaforo,0,0);
		iniSEM(semaforo,1,1);
	}
	switch(fork()){
	case -1: printf("Error en Fork\n");
		 exit(-1);
	break;
	case 0: variable=(int*)shmat(shmid,0,0);  // vinculamos la memoria compartida al proceso
		if(strcmp(argv[5],hijo)==0)
			*variable=atoi(argv[1]);
		while(*variable<atoi(argv[4])){ // si la variable no ha llegado al maximo
			if((*variable+atoi(argv[3]))<=atoi(argv[4])){
				P(semaforo,1);
				*variable=*variable+atoi(argv[3]); // incremento hijo
				printf("Hijo (%d): variable=%d\n",getpid(),*variable);
				V(semaforo,0);
			}
			else{
				P(semaforo,1);
				shmdt((char*)variable);  // desvinculamos la memoria compartida del proceso
				shmctl(shmid,IPC_RMID,0);  //eliminamos el sector de memoria (lo borramos)
				V(semaforo,0);
				sleep(1);
				exit(0);
			}
		}
	break;
	default:variable=(int*)shmat(shmid,0,0);  // vinculamos la memoria compartida al proceso
		if(strcmp(argv[5],padre)==0)
			*variable=atoi(argv[1]);
		while(*variable<atoi(argv[4])){
		 	if((*variable+atoi(argv[2]))<=atoi(argv[4])){
				P(semaforo,0);
				*variable=*variable+atoi(argv[2]); // incremento padre
				printf("Padre (%d): variable=%d\n",getpid(),*variable);
				V(semaforo,1);
			}
			else {
				P(semaforo,0);
				shmdt((char*)variable);  // desvinculamos la memoria compartida del proceso
				shmctl(shmid,IPC_RMID,0);  //eliminamos el sector de memoria (lo borramos)
				V(semaforo,1);
				sleep(1);
				exit(0);
			}
		}
	}
return 0;
}
