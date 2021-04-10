#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libgen.h>
#include <time.h>

#define VERSION			24
#define BUFSIZE			8096
#define ERROR			42
#define LOG				44
#define MAXACCES		10
#define COOKIEEXPIRE	120
#define DATESIZE		36

// Protocol Codes
#define OK				200
#define BADREQUEST		400
#define FORBIDDEN		403
#define NOTFOUND		404
#define NOTALLOWED		405
#define UMEDIATYPE		415
#define INTERNALE		500

// Strings
#define STRSERVER		"Server: web_sstt\r\n"
#define STRTYPE			"Content-Type: text/html; charset=utf-8\r\n"
#define STRCNTLENGTH	"Content-Length: %d\r\n"
#define STRCONNECT		"Connection: Keep-Alive\r\n"
#define STRKEEPA		"Keep-Alive: timeout=5, max=1000\r\n"
#define STRCOOKIE		"Cookie: ccount="
#define STRSETCOOK		"Set-Cookie: ccount=%d\r\n"
#define STROK			"HTTP/1.1 200 OK\r\n"
#define STRBADREQUEST 	"HTTP/1.1 400 Bad Request\r\n"
#define STRFORBIDDEN 	"HTTP/1.1 403 Forbidden\r\n"
#define STRNOTFOUND 	"HTTP/1.1 404 Not Found\r\n"
#define STRMETHODNOTA	"HTTP/1.1 405 Method Not Allowed\r\n"
#define STRUMEDIATYPE 	"HTTP/1.1 415 Unsupported Media Type\r\n"
#define STRINTERNALE	"HTTP/1.1 500 Internal Server Error\r\n"


// Email
#define EMAIL "adrian.canovas1%40um.es"
#define HOST "Host: web.sstt5896.org:%d**"

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{0,0} };

void debug(int log_message_type, char *message, char *additional_info, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];
	
	switch (log_message_type) {
		case ERROR: (void)sprintf(logbuffer,"ERROR: %s: %s Errno=%d exiting pid=%d",message, additional_info, errno,getpid());
			break;
		case FORBIDDEN:
			// Enviar como respuesta 403 Forbidden
			(void)sprintf(logbuffer,"FORBIDDEN: %s: %s",message, additional_info);
			break;
		case NOTFOUND:
			// Enviar como respuesta 404 Not Found
			(void)sprintf(logbuffer,"NOT FOUND: %s: %s",message, additional_info);
			break;
		case LOG: (void)sprintf(logbuffer,"INFO: %s: %s: %d",message, additional_info, socket_fd); break;
	}

	if((fd = open("webserver.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
}

//
// ---------------Funciones auxiliares ------------------------------------
//
void getDate(char *date){
	time_t d = time(NULL);
	struct tm * t = gmtime(&d);
	strftime(date, DATESIZE, "Date: %a, %e %b %Y %T GMT\r\n", t);
}

//
// Elimina los caracteres de retorno en una petición y los sustituye por *
//
int eliminarCaracteresRetorno(char *buffer){
	int i = 0;
	int c = 0;
	while(buffer[i] != '\0'){
		if(buffer[i] == '\r' && buffer[i+1] == '\n'){
			buffer[i]='*';
			buffer[i+1]='*';
			c ++;
			i ++;
		}
		i ++;
	}
	return c;
}

//
// Parsear la primera linea de la petición
//
char *parserFL(char * buffer) {
    char aux[BUFSIZE] = {0};
	char *ptr;
	char *auxptr;

	strcpy(aux, buffer);

	ptr = strtok_r(buffer, " ", &auxptr);
	ptr = strtok_r(NULL, " ", &auxptr);
	return ptr;
}

//
// Obtener cookie de una petición
//
int getCookie(char * buffer) {
    // Buscamos donde está la cookie
    const char *ptrcookie = strstr(buffer, STRCOOKIE);
	// Si no encuentra la cookie
	if(ptrcookie == NULL){
		return -1;
	}else{
		ptrcookie += strlen(STRCOOKIE);
		char * eoc = strchr(ptrcookie, '*');
		char value[eoc - ptrcookie +1];
		strncpy(value, ptrcookie, eoc - ptrcookie);
		value[sizeof(value)] = '\0';
		return atoi(value);
	}
}

//
// Determina si la extensión está soportada
//
int isExtension(char *path){
	char *file;
	char *extension;
	file = basename(path);
	extension = strrchr(file, '.');

	if (extension == NULL){
		return -1;
	}else{
		for(int i = 0; extensions[i].ext != 0; i++){
			if(strcmp(extension + 1, extensions[i].ext) == 0)
				return i;
		}
		return -1;
	}
}

//
// Enviar mensaje por socket: Generar la cabecera y enviar
//
void sendMessage(int fd, char * header, char *content, int cookie) {
	debug(LOG, "response", "Enviando Mensaje Interno", fd);
    char head[BUFSIZE]= {0};
	char body[BUFSIZE] = {0};
	// Cargamos el cuerpo
	body[0] = '\0';
	strcat(body, content);
	// Cargamos la cabecera
	head[0] = '\0';
	strcat(head, header);

	// ----- Campos de la cabecera -----
	// Campo Server
	strcat(head, STRSERVER);
	// Campo Content-Type
	strcat(head, STRTYPE);
	// Campo Content-Length
	int contentLength = strlen(body);
	sprintf(head + strlen(head), STRCNTLENGTH, contentLength);
	// Campo Date
	char fecha[DATESIZE];
	getDate(fecha);
	strcat(head, fecha);
	// Campo Connection
	strcat(head, STRCONNECT);
	// Campo Keep-Alive.
	strcat(head, STRKEEPA);
	// Campo COOKIE
	sprintf(head + strlen(head), STRSETCOOK, cookie);

	// ----- Añadimos el cuerpo -----
	strcat(head, "\r\n");
	strcat(head, body);

	// ----- Enviar mensaje -----
	write(fd, head, strlen(head));
}

//
// Función que carga el error y llama a enviar mensaje para completar la cabecera
//
void sendError(int fd, int errorType, int cookie) {
	debug(ERROR, "Message: ", "Se ha producido un error, evaluando error", fd);
	char head[BUFSIZE] = {0};
	char body[BUFSIZE] = {0};
    switch (errorType){
		case BADREQUEST:
			sprintf(head, STRBADREQUEST);
			sprintf(body, "<html><h1>400: BAD REQUEST</h1></html>");
			debug(ERROR, "Message: ", STRBADREQUEST, fd);
			break;
		case FORBIDDEN:
			sprintf(head, STRFORBIDDEN);
			sprintf(body, "<html><h1>403: FORBIDDEN</h1></html>");
			debug(ERROR, "Message: ", STRFORBIDDEN, fd);
			break;
		case NOTFOUND:
			sprintf(head, STRNOTFOUND);
			sprintf(body, "<html><h1>404: NOT FOUND</h1></html>");
			debug(ERROR, "Message: ", STRNOTFOUND, fd);
			break;
		case NOTALLOWED:
			sprintf(head, STRMETHODNOTA);
			sprintf(body, "<html><h1>405: NOT ALLOWED</h1></html>");
			debug(ERROR, "Message: ", STRMETHODNOTA, fd);
			break;
		case UMEDIATYPE:
			sprintf(head, STRUMEDIATYPE);
			sprintf(body, "<html><h1>415: UNSUPPORTED MEDIA TYPE</h1></html>");
			debug(ERROR, "Message: ", STRUMEDIATYPE, fd);
			break;
		case INTERNALE:
			sprintf(head, STRINTERNALE);
			sprintf(body, "<html><h1>500: INTERNAL SERVER ERROR</h1></html>");
			debug(ERROR, "Message: ", STRINTERNALE, fd);
			break;
	}
	sendMessage(fd, head, body, cookie);
}

//
// Función para enviar un archivo
//
void sendFile(int fd, int file, char *ext, int cookie){
	debug(LOG, "response", "Enviando archivo", fd);
	char head[BUFSIZE]= {0};
	char body[BUFSIZE] = {0};
	// Cargamos el cuerpo
	body[0] = '\0';
	// Cargamos la cabecera
	head[0] = '\0';
	strcat(head, STROK);

	// ----- Campos de la cabecera -----
	// Campo Server
	strcat(head, STRSERVER);
	// Campo Content-Type
	strcat(head, STRTYPE);
	// Campo Content-Length
	int contentLength = lseek(file, 0, SEEK_END);
	sprintf(head + strlen(head), STRCNTLENGTH, contentLength);
	// Reiniciamos el apuntador
	lseek(file, 0, SEEK_SET);
	// Campo Date
	char fecha[DATESIZE];
	getDate(fecha);
	strcat(head, fecha);
	// Campo Connection
	strcat(head, STRCONNECT);
	// Campo Keep-Alive.
	strcat(head, STRKEEPA);
	// Campo COOKIE
	sprintf(head + strlen(head), STRSETCOOK, cookie);
	// ----- Cargamos la cabecera ----
	strcat(head, "\r\n");
	char message [BUFSIZE] = {0};
	strcat(message, head);
	int total = contentLength + strlen(head);
	// ----- Enviamos el cuerpo -----
	// ----- Si el archivo es demasiado grande se fragmenta -----
	do{
		// ----- Enviar mensaje -----
		debug(LOG, "sending", "Loop para el envio del archivo", fd);
		// Variable para leer el contenido del archivo
		char fileContent [BUFSIZE] = {0};
		// Leemos el contenido del archivo
		int readed = read(file, fileContent, BUFSIZE - strlen(message));
		printf("\n LEIDOS: %d\n", readed);
		// Se lo ponemos a la cabecera
		strcat(message, fileContent);
		// Enviamos el mensaje
		int writed = write(fd, message, strlen(message));
		total = total - writed;
		printf("\n TOTAL: %d\n", total);
		debug(LOG, "send: content", message, fd);
		// Miramos si queda algo por mandar
		// Eliminamos el contenido de la cabecera
		free(message);
		char message [BUFSIZE] = {0};
	}while(total > 0);

	// ----- Cerramos el archivo -----
	debug(LOG, "sending", "Terminado correctamente", fd);
	close(file);
}

void process_web_request(int descriptorFichero)
{
	debug(LOG,"request","Ha llegado una peticion",descriptorFichero);
	//
	// Declaración de variables
	//
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(descriptorFichero, &readfds);
	fd_set writefds;
	fd_set exceptfds;
	struct timeval timeout;
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;

	//
	// Loop de conexión persistente
	//	
	do
	{
		//
		// Llega una petición
		//
		debug(LOG,"request", "Entrando en el bucle de peticiones", descriptorFichero);
		//
		// Definir buffer y variables necesarias para leer las peticiones
		//
		char buffer[BUFSIZE]= {0};
		int readed, cookie = 1;
		
		//
		// Leer la petición HTTP
		//
		debug(LOG, "request", "Comienza la lectura de datos", descriptorFichero);
		readed = read(descriptorFichero, &buffer, BUFSIZE);
		//
		// Comprobación de errores de lectura
		//
		if(readed < 0){
			debug(ERROR, "request", strerror(errno), descriptorFichero);
			close(descriptorFichero);
			exit(1);
		}
		//
		// Si la lectura tiene datos válidos terminar el buffer con un \0
		//
		debug(LOG, "request", "Lectura de solicitud finalizada", descriptorFichero);
		buffer[readed] = '\0';
		
		//
		// Se eliminan los caracteres de retorno de carro y nueva linea
		//
		debug(LOG, "request", "Eliminando caracteres de retorno", descriptorFichero);
		int charRemove = eliminarCaracteresRetorno(buffer);
		
		//
		// Generamos la cookie en base a su valor anterior
		//
		debug(LOG, "request", "Generando el valor de la cookie", descriptorFichero);
		if (strncmp(buffer, "Cookie", 6) == 0) {
   			cookie = getCookie(buffer) + 1;
   		}
		//
		// Si el valor de cookie no es valido cerrar conexión
		// si el valor es valido se enviará la cookie
		//
		if (cookie == 0) {
			sendError(descriptorFichero, 400, cookie);
			debug(ERROR, "cookie", "Valor de la cookie no valido", descriptorFichero);
			close(descriptorFichero);
			exit(1);
		} else if (cookie >= MAXACCES) {
			sendError(descriptorFichero, 403, cookie);
			debug(LOG, "cookie", "Valor máximo de conexiones alcanzado", descriptorFichero);
			close(descriptorFichero);
			exit(1);
		}

		//
		//	TRATAR LOS CASOS DE LOS DIFERENTES METODOS QUE SE USAN
		//
		debug(LOG, "request", "Parseando petición...", descriptorFichero);
		char *path = parserFL(buffer);

		// ----------- Petición de tipo GET ----------- 
		if(strncmp("GET", buffer, 3) == 0){
			if (strstr(path, "../") != NULL || strstr(path, "//") != NULL){
				//
				//	Como se trata el caso de acceso ilegal a directorios superiores de la
				//	jerarquia de directorios del sistema
				//
				debug(ERROR, "GET: Path", "Error en el acceso a la jerarquia de directorios", descriptorFichero);
				free(path);
				sendError(descriptorFichero, 403, cookie);
				close(descriptorFichero);
				exit(1);
			}
			// Si la ruta está vacia cargar el index
			if (strcmp(path, "/") == 0)
				strcat(path, "index.html");
			// Comprueba si el archivo está soportado
			int fExt;
			if ((fExt = isExtension(path)) == -1)
			{
				debug(ERROR, "GET: Path", "Extension no soportada", descriptorFichero);
				sendError(descriptorFichero, 415, cookie);
				free(path);
				close(descriptorFichero);
				exit(1);
			}
			int file = open(path+1, O_RDONLY);
			if (file < 0){
				if (errno == EACCES){
					sendError(descriptorFichero, 500, cookie);
					debug(ERROR, "GET: Open", "Problema con permisos", descriptorFichero);
					free(path);
					close(descriptorFichero);
					exit(1);
				} else if (errno == ENOENT){
					sendError(descriptorFichero, 404, cookie);
					debug(ERROR, "GET: Open", "No existe el directorio", descriptorFichero);
					free(path);
					close(descriptorFichero);
					exit(1);
				} else {
					sendError(descriptorFichero, 500, cookie);
					debug(ERROR, "GET: Open", "Otro problema sin determinar", descriptorFichero);
					free(path);
					close(descriptorFichero);
					exit(1);
				}
			}
			// Enviar fichero
			debug(LOG, "SERVING: file", path, descriptorFichero);
			sendFile(descriptorFichero, file, extensions[fExt].filetype, cookie);
		}
		//  ----------- Petición de tipo POST ----------- 
		else if (strncmp("POST", buffer, 4) == 0){

		}
		//  ----------- Petición NO IMPLEMENTADA ----------- 
		else if (strncmp("HEAD", buffer, 4) == 0 || strncmp("PUT", buffer, 3) == 0){
			sendError(descriptorFichero, 405, cookie);
			debug(ERROR, "OTHER", "Petición no soportada", descriptorFichero);
			close(descriptorFichero);
			exit(1);
		}
		//  ----------- Petición NO SOPORTADA ----------- 
		else{
			sendError(descriptorFichero, 405, cookie);
			debug(ERROR, "OTHER", "Petición no soportada", descriptorFichero);
			close(descriptorFichero);
			exit(1);
		}
		free(path);
		debug(LOG, "request", "...Petición parseaeda",  descriptorFichero);

		//
		//	Como se trata el caso excepcional de la URL que no apunta a ningún fichero
		//	html
		//
		
		
		//
		//	Evaluar el tipo de fichero que se está solicitando, y actuar en
		//	consecuencia devolviendolo si se soporta u devolviendo el error correspondiente en otro caso
		//
		
		
		//
		//	En caso de que el fichero sea soportado, exista, etc. se envia el fichero con la cabecera
		//	correspondiente, y el envio del fichero se hace en blockes de un máximo de  8kB
		//

	}
	while (select(descriptorFichero, &readfds, &writefds, &exceptfds, &timeout));
	
	close(descriptorFichero);
	exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd;
	socklen_t length;
	static struct sockaddr_in cli_addr;		// static = Inicializado con ceros
	static struct sockaddr_in serv_addr;	// static = Inicializado con ceros
	
	//  Argumentos que se esperan:
	//
	//	argv[1]
	//	En el primer argumento del programa se espera el puerto en el que el servidor escuchara
	//
	//  argv[2]
	//  En el segundo argumento del programa se espera el directorio en el que se encuentran los ficheros del servidor
	//
	//  Verficiar que los argumentos que se pasan al iniciar el programa son los esperados
	//

	//
	//  Verficiar que el directorio escogido es apto. Que no es un directorio del sistema y que se tienen
	//  permisos para ser usado
	//

	if(chdir(argv[2]) == -1){ 
		(void)printf("ERROR: No se puede cambiar de directorio %s\n",argv[2]);
		exit(4);
	}
	// Hacemos que el proceso sea un demonio sin hijos zombies
	if(fork() != 0)
		return 0; // El proceso padre devuelve un OK al shell

	(void)signal(SIGCHLD, SIG_IGN); // Ignoramos a los hijos
	(void)signal(SIGHUP, SIG_IGN); // Ignoramos cuelgues
	
	debug(LOG,"web server starting...", argv[1] ,getpid());
	
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		debug(ERROR, "system call","socket",0);
	
	port = atoi(argv[1]);
	
	if(port < 0 || port >60000)
		debug(ERROR,"Puerto invalido, prueba un puerto de 1 a 60000",argv[1],0);
	
	/*Se crea una estructura para la información IP y puerto donde escucha el servidor*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*Escucha en cualquier IP disponible*/
	serv_addr.sin_port = htons(port); /*... en el puerto port especificado como parámetro*/
	
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		debug(ERROR,"system call","bind",0);
	
	if( listen(listenfd,64) <0)
		debug(ERROR,"system call","listen",0);
	
	while(1){
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			debug(ERROR,"system call","accept",0);
		if((pid = fork()) < 0) {
			debug(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	// Proceso hijo
				(void)close(listenfd);
				process_web_request(socketfd); // El hijo termina tras llamar a esta función
			} else { 	// Proceso padre
				(void)close(socketfd);
			}
		}
	}
}
