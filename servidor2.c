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
#define N_GRUPO_SOCKETS 2

// STRUCT CON EL CONTENIDO DEL TABLERO Y VARIABLES DEL JUEGO
////////////////////////////////////////////////////////////////////////////////////
typedef struct board{
	int rows;
	int cols;
	char tab[MAX_ROWS][MAX_COLS];
	char rivalUserName[20];
	int lastMove;
}board;

// STRUCT CON EL CONTENIDO DEL JUGADOR/CLIENTE
////////////////////////////////////////////////////////////////////////////////////
typedef struct s_client {
	int vacio;
	int socket;
	FILE *stream;
	char username[20];
	char id[15];
	int answer;
	char symbol;
	bool logged;
	bool named;
	int lastMove;
	char turn;
} client ;

// GRUPO DE SOCKETS grupodesockets.h
////////////////////////////////////////////////////////////////////////////////////
typedef struct s_grupo_de_sockets {
	int serv_socket;
	client client_info[N_GRUPO_SOCKETS];
	struct pollfd poll[1+N_GRUPO_SOCKETS];
	int nclients;
} grupo_de_sockets ;

// FUNCIONES DEL JUEGO
////////////////////////////////////////////////////////////////////////////////////
void initializeBoard(board *board);
bool legalMove(board *board, int col);
bool boardState(board *board, client *player);
void passTurn(board *board, char symbol);
bool fullBoard(board tablero);
bool fourInARow(board tablero, char ficha);
bool endGame(board board, char symbol);

// FUNCOINES DEL GRUPO DE SOCKETS
////////////////////////////////////////////////////////////////////////////////////
void init_grupo_de_sockets(grupo_de_sockets *grupo, int servsocket);
int grupo_de_sockets_guarda_socket(grupo_de_sockets *grupo, int socket, FILE *stream);
int grupo_de_sockets_borra_socket(grupo_de_sockets *grupo, int socket);
int grupo_de_sockets_acepta_nuevo_cliente(grupo_de_sockets *grupo);
void grupo_de_sockets_genera_fd_set(grupo_de_sockets *grupo, fd_set *fdset, int *maxfd);
void grupo_de_sockets_genera_pollinfo(grupo_de_sockets *grupo);
void grupo_de_sockets_poll(grupo_de_sockets *grupo,
    void (*call_new_client)(client *),
    int (*call_client_data)(client *)
);

// FUNCIONES DEL SERVIDOR
////////////////////////////////////////////////////////////////////////////////////
void assign_id(int lenght, char *id);
int contesta_mensaje_del_cliente(client *player, board *board, grupo_de_sockets *sockets);

////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]){
	
	// Comprobamos que la introduccion de los datos es la correcta
	////////////////////////////////////////////////////////////////////////////
	if (argc < 4){
		perror("\nERROR: Usa ./servidor <puerto> <filas> <columnas>\n");
		exit(4);	
	}	
	
	if (atoi(argv[2]) < 6 || atoi(argv[3]) < 7){
		perror("\nERROR: El board debe tener un tamaño minimo de 6 filas y 7 columnas\n");
		exit(6);	
	}
	////////////////////////////////////////////////////////////////////////////
	printf("\n[SERVIDOR] -> Bienvenido . . .\n");
	
	// Inicializamos el tablero de juego
	////////////////////////////////////////////////////////////////////////////
	board board;
	board.rows = atoi(argv[2]);
	board.cols = atoi(argv[3]);
	initializeBoard(&board);
	
	// Inicioamos la comunicacion entre el servidor y el cliente
	////////////////////////////////////////////////////////////////////////////
	int sock1 = socket(AF_INET, SOCK_STREAM, 0);
	if (sock1 < 0){
		perror("'SOCKET'");
		exit(-1);
	}
	struct sockaddr_in servidor;
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(atoi(argv[1])); //htons (host to network short) : 16 bits
	servidor.sin_addr.s_addr = INADDR_ANY;
	
	////////////////////////////////////////////////////////////////////////////
	if(bind(sock1, (struct sockaddr*)&servidor, sizeof(servidor)) < 0){
		perror("'BIND'");
		exit(-1);
	}
	
	//Establecemos el modo escucha para un maximo de 2 clientes
	////////////////////////////////////////////////////////////////////////////
	listen(sock1, 1);
	printf("\n[SERVIDOR] -> Escuchando conexiones en el puerto\n");
	
	//Creamos el grupo de sockets
	////////////////////////////////////////////////////////////////////////////
	grupo_de_sockets sockets;
	init_grupo_de_sockets(&sockets, sock1);
	
	// Inicializamos los jugadores/clientes
	////////////////////////////////////////////////////////////////////////////
	client *player1;
	player1 = &(sockets.client_info[0]);
	player1->logged = player1->named = false;
	////////////////////////////////////////////////////////////////////////////
	while(1){
			
		srand(time(NULL));	
		
		//Comprobamos que ambos clientes se han conectado a nuestro servidor y que ya se han asignado un nombre
		////////////////////////////////////////////////////////////////////

		if((player1->logged && player1->named)){
			printf("\n\n[SERVIDOR] > cliente conectado, iniciamos partida\n");
		
			fprintf(player1->stream, "START <BOT> %d %d\n", board.rows, board.cols);
			player1->symbol = 'o';
			player1->turn = 'o';
			//URTURN <opponent> -----> primer turno solo recibe URTURN\n
			fprintf(player1->stream, "URTURN\n");				


			//Borramos los datos para jugar la siguiente partida
			////////////////////////////////////////////////////////////////////////////	
			player1->logged = false;
			player1->named = false;

		
		}
		////////////////////////////////////////////////////////////////////////////
		else{
			grupo_de_sockets_genera_pollinfo(&sockets);
			//grupo_de_sockets_print_debug(&sockets);
			int r = poll(sockets.poll, 1 + N_GRUPO_SOCKETS, -1);	//¿?
			//printf("poll > %d\n", r);
			//grupo_de_sockets_print_debug(&sockets);
			
			struct pollfd *pollinfo = &(sockets.poll[0]);
			
			// miramos el socketservidor
			if(pollinfo->revents & POLLIN){
				// hay algo que aceptar
				int r = grupo_de_sockets_acepta_nuevo_cliente(&sockets);
				printf("\n[SERVIDOR] -> %s\n", r ? "Aceptado nuevo cliente" : "No hay sitio para el nuevo cliente");
			}
			
			for (int i=0; i<N_GRUPO_SOCKETS; i++) {
			   // printf("cli %d\n", i);
			    client *cli = &(sockets.client_info[i]);
			    pollinfo=&(sockets.poll[i+1]);

			    if (pollinfo->revents ) {
				int r = contesta_mensaje_del_cliente(cli, &board, &sockets);
				if ( r == 0 ) {
				    grupo_de_sockets_borra_socket(&sockets,cli->socket);
				    printf("\n[SERVIDOR] -> cliente desconectado\n");
				}
			    }
		       }
		}
		////////////////////////////////////////////////////////////////////////////			
	}		 
}


// FUNCION PARA ASINAR UN ID A UN NUEVO CLIENTE
////////////////////////////////////////////////////////////////////////////////////
void assign_id(int lenght, char *id){
	char characters[] = "0123456789abcdefghijklmnopqrstxyzABCDEFGHAIJKLMNOPQRSTXYZ";
	int size_characters = (int)strlen(characters); // tamaño de la secuencia de caracteres
	for(int i = 0; i < lenght; i++){
		id[i] = characters[rand()%size_characters+1]; // funcion de numeros aleatorios rand()max+1 		
							     //-> max es el numero mas alto que queremos generar
	}
	//indicamos el final del array añadiendo '\0'
	id[lenght] = '\0';
}

// FUNCION ENCARGADA DE PROCESAR LOS MENSAJES/INSTRUCCIONES DEL CLIENTE
////////////////////////////////////////////////////////////////////////////////////
int contesta_mensaje_del_cliente(client *player, board *board, grupo_de_sockets *sockets){
	char buffer[20000], buffer_aux[20000], buffer_aux2[20000], option[20], id[15], id_aux[15], name_aux[50];
	int a, b, answer_aux, col;
	FILE *registered_customers, *registered_copy;
	char *token, *token_aux;
	char *delimiter = " ";
	bool cambiado = false;
	char accion[15], file_id[15];
	
	setbuf(player->stream, NULL);
	memset(buffer, 0, sizeof(buffer));
	fgets(buffer, 2000, player->stream);
	// En vez de dividir con strtok como en la practica 3, usamos sscanf() ---> lee datos de buffer en la ubicacion proporcionada por cada argumento
	sscanf(buffer, "%s\n", option);
	
	////////////////////////////////////////////////////////////////////////////////////
	if(strcmp(option, "REGISTRAR") == 0){
	
		printf("\n[SERVIDOR] -> Recibida peticion de registro\n");
		registered_customers = fopen("servidor.txt", "a+");
		//fseek(registered_customers, 0, SEEK_END);

		a = rand()%10+1;
		b = rand()%10+1;
		answer_aux = a + b;
		
		setbuf(player->stream, NULL);
		memset(buffer, 0, sizeof(buffer));
		fprintf(player->stream, "RESUELVE %d %d\n", a, b);
		
		fgets(buffer, 20000, player->stream);
		sscanf(buffer, "RESPUESTA %d\n", &player->answer);
		
		printf("\n[SERVIDOR] -> Estableciendo prueba %d + %d = ", a, b);
		
		if(player->answer == answer_aux){
			printf(" %d, prueba superada\n", player->answer);
			memset(buffer, 0, sizeof(buffer));
			
			int lenght = (rand()%(15-7+1)) + 7;
			char id[lenght + 1];
			assign_id(lenght, id);
			strcpy(player->id, id);
			
			printf("\n[SERVIDOR] -> Asignado a cliente la id: %s\n", player->id);
			fprintf(player->stream, "REGISTRADO OK %s\n", player->id);
			
			strcpy(player->username, "Invitado");
			
			//printf("\n[SERVIDOR] Player username %s\n", player->username);
			//printf("\n[SERVIDOR] Player id %s\n", player->id);
			//printf("\n[SERVIDOR] Player answer %d\n", player->answer);
			fprintf(registered_customers, "%s %d %s\n", player->id, player->answer, player->username);
			player->logged = true;

			fclose(registered_customers);	
			return 1;			
		}
		else{
			printf(" %d, prueba NO superada\n", player->answer);
			fprintf(player->stream, "REGISTRADO ERROR\n");
			
			fclose(registered_customers);
			fclose(player->stream);
			close(player->socket);
			return 0;
		}
		return 1;
	}
	////////////////////////////////////////////////////////////////////////////////////
	if(strcmp(option, "LOGIN") == 0){
		player->logged = true;
		player->named = true;
		sscanf(buffer, "LOGIN %s %d", player->id, &player->answer);
		
		printf("\n[SERVIDOR] -> Solicitud de inicio de Sesion del usuario %s\n", player->id);
		
		registered_customers = fopen("servidor.txt", "r");
		
		//Buscamos en nuestro fichero la id que coincida con la que nos ha mandado el cliente para comprobar que ya se ha registrado previamente
		//printf("\n[SERVIDOR] -> Buscando usuario: %s . . . \n", player->id);
		
		fscanf(registered_customers, "%s %d %s\n", id_aux, &answer_aux, name_aux);
		//printf("\n[SERVIDOR] -> leyendo %s %d %s\n", id_aux, answer_aux, name_aux);
		//printf("\n[SERVIDOR]-> deberia leerse %s %d\n", player->id, player->answer);
		while(!feof(registered_customers) && (strcmp(player->id, id_aux) != 0)){
			fscanf(registered_customers, "%s %d %s\n", id_aux, &answer_aux, name_aux);	
			//printf("\n[SERVIDOR] -> leyendo %s %d %s\n", id_aux, answer_aux, name_aux);
			//printf("\n[SERVIDOR]-> deberia leerse %s %d\n", player->id, player->answer);
			//printf("\n[SERVIDOR] -> comparando %s == %s\n", id_aux, player->id);	
		}
		//printf("\n[SERVIDOR] -> Encontrado\n");
		//Comprobamos si se ha salido del bucle por fin de fichero o por que ha encontrado la id
		if(!feof(registered_customers)){
			printf("\n[SERVIDOR] -> Usuario encontrado, verificando . . .\n");
			//ahora comprobamos si la operacion es la misma
			if(answer_aux == player->answer){
				printf("\n[SERVIDOR] -> Verificacion correcta\n");
				printf("\n[SERVIDOR] -> LOGIN realizado con exito\n");
				strcpy(player->username, name_aux);
				printf("\n[SERVIDOR] -> Jugador <%s> %d> <%s>\n", player->id, player->answer, player->username);
				memset(buffer, 0, sizeof(buffer));
				fprintf(player->stream, "LOGIN OK\n");
				player->logged = true;
				fclose(registered_customers);
				return 1;
			}
			else{
				printf("\n[SERVIDOR] -> Verificacion Incorrecta\n");
				printf("\n[SERVIDOR] -> ERROR en el login: usuario u operacion incorrectos\n");
				memset(buffer, 0, sizeof(buffer));
				fprintf(player->stream, "LOGIN ERROR\n");
				player->logged = false;
				fclose(registered_customers);
				return 0;
			}
		}
		else{
			//Comprobamos la ultima linea del fichero
			if(strcmp(player->id, id_aux) == 0){
				if(answer_aux == player->answer){	
					printf("\n[SERVIDOR] -> Verificacion correcta\n");
					printf("\n[SERVIDOR] -> LOGIN realizado con exito\n");
					strcpy(player->username, name_aux);
					printf("\n[SERVIDOR] -> Jugador <%s> %d> <%s>\n", player->id, player->answer, player->username);
					memset(buffer, 0, sizeof(buffer));
					fprintf(player->stream, "LOGIN OK\n");
					player->logged = true;
					fclose(registered_customers);
					return 1;
				}
			}
			
			printf("\n[SERVIDOR] -> Verificacion Incorrecta\n");
			printf("\n[SERVIDOR] -> ERROR en el LOGIN: usuario u operacion incorrectos\n");
			memset(buffer, 0, sizeof(buffer));
			fprintf(player->stream, "LOGIN ERROR\n");
			player->logged = false;
			fclose(registered_customers);
			return 0;			
		}
		fclose(registered_customers);
		return 1;
	}
	////////////////////////////////////////////////////////////////////////////////////
	if(strcmp(option, "SETNAME") == 0){
		printf("\n\nSETNAME");
		player->named = true;
		sscanf(buffer, "SETNAME %s\n", player->username);
		//printf("\n[SERVIDOR] -> Recibido nombre de usuario %s\n", player->username);
		registered_customers = fopen("servidor.txt", "r");
		registered_copy = fopen("serverCopy.txt", "w+");
		

		//printf("\n[SERVIDOR] -> Buscando usuario: %s . . . \n", player->id);
		fscanf(registered_customers, "%s %d %s\n", id_aux, &answer_aux, name_aux);
		//printf("\n[SERVIDOR] -> comparando %s == %s\n", id_aux, player->id);
		//fprintf(registered_copy, "%s %d %s\n", id_aux, answer_aux, name_aux);
		
		while(!feof(registered_customers) && (strcmp(player->id, id_aux) != 0)){
			fprintf(registered_copy, "%s %d %s\n", id_aux, answer_aux, name_aux);
			fscanf(registered_customers, "%s %d %s\n", id_aux, &answer_aux, name_aux);	
			//printf("\n[SERVIDOR] -> comparando %s == %s\n", id_aux, player->id);
		}
		//printf("[SERVIDOR] -> Verificando . . .\n");
		
		if (!feof(registered_customers)){
			printf("\n[SERVIDOR] -> Usuario encontrado, verificando . . .\n");
			if(answer_aux == player->answer){
				printf("\n[SERVIDOR] -> Verificacion correcta\n");
				//printf("\n[SERVIDOR] -> Procesando peticion de cambio de nombre\n");
				printf("\n[SERVIDOR] -> Jugador %s %d %s\n", player->id, player->answer, player->username);
				fprintf(registered_copy, "%s %d %s\n", player->id, player->answer, player->username);
				player->named = true;
				fclose(registered_customers);
				fclose(registered_copy);
				remove("servidor.txt");
				rename("serverCopy.txt", "servidor.txt");
				player->named = true;
				fprintf(player->stream, "SETNAME OK\n");
				
				return 1;
			}
			else{
				printf("\n[SERVIDOR2] -> Verificacion incorrecta\n");
				printf("\n[SERVIDOR2] -> Peticion de cambio de nombre denegada\n");
				fprintf(player->stream, "SETNAME ERROR\n");
				fclose(registered_customers);
				fclose(registered_copy);
				remove("serverCopy.txt");
				return 0;
			}
			return 1;
		}
		else{
			//printf("\n[SERVIDOR] -> verificando . . .\n");
			//printf("\n[SERVIDOR] -> %s == %s\n", id_aux, player->id);
			if(strcmp(id_aux, player->id) == 0){
				printf("\n[SERVIDOR] -> Usuario encontrado, verificando . . .\n");
				if(answer_aux == player->answer){
					printf("\n[SERVIDOR] -> Verificacion correcta\n");
					//printf("\n[SERVIDOR] -> Procesando peticion de cambio de nombre\n");
					printf("\n[SERVIDOR] -> Jugador %s %d %s\n", player->id, player->answer, player->username);
					fprintf(registered_copy, "%s %d %s\n", player->id, player->answer, player->username);
					
					fclose(registered_customers);
					fclose(registered_copy);
					remove("servidor.txt");
					rename("serverCopy.txt", "servidor.txt");
					player->named = true;
					fprintf(player->stream, "SETNAME OK\n");	
					return 1;
				}
			}
			else{
				printf("\n[SERVIDOR] -> Verificacion incorrecta\n");
				printf("\n[SERVIDOR] -> Peticion de cambio de nombre denegada\n");
				fprintf(player->stream, "SETNAME ERROR\n");
				fclose(registered_customers);
				fclose(registered_copy);
				remove("serverCopy.txt");
				return 0;
			}
		
		}
		memset(buffer, 0, 20000);
		return 1;
	}
	////////////////////////////////////////////////////////////////////////////////////
	if(strcmp(option, "GETNAME") == 0){
		memset(buffer, 0, sizeof(buffer));
		fprintf(player->stream, "GETNAME %s\n", player->username);
		return 1;
	}
	////////////////////////////////////////////////////////////////////////////////////

	//sleep(500000);
	printf("\n/////////////////////////////////////////////////////////////////////////\n");
	printf("\n[SERVIDOR]-> INICIANDO CONECTA 4");
	printf("\n/////////////////////////////////////////////////////////////////////////\n");
	//usleep(500000);

	
	if(strcmp(option, "COLUMN") == 0){
	
		sscanf(buffer, "COLUMN %d\n", &col);
		
		if(!legalMove(board, col)){
			printf("\n[SERVIDOR] -> Movimiento erroneo\n");
			fprintf(player->stream, "COLUMN ERROR\n");
			return 1;
		}
		printf("mocimientocorrecto");
		player->lastMove = col;

		fprintf(player->stream, "COLUMN OK\n");
		printf("movimiento correcto");
		if(boardState(board, player)){
			printf("boardstate");
			printf("\n");
			if(endGame(*board, player->symbol)){
				printf("\n[SERVIDOR] -> Fin de la partida, ganan %c\n", player->symbol);
				// La mandamos al ganador el mensaje de victoria
				fprintf(player->stream, "VICTORY\n");
				return 0;
			}
			printf("no endgame");
			if(player->symbol == 'x'){
				passTurn(board, 'o');
				printf("\n[SERVIDOR] -> insertado en la columna <%d> la ficha <o>", board->lastMove);
				if(endGame(*board, 'o')){
					fprintf(player->stream, "DEFEAT\n");
					return 0;
				}
			}
			else{
				passTurn(board, 'x');
				if(endGame(*board, 'x')){
				printf("\n[SERVIDOR] -> insertado en la columna <%d> la ficha <x>", board->lastMove);
					fprintf(player->stream, "DEFEAT\n");
					return 0;
				}

			}
			fprintf(player->stream, "URTURN %d\n", board->lastMove);
				
			return 1;			
		}
		else{
			if(endGame(*board, player->symbol)){
				printf("\n[SERVIDOR] -> Fin de la partida, ganan %c\n", player->symbol);
				// La mandamos al ganador el mensaje de victoria
				fprintf(player->stream, "VICTORY\n");
				return 0;
			}
			else{
				printf("\n[SERVIDOR] -> Fin de la partida, empate\n");
				// mandamos mensaje de empate
				fprintf(player->stream, "TIE\n");
				return 0;	
			}
		}
		return 1;
	}
	return 0;
	printf("\n[SERVIDOR] -> FIN DE LA PARTIDA\n\n");
	////////////////////////////////////////////////////////////////////////////////////
}

//FUNCION PARA INICIALIZAR EL TABLERO
////////////////////////////////////////////////////////////////////////////////////////////
void initializeBoard(board *board){
	for(int i = 0; i < board->rows; i++){
		for(int j = 0; j < board->cols; j++)
			board->tab[i][j] = '-';
	}
}

// FUNCION PARA SABER SI EL MOVIMIENTO SE PUEDE HACER O NO
////////////////////////////////////////////////////////////////////////////////////////////
bool legalMove(board *board, int col){
	return(col >= 0 && col < board->cols && board->tab[0][col] == '-');
}


// FUNCION PARA SABER EL ESTADO DEL TABLERO Y ACTUALIZARLO EN CASO DE PODER PONER LA PIEZA
////////////////////////////////////////////////////////////////////////////////////////////
// 	| 0 | 1 | 2 | 3 | 4 |
//	| - | - | - | - | - |	
// 	| - | - | - | - | - |	
// 	| - | - | - | - | - |
// 	| x | o | x | o | o |
// 	|---|---|---|---|---|
// comprobamos de abajo hacia arriba en la columna en la que queriamos ingresar la pieza
// si salimos del primer bucle significa que o bien la columna esta llena o que hemos encontrado
// una posicion en la que entra nuestra ficha. En caso de ser asi se actualiza el tablero
////////////////////////////////////////////////////////////////////////////////////////////
bool boardState(board *board, client *player){
	int row = board->rows;
	while(row >= 0 && board->tab[row][player->lastMove] != '-'){
		row--;
	}
	if (row < 0){
		return false;
	}
	if(player->symbol == 'x' && fourInARow(*board, 'o'))
		return true;	
	else if(player->symbol == 'o' && fourInARow(*board, 'x'))
		return true;
	usleep(50000);
	printf("\n[SERVIDOR] -> insertado en la columna <%d> la ficha <%c>\n", player->lastMove, player->symbol); 
	board->tab[row][player->lastMove] = player->symbol;
	return true;
	
}

// FUNCION PARA REALIZAR EL CAMBIO DE TURNO ENTRE LOS JUGADOREs
////////////////////////////////////////////////////////////////////////////////////////////
void passTurn(board *board, char symbol){
	int cols, rows;
	for(int cols = 0; cols < board->cols; cols++){
		if(legalMove(board, cols)){
			rows = board->rows-1;
			while(board->tab[cols][rows] == '-' && rows > 0)
				rows--;
			board->tab[rows][cols] = symbol;
			board->lastMove = cols;
			break;
		}
	}

}
bool fourInARow(board tablero, char ficha){
    int i, j;

    // Comprobación horizontal
    for (i = 0; i < tablero.rows; i++)
        for (j = 0; j < tablero.cols - 3; j++)
            if (tablero.tab[i][j] == ficha && tablero.tab[i][j + 1] == ficha && tablero.tab[i][j + 2] == ficha && tablero.tab[i][j + 3] == ficha)
                return true;

    // Comprobación vertical
    for (i = 0; i < tablero.rows - 3; i++)
        for (j = 0; j < tablero.cols; j++)
            if (tablero.tab[i][j] == ficha && tablero.tab[i + 1][j] == ficha && tablero.tab[i + 2][j] == ficha && tablero.tab[i + 3][j] == ficha)
                return true;

    // Comprobación diagonal ascendente
    for (i = 3; i < tablero.rows; i++)
        for (j = 0; j < tablero.cols - 3; j++)
            if (tablero.tab[i][j] == ficha && tablero.tab[i - 1][j + 1] == ficha && tablero.tab[i - 2][j + 2] == ficha && tablero.tab[i - 3][j + 3] == ficha)
                return true;

    // Comprobación diagonal descendente
    for (i = 3; i < tablero.rows; i++)
        for (j = 3; j < tablero.cols; j++)
            if (tablero.tab[i][j] == ficha && tablero.tab[i - 1][j - 1] == ficha && tablero.tab[i - 2][j - 2] == ficha && tablero.tab[i - 3][j - 3] == ficha)
                return true;

    return false;
}

bool fullBoard(board tablero) {
    for (int i = 0; i < tablero.rows; i++)
        for (int j = 0; j < tablero.cols; j++)
            if (tablero.tab[i][j] == '-')
                return false;
    return true;
}

bool endGame(board tablero, char ficha) {
    if (fourInARow(tablero, ficha) || fullBoard(tablero))
        return true;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
// GRUPO DE SOCKETS grupodescokets.c
////////////////////////////////////////////////////////////////////////////////////////////

void init_grupo_de_sockets(grupo_de_sockets *grupo, int servsocket) {
    grupo->serv_socket=servsocket;
    grupo->nclients=0;
    int i;
    for (i=0; i<N_GRUPO_SOCKETS; i++) {
        grupo->client_info[i].vacio=1;
    }
}

int grupo_de_sockets_guarda_socket(grupo_de_sockets *grupo, int socket, FILE *stream) {
    int i;
    for (i=0; i<N_GRUPO_SOCKETS; i++) {
        if (grupo->client_info[i].vacio) {
            grupo->client_info[i].vacio=0;
            grupo->client_info[i].socket=socket;
            grupo->client_info[i].stream=stream;
            grupo->nclients+=1;
            return 1;
        }
    }
    return 0;
}

int grupo_de_sockets_borra_socket(grupo_de_sockets *grupo, int socket) {
    int i;
    for (i=0; i<N_GRUPO_SOCKETS; i++) {
        if ( (!grupo->client_info[i].vacio) && grupo->client_info[i].socket==socket) {
            grupo->client_info[i].vacio=1;
            grupo->nclients-=1;
            return 1;
        }
    }
    return 0;
}

int grupo_de_sockets_acepta_nuevo_cliente(grupo_de_sockets *grupo) {
    struct sockaddr_in cli_dir;
    socklen_t cli_dir_len = sizeof(cli_dir);
    int cli_sock = accept(grupo->serv_socket,(struct sockaddr *)&cli_dir,&cli_dir_len);
    FILE *cli_stream=fdopen(cli_sock,"r+");
    setbuf(cli_stream,NULL);
    if ( ! grupo_de_sockets_guarda_socket(grupo,cli_sock,cli_stream) ) {
        fprintf(cli_stream,"FULL\n");
        fclose(cli_stream);
        return 0;

    }
    else{

        fprintf(cli_stream,"WELCOME\n");

    }
    return cli_sock;
}

void grupo_de_sockets_genera_fd_set(grupo_de_sockets *grupo, fd_set *fdset, int *maxfd) {
    FD_ZERO(fdset);
    FD_SET(grupo->serv_socket,fdset);
    *maxfd=grupo->serv_socket+1;
    int i;
    for (i=0; i<N_GRUPO_SOCKETS; i++) {
        client *cli = &(grupo->client_info[i]);
        if (!cli->vacio) {
            FD_SET(cli->socket,fdset);
            if (cli->socket>=*maxfd) *maxfd=cli->socket+1;
        }
    }
}

void grupo_de_sockets_genera_pollinfo(grupo_de_sockets *grupo) {
    grupo->poll[0].fd=grupo->serv_socket;
    grupo->poll[0].events=POLLIN;
    grupo->poll[0].revents=0;
    int i;
    for (i=0; i<N_GRUPO_SOCKETS; i++) {
        client *cli = &(grupo->client_info[i]);
        if (!cli->vacio) {
            grupo->poll[i+1].fd=cli->socket;
            grupo->poll[i+1].events=POLLIN;
            grupo->poll[i+1].revents=0;
        } else {
            grupo->poll[i+1].fd=-1;
        }
    }
}


void grupo_de_sockets_poll(grupo_de_sockets *grupo,
    void (*call_new_client)(client *),
    int (*call_client_data)(client *)
) {

    grupo_de_sockets_genera_pollinfo(grupo);

    int r = poll(grupo->poll,1+N_GRUPO_SOCKETS,-1);
    //printf("poll...\n");

    if ( grupo->poll[0].revents & POLLIN ) {
        // hay nueva conexion para aceptar
        grupo_de_sockets_acepta_nuevo_cliente(grupo);
        if (call_new_client) call_new_client(NULL);
    }
    int i;
    for (i=0; i<N_GRUPO_SOCKETS; i++) {
        if (grupo->poll[i+1].revents) {
            // han llegado datos puede ser el cierre
            if (call_client_data) {
                int r=call_client_data(&(grupo->client_info[i]));
                if ( r==0 ) {
                    grupo_de_sockets_borra_socket(grupo,grupo->poll[i+1].fd);
                } 
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////





