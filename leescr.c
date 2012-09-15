#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

typedef struct{
	char etiqueta;
	int retardo;
	int tiempoCPU;
}TProceso;

int shmid;
int *variable=0;
int shmid2;
int *variable2=0;

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
	time_t tiempo;
	int semaforo,archivo,leido,estado,i,procesos=0;
	char caracter;
	shmid=shmget(IPC_PRIVATE,sizeof(int),IPC_CREAT|0666);  // Creamos el segmento de memoria compartida
	shmid2=shmget(IPC_PRIVATE,sizeof(int),IPC_CREAT|0666);  // Creamos el segmento de memoria compartida
	semaforo=creaSEM(4,"clave");      // 0-Escritores  1-Lectores  2-MUTEX1(Lec)  3-MUTEX2(Esc)
	iniSEM(semaforo,0,1);
	iniSEM(semaforo,1,1);
	iniSEM(semaforo,2,1);
	iniSEM(semaforo,3,1);
	archivo=open(argv[1],O_RDONLY);
	do{                  // vemos cuantas lineas tiene el archivo, que sera el numero de procesos a crear
		leido=read(archivo,&caracter,1);
		if((leido)==1){
			if(caracter=='\n') procesos++;
		}
	} while(leido==1);
	close(archivo);
	//printf("lineas del archivo=%d",procesos);
	TProceso traza[procesos];
	archivo=open(argv[1],O_RDONLY);
	for(i=0;i<procesos;i++){
		if((leido=read(archivo,&caracter,1))==1) traza[i].etiqueta=caracter;
		if((leido=read(archivo,&caracter,1))==1); //leemos un espacio en blanco
		if((leido=read(archivo,&caracter,1))==1) traza[i].retardo=(int)caracter-48;
		if((leido=read(archivo,&caracter,1))==1) if((caracter>='0')&&(caracter<='9')){
								traza[i].retardo=(traza[i].retardo*10)+((int)caracter-48);
								if((leido=read(archivo,&caracter,1))==1); //leemos un espacio en blanco
							}
		if((leido=read(archivo,&caracter,1))==1) traza[i].tiempoCPU=(int)caracter-48;
		if((leido=read(archivo,&caracter,1))==1) if((caracter>='0')&&(caracter<='9')){
								traza[i].tiempoCPU=(traza[i].tiempoCPU*10)+((int)caracter-48);
								if((leido=read(archivo,&caracter,1))==1); //leemos el final de linea \n
							}
	}
	close(archivo);
	//for(i=0;i<procesos;i++) printf("%c %d %d\n",traza[i].etiqueta,traza[i].retardo,traza[i].tiempoCPU);
	printf("Tiempo       Tipo    Accion\n");
	for(i=0;i<procesos;i++){
		pid=fork();
		switch(pid){
		case -1:
			printf("Error al crear el proceso hijo.\n");
		break;
		case 0:
			//printf("Soy el hijo %d. Mi pid es %d y mi ppid es %d\n",i,getpid(),getppid());
			variable=(int*)shmat(shmid,0,0);  // vinculamos la memoria compartida al proceso
			variable2=(int*)shmat(shmid2,0,0);  // vinculamos la memoria compartida al proceso
			if(traza[i].etiqueta=='E'){
				sleep(traza[i].retardo);

				(void)time(&tiempo); printf("%u     %c    P(MUTEX2)\n",tiempo,traza[i].etiqueta);
				P(semaforo,3);
				*variable2++;
				if(*variable2==1) P(semaforo,1);
				V(semaforo,3);
				(void)time(&tiempo); printf("%u     %c    V(MUTEX2)\n",tiempo,traza[i].etiqueta);

				P(semaforo,0);
				(void)time(&tiempo); printf("%u     %c    Entra_Seccion\n",tiempo,traza[i].etiqueta);
				sleep(traza[i].tiempoCPU);
				(void)time(&tiempo); printf("%u     %c    Sale_Seccion\n",tiempo,traza[i].etiqueta);
				V(semaforo,0);

				(void)time(&tiempo); printf("%u     %c    P(MUTEX2)\n",tiempo,traza[i].etiqueta);
				P(semaforo,3);
				*variable2--;
				if(*variable2==0) V(semaforo,1);
				V(semaforo,3);
				(void)time(&tiempo); printf("%u     %c    V(MUTEX2)\n",tiempo,traza[i].etiqueta);

				shmdt((char*)variable);  // desvinculamos la memoria compartida del proceso
				exit(0);
			}
			else{
				sleep(traza[i].retardo);

				P(semaforo,1);
				(void)time(&tiempo); printf("%u     %c    P(MUTEX1)\n",tiempo,traza[i].etiqueta);
				P(semaforo,2);
				*variable++;
				if(*variable==1) P(semaforo,0);
				V(semaforo,2);
				(void)time(&tiempo); printf("%u     %c    V(MUTEX1)\n",tiempo,traza[i].etiqueta);
				V(semaforo,1);

				(void)time(&tiempo); printf("%u     %c    Entra_Seccion\n",tiempo,traza[i].etiqueta);
				sleep(traza[i].tiempoCPU);
				(void)time(&tiempo); printf("%u     %c    Sale_Seccion\n",tiempo,traza[i].etiqueta);

				(void)time(&tiempo); printf("%u     %c    P(MUTEX1)\n",tiempo,traza[i].etiqueta);
				P(semaforo,2);
				*variable--;
				if(*variable==0) V(semaforo,0);
				V(semaforo,2);
				(void)time(&tiempo); printf("%u     %c    V(MUTEX1)\n",tiempo,traza[i].etiqueta);

				shmdt((char*)variable);  // desvinculamos la memoria compartida del proceso
				exit(0);
			}
		}
	}
	shmctl(shmid,IPC_RMID,0);  //eliminamos el sector de memoria (lo borramos)
	for(i=1;i<=procesos;i++){
		wait(&estado);
	}
	exit(0);


return 0;
}
