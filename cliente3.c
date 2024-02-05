#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <poll.h>

#define MAX_ROWS 50
#define MAX_COLS 50

// STRUCT CON EL CONTENIDO DEL TABLERO Y VARIABLES DEL JUEGO
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct board{
	int rows;
	int cols;
	char tab[MAX_ROWS][MAX_COLS];
	char rivalUserName[20];
	char symbol;
	char rival;
	int myCol;
	int rivalCol;
}board;

// STRUCT CON EL CONTENIDO DEL JUGADOR/CLIENTE
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct player{
	char username[20];
	char id[15];
	char registered[5];
	int answer;
	bool played;
}player;

// FUNCIONES DEL JUEGO
////////////////////////////////////////////////////////////////////////////////////////////
void initializeBoard(board *board);
void initializePlayers(board *board);
void boardState(board *board, char symbol, int col, int row);
void printBoard(board board);
void dropPieceAnimation(board *board, int col, char piece);

////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]){
	
	// Comprobamos que la introduccion de los datos es la correcta
	////////////////////////////////////////////////////////////////////////////////////
	if(argc < 4){
		perror("\nERROR: Usa ./cliente <IP> <puerto> <nombre>\n");
		exit(4);	
	}
	
	// Inicializamos el jugador
	/////////////////////////////////////////////////////////////////////////////
	player player;
	strcpy(player.username, argv[3]);
	
	printf("\n[SERVIDOR] -> Estableciondo conexion . . .\n");
	
	// Inicioamos la comunicacion entre el servidor y el cliente
	////////////////////////////////////////////////////////////////////////////////////
	int sock1 = socket(PF_INET, SOCK_STREAM, 0);
	if (sock1 < 0){
		perror("'SOCKET'");
		exit(-1);
	}
	
	struct sockaddr_in servidor;
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(atoi(argv[2])); //htons (host to network short) : 16 bits
	servidor.sin_addr.s_addr = inet_addr(argv[1]);
	
	////////////////////////////////////////////////////////////////////////////////////
	int r = connect(sock1, (struct sockaddr*)&servidor, sizeof(servidor));
	if(r != 0){
		perror("'CONNECT'");
		exit(0);
	}
	////////////////////////////////////////////////////////////////////////////////////
		
	FILE *sock_stream;
	sock_stream = fdopen(sock1, "r+");	

	char buffer[20000];
	char msg[20];
	int a, b, answer;
	bool legalMove, jugando;
	jugando = true;
	
	FILE *register_file;
	
	printf("\n[SERVIDOR] Conectando partida . . .\n");
	
	////////////////////////////////////////////////////////////////////////////////////
	while(1){
		fgets(buffer, 20000, sock_stream);
		sscanf(buffer, "%s", msg);
		if (strcmp(msg, "WELCOME") == 0)
			break;	
	}
	printf("\n[SERVIDOR] Bienvenido a Conecta 4\n");
	
	////////////////////////////////////////////////////////////////////////////////////
	// Comprobamos si es la  primera vez que el cliente se conecta mirando si exite el .txt cliente
	////////////////////////////////////////////////////////////////////////////////////
	if(access("cliente.txt", F_OK) != 0){
	
		printf("\n[SERVIDOR] -> No existe id almacenado, realizando peticion de registro\n");
		register_file = fopen("cliente.txt", "a+b");
		
		fprintf(sock_stream, "REGISTRAR\n");
		
		fgets(buffer, 20000, sock_stream);
		sscanf(buffer, "RESUELVE %d %d\n", &a, &b);
		
		printf("\n[SERVIDOR] -> RESUELVE %d + %d\n", a, b);
		player.answer = a + b;
		
		memset(buffer, 0, sizeof(buffer));
		
		fprintf(sock_stream, "RESPUESTA %d\n", player.answer);
		
		fgets(buffer, 20000, sock_stream);
		
		sscanf(buffer, "REGISTRADO %s %s\n", msg, player.id);
		
		//printf("[SERVIDOR] -> %s", msg);
		
		if (strcmp(msg, "OK") != 0){
			printf("\n[SERVIDOR] --> Error en el registro\n");
			close(sock1);
			fclose(register_file);
			remove("cliente.txt");
			return 0;
		}
		printf("\n[SERVIDOR] -> Conexion con el servidor exitosa\n");
		fprintf(register_file, "%s %d\n", player.id, player.answer);
		fclose(register_file);
		//printf("\n[SERVIDOR] -> marca 1\n");
		player.played = false;
	}
	////////////////////////////////////////////////////////////////////////////////////
	// El cliente ya se habia conectado antes
	////////////////////////////////////////////////////////////////////////////////////
	else{
		player.played = true;
		register_file = fopen("cliente.txt", "r");	
		fscanf(register_file, "%s %d", player.id, &player.answer);
		printf("\n[SERVIDOR] -> Hay datos para el usuario %s, probamos autentificacion\n", player.id);
		
		fprintf(sock_stream, "LOGIN %s %d\n", player.id, player.answer);
		memset(buffer, 0, 20000);
		fgets(buffer, 20000, sock_stream);
		//printf("BUFFER: %s", buffer);
		sscanf(buffer, "LOGIN %s", msg);
		memset(buffer, 0, 20000);
		if(strcmp(msg, "OK") != 0){
			printf("\n[SERVIDOR] -> Error en el login\n");
			close(sock1);
			fclose(register_file);
			exit(0);
		}
		printf("\n[SERVIDOR] -> Usuario con id %s aceptado\n", player.id);
		fclose(register_file);
		//player.played = true;
		printf("\n[SERVIDOR] -> Jugador <%s> <%d> <%s> ya listo para jugar\n\n", player.id, player.answer, player.username);
	}
	// Comprobamos haber si el cliente tiene asignado ya un nombre en el servidor
	////////////////////////////////////////////////////////////////////////////////////
	if(!player.played){
		memset(buffer, 0, 20000);
		fprintf(sock_stream, "SETNAME %s\n", player.username);
		memset(buffer, 0, 20000);
		fgets(buffer, 20000, sock_stream);
		//printf("\n[SERVIDOR] -> %s", buffer);
		sscanf(buffer, "SETNAME %s\n", msg);
		memset(buffer, 0, 20000);
		if(strcmp(msg, "OK") != 0){
			printf("\n[SERVIDOR] -> ERROR a la hora de cambiar el nombre\n");
			exit(0);
		}
		printf("\n[SERVIDOR] -> Jugador <%s> <%d> <%s> listo para jugar\n\n", player.id, player.answer, player.username);
		player.played = true;
	}
	
	////////////////////////////////////////////////////////////////////////////////////
	// Inicializamos el tablero y el jugador
	////////////////////////////////////////////////////////////////////////////////////
	board board;
	//board.tab[1][1] = 'x';
	memset(buffer, 0, sizeof(buffer));
	fgets(buffer, 20000, sock_stream);
	
	// Presentamos a los jugadores entre si
	////////////////////////////////////////////////////////////////////////////////////
	//printf("\n[SERVIDOR] -> Buffer: %s\n", buffer);
	sscanf(buffer, "START %s %d %d\n", board.rivalUserName, &board.rows, &board.cols);
	printf("\n[SERVIDOR] -> Va a comenzar la partida . . .\n|-> Rival %s\n", board.rivalUserName);	
	
	//printf("\nPreInitialize: %c", board.tab[1][1]);
	initializeBoard(&board);
	//printf("\nPostInitialize: %c", board.tab[1][1]);
	initializePlayers(&board);
	
	
	sleep(1);
	system("clear");
	printf("\n/////////////////////////////////////////////////////////////////////////\n");
	printf("\n[SERVIDOR]-> INICIANDO CONECTA 4");
	printf("\n[SERVIDOR]-> TABLERO DIMENSIONES %dx%d", board.rows, board.cols);
	printf("\n[SERVIDOR]-> Las columnas van de la 0 a la %d\n", board.cols-1);
	printf("\n/////////////////////////////////////////////////////////////////////////\n");
	printf("\n\t\t[SERVIDOR]-> <%s> VS <%s>\n", player.username, board.rivalUserName);
	printf("\n/////////////////////////////////////////////////////////////////////////\n");
	sleep(3);
	system("clear");
	
	
	
	////////////////////////////////////////////////////////////////////////////////////
	// Mientras se indique que estamos jugando nos mantenemmos en el bucle
	// El salir dependera de si el tablero esta lleno o de si hay un ganador
	////////////////////////////////////////////////////////////////////////////////////
	while(jugando){		
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 20000, sock_stream);
		//URTURN <opponent> -----> primer turno solo recibe URTURN\n
		sscanf(buffer, "%s %d\n", msg, &board.rivalCol);
		legalMove = false;
		//printf("\n[SERVIDOR] -> msg: %s. %d\n", msg, board.rivalCol); 
		////////////////////////////////////////////////////////////////////////////
		if(strcmp(msg, "URTURN") == 0){
			//printf("\n[SERVIDOR] -> Su turno\n");			
			if (board.rivalCol >= 0){ //La partida la empieza el rival
		
				if(board.symbol == '-' && board.rival == '-'){
					printf("\n[SERVIDOR] -> Jugara con X y su rival con O\n");
					board.rival = 'o';
					board.symbol = 'x';
					
				}
				// actualizamos el tablero añadiendo la ultima jugada del rival
				dropPieceAnimation(&board, board.rivalCol, board.rival);
				//boardState(&board, board.rival, board.rivalCol);
	
				while(!legalMove){
					printf("\n[SERVIDOR] -> <%c> Introduzca la columna en la que desea poner la ficha: ", board.symbol);
					scanf(" %d", &board.myCol);
					fprintf(sock_stream, "COLUMN %d\n", board.myCol);
					
					memset(buffer, 0, sizeof(buffer));
					fgets(buffer, 20000, sock_stream);
					//printf("\n[SERVIDOR] -> buffer: %s\n", buffer);
					
					sscanf(buffer, "COLUMN %s\n", msg);
					legalMove = (strcmp(msg, "OK") == 0);
				}
				//printf("\n[SERVIDOR] -> movimiento legal\n");
				dropPieceAnimation(&board, board.myCol, board.symbol);
				//boardState(&board, board.symbol,board.myCol);
				printf("\n");
			}
			else{
				board.symbol = 'o';
				board.rival = 'x';
				//printf("\n[SERVIDOR] -> Juegas con o\n");
				//printBoard(board);
			
				while(!legalMove){
					printf("\n[SERVIDOR] -> <%c> Introduzca la columna en la que desea poner la ficha: ", board.symbol);
					scanf(" %d", &board.myCol);
					fprintf(sock_stream, "COLUMN %d\n", board.myCol);
					
					memset(buffer, 0, sizeof(buffer));
					fgets(buffer, 20000, sock_stream);
					//printf("\n[SERVIDOR] -> buffer: %s\n", buffer);
					
					sscanf(buffer, "COLUMN %s\n", msg);
					legalMove = (strcmp(msg, "OK") == 0);
				}
				//printf("\n[SERVIDOR] -> movimiento legal\n");
				// actualizamos el tabelro añadiendo la ultima juada nuestra
				dropPieceAnimation(&board, board.myCol, board.symbol);
				//boardState(&board, board.symbol, board.myCol);
				printf("\n");
			}
		}
		////////////////////////////////////////////////////////////////////////////
		// Si el mensaje que nos llega no es UTURN significa que la partida ha terminado
		////////////////////////////////////////////////////////////////////////////
		else{
			jugando = false;
		}	
	}
	if(strcmp(msg, "VICTORY") == 0){
		printf("\n\n\t\t¡¡¡ENHORABUENA, HAS GANADO!!!\n\n\n");
		return 0;
	}
	//printf("\n[SERVIDOR] -> RIVAL <%d> <%c>", board.rivalCol, board.rival);
	//dropPieceAnimation(&board, board.rivalCol, board.rival);
	if(strcmp(msg, "DEFEAT") == 0){
		//dropPieceAnimation(&board, board.rivalCol, board.rival);
		system("clear");
		//printBoard(board);
		printf("\n\n\t\tLO SIENTO, HAS PERDIDO :(\n\n\n");
		return 0;
	}
	else if(strcmp(msg, "TIE") == 0){
		//dropPieceAnimation(&board, board.rivalCol, board.rival);
		//system("clear");
		//printBoard(board);
		printf("\n\n\t\tVAYA... EMPATE\n\n\n");
		return 0;
	}
	else{
		system("clear");	
		printf("\n\n\t\tERROR EN LA PARTIDA\n\n\n");
		return 0;
	}
	fclose(sock_stream);
	close(sock1);
}
////////////////////////////////////////////////////////////////////////////////////////////

// FUNCION PARA INICIALIZAR EL TABLERO
////////////////////////////////////////////////////////////////////////////////////////////
void initializeBoard(board *board){
	for(int i = 0; i < board->rows; i++){
		for(int j = 0; j < board->cols; j++)
			board->tab[i][j] = '-';
	}
}

// FUNCION PARA INICLIACIAR AL JUGADOR
////////////////////////////////////////////////////////////////////////////////////////////
void initializePlayers(board *board){
	board->symbol = '-';
	board->rival = '-';
	board->myCol = -1;
	board->rivalCol = -1;
}


// FUNCION PARA SABER EL ESTADO DEL TABLERO Y ACTUALIZARLO EN CASO DE PODER PONER LA PIEZA
////////////////////////////////////////////////////////////////////////////////////////////
void boardState(board *board, char symbol, int col, int row){	
	/*int row = board->rows;
	while(row >= 0 && board->tab[row][col] != '-'){
		row--;
	}*/
	if (row != -1)
		board->tab[row][col] = symbol;
	
}

// FUNCION PARA IMPRIMIR POR PANTALLA EL TABLERO
////////////////////////////////////////////////////////////////////////////////////////////
void printBoard(board board){
	system("clear");
	// Imprimimos el cabecero
	printf("\n|");
	for(int i = 0; i<board.cols; i++)
		printf(" %d |", i);
	printf("\n|");
	for(int i = 0; i<board.cols; i++)
		printf("---|");
	printf("\n");
	for(int i = 0; i < board.rows; i++){
		printf("|");
		for(int j = 0; j < board.cols; j++){
			printf(" %c |", board.tab[i][j]);
		}
		if(i != board.rows-1) printf("\n");
	}
	printf("\n|");
	for(int i = 0; i<board.cols; i++)
		printf("---|");
	printf("\n");
}

void dropPieceAnimation(board *board, int col, char piece){
    int row = -1;
    // Encuentra la fila final en la columna dada
    for (int i = board->rows - 1; i >= 0; i--) {
        if (board->tab[i][col] == '-') {
            row = i;
            break;
        }
    }

    if (row == -1) {
        printf("Columna llena. No se puede colocar más fichas.\n");
        return;
    }

    // Animación de la ficha cayendo
    for (int i = 0; i <= row; i++) {
        usleep(200000); // Ajusta la velocidad de caída
        //system("clear");
        printBoard(*board);
        //usleep(5000);
        if(col > 0)
	        board->tab[i][col] = piece;
        if (i > 0) {
            board->tab[i - 1][col] = '-';
        }
    }
    boardState(board, piece, col, row);
    //printf("\n[SERVIDOR] -> TABLERO ACTUALIZADO\n");   
    system("clear"); 
    printBoard(*board);
}


