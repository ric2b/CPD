#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

#define max(a,b) (((a) > (b)) ? (a) : (b)) /*Retorna o máximo entre A e B*/

int id=0, p=0;
int gN=0, gM=0;
int sizeLCS=0;
int *chunkN=NULL, *chunkM=NULL;
MPI_Status status;

/*Estrutura definida como pilha - Armazena a LCS*/
typedef struct t_stack{ 
	char *st;
	int top;
}stack;

/*Aloca memória necessária para a estrutura pilha*/
stack createStack(int sizeStack){
	stack new;

	new.st = (char*)calloc(sizeStack, sizeof(char));
	if(new.st == NULL){
		printf("\n[ERROR] Creating Stack\n\n");
		exit(-1);
	}
	
	new.top = -1;
	
	return new;
}

/*Aloca memória necessária para a LCS obtida por cada processo*/
stack reallocStack(stack seq, int count){

	seq.st = (char*)realloc(seq.st, count*sizeof(char));
	if(seq.st == NULL){
		printf("\n[ERROR] Reallocating memory for stack\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	return seq;
}

/*Insere novo elemento na pilha*/
stack push(stack seq, char item, int x){
  seq.top=x;
  
  seq.st[seq.top] = item;
  
	return seq;
}

/*Imprime os caractéres armazenas na pilha*/
void display(stack seq){ 
  int i=0;
   
	for(i=seq.top; i>=0; i--){
  	printf("%c", seq.st[i]);
	}
	putchar('\n');
  
  return;
}

/*Estrutura necessária para a computação da LCS*/
typedef struct t_data{
	short **matrix;
	char *rows;
	char *columns;
}data;

/*Cria matriz como um vector contínuo de elementos*/
short **createMatrix(int N, int M){ 
	int i=0;
	short *aux=NULL;
	short **newM=NULL;
	
	aux = (short*)calloc((N+1)*(M+1), sizeof(short));
	if(aux == NULL){
		printf("\n[ERROR] Creating matrix\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	newM = (short**)calloc((N+1), sizeof(short*));
	if(newM == NULL){
		printf("\n[ERROR] Creating matrix\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	for(i=0; i<(N+1); i++){
		newM[i] = &(aux[(M+1)*i]);
	}
	
	return newM;
}

/*Cria vector para armazenamento das sequências de caractéres*/
char *createVector(int SIZE){ 
	char *newV=NULL;
	
	newV = (char*)calloc(SIZE, sizeof(char));
	if(newV == NULL){
		printf("\n[ERROR] Creating Vector\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	return newV;
}

/*Cálculo dos "chunks"*/
int *defineChunk(int DIM, int ft){
	int *aux=NULL;
	int i=0;
	
	aux=(int*)calloc(p, sizeof(int)); 
	if(aux == NULL){
		if(!id) printf("\n[ERROR] Allocating memory in function defineChunk(int,int)\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	if((DIM % (ft*p)) == 0){
		for(i=0; i<p; i++) aux[i]=DIM/(ft*p);
	}else{
		for(i=0; i<(p-1); i++) aux[i] = (DIM/(ft*p))+1;
		aux[p-1] = (DIM - ((ft*p)-1)*((DIM/(ft*p))+1));
	}
	
	return aux;
}

/*Abre ficheiro de input e retorna o ponteiro para este*/
FILE *openFile(char *fname){
	FILE *fp=NULL;
	
	fp = fopen(fname, "r");
	if(fp == NULL){
		printf("\n[ERROR] Opening File\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	return fp;
}

/*Leitura do ficheiro de input e retorno da informação necessária a cada processo*/
data readFile(char *fname){
	FILE *pFile=NULL;
	data newData;
	
	if(!id){
		pFile = openFile(fname);
		
		if(fscanf(pFile, "%d %d", &gN, &gM) != 2){
    	printf("\n[ERROR] Reading variables from the input file\n\n");
    	fclose(pFile);
    	MPI_Finalize();
    	exit(-1); 
  	}
  	if(fscanf(pFile, "\n") != 0){
  		printf("\n[ERROR] Reading EOF from the input file\n\n");
   	 	fclose(pFile);
    	MPI_Finalize();
    	exit(-1);
  	}
	}
	MPI_Bcast(&gN, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&gM, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
	chunkN=defineChunk(gN, 1);
	chunkM=defineChunk(gM, log10(gM));
	
	if(!id){
		int aux=0, j=0;
		char *rows=NULL;
		
		rows = createVector(gN);
	
		if(fread(rows, 1, gN, pFile) != gN){
	  	printf("\n[ERROR] Reading information from file\n\n");
	  	free(rows);	
	  	fclose(pFile);
	  	MPI_Finalize();
	  	exit(-1); 
		}
		if(fscanf(pFile, "\n") != 0){
			printf("\n[ERROR] Reading EOF from file\n\n");
			free(rows);
			fclose(pFile);
			MPI_Finalize();
			exit(-1);
		}

		newData.columns = createVector(gM);
	
		if(fread(newData.columns, 1, gM, pFile) != gM){
			printf("\n[ERROR] Reading information from file\n\n");
			free(rows);
			free(newData.columns);
			fclose(pFile);
			MPI_Finalize();
			exit(-1); 
		}
		for(j=0; j<p; j++){
			MPI_Send(&(rows[aux]), chunkN[j], MPI_CHAR, j, j, MPI_COMM_WORLD);
			aux+=chunkN[j];	
		}
		free(rows);
	}
	
	newData.rows = createVector(chunkN[id]);
	if(id) newData.columns = createVector(gM);
	
	/*Cada processo só precisa de parte da string correspondente às linhas*/
	MPI_Recv(&(newData.rows[0]), chunkN[id], MPI_CHAR, 0, id, MPI_COMM_WORLD, &status);
	/*Todos os processos precisam da totalidade da string correspondente às colunas*/
	MPI_Bcast(&(newData.columns[0]), gM, MPI_CHAR, 0, MPI_COMM_WORLD);
	
	/*Cada máquina terá a sua própria matriz de dimensões (chunkN[id]+1)x(M+1)*/
	newData.matrix = createMatrix(chunkN[id], gM);
	
	if(!id) fclose(pFile);
	
	return newData;
}

/*Função cost fornecida*/
short cost(int x){
	int i=0, n_iter=20;
	double dcost=0;
	
	for(i=0; i<n_iter; i++){
		dcost += pow(sin((double) x), 2) + pow(cos((double) x), 2);
	}	
	
	return (short) (dcost / n_iter + 0.1);	
}

/*Realiza a computação do bloco da matriz correspondente*/
short **computeMatrix(data lcs, int N, int M, int mj){
	int i=0, jj=0, j=0, it=0, k=0, w1=0, w2=0;
	
	#pragma omp parallel firstprivate(N,M) private(it)
	{
		for(it = 1; it < (M+N); it++){
			w1 = it < M ? 0 : it - M;
			w2 = it < N ? 0 : it - N;
			#pragma omp for private(k, i, j)
			for(k=it-w2; k > w1; k--){
				i = k;
				j = ((it-k)+1)+mj;
				if(lcs.rows[i-1] == lcs.columns[j-1]){
					lcs.matrix[i][j] = lcs.matrix[i-1][j-1] + cost(i);
				}else{
					lcs.matrix[i][j] = max(lcs.matrix[i][j-1], lcs.matrix[i-1][j]);
				}
			}
		} 
	}
	
	return lcs.matrix;
}

data distributeMatrix(data lcs, int ft){
	int k=0, nj=0, idN=0, idM=0;
	MPI_Request request;
	
	for(k=0; k < p*(1+ft)-1; k++){
		if(k < (ft*p)){/*Aumento da carga de trabalho + Carga de trabalho constante*/
			if(!id){ /*O processo 0 nunca recebe informação de outro processo*/
				idN=chunkN[id];
				idM=chunkM[id];
				if(k == (ft*p)-1) idM=chunkM[p-1];
				
				lcs.matrix = computeMatrix(lcs, idN, idM, nj); /*Computação do bloco correspondente*/
				
				MPI_Isend(&(lcs.matrix[idN][nj+1]), idM, MPI_SHORT, id+1, id, MPI_COMM_WORLD, &request); /*Envia informação para o processo id+1*/
		
				nj += idM; /*Indica a primeira coluna do bloco seguinte*/
			}else{
				if(k >= id){ /*O primeiro bloco a ser processado, por cada processo, poderá ter dimensões diferentes dos próximos blocos*/
					idN=chunkN[id];
					idM=chunkM[id-1];
				
					/*Recebe informação necessária do processo id-1*/
					MPI_Recv(&(lcs.matrix[0][nj+1]), idM, MPI_SHORT, id-1, id-1, MPI_COMM_WORLD, &status);
					
					lcs.matrix = computeMatrix(lcs, idN, idM, nj); /*Computação do bloco correspondente*/
					
					/*Envia informação para o processo id+1 - o processo id=p-1 nunca envia informação para outro processo*/
					if(id != p-1) MPI_Isend(&(lcs.matrix[idN][nj+1]), idM, MPI_SHORT, id+1, id, MPI_COMM_WORLD, &request);
					
					nj += idM;
				}
			}
		}else{ /*Redução da carga de trabalho*/
			if(id > k-(ft*p)){
				idN=chunkN[id];
				idM=chunkM[id-1];
				if(k-((ft*p)-1) == id) idM=chunkM[p-1];
				 
				/*Recebe informação necessária do processo id-1*/
				MPI_Recv(&(lcs.matrix[0][nj+1]), idM, MPI_SHORT, id-1, id-1, MPI_COMM_WORLD, &status);
				
				lcs.matrix = computeMatrix(lcs, idN, idM, nj); /*Computação do bloco correspondente*/
				
				/*Envia informação para o processo id+1 - o processo id=p-1 nunca envia informação para outro processo*/
				if(id != p-1) MPI_Isend(&(lcs.matrix[idN][nj+1]), idM, MPI_SHORT, id+1, id, MPI_COMM_WORLD, &request);
					
				nj += idM;
			}
		}
	}

	return lcs;
}

stack computeLCS(data lcs, stack seq, int *flag){
	MPI_Request request;
	int i=0, j=0, k=0, count=0, t=0;
	int *ij=NULL;
	
	if(id == p-1){
		i=chunkN[id];
		j=gM;
		sizeLCS = lcs.matrix[i][j];
		MPI_Isend(&sizeLCS, 1, MPI_INT, 0, id, MPI_COMM_WORLD, &request);
	}
	
	if(!id){
		MPI_Recv(&sizeLCS, 1, MPI_INT, p-1, p-1, MPI_COMM_WORLD, &status);
		seq = createStack(sizeLCS);
	}
	
	ij=(int*)calloc(2, sizeof(int));
	if(ij == NULL){
		printf("\n[ERROR] Allocating memory in function computeLCS(data,stack,int)\n\n");
		MPI_Finalize(); 
		exit(-1);
	}
	ij[0]=gN;
	ij[1]=gM;
	
	k=p;
	
	while(ij[0] != 0 && ij[1] != 0){
		
		k--;
		count=0;
		
		if(k == id){
			i=chunkN[id];
			j=ij[1];
			while(i != 0 && j != 0){
				if(lcs.rows[i-1] == lcs.columns[j-1]){
					count++;
					(*flag)=1;
					
					if(id){ 
						if(count==1) seq = createStack(count);
						if(count>1)  seq = reallocStack(seq, count);
					}
					
					if(!id && count==1) count+=t;
					
					seq=push(seq, lcs.rows[i-1], count-1);
					
					i=i-1;
					j=j-1;
				}else{
					if(lcs.matrix[i][j-1] >= lcs.matrix[i-1][j]){
						j=j-1;
					}else{
						i=i-1;
					}
				}
			}
			if(!id && i==0) ij[0]=0; /*Apenas o processo 0 pode obter i=0*/
			ij[1]=j;
			if(id){
				MPI_Send(&count, 1, MPI_INT, 0, k, MPI_COMM_WORLD);
				MPI_Send(&(seq.st[0]), count, MPI_CHAR, 0, k, MPI_COMM_WORLD);
			}
		}
		
		if(!id){
			if(ij[0] != 0 && ij[1] != 0){
				MPI_Recv(&count, 1, MPI_INT, k, k, MPI_COMM_WORLD, &status);
				MPI_Recv(&(seq.st[t]), count, MPI_CHAR, k, k, MPI_COMM_WORLD, &status);
			}
			t+=count;
		}
		
		MPI_Bcast(&ij[0], 2, MPI_INT, k, MPI_COMM_WORLD);
	}
	
	free(ij);
			
	return seq;
}

void freeMem(data lcs, stack seq, int flag){
	
	if(flag == 1) free(seq.st);
	
	/*Libertart memória alocada para a matriz de cada máquina*/
	free(lcs.matrix[0]);
	free(lcs.matrix);
	
	/*Libertar memória alocada para os vectores de caractéres de cada máquina*/
	free(lcs.rows);
	free(lcs.columns);
	
	return;
}

void print(stack seq){ /* Imprime o comprimento da LCS e a actual LCS */
	
	printf("%d\n", sizeLCS);
	display(seq);
	
	return;
}

int main(int argc, char *argv[]){
	char *fname=NULL;
	int flag=0;
	int i=0;
	data lcs;
	stack seq;
	
	MPI_Init(&argc, &argv);
	
	MPI_Comm_size(MPI_COMM_WORLD, &p);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	
	if(argc != 2){
		if(!id) printf("\n[ERROR] Missing initial arguments\n\n");
		MPI_Finalize();
		exit(-1);
	}
	
	if(!id) fname = argv[1];
	
	/*Leitura do Ficheiro*/
	lcs = readFile(fname);
	
	/*Computação da Matriz*/
	lcs = distributeMatrix(lcs, log10(gM));
	
	MPI_Barrier(MPI_COMM_WORLD);
	
	/*Obtenção da maior subsequência comum*/
	seq = computeLCS(lcs, seq, &flag);
	
	/*Impressão do resultado*/
	if(!id) print(seq);
	
	/*Libertação da memória alocada*/
	freeMem(lcs, seq, flag); 
	
	MPI_Finalize();
	
	exit(0);
}
